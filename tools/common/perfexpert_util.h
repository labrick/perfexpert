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

#ifndef PERFEXPERT_UTIL_H_
#define PERFEXPERT_UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _SYS_STAT_H
#include <sys/stat.h>
#endif

#ifndef _ERRNO_H
#include <errno.h>
#endif

#ifndef _STRING_H
#include <string.h>
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

#ifndef _LINUX_LIMITS_H
#include <linux/limits.h>
#endif

#include "perfexpert_alloc.h"
#include "perfexpert_constants.h"
#include "perfexpert_output.h"

/* perfexpert_util_make_path (create directory tree like 'mkdir -p') */
static inline int perfexpert_util_make_path(const char *path, int nmode) {
    char *p = NULL, *npath = NULL;
    struct stat sb;

    /* Check if we can create such directory */
    if (0 == stat(path, &sb)) {
        if (0 == S_ISDIR (sb.st_mode)) {
            OUTPUT(("%s '%s'",
                _ERROR((char *)"file exists but is not a directory"), path));
            return PERFEXPERT_ERROR;
        }
        if (chmod(path, nmode)) {
            OUTPUT(("%s %s: %s", _ERROR((char *)"system error"), path,
                strerror(errno)));
            return PERFEXPERT_ERROR;
        }
        return PERFEXPERT_SUCCESS; // directory already exists
    }

    /* Save a copy, so we can write to it */
    PERFEXPERT_ALLOC(char, npath, (strlen(path) + 1));
    strncpy(npath, path, strlen(path));

    /* Skip leading slashes */
    p = npath;
    while ('/' == *p) {
        p++;
    }
    while (p = strchr(p, '/')) {
        *p = '\0';
        if (0 != stat(npath, &sb)) {
            if (mkdir(npath, nmode)) {
                OUTPUT(("%s '%s' %s", _ERROR((char *)"cannot create directory"),
                    npath, strerror(errno)));
                PERFEXPERT_DEALLOC(npath);
                return PERFEXPERT_ERROR;
            }
        } else if (0 == S_ISDIR(sb.st_mode)) {
            OUTPUT(("'%s': %s", npath,
                _ERROR((char *)"file exists but is not a directory")));
            PERFEXPERT_DEALLOC(npath);
            return PERFEXPERT_ERROR;
        }
        *p++ = '/'; /* restore slash */
        while ('/' == *p) {
            p++;
        }
    }
    
    /* Create the final directory component */
    if (stat(npath, &sb) && mkdir(npath, nmode)) { // This is not portable
        OUTPUT(("%s '%s' %s", _ERROR((char *)"cannot create directory"),
            npath, strerror(errno)));
        PERFEXPERT_DEALLOC(npath);
        return PERFEXPERT_ERROR;
    }

    PERFEXPERT_DEALLOC(npath);
    return PERFEXPERT_SUCCESS;
}

/* perfexpert_util_dir_exists */
static inline int perfexpert_util_dir_exists(const char *dir) {
    struct stat sb;

    if (0 != stat(dir, &sb)) {
        OUTPUT_VERBOSE((1, "%s (%s)",
            _RED((char *)"directory not found or you do not have permissions"),
            dir));
        return PERFEXPERT_ERROR;
    }
    if (!S_ISDIR(sb.st_mode)) {
        OUTPUT_VERBOSE((1, "%s (%s)", _RED((char *)"is not a directory"), dir));
        return PERFEXPERT_ERROR;        
    }

    return PERFEXPERT_SUCCESS;
}

/* perfexpert_util_remove_dir */
static inline int perfexpert_util_remove_dir(const char *dir) {
    char *command = NULL;

    if (PERFEXPERT_SUCCESS != perfexpert_util_dir_exists(dir)) {
        return PERFEXPERT_ERROR;
    }

    PERFEXPERT_ALLOC(char, command, (strlen(dir) + 8));
    sprintf(command, "rm -rf %s", dir); // It is pretty ugly :-(
    if (-1 == system(command)) {
        PERFEXPERT_DEALLOC(command);
        return PERFEXPERT_ERROR;
    }
    PERFEXPERT_DEALLOC(command);

    return PERFEXPERT_SUCCESS;
}

/* perfexpert_util_file_exists */
static inline int perfexpert_util_file_exists(const char *file) {
    if (0 != access(file, F_OK)) {
        OUTPUT_VERBOSE((10, "%s (%s)", _RED((char *)"file not found"), file));
        return PERFEXPERT_ERROR;
    }
    return PERFEXPERT_SUCCESS;
}

/* perfexpert_util_file_is_exec */
static inline int perfexpert_util_file_is_exec(const char *file) {
    if (0 != access(file, X_OK)) {		// 可执行文件
        OUTPUT_VERBOSE((10, "%s (%s)",
            _RED((char *)"file is not executable or does not exist"), file));
        return PERFEXPERT_ERROR;
    }
    return PERFEXPERT_SUCCESS;
}

