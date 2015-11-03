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
#include <string.h>

/* Utility headers */
#include <sqlite3.h>

/* PerfExpert headers */
#include "recommender.h"
#include "perfexpert_alloc.h"
#include "perfexpert_database.h"
#include "perfexpert_list.h"
#include "perfexpert_output.h"
#include "perfexpert_util.h"

/* select_recommendations_all */
// 参数segments为指向瓶颈链表首项的指针
int select_recommendations_all(perfexpert_list_t *segments) {
    int rc = PERFEXPERT_NO_REC;		// 无建议错误
    segment_t *segment = NULL;

    OUTPUT_VERBOSE((4, "%s", _BLUE("Selecting recommendations")));

    /* For each code bottleneck... */
    segment = (segment_t *)perfexpert_list_get_first(segments);
	// 它是一个循环链表，这里就是表示遍历的意思
    while ((perfexpert_list_item_t *)segment != &(segments->sentinel)) {
        /* ...select recommendations */
        switch (select_recommendations(segment)) {
            case PERFEXPERT_NO_REC:
                OUTPUT(("%s", _GREEN("Sorry, we have no recommendations")));
                goto MOVE_ON;

            case PERFEXPERT_SUCCESS:
                rc = PERFEXPERT_SUCCESS;
                break;

            case PERFEXPERT_ERROR:
            default: 
                OUTPUT(("%s", _ERROR("Error: selecting recommendations")));
                return PERFEXPERT_ERROR;
        }
        /* Move to the next code bottleneck */
        MOVE_ON:
        segment = (segment_t *)perfexpert_list_get_next(segment);
    }
    return rc;
}

