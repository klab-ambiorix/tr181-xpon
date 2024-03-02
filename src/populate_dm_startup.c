/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2022 SoftAtHome
**
** Redistribution and use in source and binary forms, with or
** without modification, are permitted provided that the following
** conditions are met:
**
** 1. Redistributions of source code must retain the above copyright
** notice, this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above
** copyright notice, this list of conditions and the following
** disclaimer in the documentation and/or other materials provided
** with the distribution.
**
** Subject to the terms and conditions of this license, each
** copyright holder and contributor hereby grants to those receiving
** rights under this license a perpetual, worldwide, non-exclusive,
** no-charge, royalty-free, irrevocable (except for failure to
** satisfy the conditions of this license) patent license to make,
** have made, use, offer to sell, sell, import, and otherwise
** transfer this software, where such license applies only to those
** patent claims, already acquired or hereafter acquired, licensable
** by such copyright holder or contributor that are necessarily
** infringed by:
**
** (a) their Contribution(s) (the licensed copyrights of copyright
** holders and non-copyrightable additions of contributors, in
** source or binary form) alone; or
**
** (b) combination of their Contribution(s) with the work of
** authorship to which such Contribution(s) was added by such
** copyright holder or contributor, if, at the time the Contribution
** is added, such addition causes such combination to be necessarily
** infringed. The patent license shall not apply to any other
** combinations which include the Contribution.
**
** Except as expressly stated above, no rights or licenses from any
** copyright holder or contributor is granted under this license,
** whether expressly, by implication, estoppel or otherwise.
**
** DISCLAIMER
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
** CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
** INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
** CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
** USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
** AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
** ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
** POSSIBILITY OF SUCH DAMAGE.
**
****************************************************************************/

/* Related header */
#include "populate_dm_startup.h"

/* System headers */
#include <stdio.h>  /* snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strlen() */

/* Other libraries' headers */
#include <amxc/amxc_macros.h>
#include <amxc/amxc.h>
#include <amxp/amxp_timer.h>

/* Own headers */
#include "data_model.h"
#include "dm_info.h"
#include "pon_ctrl.h"
#include "xpon_mgr_constants.h" /* SHORT_TIMEOUT_MS */
#include "xpon_trace.h"

/* Timer to query for the existence of ONUs */
static amxp_timer_t* s_timer_query_onus = NULL;
/**
 * Counts the number of times the plugin has queried for the existence of ONUs
 * since startup.
 */
static uint32_t s_cntr_query_onus = 0;

/* List of tasks of type task_type_t. */
static amxc_llist_t s_tasks = { 0 };
/* Timer to handle tasks in 's_tasks' */
static amxp_timer_t* s_timer_handle_tasks = NULL;
/* List of ONU's to indicate they are initialised */
static bool s_onu_initialised[MAX_NR_OF_ONUS] = { 0 };


/**
 * Query ONU instances every 10 s during the first 5 minutes after startup.
 *
 * 30 intervals of 10 s corresponds to 5 minutes.
 */
#define QUERY_ONUS_INTERVAL_MS 10000
#define QUERY_ONUS_NR_TIMES 30

/**
 * Task type.
 *
 * When populating the XPON DM at startup, the plugin schedules 2 types of tasks:
 *
 * - tasks to find out which instances it should create for a template object,
 *   e.g., for an object such as "XPON.ONU". They have 'task_query_indexes' as
 *   type.
 *
 * - tasks to query the values to be assigned to the params of the object. They
 *   have 'task_query_content' as type.
 */
typedef enum _task_type {
    task_none,
    task_query_indexes,
    task_query_content,
    task_type_nr
} task_type_t;

/**
 * A task to help in populating the XPON DM after startup.
 *
 * - path   object path, e.g. "XPON.ONU"
 * - index  index instance for a template object. 0 if not applicable.
 * - type   task type. See task_type_t.
 * - it     to put a task in the list 's_tasks'
 *
 * If 'path' refers to a template object, then:
 * - 'index' is 0 (not applicable) if 'type' is 'task_query_indexes': the task
 *   will query the instances of the template object. The task is not for a
 *   specific instance of the template object.
 * - 'index' is an actual instance index if type is 'task_query_content'.
 */
typedef struct _task {
    amxc_string_t path;
    uint32_t index;
    task_type_t type;
    amxc_llist_it_t it;
} task_t;

