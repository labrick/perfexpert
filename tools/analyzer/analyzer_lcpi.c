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
#include <string.h>
#include <stdio.h>

/* Utility headers */
#include <matheval.h>

/* PerfExpert headers */
#include "analyzer.h"
#include "perfexpert_alloc.h"
#include "perfexpert_constants.h"
#include "perfexpert_hash.h"
#include "perfexpert_list.h"
#include "perfexpert_md5.h"
#include "perfexpert_output.h"
#include "perfexpert_string.h"

/* lcpi_parse_file */
int lcpi_parse_file(const char *file) {
    char buffer[BUFFER_SIZE];
    FILE *lcpi_FP = NULL;
    lcpi_t *lcpi = NULL;
    int line = 0;

    OUTPUT_VERBOSE((4, "%s", _BLUE("Loading LCPI metrics")));

    if (NULL == (lcpi_FP = fopen(file, "r"))) {
        OUTPUT(("%s (%s)", _ERROR("Error: unable to open LCPI file"), file));
        return PERFEXPERT_ERROR;
    }

    bzero(buffer, BUFFER_SIZE);
    while (NULL != fgets(buffer, BUFFER_SIZE - 1, lcpi_FP)) {
        char *token = NULL;

        line++;

        /* Ignore comments and blank lines */
        if ((0 == strncmp("#", buffer, 1)) ||
            (strspn(buffer, " \t\r\n") == strlen(buffer))) {
            continue;
        }

        /* Remove the end \n character */
        buffer[strlen(buffer) - 1] = '\0';

        /* Replace some characters just to provide a safe expression */
        perfexpert_string_replace_char(buffer, ':', '_');

        /* Allocate and set LCPI data */
        PERFEXPERT_ALLOC(lcpi_t, lcpi, sizeof(lcpi_t));
        lcpi->value = 0.0;

		// 以'='拆分字符串
        token = strtok(buffer, "=");
        PERFEXPERT_ALLOC(char, lcpi->name, strlen(token) + 1);
		// 将'='左侧内容去除空格后放入lcpi->name
		// md5值放入lcpi->name_md5
        strcpy(lcpi->name, perfexpert_string_remove_char(token, ' '));
        strcpy(lcpi->name_md5, perfexpert_md5_string(lcpi->name));

        token = strtok(NULL, "=");
		// Create evaluator object from string containing mathematical 
		// representation of function. Evaluator object could be used later 
		// to evaluate function for specific variable values or to calculate 
		// function derivative over some variable.
        lcpi->expression = evaluator_create(token);
        if (NULL == lcpi->expression) {
            OUTPUT(("%s (%s)", _ERROR("Error: invalid expression at line"),
                line));
            return PERFEXPERT_ERROR;
        }

        /* Add LCPI to global hash of LCPIs */
		// hash方面的一些计算，不断的进行重定义，可烦人
		// 也就是把上面处理得到的信息得到合理的存储
		// 将lcpi和lcpi_by_name通过hash表关联？？？
        perfexpert_hash_add_str(globals.lcpi_by_name, name_md5, lcpi);

        OUTPUT_VERBOSE((7, "   [%s]=[%s] (%s)", lcpi->name,
            evaluator_get_string(lcpi->expression), lcpi->name_md5));
    }

    OUTPUT_VERBOSE((4, "   (%d) %s",
        perfexpert_hash_count_str(globals.lcpi_by_name),
        _MAGENTA("LCPI metric(s) found")));

    return PERFEXPERT_SUCCESS;
}

/* lcpi_compute */
int lcpi_compute(profile_t *profile) {
    lcpi_t *hotspot_lcpi = NULL, *lcpi = NULL;
    procedure_t *hotspot = NULL;
    double *values = NULL;
    char **names = NULL;
    int count = 0;

    OUTPUT_VERBOSE((4, "   %s", _CYAN("Calculating LCPI metrics")));

    /* For each hotspot in this profile... */
    hotspot = (procedure_t *)perfexpert_list_get_first(&(profile->hotspots));
    while ((perfexpert_list_item_t *)hotspot != &(profile->hotspots.sentinel)) {
        OUTPUT_VERBOSE((8, "    [%d] %s", hotspot->id, _YELLOW(hotspot->name)));

        /* For each LCPI definition */
        for (lcpi = globals.lcpi_by_name; lcpi != NULL;
            lcpi = lcpi->hh_str.next) {

            /* Copy LCPI definitions into this hotspot LCPI hash */
            PERFEXPERT_ALLOC(lcpi_t, hotspot_lcpi, sizeof(lcpi_t));
            strcpy(hotspot_lcpi->name_md5, lcpi->name_md5);
            hotspot_lcpi->name = lcpi->name;
            hotspot_lcpi->value = lcpi->value;
            hotspot_lcpi->expression = lcpi->expression;

            /* Get the list of variables and their values */
            evaluator_get_variables(hotspot_lcpi->expression, &names, &count);
            if (0 < count) {
                int i = 0;

                PERFEXPERT_ALLOC(double, values, (sizeof(double*) * count));
                for (i = 0; i < count; i++) {
                    metric_t *metric = NULL;
                    char key_md5[33];

                    strcpy(key_md5, perfexpert_md5_string(names[i]));

                    perfexpert_hash_find_str(hotspot->metrics_by_name,
                        (char *)&key_md5, metric);
                    if (NULL == metric) {
                        perfexpert_hash_find_str(globals.machine_by_name,
                            (char *)&key_md5, metric);
                        if (NULL == metric) {
                            values[i] = 0.0;
                        } else {
                            values[i] = metric->value;
                        }
                    } else {
                        values[i] = metric->value;                    
                    }
                }
 
                /* Evaluate the LCPI expression */
                hotspot_lcpi->value = evaluator_evaluate(
                    hotspot_lcpi->expression, count, names, values);
                PERFEXPERT_DEALLOC(values);
            }

            /* Add the LCPI to the hotspot's list of LCPIs */
            perfexpert_hash_add_str(hotspot->lcpi_by_name, name_md5,
                hotspot_lcpi);

            OUTPUT_VERBOSE((10, "       %s=[%g]", hotspot_lcpi->name,
                hotspot_lcpi->value));
        }
        /* Move on */
        hotspot = (procedure_t *)perfexpert_list_get_next(hotspot);
    }
    return PERFEXPERT_SUCCESS;
}

#ifdef __cplusplus
}
#endif

// EOF
