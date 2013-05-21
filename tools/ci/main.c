/*
 * Copyright (c) 2013  University of Texas at Austin. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * This file is part of PerfExpert.
 *
 * PerfExpert is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * PerfExpert is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with PerfExpert. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Leonardo Fialho
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
#include <getopt.h>
#include <inttypes.h>

/* PerfExpert headers */
#include "config.h"
#include "ci.h"
#include "perfexpert_output.h"
#include "perfexpert_util.h"

/* Global variables, try to not create them! */
globals_t globals; // Variable to hold global options, this one is OK

/* main, life starts here */
int main(int argc, char** argv) {
    perfexpert_list_t *functions;
    function_t *function = NULL;

    /* Set default values for globals */
    globals = (globals_t) {
        .verbose       = 0,    // int
        .verbose_level = 0,    // int
        .use_stdin     = 0,    // int
        .use_stdout    = 1,    // int
        .inputfile     = NULL, // char *
        .outputdir     = NULL, // char *
        .workdir       = NULL, // char *
        .colorful      = 0     // int
    };

    /* Parse command-line parameters */
    if (PERFEXPERT_SUCCESS != parse_cli_params(argc, argv)) {
        OUTPUT(("%s", _ERROR("Error: parsing command line arguments")));
        exit(PERFEXPERT_ERROR);
    }
    if (NULL == globals.outputdir) {
        OUTPUT(("%s", _ERROR("Error: undefined output directory")));
        show_help();
    }

    /* Create the list of fragments */
    functions = (perfexpert_list_t *)malloc(sizeof(perfexpert_list_t));
    if (NULL == functions) {
        OUTPUT(("%s", _ERROR("Error: out of memory")));
        exit(PERFEXPERT_ERROR);
    }
    perfexpert_list_construct(functions);

    /* Parse input parameters */
    if (1 == globals.use_stdin) {
        if (PERFEXPERT_SUCCESS != parse_transformation_params(functions,
                                                              stdin)) {
            OUTPUT(("%s", _ERROR("Error: parsing input params")));
            exit(PERFEXPERT_ERROR);
        }
    } else {
        if (NULL != globals.inputfile) {
            FILE *inputfile_FP = NULL;

            /* Open input file */
            if (NULL == (inputfile_FP = fopen(globals.inputfile, "r"))) {
                OUTPUT(("%s (%s)", _ERROR("Error: unable to open input file"),
                        globals.inputfile));
                return PERFEXPERT_ERROR;
            } else {
                if (PERFEXPERT_SUCCESS != parse_transformation_params(functions,
                                                                      inputfile_FP)) {
                    OUTPUT(("%s", _ERROR("Error: parsing input params")));
                    exit(PERFEXPERT_ERROR);
                }
                fclose(inputfile_FP);
            }
        } else {
            OUTPUT(("%s", _ERROR("Error: undefined input")));
            show_help();
        }
    }

    /* Integrate functions: 4 steps */
    /* Step 1: open ROSE */
    function = (function_t *)perfexpert_list_get_first(functions);
    if (PERFEXPERT_ERROR == open_rose(function->source_file)) {
        OUTPUT(("%s", _ERROR("Error: starting Rose")));
        exit(PERFEXPERT_ERROR);
    }

    /* Step 2: replace functions */
    function = (function_t *)perfexpert_list_get_first(functions);

    OUTPUT_VERBOSE((7, "%s", _YELLOW("replacing functions")));

    while ((perfexpert_list_item_t *)function != &(functions->sentinel)) {
        if (PERFEXPERT_ERROR == replace_function(function)) {
            OUTPUT(("%s (%s at %s)", _ERROR("Error: replacing function"),
                    function->function_name, function->source_file));
            exit(PERFEXPERT_ERROR);
        }
        function = (function_t *)perfexpert_list_get_next(function);
    }

    /* Step 3: output modified code */
    // TODO: call 'output_modified_code'

    /* Step 4: close rose */
    if (PERFEXPERT_ERROR == close_rose()) {
        OUTPUT(("%s", _ERROR("Error: closing Rose")));
        exit(PERFEXPERT_ERROR);
    }

    /* Free memory */
    while (PERFEXPERT_FALSE == perfexpert_list_is_empty(functions)) {
        function = (function_t *)perfexpert_list_get_first(functions);
        perfexpert_list_remove_item(functions, (perfexpert_list_item_t *)function);
        free(function->source_file);
        free(function->function_name);
        free(function->replacement_file);
        free(function);
    }
    perfexpert_list_destruct(functions);
    free(functions);

    return PERFEXPERT_SUCCESS;
}

