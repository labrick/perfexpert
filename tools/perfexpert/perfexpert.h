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

#ifndef PERFEXPERT_H_
#define PERFEXPERT_H_

#ifdef __cplusplus
extern "C" {
#endif

/* WARNING: to include perfexpert_output.h globals have to be defined first */
#ifdef PROGRAM_PREFIX
#undef PROGRAM_PREFIX
#endif
#define PROGRAM_PREFIX "[perfexpert]"

#include "perfexpert_constants.h"
#include "perfexpert_module.h"

/* Structure to hold global variables */
typedef struct {
    int   verbose;		// 详细信息模式，这里设置等级(default:5(1--10))
    int   colorful; // These should be the first variables in the structure ANSI颜色机制
    char  *dbfile;	// 加入新的数据库文件替换掉默认的数据库文件
    float threshold;	// 兼容模式下为1，否则紧跟target_program第一个参数为阈值(谁的？)
    int   rec_count;	// PerfExpert应该提供建议的条数
    int   leave_garbage;		// perfexpert运行结束后不要删除临时目录文件
    char  *target;		// 以makefile的形式输入源程序编译文件(多源程序时使用)
    char  *sourcefile;	// 源程序文件
    char  *program;			// 可执行文件的名称
    char  *program_path;	// 可执行文件的路径
    char  *program_full;	// 上面两者结合成完整路径(包含程序名)
    char  *program_argv[PARAM_SIZE];	// 程序运行所需参数
    int   step;				// 处理过程标记进行到第几步，并由此产生这一步的工作目录
    char  *workdir;			// 工作目录的路径
    char  *stepdir;			// 处理过程中所用的工作目录
    char  *prefix[PARAM_SIZE];
    char  *before[PARAM_SIZE];
    char  *after[PARAM_SIZE];
    char  *knc;			// 这是个程序？？
    char  *knc_prefix[PARAM_SIZE];
    char  *knc_before[PARAM_SIZE];
    char  *knc_after[PARAM_SIZE];		// TARGET
    char  *tool;	// 采用哪个工具手机performance counter值(hpctoolkit(default)|vtune)
    perfexpert_module_t toolmodule;
    char  *order;			// hotspot在链表中的顺序如何？(relevance|performance|mixed)
    char  *inputfile;		// TARGET的输入文件
    int   only_exp;			// 仅仅运行TARGET程序，不要执行分析(后面手动分析)
    int   compat_mode;		// 兼容模式，也就是尽可能的保留信息，并且慢慢来
    long int pid;
} globals_t;

extern globals_t globals; /* This variable is declared in perfexpert_main.c */

/* Function declarations */
void show_help(void);
int parse_cli_params(int argc, char *argv[]);
int compile_program(void);
int measurements(void);
int analysis(void);
int recommendation(void);
int transformation(void);

#ifdef __cplusplus
}
#endif

#endif /* PERFEXPERT_H */