static const char* task_type_to_string(task_type_t type) {
    switch(type) {
    case task_query_indexes: return "query_indexes";
    case task_query_content: return "query_content";
    default: break;
    }
    return "none";
}

static void task_init(task_t* const task, const char* const path,
                      uint32_t index, task_type_t type) {
    amxc_string_init(&task->path, 0);
    amxc_string_set(&task->path, path);
    task->index = index;
    task->type = type;
}

static void task_clean(task_t* const task) {
    amxc_string_clean(&task->path);
}

/**
 * Create task and append it to 's_tasks'.
 *
 * If it fails to create the task or to append the task to 's_tasks', the
 * function is a no-op.
 *
 * @return task created on success, else NULL
 */
static task_t* task_create(const char* const path, uint32_t index, task_type_t type) {

    task_t* task = (task_t*) calloc(1, sizeof(task_t));
    when_null_trace(task, exit, ERROR, "Failed to allocate memory for task_t");

    task_init(task, path, index, type);

    if(amxc_llist_append(&s_tasks, &task->it)) {
        SAH_TRACEZ_ERROR(ME, "Failed to append task to 's_tasks'");
        SAH_TRACEZ_ERROR(ME, "  path='%s' index=%d type=%s", path, index,
                         task_type_to_string(type));
        task_clean(task);
        free(task);
        task = NULL;
    }

exit:
    return task;
}

static void task_delete(amxc_llist_it_t* hit) {
    task_t* task = amxc_container_of(hit, task_t, it);
    const char* path = amxc_string_get(&task->path, 0);
    SAH_TRACEZ_DEBUG(ME, "path='%s' index=%d type=%s", path, task->index,
                     task_type_to_string(task->type));
    task_clean(task);
    free(task);
}

/**
 * Do basic checks on the return values of a call towards the vendor module.
 *
 * @param[in] rc     return code of the call towards the vendor module.
 * @param[in] ret    result returned by vendor module
 * @param[in] func   name of the function called in the vendor module. For
 *                   logging only.
 * @param[in] path   object path for which function was called. This can refer
 *                   to a singleton or template object. For logging only.
 * @param[in] index  if function was called for an instance, this parameter is
 *                   the instance index. It must be 0 if the function was called
 *                   for a singleton. For logging only.
 *
 * The function returns false if:
 * - @a rc is not 0
 * - @a ret is not an htable
 *
 * @return true if function call towards the vendor module is ok according to
 *         the 1st checks done by this function, else false
 */
static bool check_return_values(int rc,
                                const amxc_var_t* const ret,
                                const char* func,
                                const char* const path,
                                uint32_t index) {
    bool rv = false;

    amxc_string_t buf; /* for logging only */
    amxc_string_init(&buf, 0);

    if(index) {
        amxc_string_setf(&buf, "%s(%s.%d)", func, path, index);
    } else {
        amxc_string_setf(&buf, "%s(%s)", func, path);
    }
    const char* const fc = amxc_string_get(&buf, 0); /*function call */

    if(rc) {
        SAH_TRACEZ_ERROR(ME, "%s failed: rc=%d", fc, rc);
        goto exit;
    }

    const uint32_t type = amxc_var_type_of(ret);
    if(type != AMXC_VAR_ID_HTABLE) {
        SAH_TRACEZ_ERROR(ME, "%s: type of 'ret'= %d != htable", fc, type);
        goto exit;
    }
    rv = true;
exit:
    amxc_string_clean(&buf);
    return rv;
}

/**
 * Query the children of an object.
 *
 * @param[in] path      object path of a singleton or template object
 * @param[in] index     instance index for a template object. 0 (not applicable)
 *                      for a singleton.
 * @param[in] id        object ID (caller normally derived it from @a path)
 * @param[in] templates true if the function should query the children which are
 *                      template objects, false if it should query the children
 *                      which are singleton objects.
 *
 * The function appends a task to 's_tasks' for each child. It does not do any
 * updates to the XPON DM.
 *
 * For a child which is a singleton, it adds a task of type 'task_query_content'.
 * For a child which is a template, it adds a task of type 'task_query_indexes'.
 *
 * Example:
 * An instance of "XPON.ONU.x.ANI" has children which are singletons, and
 * children which are templates:
 * - singletons: TC.ONUActivation,TC.PerformanceThresholds,TC.Alarms
 * - templates: TC.GEM.Port,Transceiver
 * If this function is called for such an instance, e.g., for the instance with
 * path = "XPON.ONU.1.ANI" and index = 1, then this function adds following
 * tasks to 's_tasks':
 * - 3 tasks of type 'task_query_content', for TC.ONUActivation,
 *   TC.PerformanceThresholds and TC.Alarms respectively.
 * - 2 tasks of type 'task_query_indexes', for TC.GEM.Port and Transceiver
 *   respectively.
 */
