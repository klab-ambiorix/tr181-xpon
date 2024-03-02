/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2023 SoftAtHome
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
 * Define _GNU_SOURCE to avoid following error:
 * implicit declaration of function ‘strdup’
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* Related header */
#include "upgrade_persistency.h"

/* System headers */
#include <errno.h>
#include <stdio.h>              /* snprintf() */
#include <stdlib.h>             /* free() */
#include <string.h>
#include <sys/stat.h>           /* mkdir() */
#include <sys/types.h>          /* mkdir() */
#include <unistd.h>             /* access() */

/* Other libraries' headers */
#include <amxc/amxc_macros.h>   /* when_null() */
#include <amxc/amxc_string.h>   /* amxc_string */

/* Own headers */
#include "file_utils.h"         /* write_file() */
#include "password_constants.h" /* MAX_PASSWORD_LEN_PLUS_ONE */
#include "persistency.h"        /* persistency_get_folder() */
#include "xpon_trace.h"

/**
 * Folder for upgrade persistent settings.
 *
 * TODO This folder is hardcoded for now. It must be made configurable.
 */
#define UPGR_STORAGE_DIR_OVERALL    "/cfg/system/"
#define UPGR_STORAGE_DIR_COMPONENT  UPGR_STORAGE_DIR_OVERALL "tr181-xpon/"

/* Path to storage dir for upgrade persistent files of this plugin */
static char* s_upgr_storage_dir = NULL;

/**
 * Initialize the part responsible for the upgrade persistent settings.
 *
 * The plugin must call this function once at startup after it has called
 * persistency_init(): if this function can not use the folder for upgrade
 * persistent files, it uses the folder for reboot persistent files created by
 * persistency_init().
 */
void upgr_persistency_init(void) {

    if(access(UPGR_STORAGE_DIR_OVERALL, R_OK | W_OK | X_OK) == 0) {
        if(access(UPGR_STORAGE_DIR_COMPONENT, R_OK | W_OK | X_OK) == 0) {
            s_upgr_storage_dir = strdup(UPGR_STORAGE_DIR_COMPONENT);
            when_null_trace(s_upgr_storage_dir, exit, ERROR,
                            "Failed to allocate mem for s_upgr_storage_dir");
            goto exit;
        }
        if(mkdir(UPGR_STORAGE_DIR_COMPONENT, 0777) == -1) {
            SAH_TRACEZ_ERROR(ME, "Failed to create %s: %s", UPGR_STORAGE_DIR_COMPONENT,
                             strerror(errno));
            goto exit;
        }
        s_upgr_storage_dir = strdup(UPGR_STORAGE_DIR_COMPONENT);
        when_null_trace(s_upgr_storage_dir, exit, ERROR,
                        "Failed to allocate mem for s_upgr_storage_dir");
    } else {
        SAH_TRACEZ_WARNING(ME, "No folder for upgrade persistent settings");
        SAH_TRACEZ_WARNING(ME, "Use folder for reboot persistent settings");
        const char* const reboot_persistent = persistency_get_folder();
        if(access(reboot_persistent, R_OK | W_OK | X_OK) != 0) {
            SAH_TRACEZ_ERROR(ME, "No folder for any persistent settings");
            goto exit;
        }
        s_upgr_storage_dir = strdup(reboot_persistent);
        when_null_trace(s_upgr_storage_dir, exit, ERROR,
                        "Failed to allocate mem for s_upgr_storage_dir");
    }

exit:
    SAH_TRACEZ_DEBUG(ME, "dir='%s'", s_upgr_storage_dir ? : "NULL");
    return;
}

/**
 * Clean up the part responsible for the upgrade persistent settings.
 *
 * The plugin must call this function once when stopping.
 */
void upgr_persistency_cleanup(void) {
    if(s_upgr_storage_dir) {
        free(s_upgr_storage_dir);
        s_upgr_storage_dir = NULL;
    }
}

static void create_password_file_path(const char* const ani_object,
                                      bool hex,
                                      amxc_string_t* const file_path) {

    amxc_string_set(file_path, s_upgr_storage_dir);
    amxc_string_appendf(file_path, "%s", ani_object);
    if(hex) {
        amxc_string_append(file_path, "_password_hex.txt", 17);
    } else {
        amxc_string_append(file_path, "_password_ascii.txt", 19);
    }
}

/**
 * Save the password of a certain ANI instance.
 *
 * @param[in] ani_path: path to ANI instance, e.g. "XPON.ONU.1.ANI.1"
 * @param[in] password: new value for the parameter Password of the ANI
 *                      referred to by @a ani_path
 * @param[in] hex: if true @a password is in hexadecimal format; if false,
 *                 @a password is in ASCII format. The function only uses
 *                 this parameter if @a password is not empty.
 *
 * If @a password is empty, the function removes any files storing the password
 * for the ANI instance.
 */