/* show_help */
static void show_help(void) {
    OUTPUT_VERBOSE((10, "printing help"));

    /*      12345678901234567890123456789012345678901234567890123456789012345678901234567890 */
    printf("Usage: perfexpert_ci -i|-f file [-vch] [-l level] [-a dir]");
    printf("\n");
    printf("  -i --stdin           Use STDIN as input for patterns\n");
    printf("  -f --inputfile       Use 'file' as input for patterns\n");
    printf("  -a --automatic       Use automatic performance optimization and create files\n");
    printf("                       into 'dir' directory (default: off).\n");
    printf("  -v --verbose         Enable verbose mode using default verbose level (5)\n");
    printf("  -l --verbose_level   Enable verbose mode using a specific verbose level (1-10)\n");
    printf("  -c --colorful        Enable colors on verbose mode, no weird characters will\n");
    printf("                       appear on output files\n");
    printf("  -h --help            Show this message\n");

    /* I suppose that if I've to show the help is because something is wrong,
     * or maybe the user just want to see the options, so it seems to be a
     * good idea to exit here with an error code.
     */
    exit(PERFEXPERT_ERROR);
}

/* parse_env_vars */
static int parse_env_vars(void) {
    // TODO: add here all the parameter we have for command line arguments
    char *temp_str;

    /* Get the variables */
    temp_str = getenv("PERFEXPERT_VERBOSE_LEVEL");
    if (NULL != temp_str) {
        globals.verbose_level = atoi(temp_str);
        if (0 != globals.verbose_level) {
            OUTPUT_VERBOSE((5, "ENV: verbose_level=%d", globals.verbose_level));
        }
    }

    OUTPUT_VERBOSE((4, "=== %s", _BLUE("Environment variables")));

    return PERFEXPERT_SUCCESS;
}

