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

/* recommendation */
int recommendation(void) {
    char temp_str[8][BUFFER_SIZE];
    char *argv[2];
    test_t test;
    int rc;

    OUTPUT_VERBOSE((4, "%s", _BLUE("Recommendations phase")));
    OUTPUT(("%s", _YELLOW("Selecting optimizations")));

    /* Set some environment variables to avoid working arguments */
    // 同样先设置一大堆环境变量，哦哦，这是一种传递参数的方式
    bzero(temp_str[1], BUFFER_SIZE);
    sprintf(temp_str[1], "%d", globals.colorful);
    if (0 != setenv("PERFEXPERT_RECOMMENDER_COLORFUL", temp_str[1], 0)) {
        OUTPUT(("%s", _ERROR("Error: unable to set environment variable")));
        return PERFEXPERT_ERROR;
    }
    bzero(temp_str[2], BUFFER_SIZE);
    sprintf(temp_str[2], "%d", globals.verbose);
    if (0 != setenv("PERFEXPERT_RECOMMENDER_VERBOSE_LEVEL", temp_str[2], 0)) {
        OUTPUT(("%s", _ERROR("Error: unable to set environment variable")));
        return PERFEXPERT_ERROR;
    }
    bzero(temp_str[3], BUFFER_SIZE);
    sprintf(temp_str[3], "%d", (int)getpid());
    if (0 != setenv("PERFEXPERT_RECOMMENDER_PID", temp_str[3], 0)) {
        OUTPUT(("%s", _ERROR("Error: unable to set environment variable")));
        return PERFEXPERT_ERROR;
    }
    bzero(temp_str[4], BUFFER_SIZE);
    sprintf(temp_str[4], "%d", globals.rec_count);
    if (0 != setenv("PERFEXPERT_RECOMMENDER_REC_COUNT", temp_str[4], 0)) {
        OUTPUT(("%s", _ERROR("Error: unable to set environment variable")));
        return PERFEXPERT_ERROR;
    }
    bzero(temp_str[5], BUFFER_SIZE);
    sprintf(temp_str[5], "%s/%s", globals.stepdir, ANALYZER_METRICS);
    if (0 != setenv("PERFEXPERT_RECOMMENDER_INPUT_FILE", temp_str[5], 1)) {
        OUTPUT(("%s", _ERROR("Error: unable to set environment variable")));
        return PERFEXPERT_ERROR;
    }
    bzero(temp_str[6], BUFFER_SIZE);
    sprintf(temp_str[6], "%s/%s", globals.stepdir, RECOMMENDER_REPORT);
    if (0 != setenv("PERFEXPERT_RECOMMENDER_OUTPUT_FILE", temp_str[6], 1)) {

        OUTPUT(("%s", _ERROR("Error: unable to set environment variable")));
        return PERFEXPERT_ERROR;
    }
    bzero(temp_str[7], BUFFER_SIZE);
    sprintf(temp_str[7], "%s/%s", globals.stepdir, RECOMMENDER_METRICS);
    if (0 != setenv("PERFEXPERT_RECOMMENDER_METRICS_FILE", temp_str[7], 1)) {
        OUTPUT(("%s", _ERROR("Error: unable to set environment variable")));
        return PERFEXPERT_ERROR;
    }
    if (0 != setenv("PERFEXPERT_RECOMMENDER_DATABASE_FILE", globals.dbfile,
        0)) {
        OUTPUT(("%s", _ERROR("Error: unable to set environment variable")));
        return PERFEXPERT_ERROR;
    }
    if (0 != setenv("PERFEXPERT_RECOMMENDER_WORKDIR", globals.stepdir, 1)) {
        OUTPUT(("%s", _ERROR("Error: unable to set environment variable")));
        return PERFEXPERT_ERROR;
    }

    /* Arguments to run analyzer */
    // 没给路径怎么运行？哦哦，他把这些程序编译好后都放入了环境变量中了
    argv[0] = RECOMMENDER_PROGRAM;
    argv[1] = NULL;

    /* The super-ninja test sctructure */
    bzero(temp_str[0], BUFFER_SIZE);
    sprintf(temp_str[0], "%s/%s", globals.stepdir, RECOMMENDER_OUTPUT);
    test.output = temp_str[0];
    test.input = NULL;
    // metrics.txt
    test.info = temp_str[5];

    /* run_and_fork_and_pray */
    return fork_and_wait(&test, argv);
}

#ifdef __cplusplus
}
#endif

// EOF
