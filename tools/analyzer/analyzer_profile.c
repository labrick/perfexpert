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
#include <strings.h>

/* PerfExpert headers */
#include "analyzer.h"
#include "analyzer_profile.h"
#include "perfexpert_alloc.h"
#include "perfexpert_constants.h"
#include "perfexpert_list.h"
#include "perfexpert_md5.h"
#include "perfexpert_output.h"
#include "perfexpert_util.h"

/* profile_parse_file */
int profile_parse_file(const char* file, const char* tool,
    perfexpert_list_t *profiles) {
	// 加载hpctoolkit工具
    if (PERFEXPERT_SUCCESS !=
        perfexpert_load_module(globals.tool, &(globals.toolmodule))) {
        OUTPUT(("%s [%s]", _ERROR("Error: unable to local tool module"),
            globals.tool));
        return PERFEXPERT_ERROR;
    }

    /* Just to be sure... */
	// 这是module模块的程序(../../module/...)，
    if (NULL == globals.toolmodule.parse_file) {
        OUTPUT(("%s [%s]",
            _ERROR("Error: tool module does not implement measurements()"),
            globals.tool));
        return PERFEXPERT_ERROR;
    }

    /* Call the measurement function for this tool */
    return globals.toolmodule.parse_file(file, profiles);
}

/* profile_aggregate_hotspots */
static int profile_aggregate_hotspots(profile_t *profile) {
    procedure_t *hotspot = NULL;
    procedure_t *aggregated_hotspot = NULL;

    OUTPUT_VERBOSE((5, "%s", _YELLOW("   Aggregating hotspots")));

    /* Filling the hotspot that should last */
    PERFEXPERT_ALLOC(procedure_t, aggregated_hotspot, sizeof(procedure_t));
    aggregated_hotspot->id = 0;
    aggregated_hotspot->line = 0;
    aggregated_hotspot->valid = PERFEXPERT_TRUE;
    aggregated_hotspot->cycles = 0.0;
    aggregated_hotspot->variance = 0.0;
    aggregated_hotspot->importance = 0.0;
    aggregated_hotspot->instructions = 0.0;
    aggregated_hotspot->name = profile->name;
    aggregated_hotspot->type = PERFEXPERT_HOTSPOT_PROGRAM;
    aggregated_hotspot->file = NULL;
    aggregated_hotspot->module = NULL;
    aggregated_hotspot->lcpi_by_name = NULL;
    aggregated_hotspot->metrics_by_id = NULL;
    aggregated_hotspot->metrics_by_name = NULL;
    perfexpert_list_construct(&(aggregated_hotspot->metrics));
    perfexpert_list_item_construct(
        (perfexpert_list_item_t *)aggregated_hotspot);

    /* For each hotspot... */
	// 这里就是把profile中所有热点的metri加入到aggergated_hotspot表中，并从原表中删除
    hotspot = (procedure_t *)perfexpert_list_get_first(&(profile->hotspots));
    while ((perfexpert_list_item_t *)hotspot != &(profile->hotspots.sentinel)) {
        metric_t *metric;

        OUTPUT_VERBOSE((10,
            "      moving %d metrics from %s to the aggregated hotspot",
            (int)perfexpert_list_get_size(&(hotspot->metrics)),
            _RED(hotspot->name)));

        /* For all metrics in hotspot... */
		// 本来hotspot放在profiles->profile(cycle)->hotspots->hotspot(cycle)->
		// metrics->metric(cycle)
		// 现在把它转移到profiles->profile(cycle)->hotspots->metric(cycle)中
        while (0 != perfexpert_list_get_size(&(hotspot->metrics))) {
            metric = (metric_t *)perfexpert_list_get_first(&(hotspot->metrics));

            /* Move this metric to the aggregated hotspot's list of metrics */
        // profile->hotspots->hotspot(cycle)->metrics->metric(cycle)(一个一个删除)
            perfexpert_list_remove_item(&(hotspot->metrics),
                (perfexpert_list_item_t *)metric);
            perfexpert_list_append(
                (perfexpert_list_t *)&(aggregated_hotspot->metrics),
                (perfexpert_list_item_t *)metric);
        }

        /* Remove hotspot from the profile's list of hotspots and move on */
		// 把整个hotspot条目去除掉
        // profile->hotspots->hotspot(cycle)(一个一个删除)
        perfexpert_list_remove_item(&(profile->hotspots), 
            (perfexpert_list_item_t *)hotspot);
        hotspot = (procedure_t *)perfexpert_list_get_next(hotspot);
    }

    /* Add the aggregated hotspot to the profile's list of hotspots */
	// aggregated_hotspot是个局部变量，再把这个局部变量加入到
    // 这里profile->hotspots->metrics
    perfexpert_list_append(&(profile->hotspots), 
        (perfexpert_list_item_t *)aggregated_hotspot);

    OUTPUT_VERBOSE((10, "      aggregated hotspot has %d metrics",
        perfexpert_list_get_size(&(aggregated_hotspot->metrics))));

    return PERFEXPERT_SUCCESS;
}