static void query_children(const char* const path, uint32_t index,
                           object_id_t id, bool templates) {

    const object_info_t* const info = dm_get_object_info(id);
    when_null(info, exit_no_cleanup);

    const char* const children_char = templates ? info->templates : info->singletons;
    if(NULL == children_char) {
        /* Nothing to do */
        return;
    }

    amxc_llist_t children;
    amxc_llist_init(&children);

    amxc_string_t str;
    amxc_string_init(&str, 0);
    amxc_string_set(&str, children_char);

    if(AMXC_STRING_SPLIT_OK != amxc_string_split_to_llist(&str, &children, ',')) {
        SAH_TRACEZ_ERROR(ME, "Failed to split '%s'", children_char);
        goto exit;
    }

    char child_path[256];

    amxc_llist_iterate(it, &children) {
        const amxc_string_t* const child = amxc_string_from_llist_it(it);
        const char* const child_cstr = amxc_string_get(child, 0);
        if(index) {
            snprintf(child_path, 256, "%s.%d.%s", path, index, child_cstr);
        } else {
            snprintf(child_path, 256, "%s.%s", path, child_cstr);
        }
        SAH_TRACEZ_DEBUG(ME, "Schedule query %s of %s",
                         templates ? "indexes" : "content", child_path);

        task_create(child_path, /*index=*/ 0,
                    templates ? task_query_indexes : task_query_content);
    }

exit:
    amxc_llist_clean(&children, amxc_string_list_it_free);
    amxc_string_clean(&str);
exit_no_cleanup:
    return;
}

/**
 * Ask vendor module for object content, update DM and add tasks to query children.
 *
 * @param[in] task  has info about the object to query. If the field 'index' is
 *                  0, the task queries a singleton object, else it queries the
 *                  instance with that index of a template object.
 *
 * The object content consists of the values to be assigned to the params of the
 * object.
 *
 * The function calls the function get_object_content() in the pon_ctrl
 * namespace of the vendor module. It adds the instance to the XPON DM if it
 * queried the instance of a template object, or it updates the object if it
 * queried a singleton object.
 *
 * The function subsequently adds tasks to 's_tasks' to handle the children of
 * the object. It adds a task of type 'task_query_content' for each child which
 * is a singleton, and it adds a task of type 'task_query_indexes' for each
 * child which is a template.
 */
static void query_content(const task_t* task) {

    const char* path = amxc_string_get(&task->path, 0);
    if(!path) {
        SAH_TRACEZ_ERROR(ME, "Failed to get path");
        return;
    }
    SAH_TRACEZ_DEBUG(ME, "path='%s' index=%d", path, task->index);

    amxc_var_t ret;
    amxc_var_init(&ret);

    const int rc = pon_ctrl_get_object_content(path, task->index, &ret);

    if(!check_return_values(rc, &ret, "get_object_content", path, task->index)) {
        goto exit;
    }
    if(amxc_var_add_key(cstring_t, &ret, "path", path) == NULL) {
        SAH_TRACEZ_ERROR(ME, "Failed to add 'path' to 'ret'");
        goto exit;
    }
    if(task->index) {
        if(amxc_var_add_key(uint32_t, &ret, "index", task->index) == NULL) {
            SAH_TRACEZ_ERROR(ME, "Failed to add 'index' to 'ret'");
            goto exit;
        }
        if(dm_add_or_change_instance_impl(&ret)) {
            SAH_TRACEZ_ERROR(ME, "Failed to create %s.%d", path, task->index);
            goto exit;
        }
    } else {
        if(dm_change_object(&ret)) {
            SAH_TRACEZ_ERROR(ME, "Failed to update %s", path);
            /* Do not jump to 'exit'. Still attempt to update any sub-objects. */
        }
    }

    const object_id_t id = dm_get_object_id(path);
    if(obj_id_unknown == id) {
        SAH_TRACEZ_DEBUG(ME, "%s has no sub-objects", path);
        goto exit;
    }

    query_children(path, task->index, id, false);
    query_children(path, task->index, id, true);

    if(obj_id_onu == id) {
        if((task->index != 0) && (task->index <= MAX_NR_OF_ONUS)) {
            SAH_TRACEZ_INFO(ME, "XPON.ONU.%d is initialised", task->index);
            s_onu_initialised[task->index - 1] = 1;
        } else {
            SAH_TRACEZ_ERROR(ME, "XPON.ONU: invalid index: %d: not in [1, %d]",
                             task->index, MAX_NR_OF_ONUS);
        }
    }

exit:
    amxc_var_clean(&ret);
}

