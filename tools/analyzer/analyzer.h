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

#ifndef ANALYZER_H_
#define ANALYZER_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _STDIO_H__
#include <stdio.h>
#endif

#ifndef __XML_PARSER_H__
#include <libxml/parser.h>
#endif

#include "analyzer_types.h"
#include "perfexpert_constants.h"
#include "perfexpert_hash.h"
#include "perfexpert_list.h"
#include "perfexpert_module.h"

/* Structure to hold global variables */
typedef struct {
    int      verbose;       // 信息输出的详细程度(default:0)
    int      colorful; // These should be the first variables in the structure
    double threshold;       // 确定代码瓶颈的相关性(运行时)([0,1])
    char     *tool;             // 收集performance measurements的工具(default:hpctoolkit)
    perfexpert_module_t toolmodule;
    char     *inputfile;        // 性能measurements的输入文件
    char     *outputfile;       // 性能分析的输出文件(default:STDOUT)
    FILE     *outputfile_FP;
    int      aggregate;         // 显示整个程序的信息而不是显示每个hotspot的信息
    int      thread;            // 仅仅显示特定线程ID的信息
    char     *machinefile;      // 硬件特征的输入文件(default:MACHINE_FILE)
    metric_t *machine_by_name;
    char     *lcpifile;         // LCPI指标的输入文件，默认PERFEXPERT_ETCDIR/CLPI_FILE
    lcpi_t   *lcpi_by_name;     // lcpi信息都在这里
    char     *outputmetrics;
    char     *order;            // hotspot的排序情况(relevance|performance|mixed)
    char     *workdir;          // 临时的工作目录的创建路径
    int      found_hotspots;
} globals_t;

extern globals_t globals; /* This variable is defined in analyzer_main.c */

/* WARNING: to include perfexpert_output.h globals have to be defined first */
#ifdef PROGRAM_PREFIX
#undef PROGRAM_PREFIX
#endif
#define PROGRAM_PREFIX "[analyzer]"

/* Function declarations */
int parse_cli_params(int argc, char *argv[]);
int profile_parse_file(const char* file, const char* tool,
    perfexpert_list_t *profiles);
int profile_check_all(perfexpert_list_t *profiles);
int profile_flatten_all(perfexpert_list_t *profiles);
int lcpi_parse_file(const char *file);
int lcpi_compute(profile_t *profile);
int machine_parse_file(const char *file);
int calculate_importance_variance(profile_t *profile);
int output_analysis_all(perfexpert_list_t *profiles);
int output_metrics_all(perfexpert_list_t *profiles);
int hotspot_sort(perfexpert_list_t *profiles);

/* generic_get */
#include "perfexpert_alloc.h"
#include "perfexpert_md5.h"
static inline double generic_get(lcpi_t *db, char key[]) {
    lcpi_t *entry = NULL;
    char *key_md5 = NULL;
    double value;

    PERFEXPERT_ALLOC(char, key_md5, 33);
    strcpy(key_md5, perfexpert_md5_string(key));
    perfexpert_hash_find_str(db, key_md5, entry);
    PERFEXPERT_DEALLOC(key_md5);

    if (NULL == entry) {
        return 0.0;
    } else {
        return entry->value;
    }
}

/* Convenience macros to get data from machine_t and lcpi_t */
#define perfexpert_lcpi_definition_get(_1) generic_get(globals.lcpi_by_name, _1)
#define perfexpert_lcpi_hotspot_get(_1, _2) generic_get(_1->lcpi_by_name, _2)
#define perfexpert_machine_get(_1) \
    generic_get((lcpi_t *)globals.machine_by_name, _1)

#ifdef __cplusplus
}
#endif

#endif /* ANALYZER_H_ */
