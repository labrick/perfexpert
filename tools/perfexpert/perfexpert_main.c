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
#include <string.h>

/* PerfExpert headers */
#include "config.h"
#include "perfexpert.h"
#include "perfexpert_alloc.h"
#include "perfexpert_constants.h"
#include "perfexpert_database.h"
#include "perfexpert_output.h"
#include "perfexpert_util.h"

/* Global variables, try to not create them! */
globals_t globals; // Variable to hold global options, this one is OK

/* main, life starts here */
int main(int argc, char** argv) {
    char workdir_template[] = ".perfexpert-temp.XXXXXX";
    char *workdir;
    char *file;
    int rc = PERFEXPERT_ERROR;

    /* Set default values for globals */
    globals = (globals_t) {
        .verbose       = 0,                // int
        .colorful      = PERFEXPERT_FALSE, // int
        .dbfile        = NULL,             // *char
        .threshold     = 0.0,              // float
        .rec_count     = 3,                // int
        .leave_garbage = PERFEXPERT_FALSE, // int
        .pid           = (long)getpid(),   // long int
        .target        = NULL,             // *char
        .sourcefile    = NULL,             // *char
        .program       = NULL,             // *char
        .program_path  = NULL,             // *char
        .program_full  = NULL,             // *char
        .program_argv  = { 0 },            // *char[]
        .step          = 1,                // int
        .workdir       = NULL,             // *char
        .stepdir       = NULL,             // *char
        .prefix        = { 0 },            // *char[]
        .before        = { 0 },            // *char[]
        .after         = { 0 },            // *char[]
        .knc           = NULL,             // *char
        .knc_prefix    = { 0 },            // *char[]
        .knc_before    = { 0 },            // *char[]
        .knc_after     = { 0 },            // *char[]
        .inputfile     = NULL,             // *char
        .only_exp      = PERFEXPERT_FALSE, // int
        .compat_mode   = PERFEXPERT_FALSE, // int
        .order         = "none",           // *char
        .tool          = "hpctoolkit"     // *char
    };

    /* Parse command-line parameters */
	// 分析执行该程序时输入的参数，将输入放入globals结构体，并按要求行事
    if (PERFEXPERT_SUCCESS != parse_cli_params(argc, argv)) {
        OUTPUT(("%s", _ERROR("Error: parsing command line arguments")));
        return PERFEXPERT_ERROR;
    }

    /* Create a work directory */
    workdir = mkdtemp(workdir_template);
    if (NULL == workdir) {
        OUTPUT(("%s", _ERROR("Error: creating working directory")));
        return PERFEXPERT_ERROR;
    }
    PERFEXPERT_ALLOC(char, globals.workdir,
        (strlen(getcwd(NULL, 0)) + strlen(workdir) + 2));
    sprintf(globals.workdir, "%s/%s", getcwd(NULL, 0), workdir);
    OUTPUT_VERBOSE((5, "   %s %s", _YELLOW("workdir:"), globals.workdir));

    /* If database was not specified, check if there is any local database and
     * if this database is update
	 * 在一个新的文件中记录系统和输入的db版本号，如果新输入的版本号新则报错，
	 * 相同则直接执行，输入的老，则更新输入的db，并将版本号更新为系统的
     */
    if (NULL == globals.dbfile) {
        if (PERFEXPERT_SUCCESS !=
            perfexpert_database_update(&(globals.dbfile))) {
            OUTPUT(("%s", _ERROR((char *)"Error: unable to copy database")));
            goto CLEANUP;
        }        
    } else {
		// 输入的db文件存在
        if (PERFEXPERT_SUCCESS != perfexpert_util_file_exists(globals.dbfile)) {
            OUTPUT(("%s", _ERROR((char *)"Error: database file not found")));
            goto CLEANUP;
        }
    }
    OUTPUT_VERBOSE((5, "   %s %s", _YELLOW("database:"), globals.dbfile));

    /* Iterate until some tool return != PERFEXPERT_SUCCESS */
    while (1) {
        /* Create step working directory */
        PERFEXPERT_ALLOC(char, globals.stepdir, (strlen(globals.workdir) + 5));
        sprintf(globals.stepdir, "%s/%d", globals.workdir, globals.step);
		// mkdir -p stepdir，创建临时工作目录
        if (PERFEXPERT_ERROR == perfexpert_util_make_path(globals.stepdir,
            0755)) {
            OUTPUT(("%s", _ERROR((char *)"Error: cannot create step workdir")));
            goto CLEANUP;
        }
        OUTPUT_VERBOSE((5, "   %s %s", _YELLOW("stepdir:"), globals.stepdir));

        /* If necessary, compile the user program */
        if ((NULL != globals.sourcefile) || (NULL != globals.target)) {
            if (PERFEXPERT_SUCCESS != compile_program()) {
                OUTPUT(("%s", _ERROR("Error: program compilation failed")));
                if (NULL != globals.knc) {
                    OUTPUT(("Are you adding the flags to to compile for MIC?"));
                }
                goto CLEANUP;
            }
        }

        /* Call measurement tool */
        // 这里都是干了什么事？这个先不管
        if (PERFEXPERT_SUCCESS != measurements()) {
            OUTPUT(("%s", _ERROR("Error: while taking measurements")));
            goto CLEANUP;
        }

        /* Escape to perfexpert_run_exp */
        // 如果设置了仅仅执行TARGET，不分析，则直接退出
        if (PERFEXPERT_TRUE == globals.only_exp) {
            OUTPUT((""));
            OUTPUT(("%s the performance measurements collected are available",
                _BOLDRED("NOTICE:")));
            OUTPUT(("        in %s", _MAGENTA(globals.stepdir)));
            OUTPUT(("        to analyze them, run:"));
            OUTPUT((""));
            OUTPUT(("perfexpert_analyzer threshold %s/%s", globals.stepdir,
                globals.toolmodule.profile_file));
            OUTPUT((""));
            goto CLEANUP;
        }

        /* Call analyzer */
        // ../analyzer/analyzer_main.c程序
        switch ((rc = analysis())) {
            case PERFEXPERT_FAILURE:
            case PERFEXPERT_ERROR:
            case PERFEXPERT_FORK_ERROR:
                OUTPUT(("%s", _ERROR("Error: while running analyzer")));
                goto CLEANUP;

            case PERFEXPERT_NO_HOTSPOTS:
                OUTPUT(("%s no hotspots found. Generally, this happens when the"
                    " target", _BOLDRED("WARNING:")));
                OUTPUT(("         program execution is to short, thus "
                    "PerfExpert is unable"));
                OUTPUT(("         to collect enough samples from program run. "
                    "Try again"));
                OUTPUT(("         using a bigger workload"));
                goto CLEANUP;

            case PERFEXPERT_SUCCESS:
                PERFEXPERT_ALLOC(char, file,
                    (strlen(globals.stepdir) + strlen(ANALYZER_REPORT) + 2));
                sprintf(file, "%s/%s", globals.stepdir, ANALYZER_REPORT);
                if (PERFEXPERT_SUCCESS != perfexpert_util_file_print(file)) {
                    OUTPUT(("%s", _ERROR("Error: unable to show report file")));
                }
                PERFEXPERT_DEALLOC(file);
        }

        /* Call recommender */
        switch ((rc = recommendation())) {
            case PERFEXPERT_ERROR:
            case PERFEXPERT_FAILURE:
            case PERFEXPERT_FORK_ERROR:
                OUTPUT(("%s", _ERROR("Error: while running recommender")));
                goto CLEANUP;

            case PERFEXPERT_NO_HOTSPOTS:
                OUTPUT(("No hostspots found"));
                goto CLEANUP;

            case PERFEXPERT_NO_REC:
                OUTPUT(("PerfExpert has no recommendations for this code"));
                goto CLEANUP;

            case PERFEXPERT_SUCCESS:
                PERFEXPERT_ALLOC(char, file,
                    (strlen(globals.stepdir) + strlen(RECOMMENDER_REPORT) + 2));
                sprintf(file, "%s/%s", globals.stepdir, RECOMMENDER_REPORT);
                if (PERFEXPERT_SUCCESS != perfexpert_util_file_print(file)) {
                    OUTPUT(("%s",
                        _ERROR("Error: unable to show recommendations")));
                }
                PERFEXPERT_DEALLOC(file);
        }

        if ((NULL != globals.sourcefile) || (NULL != globals.target)) {
            #if HAVE_CODE_TRANSFORMATION
            /* Call code transformer */
            switch ((rc = transformation())) {
                case PERFEXPERT_ERROR:
                case PERFEXPERT_FAILURE:
                case PERFEXPERT_FORK_ERROR:
                    OUTPUT(("%s",
                        _ERROR("Error: while running code transformer")));
                    goto CLEANUP;

                case PERFEXPERT_NO_TRANS:
                    OUTPUT(("PerfExpert has no automatic optimization for this "
                        "code. If there is"));
                    OUTPUT(("any recommendation (shown above) try to apply them"
                        " manually"));
                    goto CLEANUP;

                case PERFEXPERT_SUCCESS:
                    PERFEXPERT_ALLOC(char, file,
                        (strlen(globals.stepdir) + strlen(CT_REPORT) + 2));
                    sprintf(file, "%s/%s", globals.stepdir, CT_REPORT);
                    if (PERFEXPERT_SUCCESS != perfexpert_util_file_print(file)) {
                        OUTPUT(("%s",
                            _ERROR("Error: unable to show transformations")));
                    }
                    PERFEXPERT_DEALLOC(file);
            }
            #else
            rc = PERFEXPERT_SUCCESS;
            goto CLEANUP;            
            #endif
        } else {
            rc = PERFEXPERT_SUCCESS;
            goto CLEANUP;
        }
        OUTPUT(("%s", _BLUE("Starting another optimization round...")));
        PERFEXPERT_DEALLOC(globals.stepdir);
        globals.step++;
    }

    CLEANUP:
    /* Remove the garbage */
    if (PERFEXPERT_FALSE == globals.leave_garbage) {
        OUTPUT_VERBOSE((1, "%s", _BLUE("Removing temporary directory")));
        if (PERFEXPERT_SUCCESS != perfexpert_util_remove_dir(globals.workdir)) {
            OUTPUT(("unable to remove work directory (%s)", globals.workdir));
        }        
    } else {
        OUTPUT(("%s [%s]", _CYAN("Temporary files are available in"),
            globals.workdir));
    }

    /* Free memory */
    PERFEXPERT_DEALLOC(globals.workdir);
    PERFEXPERT_DEALLOC(globals.stepdir);

    return rc;
}

#ifdef __cplusplus
}
#endif

// EOF
