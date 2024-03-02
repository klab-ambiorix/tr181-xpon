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
#include "restore_to_hal.h"

/* System headers */
#include <stdlib.h> /* free() */
#include <string.h> /* strcmp() */

/* Other libraries' headers */
#include <amxc/amxc_macros.h>
#include <amxc/amxc.h>
#include <amxp/amxp_timer.h>

/* Own headers */
#include "data_model.h"         /* dm_get_nr_of_ethernet_uni_instances() */
#include "dm_info.h"            /* dm_get_object_id() */
#include "pon_ctrl.h"           /* pon_ctrl_set_enable() */
#include "xpon_mgr_constants.h" /* SHORT_TIMEOUT_MS */
#include "xpon_trace.h"

/* List of tasks of type rth_task_t */
static amxc_llist_t s_rth_tasks;

/* Timer to process tasks in s_rth_tasks */
static amxp_timer_t* s_timer_handle_rth_tasks = NULL;

static const uint8_t MAX_CHECKS_ON_NR_OF_UNIS_AND_ANIS = 4;

/**
 * Task to enable an object in the HAL.
 *
 * - path: path of object to be enabled, e.g. "XPON.ONU.1"
 *
 * - cntr: when enabling an XPON.ONU instance, tr181-xpon waits for a few
 *     seconds until the XPON.ONU instance has at least one EthernetUNI instance
 *     and one ANI instance. Enabling the XPON.ONU instance before tr181-xpon
 *     has discovered its EthernetUNI or ANI instance(s) might cause errors.
 *     E.g. the vendor module might already try to update the Status field of
 *     the EthernetUNI instance. This counter keeps track of how many times
 *     tr181-xpon checked the number of EthernetUNIs and ANIs of the XPON.ONU
 *     instance given by 'path'. If the counter goes above
 *     MAX_CHECKS_ON_NR_OF_UNIS_AND_ANIS, tr181-xpon enables the XPON.UNI
 *     instance even if its number of EthernetUNIs or ANIs is still 0. The
 *     field 'cntr' tracks more or less a number of seconds. By setting the
 *     MAX_CHECKS value to 4, tr181-xpon waits max about 4 to 5 seconds
 *     for both an EthernetUNI and ANI to appear.
 *     If 'path' refers to something else than an XPON.ONU instance, this
 *     counter is don't care.
 *
 * - it: iterator to put it in s_rth_tasks
 */
typedef struct _rth_task {
    amxc_string_t path;
    uint8_t cntr;
    amxc_llist_it_t it;
} rth_task_t;


static void rth_task_clean(rth_task_t* const task) {
    amxc_string_clean(&task->path);
}

/**
 * Create task and append it to 's_rth_tasks'.
 *
 * If it fails to create the task or to append the task to 's_rth_tasks', the
 * function is a no-op.
 *
 * @return task created on success, else NULL
 */
static rth_task_t* rth_task_create(const char* const path) {

    rth_task_t* task = (rth_task_t*) calloc(1, sizeof(rth_task_t));
    when_null_trace(task, exit, ERROR, "Failed to allocate memory");

    amxc_string_init(&task->path, 0);
    amxc_string_set(&task->path, path);

    if(amxc_llist_append(&s_rth_tasks, &task->it)) {
        SAH_TRACEZ_ERROR(ME, "Failed to append task to 's_rth_tasks'");
        SAH_TRACEZ_ERROR(ME, "   path='%s'", path);
        rth_task_clean(task);
        free(task);
        task = NULL;
    }

exit:
    return task;
}

static void rth_task_delete(amxc_llist_it_t* hit) {
    rth_task_t* task = amxc_container_of(hit, rth_task_t, it);
    const char* path = amxc_string_get(&task->path, 0);
    SAH_TRACEZ_DEBUG(ME, "path='%s'", path);
    rth_task_clean(task);
    free(task);
}

/**
 * Handle all tasks in the list s_rth_tasks to restore their Enable=1 setting.
 *
 * For each task in the list, do the following:
 *
 * - If the path in the task does not refer to an XPON.ONU instance, call
 *   pon_ctrl_set_enable(), remove the task from the list and delete it.
 *
 * - If the path in the task refers to an XPON.ONU instance, check its number of
 *   EthernetUNIs and ANIs. If they are both non zero, or if the 'cntr' field of
 *   the task is larger than MAX_CHECKS_ON_NR_OF_UNIS_AND_ANIS, call
 *   pon_ctrl_set_enable(), remove the task from the list and delete it. Else
 *   increment the field 'cntr' and keep the task in the list. See doc of field
 *   'cntr' of 'struct _rth_task' above for more info on this behavior.
 *
 * Restart @a timer with timeout of 1 s if there are still tasks in the list.
 */