void upgr_persistency_set_password(const char* const ani_path,
                                   const char* const password,
                                   bool hex) {

    when_null_trace(s_upgr_storage_dir, exit, DEBUG, "No persistency");
    when_null(ani_path, exit);
    when_null(password, exit);

    SAH_TRACEZ_DEBUG(ME, "ani_path='%s' password='%s' hex=%d",
                     ani_path, password, hex);

    amxc_string_t file_path_ascii;
    amxc_string_t file_path_hex;
    amxc_string_init(&file_path_ascii, 0);
    amxc_string_init(&file_path_hex, 0);

    create_password_file_path(ani_path, /*hex=*/ false, &file_path_ascii);
    create_password_file_path(ani_path, /*hex=*/ true, &file_path_hex);

    const char* const ascii_cstr = amxc_string_get(&file_path_ascii, 0);
    const char* const hex_cstr = amxc_string_get(&file_path_hex, 0);

    if(strlen(password) == 0) {
        unlink(ascii_cstr);
        unlink(hex_cstr);
    } else {
        write_file(hex ? hex_cstr : ascii_cstr, password);
        unlink(hex ? ascii_cstr : hex_cstr);
    }

    amxc_string_clean(&file_path_ascii);
    amxc_string_clean(&file_path_hex);

exit:
    return;
}

#define ASCII_PASSWORD_FILE_INDEX 0
#define HEX_PASSWORD_FILE_INDEX 1

/**
 * Get the saved password for a certain ANI instance.
 *
 * @param[in] ani_path: path to ANI instance, e.g. "XPON.ONU.1.ANI.1"
 * @param[in,out] password: the function writes the saved password to this
 *     parameter if a saved password for the ANI exists. If no saved password
 *     exists, the function does not write to this parameter. The caller must
 *     pass a buffer of size MAX_PASSWORD_LEN_PLUS_ONE to ensure this function
 *     has enough space to write a password.
 * @param[in,out] hex: the function sets this parameter to true if the password
 *     it writes to @a password is in hexadecimal format.
 *
 * If no saved password exists for the ANI, the function does not update
 * @a password or @a hex, and returns true.
 *
 * @return true on success, else false
 */
bool upgr_persistency_get_password(const char* const ani_path,
                                   char* const password,
                                   bool* hex) {

    bool rv = false;
    int i;
    char password_saved[MAX_PASSWORD_LEN_PLUS_ONE] = { 0 };
    const char* password_file_used = NULL;

    amxc_string_t file_path_ascii;
    amxc_string_t file_path_hex;
    amxc_string_init(&file_path_ascii, 0);
    amxc_string_init(&file_path_hex, 0);

    create_password_file_path(ani_path, /*hex=*/ false, &file_path_ascii);
    create_password_file_path(ani_path, /*hex=*/ true, &file_path_hex);
    const char* files[2] = { 0 };
    files[ASCII_PASSWORD_FILE_INDEX] = amxc_string_get(&file_path_ascii, 0);
    files[HEX_PASSWORD_FILE_INDEX] = amxc_string_get(&file_path_hex, 0);

    for(i = 0; i < 2; ++i) {
        if(access(files[i], F_OK) == 0) {
            if(!read_first_line_from_file(files[i], password_saved,
                                          MAX_PASSWORD_LEN_PLUS_ONE)) {
                SAH_TRACEZ_ERROR(ME, "Failed get password from %s", files[i]);
                goto exit;
            }
            password_file_used = files[i];
            if(HEX_PASSWORD_FILE_INDEX == i) {
                *hex = true;
            }
            break;
        }
    }
    if(password_file_used == NULL) {
        SAH_TRACEZ_DEBUG(ME, "%s: no password", ani_path);
        rv = true;
        goto exit;
    }


    if(strlen(password_saved) == 0) {
        SAH_TRACEZ_ERROR(ME, "Password read from %s is empty",
                         password_file_used);
        goto exit;
    }

    if(snprintf(password, MAX_PASSWORD_LEN_PLUS_ONE, "%s",
                password_saved) < 0) {
        SAH_TRACEZ_ERROR(ME, "snprintf() to copy password failed");
        goto exit;
    }
    SAH_TRACEZ_DEBUG(ME, "ani='%s': password='%s' hex=%d", ani_path,
                     password, *hex);
    rv = true;

exit:
    amxc_string_clean(&file_path_ascii);
    amxc_string_clean(&file_path_hex);
    return rv;
}

