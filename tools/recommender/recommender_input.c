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

/* Utility headers */
#include <sqlite3.h>

/* PerfExpert headers */
#include "recommender.h"
#include "perfexpert_alloc.h"
#include "perfexpert_database.h"
#include "perfexpert_list.h"
#include "perfexpert_output.h"
#include "perfexpert_string.h"
#include "install_dirs.h"

/* parse_segment_params */
int parse_segment_params(perfexpert_list_t *segments) {
    segment_t *item = NULL;
    char *error_msg = NULL;
    char *temp_str = NULL;
    char buffer[BUFFER_SIZE];
    char sql[BUFFER_SIZE];
    int  input_line = 0;
    int  rowid = 0;

    OUTPUT_VERBOSE((4, "%s", _BLUE("Parsing measurements")));
    
    /* To improve SQLite performance and keep database clean */
    if (SQLITE_OK != sqlite3_exec(globals.db, "BEGIN TRANSACTION;", NULL, NULL,
        &error_msg)) {
        OUTPUT(("%s %s", _ERROR("Error: SQL error"), error_msg));
        sqlite3_free(error_msg);
        return PERFEXPERT_ERROR;
    }

    bzero(buffer, BUFFER_SIZE);
	// 这里使用的是输入文件，怎么老是去更新指标metrics_table呢？？
    while (NULL != fgets(buffer, BUFFER_SIZE - 1, globals.inputfile_FP)) {
        node_t *node = NULL;
        
        input_line++;

        /* Ignore comments and blank lines */
        if ((0 == strncmp("#", buffer, 1)) ||
            (strspn(buffer, " \t\r\n") == strlen(buffer))) {
            continue;
        }

        /* Remove the end \n character */
        buffer[strlen(buffer) - 1] = '\0';

        /* Is this line a new code bottleneck specification? */
		// 判断该行第一个字符是不是'%'，如果是，则是代码瓶颈
		// 这里是说在其他地方已经判断是否是瓶颈？？？给瓶颈的行加上了'%'？？？
        if (0 == strncmp("%", buffer, 1)) {
            OUTPUT_VERBOSE((5, "   (%d) %s", input_line,
                _GREEN("new bottleneck found")));

            /* Create a list item for this code bottleneck */
			// 申请以item为首地址以sizeof为大小以segment_t类型的空间
            PERFEXPERT_ALLOC(segment_t, item, sizeof(segment_t));
            perfexpert_list_item_construct((perfexpert_list_item_t *)item);
            
            /* Initialize some elements on segment */
			// 这里又构建了一个list结构体(应该说segment_t *item中构建的)
			// 注意：item->functions和segments一样的类型
            perfexpert_list_construct((perfexpert_list_t *)&(item->functions));
            item->function_name = NULL;
            item->section_info = NULL;
            item->extra_info = NULL;
            item->filename = NULL;
            item->type = PERFEXPERT_HOTSPOT_UNKNOWN;
            item->rowid = 0;
            item->runtime = 0.0;
            item->loopdepth = 0;
            item->importance = 0.0;
            item->line_number = 0;

            /* Add this item to 'segments' */
			// 注意这里segments是perfexpert_list_t类型，其中包含了一个item类型的成员
			// 先添加到segments中，然后再更新内容
            perfexpert_list_append(segments, (perfexpert_list_item_t *)item);

            /* Create the SQL statement for this new segment */
			// 这一步很关键
            bzero(sql, BUFFER_SIZE);
			// sql内容为：
            // INSERT INTO metrics_table (code_filename) VALUES ('new_code- pid');
            // SELECT id FROM metrics_table WHERE code_filename = 'new_code- pid';
			// 语法正确？？？
 			sprintf(sql, "%s %s %s %d'); %s %s %s %d';",
                "INSERT INTO", globals.metrics_table,
                "(code_filename) VALUES ('new_code-", (int)getpid(),
                "SELECT id FROM", globals.metrics_table,
                "WHERE code_filename = 'new_code-", (int)getpid());
            OUTPUT_VERBOSE((5, "     SQL: %s", _CYAN(sql)));
            
            /* Insert new code fragment into metrics database, retrieve id */
			// 我可以说没明白这部分的意思吗？？？？？以怎样的形式插入的code fragment??
			// 又和pid有毛关系？？？
			// rowid是传给回调函数的第一个参数
			// 回调函数的第一个参数是sqlite3_exec直接传递的
			// 第二个参数是返回结果中的columns数(纵列数？)
			// 第三个参数是一个指针数组，指针分别指向columns的内容
			// 第四个参数是columns的名字；
			// The name of a result column is the value of the "AS" clause for that 
			// column, if there is an AS clause. If there is no AS clause then the name 
			// of the column is unspecified and may change from one release of SQLite 
			// to the next.
            if (SQLITE_OK != sqlite3_exec(globals.db, sql,
                perfexpert_database_get_int, (void *)&rowid, &error_msg)) {
                OUTPUT(("%s %s", _ERROR("Error: SQL error"), error_msg));
                OUTPUT(("   SQL: %s", _CYAN(sql)));
                sqlite3_free(error_msg);
                return PERFEXPERT_ERROR;
            }

            /* Store the rowid on the segment structure */
			// 上面有变更rowid的值吗？？？？where??
            OUTPUT_VERBOSE((5, "     ID: %d", rowid));
            item->rowid = rowid;
 
            continue;
        }

        PERFEXPERT_ALLOC(node_t, node, (sizeof(node_t) + strlen(buffer) + 1));
		// 将buffer里的内容直接复制到node之后
		// node->key指向复制后的头部，内容是'='之前的内容？？相当于变量
        node->key = strtok(strcpy((char*)(node + 1), buffer), "=\r\n");
		// NULL意思是参数和上次一样，只不过是接着上次查找'='之后的内容？相当于参数
        node->value = strtok(NULL, "\r\n");

        /* OK, now it is time to check which parameter is this, and add it to
         * 'segments' (only code.* parameters) other parameter should fit into
         * the SQL DB.
         */

        /* Code param: code.filename */
		// 从上面可以看到buffer是从输入文件中获取的一行信息
		// 以下都是基于这一行信息来进行分析的？？？？
		// 这个文件应该是经过处理的特定格式文件
        if (0 == strncmp("code.filename", node->key, 13)) {
            /* Remove the "./src" string from the front of the filename */
			// 这里已经确定前5个字符构成的字符串是："./src"，和下一句又重复？
            if (0 == strncmp(node->value, "./src", 5)) {
				// 判断"./src"是不是node->value的子串，如果是，返回子串首地址
                node->value = strstr(node->value, "./src");
				// 也就是将"./src"去掉
                memmove(node->value, node->value + strlen("./src"), 1 +
                    strlen(node->value + strlen("./src")));
            }
			// 现在node->value仅仅保存了不包含路径的文件名
            PERFEXPERT_ALLOC(char, item->filename, (strlen(node->value) + 1));
            strcpy(item->filename, node->value);
            OUTPUT_VERBOSE((10, "   (%d) %s [%s]", input_line,
                _MAGENTA("filename:"), item->filename));
        }
        /* Code param: code.line_number */
        if (0 == strncmp("code.line_number", node->key, 16)) {
            item->line_number = atoi(node->value);
            OUTPUT_VERBOSE((10, "   (%d) %s [%d]", input_line,
                _MAGENTA("line number:"), item->line_number));
        }
        /* Code param: code.type */
        if (0 == strncmp("code.type", node->key, 9)) {
            item->type = atoi(node->value);
            OUTPUT_VERBOSE((10, "   (%d) %s [%d]", input_line,
                _MAGENTA("type:"), item->type));
        }
        /* Code param: code.extra_info */
        if (0 == strncmp("code.extra_info", node->key, 15)) {
            PERFEXPERT_ALLOC(char, item->extra_info, (strlen(node->value) + 1));
            strcpy(item->extra_info, node->value);
            OUTPUT_VERBOSE((10, "   (%d) %s [%s]", input_line,
                _MAGENTA("extra info:"), item->extra_info));
        }
        /* Code param: code.importance */
        if (0 == strncmp("code.importance", node->key, 15)) {
            item->importance = atof(node->value);
            OUTPUT_VERBOSE((10, "   (%d) %s [%f], ", input_line,
                _MAGENTA("importance:"), item->importance));
        }
        /* Code param: code.section_info */
        if (0 == strncmp("code.section_info", node->key, 17)) {
            PERFEXPERT_ALLOC(char, item->section_info,
                (strlen(node->value) + 1));
            strcpy(item->section_info, node->value);
            OUTPUT_VERBOSE((10, "   (%d) %s [%s]", input_line,
                _MAGENTA("section info:"), item->section_info));
            PERFEXPERT_DEALLOC(node);
            continue;
        }
        /* Code param: code.function_name */
        if (0 == strncmp("code.function_name", node->key, 18)) {
            /* Remove everyting after the '.' (for OMP functions) */
            temp_str = node->value;
			// 拆分字符串，NULL代替delim
            strsep(&temp_str, ".");
            PERFEXPERT_ALLOC(char, item->function_name,
                (strlen(node->value) + 1));
            strcpy(item->function_name, node->value);
            OUTPUT_VERBOSE((10, "   (%d) %s [%s]", input_line,
                _MAGENTA("function name:"), item->function_name));
        }
        /* Code param: code.runtime */
        if (0 == strncmp("code.runtime", node->key, 12)) {
            item->runtime = atof(node->value);
            OUTPUT_VERBOSE((10, "   (%d) %s [%f], ", input_line,
                _MAGENTA("runtime:"), item->runtime));
        }
        /* Code param: code.loopdepth */
        if (0 == strncmp("code.loopdepth", node->key, 21)) {
            item->loopdepth = atoi(node->value);
            OUTPUT_VERBOSE((10, "   (%d) %s [%d]", input_line,
                _MAGENTA("loop depth:"), item->loopdepth));
        }

        /* Clean the node->key (remove undesired characters) */
		// 清除以方面SQL操作？？？
        perfexpert_string_replace_char(node->key, '%', '_');
        perfexpert_string_replace_char(node->key, '.', '_');
        perfexpert_string_replace_char(node->key, '(', '_');
        perfexpert_string_replace_char(node->key, ')', '_');
        perfexpert_string_replace_char(node->key, '-', '_');
        perfexpert_string_replace_char(node->key, ':', '_');

        /* Assemble the SQL query */
        bzero(sql, BUFFER_SIZE);
		// 将id=rowid的条目中的node-key关键字内容设置为node->value
		// 相当于把输入文件中的所有内容更新到了metrics_table表格中
        sprintf(sql, "UPDATE %s SET %s='%s' WHERE id=%d;",
            globals.metrics_table, node->key, node->value, rowid);

        /* Update metrics table */
        if (SQLITE_OK != sqlite3_exec(globals.db, sql, NULL, NULL,
            &error_msg)) {
            OUTPUT_VERBOSE((4, "   (%d) %s (%s = %s)", input_line,
                _RED("ignored line"), node->key, node->value));
            sqlite3_free(error_msg);
        } else {
            OUTPUT_VERBOSE((10, "   (%d) %s", input_line, sql));
        }
        PERFEXPERT_DEALLOC(node);
    }

    /* LOG this bottleneck */
    bzero(sql, BUFFER_SIZE);
    sprintf(sql, "SELECT * FROM %s WHERE id=%d;", globals.metrics_table, rowid);

    if (SQLITE_OK != sqlite3_exec(globals.db, sql, log_bottleneck, NULL,
        &error_msg)) {
        OUTPUT(("%s %s", _ERROR("Error: SQL error"), error_msg));
        sqlite3_free(error_msg);
        return PERFEXPERT_ERROR;
    }

    /* To improve SQLite performance and keep database clean */
    if (SQLITE_OK != sqlite3_exec(globals.db, "END TRANSACTION;", NULL, NULL,
        &error_msg)) {
        OUTPUT(("%s %s", _ERROR("Error: SQL error"), error_msg));
        sqlite3_free(error_msg);
        return PERFEXPERT_ERROR;
    }

    /* Return PERFEXPERT_NO_HOTSPOTS list count = 0 */
    if (0 == perfexpert_list_get_size(segments)) {
        return PERFEXPERT_NO_HOTSPOTS;
    }

    /* print a summary of 'segments' */
    OUTPUT_VERBOSE((4, "   %s (%d)", _MAGENTA("code segment(s) found"),
        perfexpert_list_get_size(segments)));

    item = (segment_t *)perfexpert_list_get_first(segments);
    while ((perfexpert_list_item_t *)item != &(segments->sentinel)) {
        OUTPUT_VERBOSE((4, "      %s (line %d)", _YELLOW(item->filename),
            item->line_number));
        item = (segment_t *)perfexpert_list_get_next(item);
    }
    
    return PERFEXPERT_SUCCESS;
}

