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
#include <argp.h>

/* PerfExpert headers */
#include "perfexpert.h"
#include "perfexpert_alloc.h"
#include "perfexpert_constants.h"
#include "perfexpert_options.h"
#include "perfexpert_output.h"
#include "perfexpert_string.h"
#include "perfexpert_util.h"

// 在.h文件中已定义了options结构体
static struct argp argp = { options, parse_options, args_doc, doc };
static arg_options_t arg_options = { 0 };		// 全局结构体

/* parse_cli_params */
int parse_cli_params(int argc, char *argv[]) {
    int i = 0, k = 0;

    /* perfexpert_run_exp compatibility */
    if (0 == strcmp("perfexpert_run_exp", argv[0])) {
        OUTPUT(("%s running PerfExpert in compatibility mode",
            _BOLDRED("WARNING:")));
        OUTPUT((""));
		// 如果运行的是perfexpert_run_exp，则这是以兼容模式运行。
		// 什么是兼容模式？？
        globals.compat_mode = PERFEXPERT_TRUE;
    }

    /* Set default values for globals */
	// 这个结构体通过argp_parse和gnu argp关联起来
    arg_options = (arg_options_t) {
        .program           = NULL,
        .program_argv      = NULL,
        .program_argv_temp = { 0 },
        .prefix            = NULL,
        .before            = NULL,
        .after             = NULL,
        .knc_prefix        = NULL,
        .knc_before        = NULL,
        .knc_after         = NULL,
        .do_not_run        = PERFEXPERT_FALSE
    };

    /* If some environment variable is defined, use it! */
	// 从环境变量中获得所需要的值，但是环境变量又是谁设置的呢？
    if (PERFEXPERT_SUCCESS != parse_env_vars()) {
        OUTPUT(("%s", _ERROR("Error: parsing environment variables")));
        return PERFEXPERT_ERROR;
    }

    /* Parse arguments */
	// gnu lib: error_t argp_parse (const struct argp *argp, int argc, char **argv, unsigned flags, int *arg_index, void *input)
	// 就是把**argv中的参数拆分放入globals呗
    argp_parse(&argp, argc, argv, 0, 0, &globals);

    /* Expand AFTERs, BEFOREs, PREFIXs and program arguments */
	// 把前者按空格拆分开放入分组放入后者:after中是指令，各个指令的指针交给globals
    if (NULL != arg_options.after) {
        perfexpert_string_split(perfexpert_string_remove_spaces(
            arg_options.after), globals.after, ' ');
    }
    if (NULL != arg_options.before) {
        perfexpert_string_split(perfexpert_string_remove_spaces(
            arg_options.before), globals.before, ' ');
   }
    if (NULL != arg_options.prefix) {
        perfexpert_string_split(perfexpert_string_remove_spaces(
            arg_options.prefix), globals.prefix, ' ');
    }
    if (NULL != arg_options.knc_after) {
        perfexpert_string_split(perfexpert_string_remove_spaces(
            arg_options.knc_after), globals.knc_after, ' ');
    }
    if (NULL != arg_options.knc_before) {
        perfexpert_string_split(perfexpert_string_remove_spaces(
            arg_options.knc_before), globals.knc_before, ' ');
    }
    if (NULL != arg_options.knc_prefix) {
        perfexpert_string_split(perfexpert_string_remove_spaces(
            arg_options.knc_prefix), globals.knc_prefix, ' ');
    }
	// 上面arg_options中放入的都是指令
	// 下面再次拆分TARGET程序的参数
    while (NULL != arg_options.program_argv[i]) {
        int j = 0;

        perfexpert_string_split(perfexpert_string_remove_spaces(
            arg_options.program_argv[i]), arg_options.program_argv_temp, ' ');
        while (NULL != arg_options.program_argv_temp[j]) {
            globals.program_argv[k] = arg_options.program_argv_temp[j];
            j++;
            k++;
        }
        i++;
    }

    /* Sanity check: verbose level should be between 1-10 */
    if ((0 > globals.verbose) || (10 < globals.verbose)) {
        OUTPUT(("%s", _ERROR("Error: invalid verbose level")));
        return PERFEXPERT_ERROR;
    }

    /* Sanity check: threshold is mandatory [0,1] */
    if ((0 >= globals.threshold) || (1 < globals.threshold)) {
        OUTPUT(("%s", _ERROR("Error: undefined or invalid threshold")));
        return PERFEXPERT_ERROR;
    }

    /* Sanity check: NULL program */
    if (NULL != arg_options.program) {		// TARGET需要存在
        if (PERFEXPERT_SUCCESS != perfexpert_util_filename_only(	// 不包含路径名
            arg_options.program, &(globals.program))) {
            OUTPUT(("%s", _ERROR("Error: unable to extract program name")));
            return PERFEXPERT_ERROR;
        }
        OUTPUT_VERBOSE((1, "   program only=[%s]", globals.program));

        if (PERFEXPERT_SUCCESS != perfexpert_util_path_only(arg_options.program,
            &(globals.program_path))) {
            OUTPUT(("%s", _ERROR("Error: unable to extract program path")));
            return PERFEXPERT_ERROR;
        }
        OUTPUT_VERBOSE((1, "   program path=[%s]", globals.program_path));

        PERFEXPERT_ALLOC(char, globals.program_full,
            (strlen(globals.program) + strlen(globals.program_path) + 2));
		// 结合为完整路径
        sprintf(globals.program_full, "%s/%s", globals.program_path,
            globals.program);
        OUTPUT_VERBOSE((1, "   program full path=[%s]", globals.program_full));

        if ((NULL == globals.sourcefile) && (NULL == globals.target) &&
            (PERFEXPERT_SUCCESS != perfexpert_util_file_is_exec(	// 文件可执行
                globals.program_full))) {
            OUTPUT(("%s (%s)", _ERROR("Error: unable to find program"),
                globals.program_full));
            return PERFEXPERT_ERROR;
        }
    } else {
        OUTPUT(("%s", _ERROR("Error: undefined program")));
        return PERFEXPERT_ERROR;
    }

    /* Sanity check: target and sourcefile at the same time */
	// 这两个只能有一个存在
    if ((NULL != globals.target) && (NULL != globals.sourcefile)) {
        OUTPUT(("%s", _ERROR("Error: target and sourcefile are both defined")));
        return PERFEXPERT_ERROR;
    }

    /* Sanity check: MIC options without MIC */
    if ((NULL != globals.knc_prefix[0]) && (NULL == globals.knc)) {
        OUTPUT(("%s", _RED("Warning: option -P selected without setting MIC")));
    }

    /* Sanity check: MIC options without MIC */
    if ((NULL != globals.knc_before[0]) && (NULL == globals.knc)) {
        OUTPUT(("%s", _RED("Warning: option -B selected without setting MIC")));
    }

    /* Sanity check: MIC options without MIC */
    if ((NULL != globals.knc_after[0]) && (NULL == globals.knc)) {
        OUTPUT(("%s", _RED("Warning: option -A selected without setting MIC")));
    }

    OUTPUT_VERBOSE((7, "%s", _BLUE("Summary of options")));
    OUTPUT_VERBOSE((7, "   Verbose level:       %d", globals.verbose));
    OUTPUT_VERBOSE((7, "   Colorful verbose?    %s", globals.colorful ?
        "yes" : "no"));
    OUTPUT_VERBOSE((7, "   Leave garbage?       %s", globals.leave_garbage ?
        "yes" : "no"));
    OUTPUT_VERBOSE((7, "   Only experiments?    %s", globals.only_exp ?
        "yes" : "no"));
    OUTPUT_VERBOSE((7, "   Database file:       %s", globals.dbfile));
    OUTPUT_VERBOSE((7, "   Recommendations      %d", globals.rec_count));
    OUTPUT_VERBOSE((7, "   Threshold:           %f", globals.threshold));
    OUTPUT_VERBOSE((7, "   Make target:         %s", globals.target));
    OUTPUT_VERBOSE((7, "   Program source file: %s", globals.sourcefile));
    OUTPUT_VERBOSE((7, "   Program executable:  %s", globals.program));
    OUTPUT_VERBOSE((7, "   Program path:        %s", globals.program_path));
    OUTPUT_VERBOSE((7, "   Program full path:   %s", globals.program_full));
    OUTPUT_VERBOSE((7, "   Program input file:  %s", globals.inputfile));
    if (7 <= globals.verbose) {
        i = 0;
        printf("%s    Program arguments:  ", PROGRAM_PREFIX);
        while (NULL != globals.program_argv[i]) {
            printf(" [%s]", globals.program_argv[i]);
            i++;
        }
        printf("\n%s    Prefix:             ", PROGRAM_PREFIX);
        i = 0;
        while (NULL != globals.prefix[i]) {
            printf(" [%s]", (char *)globals.prefix[i]);
            i++;
        }
        printf("\n%s    Before each run:    ", PROGRAM_PREFIX);
        i = 0;
        while (NULL != globals.before[i]) {
            printf(" [%s]", (char *)globals.before[i]);
            i++;
        }
        printf("\n%s    After each run:     ", PROGRAM_PREFIX);
        i = 0;
        while (NULL != globals.after[i]) {
            printf(" [%s]", (char *)globals.after[i]);
            i++;
        }
        printf("\n");
    }
    OUTPUT_VERBOSE((7, "   MIC card:            %s", globals.knc));
    if (7 <= globals.verbose) {
        i = 0;
        printf("%s    MIC prefix:         ", PROGRAM_PREFIX);
        while (NULL != globals.knc_prefix[i]) {
            printf(" [%s]", (char *)globals.knc_prefix[i]);
            i++;
        }
        printf("\n%s    MIC before each run:", PROGRAM_PREFIX);
        i = 0;
        while (NULL != globals.knc_before[i]) {
            printf(" [%s]", (char *)globals.knc_before[i]);
            i++;
        }
        printf("\n%s    MIC after each run: ", PROGRAM_PREFIX);
        i = 0;
        while (NULL != globals.knc_after[i]) {
            printf(" [%s]", (char *)globals.knc_after[i]);
            i++;
        }
        printf("\n");
    }
    OUTPUT_VERBOSE((7, "   Sorting order:       %s", globals.order));
    OUTPUT_VERBOSE((7, "   Measurement tool:    %s", globals.tool));

    /* Not using OUTPUT_VERBOSE because I want only one line */
    if (8 <= globals.verbose) {
        i = 0;
        printf("%s    %s", PROGRAM_PREFIX, _YELLOW("command line:"));
        for (i = 0; i < argc; i++) {
            printf(" [%s]", argv[i]);
        }
        printf("\n");
    }

    if (PERFEXPERT_TRUE == arg_options.do_not_run) {
        exit(0);
    }
    return PERFEXPERT_SUCCESS;
}