/**
 * Ask the vendor module which instances it should create for a template object.
 *
 * @param[in] task  has info about the template object
 *
 * The function calls the function get_list_of_instances() in the pon_ctrl
 * namespace of the vendor module. It appends a task of type task_query_content
 * to 's_tasks' for each instance which does not yet exist in the XPON DM.
 *
 * The function does not change anything in the XPON DM.
 *
 * If the get_list_of_instances() call fails, the function logs an error and
 * immediately returns an error. It does not retry.
 *
 * Example:
 * if task->path is XPON.ONU, it asks the vendor module if there are any
 * instances for this object. If the vendor module returns the indexes 1 and 2,
 * XPON.ONU.1 already exists and XPON.ONU.2 not, the function appends a task to
 * 's_tasks' to query the content of XPON.ONU.2.
 */
static void query_indexes(const task_t* task) {

    const char* const path = amxc_string_get(&task->path, 0);
    if(!path) {
        SAH_TRACEZ_ERROR(ME, "Failed to get path");
        return;
    }
    SAH_TRACEZ_DEBUG(ME, "path='%s' index=%d", path, task->index);

    amxc_var_t ret;
    amxc_var_init(&ret);

    amxc_string_t indexes_str;
    amxc_string_init(&indexes_str, 0);

    amxc_llist_t indexes_list;
    amxc_llist_init(&indexes_list);

    const int rc = pon_ctrl_get_list_of_instances(path, &ret);

    if(!check_return_values(rc, &ret, "get_list_of_instances", path, task->index)) {
        goto exit;
    }

    const amxc_htable_t* const htable = amxc_var_constcast(amxc_htable_t, &ret);
    if(!amxc_htable_contains(htable, "indexes")) {
        SAH_TRACEZ_ERROR(ME, "htable 'ret' does not contain 'indexes'");
        goto exit;
    }

    const char* const indexes = GET_CHAR(&ret, "indexes");
    if(0 == strlen(indexes)) {
        SAH_TRACEZ_DEBUG(ME, "path='%s' index=%d: no instances", path, task->index);
        goto exit;
    }
    SAH_TRACEZ_DEBUG(ME, "indexes='%s'", indexes);

    amxc_string_set(&indexes_str, indexes);

    if(AMXC_STRING_SPLIT_OK != amxc_string_split_to_llist(&indexes_str, &indexes_list, ',')) {
        SAH_TRACEZ_ERROR(ME, "Failed to split '%s'", indexes);
        goto exit;
    }

    amxc_llist_iterate(it, &indexes_list) {
        const amxc_string_t* const index_str = amxc_string_from_llist_it(it);
        const char* const index_cstr = amxc_string_get(index_str, 0);
        if(!amxc_string_is_numeric(index_str)) {
            SAH_TRACEZ_ERROR(ME, "'%s' is not numeric", index_cstr);
            continue;
        }
        const uint32_t index = atoi(index_cstr);

        /**
         * Check instance exists.
         *
         * For an XPON.ONU instance also check if it's already initialised.
         */
        SAH_TRACEZ_DEBUG(ME, "check instance already exists path=%s, index=%d", path, index);
        if(strcmp(path, "XPON.ONU") == 0) {
            if((0 == index) || (index > MAX_NR_OF_ONUS)) {
                SAH_TRACEZ_ERROR(ME, "Invalid index: %d: not in [1, %d]", index, MAX_NR_OF_ONUS);
                continue;
            }

            if(dm_does_instance_exist(path, index) && s_onu_initialised[index - 1]) {
                SAH_TRACEZ_DEBUG(ME, "%s.%d already exists", path, index);
                continue;
            }
        } else {
            if(dm_does_instance_exist(path, index)) {
                SAH_TRACEZ_DEBUG(ME, "%s.%d already exists", path, index);
                continue;
            }

        }

        SAH_TRACEZ_DEBUG(ME, "Schedule query content of %s.%d", path, index);

        task_create(path, index, task_query_content);
    }

exit:
    amxc_llist_clean(&indexes_list, amxc_string_list_it_free);
    amxc_string_clean(&indexes_str);
    amxc_var_clean(&ret);
}

