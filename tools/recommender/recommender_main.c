/*
 * Copyright (c) 2011-2013  University of Texas at Austin. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * This file is part of PerfExpert.
 *
 * PerfExpert is free software: you can redistribute it and/or modify it under
 * the terms of the The University of Texas at Austin Research License
 * 
 * PerfExpert is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.
 * 
 * Authors: Leonardo Fialho and Ashay Rane
 *
 * $HEADER$
 */

#ifdef __cplusplus
extern "C" {
#endif

/* System standard headers */
#include <stdio.h>
#include <stdlib.h>

/* Utility headers */
#include <sqlite3.h>

/* PerfExpert headers */
#include "recommender.h"
#include "perfexpert_alloc.h"
#include "perfexpert_database.h"
#include "perfexpert_list.h"
#include "perfexpert_output.h"

/* Global variables, try to not create them! */
globals_t globals; // Variable to hold global options, this one is OK

/* main, life starts here */
int main(int argc, char** argv) {
    perfexpert_list_t segments;
    int rc = PERFEXPERT_ERROR;
    
    /* Set default values for globals */
    globals = (globals_t) {
        .verbose          = 0,                // int
        .inputfile        = NULL,             // char *		// 文件名
        .inputfile_FP     = NULL,             // FILE *		// 文件指针
        .outputfile       = NULL,             // char *
        .outputfile_FP    = stdout,           // FILE *
        .outputmetrics    = NULL,             // char *
        .outputmetrics_FP = NULL,             // FILE *
        .dbfile           = NULL,             // char *		// SQL语句文件
        .workdir          = NULL,             // char *
        .pid              = (long)getpid(),   // long int	// 这个pid的意义何在？
        .colorful         = PERFEXPERT_FALSE, // int
        .metrics_file     = NULL,             // char *		// 什么指标？？
        .metrics_table    = METRICS_TABLE,    // char *		// 就是PMU内容吧？
        .rec_count        = 3                 // int		// 这个啥？
    };

    /* Parse command-line parameters */
	// 哦哦，分析该程序所给的参数，嗯嗯！是的，right
    if (PERFEXPERT_SUCCESS != parse_cli_params(argc, argv)) {
        OUTPUT(("%s", _ERROR("Error: parsing command line arguments")));
        return PERFEXPERT_ERROR;
    }

    /* Create the list of code bottlenecks */
	// 内联函数中只是简单的将segments的sentinel前后指针都指向自身
    perfexpert_list_construct(&segments);
    
    /* Open input file */
	// 也就是打开所要分析的程序的文件
    OUTPUT_VERBOSE((3, "   %s (%s)", _YELLOW("performance analysis input"),
        globals.inputfile));
    if (NULL == (globals.inputfile_FP = fopen(globals.inputfile, "r"))) {
        OUTPUT(("%s (%s)", _ERROR("Error: unable to open input file"),
            globals.inputfile));
        return PERFEXPERT_ERROR;
    }

    /* Print to a file or STDOUT is? */
    if (NULL != globals.outputfile) {
        OUTPUT_VERBOSE((7, "   %s (%s)", _YELLOW("printing report to file"),
            globals.outputfile));
        globals.outputfile_FP = fopen(globals.outputfile, "w+");
        if (NULL == globals.outputfile_FP) {
            OUTPUT(("%s (%s)", _ERROR("Error: unable to open report file"),
                globals.outputfile));
            goto CLEANUP;
        }
    } else {
        OUTPUT_VERBOSE((7, "   printing report to STDOUT"));
    }

    /* If necessary open outputmetrics_FP */
	// 输出指标到指标文件？？？这个的意义何在？
    if (NULL != globals.outputmetrics) {
        OUTPUT_VERBOSE((7, "   %s (%s)", _YELLOW("printing metrics to file"),
            globals.outputmetrics));
        globals.outputmetrics_FP = fopen(globals.outputmetrics, "w+");
        if (NULL == globals.outputmetrics_FP) {
            OUTPUT(("%s (%s)",
                _ERROR("Error: unable to open output metrics file"),
                globals.outputmetrics));
            goto CLEANUP;
        }
    }

    /* Connect to database */
    if (PERFEXPERT_SUCCESS != perfexpert_database_connect(&(globals.db),
        globals.dbfile)) {
        OUTPUT(("%s", _ERROR("Error: connecting to database")));
        goto CLEANUP;
    }

    /* Parse metrics file if 'm' is defined, this will create a temp table */
	// 指标文件的意义有待于确定
    if (NULL != globals.metrics_file) {
        PERFEXPERT_ALLOC(char, globals.metrics_table, (strlen("metrics_") + 6));
        sprintf(globals.metrics_table, "metrics_%d", (int)getpid());

		// 也就是以metrics文件中的变量建立了表格
        if (PERFEXPERT_SUCCESS != parse_metrics_file()) {
            OUTPUT(("%s", _ERROR("Error: parsing metrics file")));
            goto CLEANUP;
        }
    }

    /* Parse input parameters */
	// 什么是输入参数？？？怎么分析？
    if (PERFEXPERT_SUCCESS != (rc = parse_segment_params(&segments))) {
        OUTPUT(("%s", _ERROR("Error: parsing input params")));
        goto CLEANUP;
    }

    /* Select recommendations */
	// 这个是重点
    rc = select_recommendations_all(&segments);

    CLEANUP:
    /* Close input file */
    if (NULL != globals.inputfile) {
        fclose(globals.inputfile_FP);
    }
    if (NULL != globals.outputfile) {
        fclose(globals.outputfile_FP);
    }
    if (NULL != globals.outputmetrics) {
        fclose(globals.outputmetrics_FP);
    }
    perfexpert_database_disconnect(globals.db);

    /* Free memory */
    if (NULL != globals.metrics_file) {
        PERFEXPERT_DEALLOC(globals.metrics_table);
    }

    return rc;
}

#ifdef __cplusplus
}
#endif

// EOF