/* profile_aggregate_metrics */
static int profile_aggregate_metrics(profile_t *profile, procedure_t *hotspot) {
    metric_t *current = NULL;
    metric_t *next = NULL;
    metric_t *next_next = NULL;

    // globals.thread存在则只显示这个id的metrics，如果不存在，则显示所有的
    if (-1 == globals.thread) {
        OUTPUT_VERBOSE((5, "      %s (metrics count: %d)",
            _CYAN("Aggregate metrics ignoring thread ID"),
            perfexpert_list_get_size(&(hotspot->metrics))));
    } else {
        OUTPUT_VERBOSE((5, "      %s (%d) (metrics count: %d)",
            _CYAN("Aggregate metrics by thread ID"), globals.thread,
            perfexpert_list_get_size(&(hotspot->metrics))));
    }

    /* Ignore and remove from list of metrics the ones with other thread IDs */
	// 忽略或者删除和指定线程号不相同的的指标
    current = (metric_t *)perfexpert_list_get_first(&(hotspot->metrics));
    while ((perfexpert_list_item_t *)current != &(hotspot->metrics.sentinel)) {
        next = (metric_t *)current->next;

		// globals.thread存在，而且current->thread不相同，则直接删除
        if ((-1 != globals.thread) && (current->thread != globals.thread)) {
            perfexpert_list_remove_item(&(hotspot->metrics),
                (perfexpert_list_item_t *)current);
            free(current);
        }
        // 如果thread不存在，则current->thread没有作用，直接清零
        if (-1 == globals.thread) {
            current->thread = 0;
        }
        current = next;
    }

    /* For all metrics in hotspot... */
	// 遍历热点中的所有指标
    current = (metric_t *)perfexpert_list_get_first(&(hotspot->metrics));
    while ((perfexpert_list_item_t *)current != &(hotspot->metrics.sentinel)) {
        if (9 <= globals.verbose) {
            // 上面讲所有的线程都设置为一样的，为globals.thread或者0，这里还有意义？
            printf("%s          %s(%d)[%d] ", PROGRAM_PREFIX,
                _GREEN(current->name), current->thread, current->experiment);
        }

        /* Compare current metric with the next metric in the list */
        next = (metric_t *)current->next;
        while ((perfexpert_list_item_t *)next != &(hotspot->metrics.sentinel)) {
            next_next = (metric_t *)next->next;

			// 记住current指向的是metric某一个条目
			// 是判断名字和公式都相同共同？？
			// 如果相同就把value直接相加？？？接着把重复的删除？
            if ((0 == strcmp(current->name, next->name)) &&
                (current->experiment == next->experiment)) {

                current->value += next->value;
                perfexpert_list_remove_item(&(hotspot->metrics),
                    (perfexpert_list_item_t *)next);
                PERFEXPERT_DEALLOC(next);

                if (10 <= globals.verbose) {
                    printf("%s ", _CYAN("removed"));
                }
            } else {
                if (10 <= globals.verbose) {
                    printf("%s(%d)[%d] ", next->name, next->thread,
                        next->experiment);
                }
            }
            /* Compare to the next metric */
            next = next_next;
        }
        if (9 <= globals.verbose) {
            printf("\n");
            fflush(stdout);
        }

        /* Do the same with the next metric */
        current = (metric_t *)perfexpert_list_get_next(current);
    }

    OUTPUT_VERBOSE((5, "      %s (%d)", _MAGENTA("new metrics count"),
        perfexpert_list_get_size(&(hotspot->metrics))));

    /* Hash metric by name (at this point it should be unique) */
    if (10 <= globals.verbose) {
        printf("%s          ", PROGRAM_PREFIX);
    }
    current = (metric_t *)perfexpert_list_get_first(&(hotspot->metrics));
	// 再次遍历？？？干嘛？上次是删除重复的，这次应该就是做正事了
    while ((perfexpert_list_item_t *)current != &(hotspot->metrics.sentinel)) {
        if (10 <= globals.verbose) {
            printf("%s[%d](%d) ", _CYAN(current->name), current->thread,
                current->experiment);
        }
        strcpy(current->name_md5, perfexpert_md5_string(current->name));
		// 一到hash就死了。。。。。。。。。。。。。。。。。。
        perfexpert_hash_add_str(hotspot->metrics_by_name, name_md5, current);
        current = (metric_t *)perfexpert_list_get_next(current);
    }
    if (10 <= globals.verbose) {
        printf("\n");
        fflush(stdout);
    }

    return PERFEXPERT_SUCCESS;
}

