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

#ifndef PERFEXPERT_FORK_H_
#define PERFEXPERT_FORK_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _STDLIB_H
#include <stdlib.h>
#endif

#ifndef _UNISTD_H
#include <unistd.h>
#endif

#ifndef _FCNTL_H
#include <fcntl.h>
#endif

#include "perfexpert_constants.h"
#include "perfexpert_output.h"
#include "perfexpert_list.h"
#include "install_dirs.h"

/** Ninja structure to hold a list of tests to perform */
typedef struct test {
    volatile perfexpert_list_item_t *next;
    volatile perfexpert_list_item_t *prev;
    char *info;   // just to make the output meaningful
    char *input;  // send via STDIN
    char *output; // collected via STDOUT
} test_t;

/** Structure to hold experiments */
typedef struct experiment {
    volatile perfexpert_list_item_t *next;
    volatile perfexpert_list_item_t *prev;
    test_t test;
    char *argv[PARAM_SIZE];
    int  argc;
} experiment_t;

/* fork_and_wait */
static inline int fork_and_wait(test_t *test, char *argv[]) {
    int  pipe1[2], pipe2[2];
    int  pid = 0;
    int  input_FP = 0, output_FP = 0;
    int  r_bytes = 0, w_bytes = 0;
    int  rc = PERFEXPERT_UNDEFINED;
    char temp_str[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    #define PARENT_READ  pipe1[0]
    #define CHILD_WRITE  pipe1[1]
    #define CHILD_READ   pipe2[0]
    #define PARENT_WRITE pipe2[1]
    
    /* Flush everything...*/
    fflush(stdout);
    fflush(stderr);

    /* Creating pipes */
    if (-1 == pipe(pipe1)) {
        OUTPUT(("%s", _ERROR((char *)"Error: unable to create pipe1")));
        return PERFEXPERT_ERROR;
    }
    if (-1 == pipe(pipe2)) {
        OUTPUT(("%s", _ERROR((char *)"Error: unable to create pipe2")));
        return PERFEXPERT_ERROR;
    }
    
    /* Forking child */
    pid = fork();
    if (-1 == pid) {
        OUTPUT(("%s", _ERROR((char *)"Error: unable to fork")));
        return PERFEXPERT_ERROR;
    }

	// 下面主要工作：
	// 1. 创建子进程，并将子进程的输入输出重定向到管道，然后执行TARGET
	// 2. 在父进程中读取子进程的信息，并为其准备输入输出文件
    if (0 == pid) {
        /* Child */
        close(PARENT_WRITE);
        close(PARENT_READ);

        OUTPUT_VERBOSE((10, "      %s %s", _CYAN((char *)"program"), argv[0]));

		// 把标准输入重定向到管道CHILD_READ
        // 这里的意思是：父进程从输入文件中读数据并写入管道2，这里也就是直接进入
        // 了子进程的读（变相的实现子进程的输入功能）
        if (-1 == dup2(CHILD_READ, STDIN_FILENO)) {
            OUTPUT(("%s", _ERROR((char *)"Error: unable to DUP STDIN")));
            return PERFEXPERT_ERROR;
        }
        if (NULL != test->output) {
			// 把标准输出重定向到管道CHILD_WRITE
            // 如果输出文件存在，这里就把子进程的输出写入管道1，后面父进程会从
            // 管道1中读取并写入输出文件
            if (-1 == dup2(CHILD_WRITE, STDOUT_FILENO)) {
                OUTPUT(("%s", _ERROR((char *)"Error: unable to DUP STDOUT")));
                return PERFEXPERT_ERROR;
            }
            if (-1 == dup2(CHILD_WRITE, STDERR_FILENO)) {
                OUTPUT(("%s", _ERROR((char *)"Error: unable to DUP STDERR")));
                return PERFEXPERT_ERROR;
            }
        }

		// 这里就是执行argv[0]文件，argv作为参数
		// 如果执行成功则函数不会返回, 执行失败则直接返回-1, 失败原因存于errno中
        execvp(argv[0], argv);
        
        OUTPUT(("child process failed to run, check if program exists"));
        exit(PERFEXPERT_FORK_ERROR);
    } else {
        /* Parent */
        close(CHILD_READ);
        close(CHILD_WRITE);

        /* If there is any input file, open and send it to the child process */
        if (NULL != test->input) {
            if (-1 == (input_FP = open(test->input, O_RDONLY))) {
                OUTPUT(("%s (%s)",
                    _ERROR((char *)"Error: unable to open input file"),
                    test->input));
                return PERFEXPERT_ERROR;
            } else {
                OUTPUT_VERBOSE((10, "      %s   %s", _CYAN((char *)"stdin"),
                    test->input));
                bzero(buffer, BUFFER_SIZE);
                while (0 != (r_bytes = read(input_FP, buffer, BUFFER_SIZE))) {
					// 从输入文件中读取BUFFER_SIZE字节数据写入父进程管道
                    w_bytes = write(PARENT_WRITE, buffer, r_bytes);
                    bzero(buffer, BUFFER_SIZE);
                }
                close(input_FP);
                close(PARENT_WRITE);
            }
        }
        
        /* Read child process' answer and write it to output file */
        if (NULL != test->output) {
            OUTPUT_VERBOSE((10, "      %s  %s", _CYAN((char *)"stdout"),
                test->output));

            if (-1 == (output_FP = open(test->output, O_CREAT|O_WRONLY, 0644))) {
                OUTPUT(("%s (%s)",
                    _ERROR((char *)"Error: unable to open output file"),
                    test->output));
                return PERFEXPERT_ERROR;
            } else {
                bzero(buffer, BUFFER_SIZE);
                while (0 != (r_bytes = read(PARENT_READ, buffer, BUFFER_SIZE))) {
					// 从父管道中读BUFFER_SIZE字节写入输出文件
                    w_bytes = write(output_FP, buffer, r_bytes);
                    bzero(buffer, BUFFER_SIZE);
                }
                close(output_FP);
                close(PARENT_READ);
            }
        }

        /* Just wait for the child... */
		// rc=0则成功
        wait(&rc);
        OUTPUT_VERBOSE((10, "      %s  %d", _CYAN((char *)"result"), rc >> 8));
    }

    /* Evaluating the result */
    if (0 > (rc >> 8)) {
        OUTPUT_VERBOSE((7, "      [%s] [%s] >> [%s] (rc=%d)",
            _BOLDYELLOW((char *)"ERROR"), argv[0], test->info, rc >> 8));
    } else if (0 < (rc >> 8)) {
        OUTPUT_VERBOSE((7, "      [%s ] [%s] >> [%s] (rc=%d)",
            _BOLDRED((char *)"FAIL"), argv[0], test->info, rc >> 8));
    } else {
        OUTPUT_VERBOSE((7, "      [ %s  ] [%s] >> [%s] (rc=%d)",
            _BOLDGREEN((char *)"OK"), argv[0], test->info, rc >> 8));
    }

    return rc >> 8;
}

#ifdef __cplusplus
}
#endif

#endif /* PERFEXPERT_FORK_H */