/* parse_cli_params */
static int parse_cli_params(int argc, char *argv[]) {
    /** Temporary variable to hold parameter */
    int parameter;
    /** getopt_long() stores the option index here */
    int option_index = 0;

    /* If some environment variable is defined, use it! */
    if (PERFEXPERT_SUCCESS != parse_env_vars()) {
        OUTPUT(("%s", _ERROR("Error: parsing environment variables")));
        exit(PERFEXPERT_ERROR);
    }

    while (1) {
        /* get parameter */
        parameter = getopt_long(argc, argv, "a:cvhif:l:o:", long_options,
                                &option_index);

        /* Detect the end of the options */
        if (-1 == parameter) {
            break;
        }

        switch (parameter) {
            /* Verbose level */
            case 'l':
                globals.verbose = 1;
                globals.verbose_level = atoi(optarg);
                OUTPUT_VERBOSE((10, "option 'l' set"));
                if (0 >= atoi(optarg)) {
                    OUTPUT(("%s (%d)",
                            _ERROR("Error: invalid debug level (too low)"),
                            atoi(optarg)));
                    show_help();
                }
                if (10 < atoi(optarg)) {
                    OUTPUT(("%s (%d)",
                            _ERROR("Error: invalid debug level (too high)"),
                            atoi(optarg)));
                    show_help();
                }
                break;

            /* Activate verbose mode */
            case 'v':
                globals.verbose = 1;
                if (0 == globals.verbose_level) {
                    globals.verbose_level = 5;
                }
                OUTPUT_VERBOSE((10, "option 'v' set"));
                break;

            /* Activate colorful mode */
            case 'c':
                globals.colorful = 1;
                OUTPUT_VERBOSE((10, "option 'c' set"));
                break;

            /* Show help */
            case 'h':
                OUTPUT_VERBOSE((10, "option 'h' set"));
                show_help();

            /* Use STDIN? */
            case 'i':
                globals.use_stdin = 1;
                OUTPUT_VERBOSE((10, "option 'i' set"));
                break;

            /* Use input file? */
            case 'f':
                globals.use_stdin = 0;
                globals.inputfile = optarg;
                OUTPUT_VERBOSE((10, "option 'f' set [%s]", globals.inputfile));
                break;

            /* Output directory */
            case 'o':
                globals.outputdir = optarg;
                OUTPUT_VERBOSE((10, "option 'o' set [%s]", globals.outputdir));
                break;
                
            /* Use automatic optimization? */
            case 'a':
                globals.automatic = 1;
                globals.use_stdout = 0;
                globals.workdir = optarg;
                OUTPUT_VERBOSE((10, "option 'a' set [%s]", globals.workdir));
                break;

            /* Unknown option */
            case '?':
                show_help();

            default:
                exit(PERFEXPERT_ERROR);
        }
    }
    OUTPUT_VERBOSE((4, "=== %s", _BLUE("CLI params")));
    OUTPUT_VERBOSE((10, "Summary of selected options:"));
    OUTPUT_VERBOSE((10, "   Verbose:                    %s",
                    globals.verbose ? "yes" : "no"));
    OUTPUT_VERBOSE((10, "   Verbose level:              %d",
                    globals.verbose_level));
    OUTPUT_VERBOSE((10, "   Colorful verbose?           %s",
                    globals.colorful ? "yes" : "no"));
    OUTPUT_VERBOSE((10, "   Use STDOUT?                 %s",
                    globals.use_stdout ? "yes" : "no"));
    OUTPUT_VERBOSE((10, "   Use STDIN?                  %s",
                    globals.use_stdin ? "yes" : "no"));
    OUTPUT_VERBOSE((10, "   Input file:                 %s",
                    globals.inputfile ? globals.inputfile : "(null)"));
    OUTPUT_VERBOSE((10, "   Output directory:           %s",
                    globals.outputdir));
    OUTPUT_VERBOSE((10, "   Use automatic optimization? %s",
                    globals.automatic ? "yes" : "no"));
    OUTPUT_VERBOSE((10, "   Temporary directory:        %s",
                    globals.workdir ? globals.workdir : "(null)"));

    /* Not using OUTPUT_VERBOSE because I want only one line */
    if (8 <= globals.verbose_level) {
        int i;
        printf("%s complete command line:", PROGRAM_PREFIX);
        for (i = 0; i < argc; i++) {
            printf(" %s", argv[i]);
        }
        printf("\n");
    }

    return PERFEXPERT_SUCCESS;
}