/* parse_options */
static error_t parse_options(int key, char *arg, struct argp_state *state) {
    switch (key) {
        /* Should I run some program after each execution? */
        // -a=COMMAND: 每一次运行TARGET文件之后都要运行COMMAND
		case 'a':
            arg_options.after = arg;
            OUTPUT_VERBOSE((1, "option 'a' set [%s]", arg_options.after));
            break;

        /* Should I run on the KNC some program after each execution? */
		// 每次执行TARGET文件之后都要运行KNC
        case 'A':
            arg_options.knc_after = arg;
            OUTPUT_VERBOSE((1, "option 'A' set [%s]", arg_options.knc_after));
            break;

        /* Should I run some program before each execution? */
        // -a=COMMAND: 每一次运行TARGET文件之前都要运行COMMAND
        case 'b':
            arg_options.before = arg;
            OUTPUT_VERBOSE((1, "option 'b' set [%s]", arg_options.before));
            break;

        /* Should I run on the KNC some program before each execution? */
		// 之前 KNC
        case 'B':
            arg_options.knc_before = arg;
            OUTPUT_VERBOSE((1, "option 'B' set [%s]", arg_options.knc_before));
            break;

        /* Activate colorful mode : -c: 激活ANSI颜色机制 */
        case 'c':
            globals.colorful = PERFEXPERT_TRUE;
            OUTPUT_VERBOSE((1, "option 'c' set"));
            break;

        /* MIC card */
		// 这是啥？？？
        case 'C':
            globals.knc = arg;
            OUTPUT_VERBOSE((1, "option 'C' set [%s]", globals.knc));
            break;

        /* Which database file? */
		// -d=FILE: 设置新的建议数据库文件
        case 'd':
            globals.dbfile = arg;
            OUTPUT_VERBOSE((1, "option 'd' set [%s]", globals.dbfile));
            break;

        /* Only experiments */
		// -e: 告诉PerfExpert仅仅运行TARGET程序，不要执行分析(为后面手动分析作准备)
        case 'e':
            globals.only_exp = PERFEXPERT_TRUE;
            OUTPUT_VERBOSE((1, "option 'e' set"));
            break;

        /* Leave the garbage there? */
		// -g: perfexpert程序运行结束后不要删除临时目录
        case 'g':
            globals.leave_garbage = PERFEXPERT_TRUE;
            OUTPUT_VERBOSE((1, "option 'g' set"));
            break;

        /* Show help */
        case 'h':
            OUTPUT_VERBOSE((1, "option 'h' set"));
            // argp_help(&argp, NULL, ARGP_HELP_LONG, "porra");
            argp_state_help(state, stdout, ARGP_HELP_STD_HELP);
            break;

        /* Which input file? : -i=FILE: TARGET程序(二进制) */
		// 和-s(设置源码)不同时存在
        case 'i':
            globals.inputfile = arg;
            OUTPUT_VERBOSE((1, "option 'i' set [%s]", globals.inputfile));
            break;

        /* Verbose level (has an alias: v) */
		// 需要多详细的信息？level
        case 'l':
            globals.verbose = arg ? atoi(arg) : 5;
            OUTPUT_VERBOSE((1, "option 'l' set [%d]", globals.verbose));
            break;

        /* Use Makefile? : -m=TARGET: 使用make命令编译该TARGET(makefile) */
        case 'm':
            globals.target = arg;
            OUTPUT_VERBOSE((1, "option 'm' set [%s]", globals.target));
            break;

        /* Do not run */
		// 不要运行PerfExpert，仅仅分析命令行
        case 'n':
            arg_options.do_not_run = PERFEXPERT_TRUE;
            OUTPUT_VERBOSE((1, "option 'n' set [%d]", arg_options.do_not_run));
            break;

        /* Sorting order : -o=relevance|performance|mixed: hotspots在其中的顺序是怎样的 */
        case 'o':
            globals.order = arg;
            OUTPUT_VERBOSE((1, "option 'o' set [%s]", globals.order));
            break;

        /* Should I add a program prefix to the command line? */
		// -p="COMMAND"分析指令执行之前运行这里的指令
        case 'p':
            arg_options.prefix = arg;
            OUTPUT_VERBOSE((1, "option 'p' set [%s]", arg_options.prefix));
            break;

        /* Should I add a program prefix to the KNC command line? */
		// KNC 命令行之前是否要加入一个程序
        case 'P':
            arg_options.knc_prefix = arg;
            OUTPUT_VERBOSE((1, "option 'P' set [%s]", arg_options.knc_prefix));
            break;

        /* Number of recommendation to output */
		// -r=COUNT: PerfExpert应该提供建议的条数
        case 'r':
            globals.rec_count = atoi(arg);
            OUTPUT_VERBOSE((1, "option 'r' set [%d]", globals.rec_count));
            break;

        /* What is the source code filename? : -s=FILE: TARGET程序的源代码文件 */
		// 多个采用-m=TARGET，TARGET为makefile文件进行编译
        case 's':
            globals.sourcefile = arg;
            OUTPUT_VERBOSE((1, "option 's' set [%s]", globals.sourcefile));
            break;

        /* Measurement tool */
		// -t=hpctoolkit|vtune: 选择收集performance counter值的工具(默认为hpctoolkit)
        case 't':
            globals.tool = arg;
            OUTPUT_VERBOSE((1, "option 't' set [%s]", globals.tool));
            break;

        /* Verbose level (has an alias: l) */
		// 激活verbose模式,默认等级为5，和l一样
        case 'v':
            globals.verbose = arg ? atoi(arg) : 5;
            OUTPUT_VERBOSE((1, "option 'v' set [%d]", globals.verbose));
            break;

        /* Arguments: threashold and target program and it's arguments */
		// 阈值 TARGET 他的参数
        case ARGP_KEY_ARG:
            if (PERFEXPERT_TRUE == globals.compat_mode) {
                globals.only_exp = PERFEXPERT_TRUE;			// !@@!
                OUTPUT_VERBOSE((1, "option 'e' set"));
                globals.leave_garbage = PERFEXPERT_TRUE;	// !@@!
                OUTPUT_VERBOSE((1, "option 'g' set"));
                arg_options.program = arg;		// 第一个无标识参数为可执行程序
                OUTPUT_VERBOSE((1, "option 'target_program' set [%s]",
                    arg_options.program));
                globals.threshold = 1;			// 直接设置阈值为1？？？
                OUTPUT_VERBOSE((1, "option 'threshold' set [%f]",
                    globals.threshold));
                arg_options.program_argv = &state->argv[state->next];
                OUTPUT_VERBOSE((1, "option 'program_arguments' set"));
                state->next = state->argc;		// 直接指向了最后一个了
            } else {	// 兼容与否的区别在于TARGET与THRESHOLD的顺序？？
                if (0 == state->arg_num) {
                    globals.threshold = atof(arg);	// 通过参数传入
                    OUTPUT_VERBOSE((1, "option 'threshold' set [%f] (%s)",
                        globals.threshold, arg));
                }
                if (1 == state->arg_num) {		// 这里为什么颠倒了呢？？？
                    arg_options.program = arg;
                    OUTPUT_VERBOSE((1, "option 'target_program' set [%s]",
                        arg_options.program));
                    arg_options.program_argv = &state->argv[state->next];
                    OUTPUT_VERBOSE((1, "option 'program_arguments' set"));
                    state->next = state->argc;
                }
            }
            break;

        /* If the arguments are missing */
        case ARGP_KEY_NO_ARGS:
            argp_usage(state);

        /* Too few options */
        case ARGP_KEY_END:
            if (PERFEXPERT_TRUE == globals.compat_mode) {
                if (1 > state->arg_num) {	// 兼容模式程序只会执行一次，arg_num=1
                    argp_usage(state);
                }
            } else {
                if (2 > state->arg_num) {	// 非兼容模式程序执行了两次，arg_num=2
                    argp_usage(state);
                }
            }
            break;

        /* Unknown option */
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

/* parse_env_vars */
static int parse_env_vars(void) {
	// 现在是直接获取这些环境变量，但是系统的环境变量又是谁来设置的呢？
    if (NULL != getenv("PERFEXPERT_VERBOSE_LEVEL")) {
        globals.verbose = atoi(getenv("PERFEXPERT_VERBOSE_LEVEL"));
        OUTPUT_VERBOSE((1, "ENV: verbose_level=%d", globals.verbose));
    }

    if (NULL != getenv("PERFEXPERT_DATABASE_FILE")) {
        globals.dbfile = getenv("PERFEXPERT_DATABASE_FILE");
        OUTPUT_VERBOSE((1, "ENV: dbfile=%s", globals.dbfile));
    }

    if ((NULL != getenv("PERFEXPERT_REC_COUNT")) &&
        (0 <= atoi(getenv("PERFEXPERT_REC_COUNT")))) {
        globals.rec_count = atoi(getenv("PERFEXPERT_REC_COUNT"));
        OUTPUT_VERBOSE((1, "ENV: rec_count=%d", globals.rec_count));
    }

    if ((NULL != getenv("PERFEXPERT_COLORFUL")) &&
        (1 == atoi(getenv("PERFEXPERT_COLORFUL")))) {
        globals.colorful = PERFEXPERT_FALSE;
        OUTPUT_VERBOSE((1, "ENV: colorful=YES"));
    }

    if (NULL != getenv("PERFEXPERT_MAKE_TARGET")) {
        globals.target = getenv("PERFEXPERT_MAKE_TARGET");
        OUTPUT_VERBOSE((1, "ENV: target=%s", globals.target));
    }

    if (NULL != getenv("PERFEXPERT_SOURCE_FILE")) {
        globals.sourcefile = ("PERFEXPERT_SOURCE_FILE");
        OUTPUT_VERBOSE((1, "ENV: sourcefile=%s", globals.sourcefile));
    }

    if (NULL != getenv("PERFEXPERT_PREFIX")) {
        arg_options.prefix = ("PERFEXPERT_PREFIX");
        OUTPUT_VERBOSE((1, "ENV: prefix=%s", arg_options.prefix));
    }

    if (NULL != getenv("PERFEXPERT_BEFORE")) {
        arg_options.before = ("PERFEXPERT_BEFORE");
        OUTPUT_VERBOSE((1, "ENV: before=%s", arg_options.before));
    }

    if (NULL != getenv("PERFEXPERT_AFTER")) {
        arg_options.after = ("PERFEXPERT_AFTER");
        OUTPUT_VERBOSE((1, "ENV: after=%s", arg_options.after));
    }

    if (NULL != getenv("PERFEXPERT_KNC_CARD")) {
        globals.knc = ("PERFEXPERT_KNC_CARD");
        OUTPUT_VERBOSE((1, "ENV: knc=%s", globals.knc));
    }

    if (NULL != getenv("PERFEXPERT_KNC_PREFIX")) {
        arg_options.knc_prefix = ("PERFEXPERT_KNC_PREFIX");
        OUTPUT_VERBOSE((1, "ENV: knc_prefix=%s", arg_options.knc_prefix));
    }

    if (NULL != getenv("PERFEXPERT_KNC_BEFORE")) {
        arg_options.knc_before = ("PERFEXPERT_KNC_BEFORE");
        OUTPUT_VERBOSE((1, "ENV: knc_before=%s", arg_options.knc_before));
    }

    if (NULL != getenv("PERFEXPERT_KNC_AFTER")) {
        arg_options.knc_after = ("PERFEXPERT_KNC_AFTER");
        OUTPUT_VERBOSE((1, "ENV: knc)after=%s", arg_options.knc_after));
    }

    if (NULL != getenv("PERFEXPERT_SORTING_ORDER")) {
        globals.order = getenv("PERFEXPERT_SORTING_ORDER");
        OUTPUT_VERBOSE((1, "ENV: order=%s", globals.order));
    }

    if (NULL != getenv("PERFEXPERT_ANALYZER_TOOL")) {
        globals.tool = getenv("PERFEXPERT_ANALYZER_TOOL");
        OUTPUT_VERBOSE((1, "ENV: tool=%s", globals.tool));
    }

    if (NULL != getenv("PERFEXPERT_ANALYZER_INPUT_FILE")) {
        globals.inputfile = getenv("PERFEXPERT_ANALYZER_INPUT_FILE");
        OUTPUT_VERBOSE((1, "ENV: tool=%s", globals.inputfile));
    }

    return PERFEXPERT_SUCCESS;
}

#ifdef __cplusplus
}
#endif

// EOF
