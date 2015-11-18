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

#ifndef PERFEXPERT_STRING_H_
#define PERFEXPERT_STRING_H_

#ifdef __cplusplus
extern "C" {
#endif
    
#ifndef _STRING_H
#include <string.h>
#endif

#include "perfexpert_alloc.h"
#include "perfexpert_constants.h"

/* perfexpert_string_split:以token为delimiter，拆分""之外的内容放入**out中 */
static inline int perfexpert_string_split(char *in, char **out, int token) {
    int i = 0, j = 0, c = 0;
    int quoted = PERFEXPERT_FALSE;	// 作为一个是否拆分的开关，默认可拆

    while (0 != in[i]) {
        if (('"' == in[i]) && (PERFEXPERT_FALSE == quoted)) {
            quoted = PERFEXPERT_TRUE;
            i++;
            continue;
        }
        if (('"' == in[i]) && (PERFEXPERT_TRUE == quoted)) {
            quoted = PERFEXPERT_FALSE;
            i++;
            continue;
        }
        if ((token == in[i]) && (PERFEXPERT_FALSE == quoted)) {
            if (0 < (i - j)) {
                PERFEXPERT_ALLOC(char, out[c], (i - j + 1));
                memcpy((void *)&(out[c][0]), (void *)&(in[j]), i - j);
                out[c][i-j] = 0;
                c++;
            }
            j = i + 1;	// 记录上次拆过之后的位置，+1跳过delimiter
        }
        i++;
    }
    PERFEXPERT_ALLOC(char, out[c], (i - j + 1));
    memcpy((void *)&(out[c][0]), (void *)&(in[j]), i - j);
    out[c][i-j] = 0;
    out[c + 1] = NULL;

    return PERFEXPERT_SUCCESS;
}

/* perfexpert_string_remove_spaces */
static inline char* perfexpert_string_remove_spaces(char *str) {
    int i;

    while (' ' == str[0]) {
        for (i = 0; (i + 1) < strlen(str); i++) {
            str[i] = str[i + 1];
        }
        str[i] = 0;
    }
    while (' ' == str[strlen(str) - 1]) {
        str[strlen(str) - 1] = 0;
    }
    return str;
}

/* perfexpert_string_remove_char_pos */
static inline char* perfexpert_string_remove_char_pos(char *str, int token,
    int pos) {
    int i;

    if (str[pos] == (char)token) {
        for (i = pos; (i + 1) < strlen(str); i++) {
            str[i] = str[i + 1];
        }
        str[i] = 0;
    }
    return str;
}

/* perfexpert_string_remove_char */
static inline char* perfexpert_string_remove_char(char *str, int token) {
    char *output = str;
    int i, j;

    for (i = 0, j = 0; i < strlen(str); i++, j++) {
        if (str[i] != (char)token) {
            output[j] = str[i];
        } else {
            j--;
        }
    }
    output[j] = 0;
    return output;
}

/* perfexpert_string_replace_char */
static inline char* perfexpert_string_replace_char(char *str, int out, int in) {
    int i;

    for (i = 0; i < strlen(str); i++) {
        if (str[i] == (char)out) {
            str[i] = (char)in;
        }
    }
    return str;
}

#ifdef __cplusplus
}
#endif

#endif /* PERFEXPERT_STRING_H */
