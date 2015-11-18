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
#include "perfexpert_fork.h"
#include "perfexpert_constants.h"

/* transformation */
// 什么叫转换？转换什么？
int transformation(void) {
    char temp_str[6][BUFFER_SIZE];
    char *argv[2];
    test_t test;

    OUTPUT_VERBOSE((4, "%s", _BLUE("Code transformation phase")));
    OUTPUT(("%s", _YELLOW("Applying optimizations")));

    /* Set some environment variables to avoid working arguments */
    bzero(temp_str[1], BUFFER_SIZE);
    sprintf(temp_str[1], "%d", globals.verbose);
    if (0 != setenv("PERFEXPERT_CT_VERBOSE_LEVEL", temp_str[1], 0)) {
        OUTPUT(("%s", _ERROR("Error: unable to set environment variable")));
        return PERFEXPERT_ERROR;
    }
    bzero(temp_str[2], BUFFER_SIZE);
    sprintf(temp_str[2], "%d", globals.colorful);
    if (0 != setenv("PERFEXPERT_CT_COLORFUL", temp_str[2], 0)) {
        OUTPUT(("%s", _ERROR("Error: unable to set environment variable")));
        return PERFEXPERT_ERROR;
    }
    bzero(temp_str[3], BUFFER_SIZE);
    // 这些pid有啥用？
    sprintf(temp_str[3], "%d", (int)getpid());
    if (0 != setenv("PERFEXPERT_CT_PID", temp_str[3], 1)) {
        OUTPUT(("%s", _ERROR("Error: unable to set environment variable")));
        return PERFEXPERT_ERROR;
    }
    bzero(temp_str[4], BUFFER_SIZE);
    sprintf(temp_str[4], "%s/%s", globals.stepdir, RECOMMENDER_METRICS);
    if (0 != setenv("PERFEXPERT_CT_INPUT_FILE", temp_str[4], 1)) {
        OUTPUT(("%s", _ERROR("Error: unable to set environment variable")));
        return PERFEXPERT_ERROR;
    }
    bzero(temp_str[5], BUFFER_SIZE);
    sprintf(temp_str[5], "%s/%s", globals.stepdir, CT_REPORT);
    if (0 != setenv("PERFEXPERT_CT_OUTPUT_FILE", temp_str[5], 1)) {
        OUTPUT(("%s", _ERROR("Error: unable to set environment variable")));
        return PERFEXPERT_ERROR;
    }
    if (0 != setenv("PERFEXPERT_CT_DATABASE_FILE", globals.dbfile, 0)) {
        OUTPUT(("%s", _ERROR("Error: unable to set environment variable")));
        return PERFEXPERT_ERROR;
    }
    if (0 != setenv("PERFEXPERT_CT_WORKDIR", globals.stepdir, 1)) {
        OUTPUT(("%s", _ERROR("Error: unable to set environment variable")));
        return PERFEXPERT_ERROR;
    }

    /* Arguments to run analyzer */
    // ct是啥？
    argv[0] = CT_PROGRAM;
    argv[1] = NULL;

    /* The super-ninja test sctructure */
    bzero(temp_str[0], BUFFER_SIZE);
    sprintf(temp_str[0], "%s/%s", globals.stepdir, CT_OUTPUT);
    test.output = temp_str[0];
    test.input = NULL;
    test.info = temp_str[4];

    /* run_and_fork_and_pray */
    return fork_and_wait(&test, argv);
}

#ifdef __cplusplus
}
#endif

// EOF