/* perfexpert_util_filename_only */
static inline int perfexpert_util_filename_only(const char *all, char **only) {
    char *local_copy = NULL, *token = NULL, *last = NULL;

    PERFEXPERT_ALLOC(char, local_copy, (strlen(all) + 1));
    strcpy(local_copy, all);

    token = strtok(local_copy, "/");
	// 下面这个循环很有特点，最后如果token==NULL，就把上一个token返回
    while (token = strtok(NULL, "/")) {
        last = token;
    }

    if (NULL == last) {
        *only = (char *)all;
    } else {
        PERFEXPERT_ALLOC(char, token, (strlen(last) + 1));
        strcpy(token, last);
        *only = token;
    }

    PERFEXPERT_DEALLOC(local_copy);

    return PERFEXPERT_SUCCESS;
}

/* perfexpert_util_path_only */
static inline int perfexpert_util_path_only(const char *file, char **path) {
    char *prog = NULL, *my_path = NULL, *resolved_path = NULL;

    if (PERFEXPERT_SUCCESS != perfexpert_util_filename_only(file, &prog)) {
        OUTPUT(("%s", _ERROR((char *)"Error: unable to extract path")));
        return PERFEXPERT_ERROR;
    }

	// prog仅包含文件名称
    if (0 != strcmp(file, prog)) {
        PERFEXPERT_ALLOC(char, my_path, (strlen(file) - strlen(prog) + 1));
        strncpy(my_path, file, (strlen(file) - strlen(prog)));

        if (NULL == (resolved_path = realpath(my_path, resolved_path))) {
            resolved_path = getcwd(NULL, 0);
        }
        PERFEXPERT_DEALLOC(my_path);
    } else {
        /* Search in $PATH */
        char *env_path = NULL, try_file[PATH_MAX], *try_path;
        struct stat fin;

        PERFEXPERT_ALLOC(char, env_path, (strlen(getenv("PATH")) + 1));
        memcpy(env_path, getenv("PATH"), strlen(getenv("PATH")));

		// strsep:获得每个路径喽？
        while ((try_path = strsep(&env_path, ":")) != NULL) {
            if ((*try_path == '\0') || (*try_path == '.')) {
                try_path = getcwd(NULL, 0);		// 遇见'\0''.'是怎么处理的？
            }
			// 包含路径和文件名称以方便搜索
            sprintf(try_file, "%s/%s", try_path, file);

			// 在相应路径下搜索可执行文件
            if ((0 == access(try_file, X_OK)) && (0 == stat(try_file, &fin))) {
                if ((S_ISREG(fin.st_mode)) && ((0 != getuid()) ||
                    (0 != (fin.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))))) {
                    PERFEXPERT_ALLOC(char, resolved_path, strlen(try_path));
					// 搜索成功自把路径返回
                    memcpy(resolved_path, try_path, strlen(try_path));	
                }
            }
        }
        PERFEXPERT_DEALLOC(env_path);
    }

    *path = resolved_path;

    return PERFEXPERT_SUCCESS;
}

/* perfexpert_util_file_copy */
static inline int perfexpert_util_file_copy(const char *to, const char *from) {
    int fd_to = 0, fd_from = 0, saved_errno = 0;
    char buffer[1024];
    ssize_t nread = 0;

    fd_from = open(from, O_RDONLY);
    if (0 > fd_from) {
        return PERFEXPERT_ERROR;
    }

    fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (0 > fd_to) {
        goto ERROR;
    }

    while (0 < (nread = read(fd_from, buffer, sizeof(buffer)))) {
        char *out_ptr = buffer;
        ssize_t nwritten = 0;

        do {
            nwritten = write(fd_to, out_ptr, nread);
            if (0 <= nwritten) {
                nread -= nwritten;
                out_ptr += nwritten;
            } else if (EINTR != errno) {
                goto ERROR;
            }
        } while (0 < nread);
    }

    if (0 == nread) {
        if (0 > close(fd_to)) {
            fd_to = -1;
            goto ERROR;
        }
        close(fd_from);

        return PERFEXPERT_SUCCESS;
    }

    ERROR:
    saved_errno = errno;

    close(fd_from);
    if (0 <= fd_to) {
        close(fd_to);
    }

    errno = saved_errno;
    return PERFEXPERT_ERROR;
}

/* perfexpert_util_file_print */
static inline int perfexpert_util_file_print(const char *file) {
    FILE *file_FP = NULL;
    char buffer[1024];
    size_t nread = 0;

    if (PERFEXPERT_SUCCESS != perfexpert_util_file_exists(file)) {
        return PERFEXPERT_ERROR;
    }

    file_FP = fopen(file, "r");
    if (NULL != file_FP) {
        while (0 < (nread = fread(buffer, 1, sizeof(buffer), file_FP))) {
            fwrite(buffer, 1, nread, stdout);
        }
        if (ferror(file_FP)) {
            return PERFEXPERT_ERROR;
        }
        fclose(file_FP);
    } else {
        return PERFEXPERT_ERROR;
    }
    fflush(stdout);

    return PERFEXPERT_SUCCESS;
}

#ifdef __cplusplus
}
#endif

#endif /* PERFEXPERT_UTIL_H */