/* select_recommendations */
// 看看选择建议都是干了什么？？？
// 参数segment为瓶颈list中一个条目的指针，也就是一个瓶颈的信息
int select_recommendations(segment_t *segment) {
    function_t *function = NULL;
    char *error_msg = NULL;
    char sql[BUFFER_SIZE];
    double weight = 0.0;
    int rc = 0;

    OUTPUT_VERBOSE((4, "   %s (%s:%d)", _YELLOW("selecting recommendation for"),
        segment->filename, segment->line_number));

    /* Select all functions, accumulate them */
	// functions表的功能：记录有关各个function的具体信息
    if (SQLITE_OK != sqlite3_exec(globals.db,
        "SELECT id, desc, statement FROM function;", accumulate_functions,
        (void *)&(segment->functions), &error_msg)) {
        OUTPUT(("%s %s", _ERROR("Error: SQL error"), error_msg));
        sqlite3_free(error_msg);
        return PERFEXPERT_ERROR;
    }

    OUTPUT_VERBOSE((8, "      %s (%d)", _MAGENTA("function(s) found"),
        perfexpert_list_get_size(&(segment->functions))));

    /* Create a temporary table to store possible recommendations */
    bzero(sql, BUFFER_SIZE);	// 将地址sql开始的BUFFER_SIZE个字节置零(string.h)
	// 构建临时表格语句，sql语句为：
	// CREATE TEMP TABLE recommendation_pid_rowid 
	// (function_id INTEGER,
	// recommendation_id INTEGER,
	// score FLOAT, weigth FLOAT);
    sprintf(sql, "%s_%d_%d %s %s", "CREATE TEMP TABLE recommendation",
        (int)getpid(), segment->rowid, "(function_id INTEGER,",
        "recommendation_id INTEGER, score FLOAT, weigth FLOAT);");
    OUTPUT_VERBOSE((10, "      %s", _CYAN("creating temporary table")));
    OUTPUT_VERBOSE((10, "         SQL: %s", _CYAN(sql)));

    if (SQLITE_OK != sqlite3_exec(globals.db, sql, NULL, NULL, &error_msg)) {
        OUTPUT(("%s %s", _ERROR("Error: SQL error"), error_msg));
        sqlite3_free(error_msg);
        return PERFEXPERT_ERROR;
    }

    /* Print recommendations header */
	// 输出:
	// #--------------------------
	// recommendation for 文件：行数.......
	// #--------------------------
    if (PERFEXPERT_SUCCESS != output_header(segment)) {
        OUTPUT(("%s", _ERROR("Error: unable to print recommendations header")));
        return PERFEXPERT_ERROR;
    }

    /* For each function... */
    function = (function_t *)perfexpert_list_get_first(&(segment->functions));
	// 是说functions也是构成一个循环链表吗？？？
    while ((perfexpert_list_item_t *)function != &(segment->functions.sentinel)) {
        sqlite3_stmt *statement;		// 以二进制形式存放sql语句的结构体
        
        OUTPUT_VERBOSE((8, "      %s '%s' [%d bytes]", _CYAN("running"),
            function->desc, strlen(function->statement)));

        /* Prepare the SQL statement */
		// function->statement为上面从数据库中读取的语句？？
/*
int sqlite3_prepare_v2(
	sqlite3 *db,            // 打开的数据库句柄
	const char *zSql,       // UTF8编码的SQL语句,可以参数化
	int nByte,              // SQL语句的字节长度,可以传递-1,即字符串以宽字符\0结尾
	sqlite3_stmt **ppStmt,  // 输出: 预编译之后的SQL语句句柄
	const char **pzTail    	// 输出: 指向zSql缓冲区中跳过有效SQL字符串的第一个字节
);
*/
		// 这样看function->statement中可就保存了sql语句啊。。。。
		// 其实这里和前面的sqlite3_exec函数功能是一样的，只不过这里不需要回调函数
		// 问题是function->statement中的语句是怎么来的？？
		// recommendation_input中没看到，而且是对哪个表格操作的也不明确
		// 不过应该是sql语句中规定的
        if (SQLITE_OK != sqlite3_prepare_v2(globals.db, function->statement,
            BUFFER_SIZE, &statement, NULL)) {
            OUTPUT(("%s %s", _ERROR("Error: SQL error"), error_msg));
            sqlite3_free(error_msg);
            return PERFEXPERT_ERROR;
        }

        /* Bind ROWID */
		// 因为没有了回调函数，所以这里进行值的绑定
		// sqlite3_bind_parameter_index(statement, "@RID")，返回列名的索引值??
        if (SQLITE_OK != sqlite3_bind_int(statement,
            sqlite3_bind_parameter_index(statement, "@RID"), segment->rowid)) {
            OUTPUT_VERBOSE((9, "         %s (%d)", _RED("ignoring @RID"),
                segment->rowid));
        }

        /* Bind loop depth */
        if (SQLITE_OK != sqlite3_bind_int(statement,
            sqlite3_bind_parameter_index(statement, "@LPD"),
            segment->loopdepth)) {
            OUTPUT_VERBOSE((9, "         %s (%d)", _RED("ignoring @LPD"),
                segment->loopdepth));
        }
		//////////////////////////////////////////////////////
		// 上面绑定一块还不是很清楚

        /* Run query */
		// The sqlite3_step() routine return 
		// SQLITE_ROW if it is returning a single row of the result set, 
		// or SQLITE_DONE if execution has completed, 
		// either normally or due to an error. 
        while (SQLITE_ROW == (rc = sqlite3_step(statement))) {
            /* It is possible that this function does not return results... */
            if (SQLITE_DONE == rc) {
                continue;
            }

            /* It is possible that this function returns an error... */
            if (SQLITE_ROW != rc) {
                return PERFEXPERT_ERROR;
            }

            /* ... but if there is one row, check if the two first columns are
             * SQLITE_INTEGER and SQLITE_FLOAT respectivelly
             */
			// 第一列一定为整型，第二列一定为浮点型，否则继续下一条指令
            if (SQLITE_INTEGER != sqlite3_column_type(statement, 0)) {
                OUTPUT(("         %s", _ERROR("1st column is not an integer")));
                continue;
            }
            if (SQLITE_FLOAT != sqlite3_column_type(statement, 1)) {
                OUTPUT(("         %s", _ERROR("2nd column is not a float")));
                continue;
            }

            /* Consider only the results where the score is positive */
			// 获得第一列数据，类型为double，是为分数值
            if (0 < sqlite3_column_double(statement, 1)) {
                /* Insert recommendation into the temporary table */
                bzero(sql, BUFFER_SIZE);
				// sql语句：
				// INSERT INTO recommendation_pid_rowid VALUES (
				// id, 第一列内容，第二列内容，weight);
                sprintf(sql, "%s_%d_%d %s (%d, %d, %f, %f);",
                    "INSERT INTO recommendation", (int)getpid(), segment->rowid,
                    "VALUES", function->id, sqlite3_column_int(statement, 0),
                    sqlite3_column_double(statement, 1), weight);
                if (SQLITE_OK != sqlite3_exec(globals.db, sql, NULL, NULL,
                    &error_msg)) {
                    OUTPUT(("%s %s", _ERROR("Error: SQL error"), error_msg));
                    sqlite3_free(error_msg);
                    return PERFEXPERT_ERROR;
                }

                OUTPUT_VERBOSE((10, "         Function=%d, Recom=%d, Score=%f",
                    function->id, sqlite3_column_int(statement, 0),
                    sqlite3_column_double(statement, 1)));
            }
        }

        /* Something went wrong :-/ */
        if (SQLITE_DONE != rc) {
            OUTPUT(("%s %s", _ERROR("Error: SQL error"), error_msg));
            sqlite3_free(error_msg);
            return PERFEXPERT_ERROR;
        }
        
        /* SQLite3 cleanup */
        if (SQLITE_OK != sqlite3_reset(statement)) {
            OUTPUT(("%s %s", _ERROR("Error: SQL error"), error_msg));
            sqlite3_free(error_msg);
            return PERFEXPERT_ERROR;
        }
        if (SQLITE_OK != sqlite3_finalize(statement)) {
            OUTPUT(("%s %s", _ERROR("Error: SQL error"), error_msg));
            sqlite3_free(error_msg);
            return PERFEXPERT_ERROR;
        }
        
        /* Move to the next code bottleneck */
        function = (function_t *)perfexpert_list_get_next(function);
    }

    /* Select top-N recommendations, output them besides the pattern */
    bzero(sql, BUFFER_SIZE);
	// SQL语句：
	// SELECT r.desc AS desc, r.reason AS reason, r.id AS id, r.example
	// AS example FROM recommendation AS r INNER JOIN recommendation_pid_rowid
	// AS m ON r.id = m.recommendation_id ORDER BY m.score DESC LIMIT rec_count
    sprintf(sql, "%s %s%d_%d %s %s %d;",
        "SELECT r.desc AS desc, r.reason AS reason, r.id AS id, r.example",
        "AS example FROM recommendation AS r INNER JOIN recommendation_",
        (int)getpid(), segment->rowid, "AS m ON r.id = m.recommendation_id",
        "ORDER BY m.score DESC LIMIT", globals.rec_count);

    OUTPUT_VERBOSE((10, "      %s [%d]", _CYAN("top-N"), globals.rec_count));
    OUTPUT_VERBOSE((10, "         SQL: %s", _CYAN(sql)));

    rc = PERFEXPERT_NO_REC;
    if (SQLITE_OK != sqlite3_exec(globals.db, sql, output_recommendations,
        (void *)&(rc), &error_msg)) {
        OUTPUT(("%s %s", _ERROR("Error: SQL error"), error_msg));
        sqlite3_free(error_msg);
        return PERFEXPERT_ERROR;
    }

    return rc;
}

/* accumulate_functions */
// 参数functions是perfexpert_list结构，将val中function信息构成新的function并加入functions
// 参数val第一个元素为id；第二个为描述；第三个为语句
int accumulate_functions(void *functions, int count, char **val, char **names) {
    function_t *function = NULL;
    
    /* Copy SQL query result into functions list */
    PERFEXPERT_ALLOC(function_t, function, sizeof(function_t));
	// 将function的前后指针全部置为NULL
    perfexpert_list_item_construct((perfexpert_list_item_t *)function);
    function->id = atoi(val[0]);
    PERFEXPERT_ALLOC(char, function->desc, (strlen(val[1]) + 1));
    strncpy(function->desc, val[1], strlen(val[1]));
    bzero(function->statement, BUFFER_SIZE);
    strncpy(function->statement, val[2], strlen(val[2]));
	// 向functions中插入新的function
    perfexpert_list_append((perfexpert_list_t *)functions,
        (perfexpert_list_item_t *)function);

    OUTPUT_VERBOSE((10, "      '%s'", function->desc));

    return PERFEXPERT_SUCCESS;
}

#ifdef __cplusplus
}
#endif

// EOF