/* parse_metrics_file */
int parse_metrics_file(void) {
    char buffer[BUFFER_SIZE];
    char sql[BUFFER_SIZE];
    char *error_msg = NULL;
    FILE *metrics_FP;

    if (NULL == globals.metrics_file) {
        PERFEXPERT_ALLOC(char, globals.metrics_file,
            (strlen(METRICS_FILE) + strlen(PERFEXPERT_ETCDIR) + 2));
        sprintf(globals.metrics_file, "%s/%s", PERFEXPERT_ETCDIR, METRICS_FILE);
    }

    OUTPUT_VERBOSE((7, "%s (%s)", _BLUE("Reading metrics file"),
        globals.metrics_file));

    if (NULL == (metrics_FP = fopen(globals.metrics_file, "r"))) {
        OUTPUT(("%s (%s)", _ERROR("Error: unable to open metrics file"),
            globals.metrics_file));
        return PERFEXPERT_ERROR;
    }

    bzero(sql, BUFFER_SIZE);
	// sql语句为：
	// CREATE TEMP TABLE metrics_table (
	// id INTERGER PRIMARY KEY, code_filename CHAR(1024)，
	// code_line_number INTEGER, code_type CHAR(128),
	// code_extra_info CHAR(1024)，
	// 后面还有很多metrics输入文件关键字
	// 临时表特点
	// 1. 参数控制：tmp_table_size。
	// 2. 到达上线后创建文件在磁盘上。
	// 3. 表定义和数据都在内存里。
	// 4. 可以包含TEXT, BLOB等字段。
    sprintf(sql, "CREATE TEMP TABLE %s ( ", globals.metrics_table);
    strcat(sql, "id INTEGER PRIMARY KEY, code_filename CHAR( 1024 ), ");
    strcat(sql, "code_line_number INTEGER, code_type CHAR( 128 ), ");
    strcat(sql, "code_extra_info CHAR( 1024 ), ");

    bzero(buffer, BUFFER_SIZE);
    while (NULL != fgets(buffer, BUFFER_SIZE - 1, metrics_FP)) {
        /* Ignore comments and blank lines */
        if ((0 == strncmp("#", buffer, 1)) ||
            (strspn(buffer, " \t\r\n") == strlen(buffer))) {
            continue;
        }

        /* Remove the end \n character */
        buffer[strlen(buffer) - 1] = '\0';

        /* replace some characters just to provide a safe SQL clause */
        perfexpert_string_replace_char(buffer, '%', '_');
        perfexpert_string_replace_char(buffer, '.', '_');
        perfexpert_string_replace_char(buffer, '(', '_');
        perfexpert_string_replace_char(buffer, ')', '_');
        perfexpert_string_replace_char(buffer, '-', '_');
        perfexpert_string_replace_char(buffer, ':', '_');

		// buffer 中的内容都送入sql了，也就是把指标作为变量，类型为FLOAT
        strcat(sql, buffer);
        strcat(sql, " FLOAT, ");
    }
    sql[strlen(sql)-2] = '\0'; // remove the last ',' and '\n'
    strcat(sql, ");");			// sql语句结束
    OUTPUT_VERBOSE((10, "metrics SQL: %s", _CYAN(sql)));

    /* Create metrics table */
    if (SQLITE_OK != sqlite3_exec(globals.db, sql, NULL, NULL, &error_msg)) {
        OUTPUT(("%s %s", _ERROR("Error: SQL error"), error_msg));
        OUTPUT(("   SQL: %s", _CYAN(sql)));
        sqlite3_free(error_msg);
        return PERFEXPERT_ERROR;
    }

    OUTPUT(("using temporary metric table (%s)", globals.metrics_table));
    PERFEXPERT_DEALLOC(globals.metrics_file);

    return PERFEXPERT_SUCCESS;
}

#ifdef __cplusplus
}
#endif

// EOF
