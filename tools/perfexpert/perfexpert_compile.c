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
#include "perfexpert.h"
#include "perfexpert_output.h"
#include "perfexpert_util.h"
#include "perfexpert_fork.h"
#include "perfexpert_constants.h"

/* compile_program */
int compile_program(void) {
    char temp_str[BUFFER_SIZE];
    char *argv[PARAM_SIZE];
    char flags[BUFFER_SIZE];
    int  argc = 0;
    test_t test;

    OUTPUT_VERBOSE((4, "%s", _BLUE("Compiling the program")));
    OUTPUT(("%s [%s]", _YELLOW("Compiling"), globals.program));

    /* If the source file was provided generate compilation command line */
    if (NULL != globals.sourcefile) {
        /* Which compiler should I use? */
        argv[0] = getenv("CC");
        if (NULL == argv[0]) {
            argv[0] = DEFAULT_COMPILER;
        }

        /* What should be the compiler output and the source code? */
        argv[1] = "-o";
        argv[2] = globals.program;
        argv[3] = globals.sourcefile;

        /* What are the default and user defined compiler flags? */
        bzero(flags, BUFFER_SIZE);
        strcat(flags, DEFAULT_CFLAGS);
        if (NULL != getenv("CFLAGS")) {
            strcat(flags, getenv("CFLAGS"));
            strcat(flags, " ");
        }
        if (NULL != getenv("PERFEXPERT_CFLAGS")) {
            strcat(flags, getenv("PERFEXPERT_CFLAGS"));
        }

        argc = 4;
        argv[argc] = strtok(flags, " ");
        do {
            argc++;
        } while (argv[argc] = strtok(NULL, " "));
    }
	// 只是形成了编译的指令，没有执行吧？

    /* If the user chose a Makefile... */
    if (NULL != globals.target) {
        if (PERFEXPERT_SUCCESS != perfexpert_util_file_exists("./Makefile")) {
            OUTPUT(("%s", _ERROR("Error: Makefile file not found")));
            return PERFEXPERT_ERROR;                    
        }

        argv[0] = "make";
        argv[1] = globals.target;
        argc = 2;

        if (NULL != getenv("CFLAGS")) {
            strcat(flags, getenv("CFLAGS"));
            strcat(flags, " ");
        }
        if (NULL != getenv("PERFEXPERT_CFLAGS")) {
            strcat(flags, getenv("PERFEXPERT_CFLAGS"));
        }
        setenv("CFLAGS", flags, 1);
    }

    /* In both cases we should add a NULL */
    argv[argc] = NULL;		// WHY?

    /* Not using OUTPUT_VERBOSE because I want only one line */
    if (8 <= globals.verbose) {
        int i;
        printf("%s    %s", PROGRAM_PREFIX, _YELLOW("command line:"));
        for (i = 0; i < argc; i++) {
            printf(" %s", argv[i]);
        }
        printf("\n");
    }

    /* Fill the ninja test structure... */
    test.info = globals.sourcefile;
    test.input = NULL;
    bzero(temp_str, BUFFER_SIZE);
    sprintf(temp_str, "%s/compile.output", globals.stepdir);
    test.output = temp_str;

    /* fork_and_wait_and_pray */
    return fork_and_wait(&test, argv);
}

#ifdef __cplusplus
}
#endif

// EOF