/* profile_flatten_all */
int profile_flatten_all(perfexpert_list_t *profiles) {
    profile_t *profile = NULL;

    OUTPUT_VERBOSE((5, "%s", _BLUE("Flattening profiles")));

    /* For each profile in the list of profiles... */
    profile = (profile_t *)perfexpert_list_get_first(profiles);
	// 再次遍历profiles，两次遍历目的不同
    while ((perfexpert_list_item_t *)profile != &(profiles->sentinel)) {
        OUTPUT_VERBOSE((10, " [%d] %s", profile->id, _GREEN(profile->name)));

        /* Aggregate all hotspots in this profile? */
        // 显示整个程序的信息而不是单个hotspot的信息
        if (PERFEXPERT_TRUE == globals.aggregate) {
			// 下面这个函数比较明确：
			// 就是遍历所有profile，找出热点加入aggergated_hotspot表，并从原表删除
            // 本来hotspot放在profiles->profile(cycle)->hotspots->hotspot(cycle)->
			// metrics->metric(cycle)
			// 现在把它转移到profiles->profile(cycle)->hotspots->metric(cycle)中
			if (PERFEXPERT_SUCCESS != profile_aggregate_hotspots(profile)) {
                OUTPUT(("%s (%s)",
                    _ERROR("Error: unable to aggregate profile's hotspots"),
                    profile->name));
                return PERFEXPERT_ERROR;
            }
            profile = (profile_t *)perfexpert_list_get_first(profiles);
        }

        /* Flatten all hotspots in this profile */
        if (PERFEXPERT_SUCCESS != profile_flatten_hotspots(profile)) {
            OUTPUT(("%s (%s)", _ERROR("Error: unable to flatten profile"),
                profile->name));
            return PERFEXPERT_ERROR;
        }

        /* Move on */
        profile = (profile_t *)perfexpert_list_get_next(profile);
    }
    return PERFEXPERT_SUCCESS;
}

/* profile_flatten_hotspots */
static int profile_flatten_hotspots(profile_t *profile) {
    procedure_t *hotspot_prev = NULL;
    procedure_t *hotspot = NULL;
    metric_t *metric = NULL;
    char key_md5[33];

    OUTPUT_VERBOSE((4, "   %s", _CYAN("Flatenning hotspots")));

    /* For each hotspot in the profile's list of hotspots... */
	// 遍历profile列表中所有热点
    hotspot = (procedure_t *)perfexpert_list_get_first(&(profile->hotspots));
    while ((perfexpert_list_item_t *)hotspot != &(profile->hotspots.sentinel)) {
		// 热点类型为函数
        if (PERFEXPERT_HOTSPOT_FUNCTION == hotspot->type) {
            OUTPUT_VERBOSE((8, "    [%d] %s (%s@%s:%d)", hotspot->id,
                _YELLOW(hotspot->name),
                hotspot->module != NULL ? hotspot->module->shortname : "---",
                hotspot->file != NULL ? hotspot->file->shortname : "---",
                hotspot->line));
        }
        if (PERFEXPERT_HOTSPOT_LOOP == hotspot->type) {
            loop_t *loop = (loop_t *)hotspot;
            OUTPUT_VERBOSE((8, "    [%d] %s (%s@%s:%d)", hotspot->id,
                _YELLOW("loop"), loop->procedure->module->shortname,
                loop->procedure->file->shortname, hotspot->line));
        }

        /* Aggregate threads measurements */
		// 整个程序所有线程策略？
		// profile hotspot同属于profiles的成员，将所有hotspot整理后放入hash表
        if (PERFEXPERT_SUCCESS != profile_aggregate_metrics(profile, hotspot)) {
            OUTPUT(("%s (%s)", _ERROR("Error: aggregating metrics"),
                hotspot->name));
            return PERFEXPERT_ERROR;
        }

        /* Check if the total number of instructions is present, if not delete
         * this hotspot from the list and move on, but first save the total
         * cycles (if present)!
         */
        strcpy(key_md5,
            perfexpert_md5_string(globals.toolmodule.total_instructions));
        perfexpert_hash_find_str(hotspot->metrics_by_name, key_md5, metric);
        if (NULL == metric) {
            strcpy(key_md5,
                perfexpert_md5_string(globals.toolmodule.total_cycles));
            perfexpert_hash_find_str(hotspot->metrics_by_name, key_md5, metric);
            if (NULL != metric) {
                profile->cycles += metric->value;
            }

            hotspot_prev = (procedure_t *)hotspot->prev;
            perfexpert_list_remove_item(&(profile->hotspots),
                (perfexpert_list_item_t *)hotspot);
            hotspot = hotspot_prev;

            OUTPUT_VERBOSE((8, "      %s (%s not found)",
                _RED("removed from list of hotspots"),
                globals.toolmodule.total_cycles));
        }
        hotspot = (procedure_t *)perfexpert_list_get_next(hotspot);
    }

    /* Calculate importance and variance */
    if (PERFEXPERT_SUCCESS != calculate_importance_variance(profile)) {
        OUTPUT(("%s (%s)", _ERROR("Error: calculating importance and variance"),
            hotspot->name));
        return PERFEXPERT_ERROR;
    }

    /* Compute LCPI */
    if (PERFEXPERT_SUCCESS != lcpi_compute(profile)) {
        OUTPUT(("%s (%s)", _ERROR("Error: calculating LCPI"), hotspot->name));
        return PERFEXPERT_ERROR;
    }

    return PERFEXPERT_SUCCESS;
}