static void schedule_remaining_tasks(void) {

    if(!amxc_llist_is_empty(&s_tasks)) {
        amxp_timer_start(s_timer_handle_tasks, SHORT_TIMEOUT_MS);
    }
}

/**
 * Handle the 1st task from s_tasks, remove it from the list, and delete it.
 *
 * Subsequently reschedule the timer if there are remaining tasks.
 */
static void handle_task(UNUSED amxp_timer_t* timer, UNUSED void* priv) {

    if(amxc_llist_is_empty(&s_tasks)) {
        SAH_TRACEZ_WARNING(ME, "No tasks");
        return;
    }

    amxc_llist_it_t* const it = amxc_llist_get_first(&s_tasks);

    const task_t* const task = amxc_container_of(it, task_t, it);

    switch(task->type) {
    case task_query_indexes:
        query_indexes(task);
        break;
    case task_query_content:
        query_content(task);
        break;
    default:
        SAH_TRACEZ_WARNING(ME, "Unknown task type: %d", task->type);
        break;
    }
    /* Remove the 1st task from the list and delete it */
    amxc_llist_it_clean(it, task_delete);

    schedule_remaining_tasks();
}

/**
 * Ask vendor module for instances of XPON.ONU.
 *
 * @param[in,out] timer  timer to query the instances of XPON.ONU every 10 s
 *
 * The function stops the timer if the number of instances of XPON.ONU is
 * MAX_NR_OF_ONUS, or if 5 minutes passed since startup.
 *
 * Otherwise the function calls query_indexes() for XPON.ONU.
 */
static void query_onu_instances(amxp_timer_t* timer, UNUSED void* priv) {

    task_t task;
    task_init(&task, "XPON.ONU", /*index=*/ 0, task_query_indexes);

    SAH_TRACEZ_DEBUG(ME, "s_cntr_query_onus=%d", s_cntr_query_onus);

    /* Check if all the ONU instances are initialised */
    bool all_onus_initialised = true;
    for(int i = 0; i < MAX_NR_OF_ONUS; i++) {
        if(!s_onu_initialised[i]) {
            all_onus_initialised = false;
            break;
        }
    }
    if(all_onus_initialised) {
        SAH_TRACEZ_INFO(ME, "All %d ONUs found and initialised", MAX_NR_OF_ONUS);
        amxp_timer_stop(timer);
        goto exit;
    }

    ++s_cntr_query_onus;
    if(s_cntr_query_onus > QUERY_ONUS_NR_TIMES) {
        SAH_TRACEZ_DEBUG(ME, "Stop querying ONU instances");
        amxp_timer_stop(timer);
        goto exit;
    }

    query_indexes(&task);
    schedule_remaining_tasks();

exit:
    task_clean(&task);
}

/**
 * Initialize the part responsible for populating the XPON DM at startup.
 *
 * To start the whole process of the populating the XPON DM, the function
 * schedules a task to query the instances of XPON.ONU every 10 s. (The plugin
 * will stop the timer when it has found MAX_NR_OF_ONUS ONUs, or when 5 minutes
 * have passed since startup.)
 *
 * The plugin must call this function once at startup.
 */
bool pplt_dm_init(void) {
    bool rv = false;

    amxc_llist_init(&s_tasks);

    if(amxp_timer_new(&s_timer_handle_tasks, handle_task, NULL)) {
        SAH_TRACEZ_ERROR(ME, "Failed to create timer to handle tasks");
        goto exit;
    }

    if(amxp_timer_new(&s_timer_query_onus, query_onu_instances, NULL)) {
        SAH_TRACEZ_ERROR(ME, "Failed to create timer to query ONUs");
        goto exit;
    }
    amxp_timer_set_interval(s_timer_query_onus, QUERY_ONUS_INTERVAL_MS);

    /* Start querying the DM on the southbound IF */
    amxp_timer_start(s_timer_query_onus, SHORT_TIMEOUT_MS);

    rv = true;
exit:
    return rv;
}

/**
 * Clean up the part responsible for populating the XPON DM at startup.
 *
 * The plugin must call this function once when stopping.
 */
void pplt_dm_cleanup(void) {
    amxc_llist_clean(&s_tasks, task_delete);
    amxp_timer_delete(&s_timer_handle_tasks);
    amxp_timer_delete(&s_timer_query_onus);
}
