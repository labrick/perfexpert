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
    
/* PerfExpert headers */
#include "analyzer.h" 
#include "perfexpert_alloc.h"
#include "perfexpert_constants.h"
#include "perfexpert_list.h"
#include "perfexpert_output.h"
#include "install_dirs.h"

/* Global variables, try to not create them! */
globals_t globals; // Variable to hold global options, this one is OK

/* main, life starts here */
int main(int argc, char **argv) {
    perfexpert_list_t profiles;

    /* Set default values for globals */
    globals = (globals_t) {
        .threshold       = 0.0,              // double		// cpi阈值，小于它才是好
        .tool            = "hpctoolkit",     // char *
        .inputfile       = NULL,             // char *
        .outputfile      = NULL,             // char *
        .outputfile_FP   = stdout,           // char *
        .aggregate       = PERFEXPERT_FALSE, // int
        .thread          = -1,               // int
        .machinefile     = NULL,             // char *
        .machine_by_name = NULL,             // metric_t *
        .lcpifile        = NULL,             // char *		// 运行微benchmark后生成 YANG
        .lcpi_by_name    = NULL,             // lcpi_t *
        .verbose         = 0,                // int			// 这个是干嘛的？
        .colorful        = PERFEXPERT_FALSE, // int
        .order           = "none",           // char *
        .found_hotspots  = PERFEXPERT_FALSE, // int
        .outputmetrics   = NULL              // char *
    };

    /* Initialize list of profiles */
	// profiles是干什么的要搞明白。。。。
    perfexpert_list_construct(&profiles);

    /* Parse command-line parameters */
    if (PERFEXPERT_SUCCESS != parse_cli_params(argc, argv)) {
        OUTPUT(("%s", _ERROR("Error: parsing command line arguments")));
        return PERFEXPERT_ERROR;
    }

    /* Set default values */
	// 构建完整路径放入machinefile和lcpifile
    if (NULL == globals.machinefile) {
        PERFEXPERT_ALLOC(char, globals.machinefile,
            (strlen(PERFEXPERT_ETCDIR) + strlen(MACHINE_FILE) + 2));
        sprintf(globals.machinefile, "%s/%s", PERFEXPERT_ETCDIR, MACHINE_FILE);
    }
    if (NULL == globals.lcpifile) {
        PERFEXPERT_ALLOC(char, globals.lcpifile,
            (strlen(PERFEXPERT_ETCDIR) + strlen(LCPI_FILE) + 2));
        sprintf(globals.lcpifile, "%s/%s", PERFEXPERT_ETCDIR, LCPI_FILE);
    }

    /* Print to a file or STDOUT? */
    if (NULL != globals.outputfile) {
        OUTPUT_VERBOSE((7, "   %s (%s)",
            _YELLOW("printing recommendations to file"), globals.outputfile));
        if (NULL == (globals.outputfile_FP = fopen(globals.outputfile, "w+"))) {
            OUTPUT(("%s (%s)", _ERROR("Error: unable to open output file"),
                globals.outputfile));
            return PERFEXPERT_ERROR;
        }
    } else {
        OUTPUT_VERBOSE((7, "   printing recommendations to STDOUT"));
    }

	// 下面的解析基本都是以'='为delim，前后内容得到合理的存储
	//---------------------------------
    /* Parse LCPI metrics */
	// 分析LCPI指标文件，也就是将lcpifile中名称/公式通过hash表合理存储
    if (PERFEXPERT_SUCCESS != lcpi_parse_file(globals.lcpifile)) {
        OUTPUT(("%s (%s)", _ERROR("Error: LCPI file is not valid"),
            globals.lcpifile));
        return PERFEXPERT_ERROR;
    }

    /* Parse machine characterization */
	// 分析机器特征
    if (PERFEXPERT_SUCCESS != machine_parse_file(globals.machinefile)) {
        OUTPUT(("%s (%s)",
            _ERROR("Error: Machine characterization file is not valid"),
            globals.machinefile));
        return PERFEXPERT_ERROR;
    }

    /* Parse input file */
	// profile是个什么东东？？这个分析有点意思，用了一个工具，但又不知道怎么用的
    if (PERFEXPERT_SUCCESS != profile_parse_file(globals.inputfile,
        globals.tool, &profiles)) {
        OUTPUT(("%s (%s)", _ERROR("Error: unable to parse input file"),
            globals.inputfile));
        return PERFEXPERT_ERROR;
    }

    /* Check and flatten profiles */
	// 也就是把profiles(循环)->callees(循环)->callpaths(循环)三个循环全部遍历
	// 但是每个循环里存储的是什么信息呢？？？
    if (PERFEXPERT_SUCCESS != profile_check_all(&profiles)) {
        OUTPUT(("%s", _ERROR("Error: checking profile")));
        return PERFEXPERT_ERROR;
    }
    if (PERFEXPERT_SUCCESS != profile_flatten_all(&profiles)) {
        OUTPUT(("%s (%s)", _ERROR("Error: flatening profiles"),
            globals.inputfile));
        return PERFEXPERT_ERROR;
    }

    if (PERFEXPERT_FALSE == globals.found_hotspots) {
        OUTPUT(("%s (%s)", _ERROR("Error: no hotspots found. Empty file?"),
            globals.inputfile));
        return PERFEXPERT_NO_HOTSPOTS;
    }

    /* Sort hotspots */
    if (NULL != globals.order) {
        if (PERFEXPERT_SUCCESS != hotspot_sort(&profiles)) {
            OUTPUT(("%s", _ERROR("Error: while sorting hotspots")));
            return PERFEXPERT_ERROR;
        }
    }

    /* Output analysis report */
    if (PERFEXPERT_SUCCESS != output_analysis_all(&profiles)) {
        OUTPUT(("%s", _ERROR("Error: printing profiles analysis")));
        return PERFEXPERT_ERROR;
    }

    /* Output list of metrics per hotspot (input for perfexpert_recommender) */
    if (NULL != globals.outputmetrics) {
        if (PERFEXPERT_SUCCESS != output_metrics_all(&profiles)) {
            OUTPUT(("%s", _ERROR("Error: printing profiles metrics")));
            return PERFEXPERT_ERROR;
        }
    }

    return PERFEXPERT_SUCCESS;
}

#ifdef __cplusplus
}
#endif

// EOF