/* parse_transformation_params */
static int parse_transformation_params(perfexpert_list_t *functions_p,
                                       FILE *inputfile_p) {
    function_t *function;
    char buffer[BUFFER_SIZE];
    int  input_line = 0;

    OUTPUT_VERBOSE((4, "=== %s", _BLUE("Parsing fragments file")));

    /* Which INPUT we are using? (just a double check) */
    if ((NULL == inputfile_p) && (globals.use_stdin)) {
        inputfile_p = stdin;
    }
    if (globals.use_stdin) {
        OUTPUT_VERBOSE((3, "using STDIN as input for fragments"));
    } else {
        OUTPUT_VERBOSE((3, "using (%s) as input for fragments",
                        globals.inputfile));
    }

    /* For each line in the INPUT file... */
    OUTPUT_VERBOSE((7, "--- parsing input file"));

    bzero(buffer, BUFFER_SIZE);
    while (NULL != fgets(buffer, BUFFER_SIZE - 1, inputfile_p)) {
        node_t *node;
        int temp;

        input_line++;

        /* Ignore comments */
        if (0 == strncmp("#", buffer, 1)) {
            continue;
        }

        /* Is this line a new recommendation? */
        if (0 == strncmp("%", buffer, 1)) {
            char temp_str[BUFFER_SIZE];

            OUTPUT_VERBOSE((5, "(%d) --- %s", input_line,
                            _GREEN("new function found")));

            /* Create a list item for this code fragment */
            function = (function_t *)malloc(sizeof(function_t));
            if (NULL == function) {
                OUTPUT(("%s", _ERROR("Error: out of memory")));
                exit(PERFEXPERT_ERROR);
            }
            perfexpert_list_item_construct((perfexpert_list_item_t *)function);

            /* Initialize some elements on 'fragment' */
            function->source_file = NULL;
            function->function_name = NULL;
            function->replacement_file = NULL;

            /* Add this item to 'functions_p' */
            perfexpert_list_append(functions_p,
                                   (perfexpert_list_item_t *)function);

            continue;
        }

        node = (node_t *)malloc(sizeof(node_t) + strlen(buffer) + 1);
        if (NULL == node) {
            OUTPUT(("%s", _ERROR("Error: out of memory")));
            exit(PERFEXPERT_ERROR);
        }
        bzero(node, sizeof(node_t) + strlen(buffer) + 1);
        node->key = strtok(strcpy((char*)(node + 1), buffer), "=\r\n");
        node->value = strtok(NULL, "\r\n");

        /* Code param: code.filename */
        if (0 == strncmp("code.filename", node->key, 13)) {
            function->source_file = (char *)malloc(strlen(node->value) + 1);
            if (NULL == function->source_file) {
                OUTPUT(("%s", _ERROR("Error: out of memory")));
                exit(PERFEXPERT_ERROR);
            }
            bzero(function->source_file, strlen(node->value) + 1);
            strcpy(function->source_file, node->value);
            OUTPUT_VERBOSE((10, "(%d) %s [%s]", input_line,
                            _MAGENTA("source file:"), function->source_file));
            free(node);
            continue;
        }
        /* Code param: code.function_name */
        if (0 == strncmp("code.function_name", node->key, 18)) {
            function->function_name = (char *)malloc(strlen(node->value) + 1);
            if (NULL == function->function_name) {
                OUTPUT(("%s", _ERROR("Error: out of memory")));
                exit(PERFEXPERT_ERROR);
            }
            bzero(function->function_name, strlen(node->value) + 1);
            strcpy(function->function_name, node->value);
            OUTPUT_VERBOSE((10, "(%d) %s [%s]", input_line,
                            _MAGENTA("function name:"),
                            function->function_name));
            free(node);
            continue;
        }
        /* Code param: perfexpert_ct.replacement_function */
        if (0 == strncmp("perfexpert_ct.replacement_function", node->key, 31)) {
            function->replacement_file = (char *)malloc(strlen(node->value) + 1);
            if (NULL == function->replacement_file) {
                OUTPUT(("%s", _ERROR("Error: out of memory")));
                exit(PERFEXPERT_ERROR);
            }
            bzero(function->replacement_file, strlen(node->value) + 1);
            strcpy(function->replacement_file, node->value);
            OUTPUT_VERBOSE((10, "(%d) %s [%s]", input_line, _MAGENTA("type:"),
                            function->replacement_file));
            free(node);
            continue;
        }

        /* Should never reach this point, only if there is garbage in the input
         * file.
         */
        OUTPUT_VERBOSE((4, "(%d) %s (%s = %s)", input_line,
                        _RED("ignored line"), node->key, node->value));
        free(node);
    }

    /* print a summary of 'fragments' */
    OUTPUT_VERBOSE((4, "%d %s", perfexpert_list_get_size(functions_p),
                    _GREEN("function(s) found")));

    function = (function_t *)perfexpert_list_get_first(functions_p);
    while ((perfexpert_list_item_t *)function != &(functions_p->sentinel)) {
        OUTPUT_VERBOSE((4, "   [%s] [%s]", function->source_file,
                        function->function_name));
        function = (function_t *)perfexpert_list_get_next(function);
    }

    OUTPUT_VERBOSE((4, "==="));

    return PERFEXPERT_SUCCESS;
}

#ifdef __cplusplus
}
#endif

// EOF