static void handle_rth_tasks(amxp_timer_t* timer, UNUSED void* priv) {

    if(amxc_llist_is_empty(&s_rth_tasks)) {
        SAH_TRACEZ_WARNING(ME, "No tasks");
        return;
    }

    rth_task_t* task;
    const char* path;
    object_id_t id;
    bool delete_item;
    uint32_t n_ethernet_unis;
    uint32_t n_anis;
    amxc_llist_for_each(it, &s_rth_tasks) {
        delete_item = true;
        task = amxc_llist_it_get_data(it, rth_task_t, it);
        path = amxc_string_get(&task->path, 0);
        id = dm_get_object_id(path);
        SAH_TRACEZ_DEBUG(ME, "path='%s'", path);
        if(obj_id_onu == id) {
            n_ethernet_unis = dm_get_nr_of_ethernet_uni_instances(path);
            n_anis = dm_get_nr_of_ani_instances(path);
            SAH_TRACEZ_DEBUG(ME, "path='%s' n_ethernet_unis=%d n_anis=%d cntr=%d",
                             path, n_ethernet_unis, n_anis, task->cntr);
            if((n_ethernet_unis != 0) && (n_anis != 0)) {
                pon_ctrl_set_enable(path, /*enable=*/ true);
            } else if(task->cntr > MAX_CHECKS_ON_NR_OF_UNIS_AND_ANIS) {
                SAH_TRACEZ_WARNING(ME, "Enable %s despite n_ethernet_unis=%d n_anis=%d",
                                   path, n_ethernet_unis, n_anis);
                pon_ctrl_set_enable(path, /*enable=*/ true);
            } else {
                ++task->cntr;
                delete_item = false;
            }
        } else {
            pon_ctrl_set_enable(path, /*enable=*/ true);
        }
        if(delete_item) {
            amxc_llist_it_take(it);
            rth_task_clean(task);
            free(task);
        }
    }
    if(!amxc_llist_is_empty(&s_rth_tasks)) {
        amxp_timer_start(timer, /*timeout_msec=*/ 1000);
    }
}


void rth_init(void) {
    amxc_llist_init(&s_rth_tasks);

    if(amxp_timer_new(&s_timer_handle_rth_tasks, handle_rth_tasks, NULL)) {
        SAH_TRACEZ_ERROR(ME, "Failed to create timer to handle tasks");
    }
}

void rth_cleanup(void) {
    if(!amxc_llist_is_empty(&s_rth_tasks)) {
        /* Normally s_rth_tasks should be empty upon stopping */
        SAH_TRACEZ_WARNING(ME, "size(s_rth_tasks)=%zd != 0",
                           amxc_llist_size(&s_rth_tasks));
        amxc_llist_clean(&s_rth_tasks, rth_task_delete);
    }
    amxp_timer_delete(&s_timer_handle_rth_tasks);
}

/**
 * Schedule task to enable an object in the HAL.
 *
 * @param[in] object  object path, e.g. "XPON.ONU.1"
 *
 * The function creates a task to enable the object in the HAL, appends it to
 * the list of tasks managed by this restore_to_hal part, and starts the timer
 * to handle all tasks in that list.
 *
 * If there is already a task in the list to enable the object, the function
 * immediately returns.
 */
void rth_schedule_enable(const char* const object) {

    SAH_TRACEZ_DEBUG(ME, "object='%s'", object);

    when_null_trace(s_timer_handle_rth_tasks, exit, ERROR, "No timer");

    const rth_task_t* task;
    const char* path;
    amxc_llist_iterate(it, &s_rth_tasks) {
        task = amxc_llist_it_get_data(it, rth_task_t, it);
        path = amxc_string_get(&task->path, 0);
        if(strncmp(path, object, strlen(object)) == 0) {
            SAH_TRACEZ_DEBUG(ME, "%s is already scheduled to be enabled", object);
            return;
        }
    }

    if(rth_task_create(object) != NULL) {
        amxp_timer_start(s_timer_handle_rth_tasks, SHORT_TIMEOUT_MS);
    }

exit:
    return;
}

/**
 * Remove task to enable an object in the HAL if such task exists.
 *
 * @param[in] object object path, e.g. "XPON.ONU.1"
 */
void rth_disable(const char* const object) {
    if(amxc_llist_is_empty(&s_rth_tasks)) {
        return;
    }
    rth_task_t* task;
    const char* path;
    amxc_llist_for_each(it, &s_rth_tasks) {
        task = amxc_llist_it_get_data(it, rth_task_t, it);
        path = amxc_string_get(&task->path, 0);
        if(strcmp(object, path) == 0) {
            amxc_llist_it_take(it);
            rth_task_delete(it);
            break; /* out of iteration over s_rth_tasks */
        }
    }
}

