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

/**
 * @file persistency.c
 *
 * Functionality related to saving and querying the reboot persistent settings.
 * The only read-write parameters in the BBF XPON DM which need to be reboot
 * persistent are:
 * - XPON.ONU.{i}.Enable and
 * - XPON.ONU.{i}.ANI.{i}.Enable
 *
 * The object instances do not exist right after the plugin has loaded the odl
 * files defining the DM, and the _defaults.odl file. The plugin creates them
 * afterwards, while querying the state in the HAL, or when it receives a
 * notification from the HAL to create such an instance. Therefore the plugin
 * does not use the odl mechanism (via a _save.odl file) to save and restore the
 * values of the Enable parameters above.
 *
 * If one of the Enable parameters is set to true, the persistency part creates
 * an empty file in the storage dir of this plugin. E.g., if XPON.ONU.1.Enable
 * is set to true, it creates XPON.ONU.1_enabled.txt. If the plugin after reboot
 * creates the instance XPON.ONU.1, it checks if that file exists to determine
 * if it should set XPON.ONU.1.Enable to true.
 */


/**
 * Define _GNU_SOURCE to avoid following error:
 * implicit declaration of function ‘strdup’
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* Related header */
#include "persistency.h"

/* System headers */
#include <errno.h>
#include <stdio.h>            /* fopen() */
#include <stdlib.h>           /* free() */
#include <string.h>
#include <sys/stat.h>         /* mkdir() */
#include <sys/types.h>        /* mkdir() */
#include <unistd.h>           /* access() */

/* Other libraries' headers */
#include <amxc/amxc_macros.h> /* when_null() */

/* Own headers */
#include "dm_xpon_mngr.h"
#include "xpon_trace.h"

/* Use this storage dir if the one returned by parser does not exist */
static const char* const STORAGE_DIR = "/etc/config/tr181-xpon/";

/* Path to storage dir of this plugin, e.g., "/etc/config/tr181-xpon/". */
static char* s_storage_dir = NULL;

/**
 * Return the storage dir.
 *
 * The returned pointer must be freed.
 *
 * @return the storage dir upon success, else NULL
 */
static char* get_storage_dir(void) {

    char* result = NULL;
    amxo_parser_t* parser = xpon_mngr_get_parser();
    when_null(parser, exit);

    amxc_string_t dir;
    amxc_string_init(&dir, 0);
    const char* storage_dir = GETP_CHAR(&parser->config, "storage-path");
    if(!storage_dir) {
        amxc_string_clean(&dir);
        goto exit;
    }
    amxc_string_setf(&dir, "%s", storage_dir);
    amxc_string_resolve(&dir, &parser->config);
    result = amxc_string_take_buffer(&dir);

exit:
    return result;
}

/**
 * Initialize the persistency part.
 *
 * Ask the parser for the storage dir. If the parser returns NULL, take
 * STORAGE_DIR as storage dir. Create the storage dir if it does not exist.
 * Store the selected dir in s_storage_dir upon success, else set s_storage_dir
 * to NULL.
 *
 * Under normal circumstances the parser returns the storage dir, the dir
 * exists, and this function sets s_storage_dir to that value.
 *
 * The plugin must call this function once at startup.
 */
void persistency_init(void) {

    char* dir = get_storage_dir();
    SAH_TRACEZ_DEBUG(ME, "dir='%s'", dir ? dir : "NULL");

    s_storage_dir = strdup(dir ? dir : STORAGE_DIR);

    when_null_trace(s_storage_dir, exit, ERROR,
                    "Failed to allocate mem for s_storage_dir");

    if(access(s_storage_dir, R_OK | W_OK | X_OK) == 0) {
        goto exit;
    }

    SAH_TRACEZ_DEBUG(ME, "Create %s", s_storage_dir);

    /**
     * mkdir() can not create all folders in a path at once. But we assume
     * that all folders except for the last one in s_storage_dir exist.
     * It's also unlikely we end up here. amxrt normally has already created
     * the folder.
     */
    if(mkdir(s_storage_dir, 0777) == -1) {
        SAH_TRACEZ_ERROR(ME, "Failed to create %s", s_storage_dir);
        free(s_storage_dir);
        s_storage_dir = NULL;
    }

exit:
    free(dir);
}

/**
 * Clean up the persistency part.
 *
 * The plugin must call this function once when stopping.
 */
void persistency_cleanup(void) {
    if(s_storage_dir) {
        free(s_storage_dir);
        s_storage_dir = NULL;
    }
}

/**
 * Get the folder for reboot persistent settings for this plugin.
 *
 * It typically returns "/etc/config/tr181-xpon/".
 */
const char* persistency_get_folder(void) {
    return s_storage_dir;
}

/**
 * Touch file if it does not exist yet.
 *
 * @param[in] file: file path
 */
static void touch_file(const char* const file) {
    /* Avoid writing the file if it already exists */
    if(access(file, F_OK) == 0) {
        SAH_TRACEZ_DEBUG2(ME, "%s already exists", file);
        return;
    }

    FILE* f = fopen(file, "w");
    if(NULL == f) {
        SAH_TRACEZ_ERROR(ME, "Failed to create '%s': %s", file, strerror(errno));
    } else {
        fclose(f);
    }
}

static void create_enabled_file_path(amxc_string_t* const file_path,
                                     const char* const object) {

    amxc_string_set(file_path, s_storage_dir);
    amxc_string_appendf(file_path, "%s", object);
    amxc_string_append(file_path, "_enabled.txt", 12);
}

/**
 * Save whether object is enabled or disabled.
 *
 * @param[in] object  path to object in DM being enabled/disabled, e.g., "XPON.ONU.1"
 * @param[in] enable  true if object is enabled, else false
 */
void persistency_enable(const char* const object, bool enable) {

    when_null_trace(s_storage_dir, exit, DEBUG, "No persistency");
    when_null(object, exit);

    amxc_string_t file_path;
    amxc_string_init(&file_path, 0);
    create_enabled_file_path(&file_path, object);

    const char* const file_path_cstr = amxc_string_get(&file_path, 0);

    SAH_TRACEZ_DEBUG(ME, "object='%s enable=%d", object, enable);

    if(enable) {
        touch_file(file_path_cstr);
    } else {
        unlink(file_path_cstr);
    }

    amxc_string_clean(&file_path);

exit:
    return;
}

/**
 * Return true if object identified by 'object' is enabled.
 *
 * @param[in] object path to object in DM being queried, e.g. "XPON.ONU.1".
 *
 * @return true if object is enabled, else false.
 */
bool persistency_is_enabled(const char* const object) {

    bool rv = false;
    when_null_trace(s_storage_dir, exit, DEBUG, "No persistency");
    when_null(object, exit);

    amxc_string_t file_path;
    amxc_string_init(&file_path, 0);
    create_enabled_file_path(&file_path, object);

    const char* const file_path_cstr = amxc_string_get(&file_path, 0);

    rv = (access(file_path_cstr, F_OK) == 0) ? true : false;

    amxc_string_clean(&file_path);

exit:
    return rv;
}