/* profile_check_all */
// profiles是大工厂，包含了存放callees仓库(存放procedure和loop的callpath)
int profile_check_all(perfexpert_list_t *profiles) {
    profile_t *profile = NULL;

    OUTPUT_VERBOSE((5, "%s", _BLUE("Checking profiles")));

    profile = (profile_t *)perfexpert_list_get_first(profiles);
	// 遍历表中所有内容
    while ((perfexpert_list_item_t *)profile != &(profiles->sentinel)) {
        OUTPUT_VERBOSE((10, " [%d] %s", profile->id, _GREEN(profile->name)));
        if (0 < perfexpert_list_get_size(&(profile->callees))) {
            if (PERFEXPERT_SUCCESS !=
            profile_check_callpath(&(profile->callees), 0)) {
				// 看看上面函数干了什么？
                OUTPUT(("%s (%s)", _ERROR("Error: malformed profile"),
                    profile->name));
                return PERFEXPERT_ERROR;
           }
           globals.found_hotspots = PERFEXPERT_TRUE;
        }
        profile = (profile_t *)perfexpert_list_get_next(profile);
    }
    return PERFEXPERT_SUCCESS;
}

/* profile_check_callpath */
// 参数*calls，是小仓库的指针
static int profile_check_callpath(perfexpert_list_t *calls, int root) {
    callpath_t *callpath = NULL;
    char indent[256];
    int i = 0;

    bzero(indent, 256);
    if (127 < root) {
        root = 127;
    }
    for (i = 0; i <= root; i++) {
        strcat(indent, " .");
    }

    callpath = (callpath_t *)perfexpert_list_get_first(calls);
	// 再次遍历
    while ((perfexpert_list_item_t *)callpath != &(calls->sentinel)) {
		// 这里的type就是hotspot的类型
        if (PERFEXPERT_HOTSPOT_FUNCTION == callpath->procedure->type) {
            OUTPUT_VERBOSE((9, "%s [%d] %s (%s@%s:%d)", indent, callpath->id,
                _YELLOW(callpath->procedure->name),
                callpath->procedure->module->shortname,
                callpath->procedure->file->shortname,
                callpath->procedure->line));
        }
		// 如果type是循环类型
        if (PERFEXPERT_HOTSPOT_LOOP == callpath->procedure->type) {
			// 直接强制类型转换？这样做有什么问题吗？最后两个变量怎么解决？
            loop_t *loop = (loop_t *)callpath->procedure;
            OUTPUT_VERBOSE((9, "%s [%d] %s (%s@%s:%d)", indent, callpath->id,
                _YELLOW("loop"), loop->procedure->module->shortname,
                loop->procedure->file->shortname, loop->line));
        }

        if (0 < perfexpert_list_get_size(&(callpath->callees))) {
            root++;
            // 这里进行了递归，这个callees和profile中的callees有嘛区别嘛？
            // root应该表示深度，深度每次递归加深1
            profile_check_callpath(&(callpath->callees), root);
            root--;
        }
        callpath = (callpath_t *)perfexpert_list_get_next(callpath);
    }

    return PERFEXPERT_SUCCESS;
}

#ifdef __cplusplus
}
#endif

// EOF
