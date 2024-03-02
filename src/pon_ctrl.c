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
#include "pon_ctrl.h"

/* Other libraries' headers */
#include <amxc/amxc_macros.h>
#include <amxc/amxc.h>
#include <amxm/amxm.h>

/* Own headers */
#include "module_mgmt.h" /* mod_get_vendor_module_loaded() */
#include "xpon_trace.h"

static const char* const MOD_PON_CTRL = "pon_ctrl";

static const char* const SET_MAX_NR_OF_ONUS = "set_max_nr_of_onus";
static const char* const SET_ENABLE = "set_enable";
static const char* const GET_LIST_OF_INSTANCES = "get_list_of_instances";
static const char* const GET_OBJECT_CONTENT = "get_object_content";
static const char* const GET_PARAM_VALUES = "get_param_values";
static const char* const HANDLE_FILE_DESCRIPTOR = "handle_file_descriptor";
static const char* const SET_PASSWORD = "set_password";

static int call_pon_ctrl_function_common(const char* const func_name,
                                         amxc_var_t* args,
                                         amxc_var_t* ret) {

    int rc = -1;
    amxc_var_t* ret_dummy = NULL;

    when_null_trace(args, exit, ERROR, "args is NULL");

    const char* const so_name = mod_get_vendor_module_loaded(); /* shared_object_name */
    when_null_trace(so_name, exit, ERROR, "No vendor module loaded");

    if(NULL == ret) {
        if(amxc_var_new(&ret_dummy)) {
            SAH_TRACEZ_ERROR(ME, "Failed to allocate memory");
            goto exit;
        }
    }

    rc = amxm_execute_function(so_name, MOD_PON_CTRL, func_name, args,
                               ret ? ret : ret_dummy);
    if(rc) {
        SAH_TRACEZ_ERROR(ME, "%s.%s.%s() failed: rc=%d", so_name, MOD_PON_CTRL,
                         func_name, rc);
    }

exit:
    if(ret_dummy != NULL) {
        amxc_var_delete(&ret_dummy);
    }
    return rc;
}


/**
 * Forward value of MAX_NR_OF_ONUS to the vendor module.
 */
static void set_max_nr_of_onus(void) {
    amxc_var_t args;
    amxc_var_init(&args);
    amxc_var_set(uint32_t, &args, MAX_NR_OF_ONUS);

    if(call_pon_ctrl_function_common(SET_MAX_NR_OF_ONUS, &args, NULL)) {
        SAH_TRACEZ_ERROR(ME, "Failed to set max nr of ONUs");
    }

    amxc_var_clean(&args);
}

/**
 * Initialize the pon_ctrl part of this plugin.
 *
 * The function forwards value of MAX_NR_OF_ONUS to the vendor module.
 *
 * The plugin must call this function once at startup, after loading the vendor
 * module.
 */
void pon_ctrl_init(void) {
    set_max_nr_of_onus();
}

/**
 * Let vendor module know that a read-write Enable field was changed.
 *
 * @param[in] path     path of object whose Enable field was changed, e.g.,
 *                     "XPON.ONU.1", or "XPON.ONU.1.ANI.1"
 * @param[in] enable   true if Enable field was set to true, else false
 */
void pon_ctrl_set_enable(const char* const path, bool enable) {

    amxc_var_t args;
    amxc_var_init(&args);

    SAH_TRACEZ_DEBUG(ME, "path='%s' enable=%d", path, enable);

    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);

    amxc_var_add_key(cstring_t, &args, "path", path);
    amxc_var_add_key(bool, &args, "enable", enable);

    if(call_pon_ctrl_function_common(SET_ENABLE, &args, NULL)) {
        SAH_TRACEZ_ERROR(ME, "path='%s' enable=%d: %s() failed",
                         path, enable, SET_ENABLE);
    }

    amxc_var_clean(&args);
}

/**
 * Ask vendor module which instances exist for a template object.
 *
 * @param[in] path     path to template object, e.g. "XPON.ONU" or
 *                     "XPON.ONU.1.SoftwareImage"
 * @param[in,out] ret  function returns result via this parameter. See below for
 *                     more info.
 *
 * The param @a ret should be an htable with following keys upon success:
 * - 'indexes': a string with comma-separated integers representing the instance
 *              indexes. It's an empty string if no instances exist.
 *
 * @return 0 on success, -1 upon error
 */
int pon_ctrl_get_list_of_instances(const char* const path, amxc_var_t* ret) {

    amxc_var_t args;
    amxc_var_init(&args);
    amxc_var_set(cstring_t, &args, path);

    const int rc =
        call_pon_ctrl_function_common(GET_LIST_OF_INSTANCES, &args, ret);

    amxc_var_clean(&args);
    return rc;
}

/**
 * Ask vendor module for the content (parameter values) of an object.
 *
 * @param[in] path     object path. This can be a singleton or template object.
 * @param[in] index    instance index when querying the content of an instance.
 *                     Must be 0 when querying a singleton.
 * @param[in,out] ret  function returns result via this parameter. See below for
 *                     more info.
 *
 * The param @a ret should be an htable with following keys upon success:
 * - 'parameters': values for the parameters of the object
 * - 'keys': only present in @a ret when querying an instance. It should
 *           contain values for the keys of the template object.
 *
 * @return 0 on success, -1 upon error
 */
int pon_ctrl_get_object_content(const char* const path, uint32_t index, amxc_var_t* ret) {

    amxc_var_t args;
    amxc_var_init(&args);

    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "path", path);
    if(index) {
        amxc_var_add_key(uint32_t, &args, "index", index);
    }

    const int rc = call_pon_ctrl_function_common(GET_OBJECT_CONTENT, &args, ret);

    amxc_var_clean(&args);
    return rc;
}

/**
 * Ask vendor module for the values of one or more parameters of an object.
 *
 * @param[in] path     object path of a singleton or an instance
 * @param[in] names    parameter names formatted as a comma-separated list. The
 *                     vendor module might support passing max 1 parameter name.
 *                     Example: "RxPower".
 * @param[in,out] ret  function returns result via this parameter. See below for
 *                     more info.
 *
 * The param @a ret should be an htable with following keys upon success:
 * - 'parameters': values for the requested parameter(s) of the object
 *
 * @return 0 on success, -1 upon error
 */
int pon_ctrl_get_param_values(const char* const path,
                              const char* const names,
                              amxc_var_t* ret) {

    amxc_var_t args;
    amxc_var_init(&args);

    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "path", path);
    amxc_var_add_key(cstring_t, &args, "names", names);

    SAH_TRACEZ_DEBUG(ME, "path='%s' names='%s'", path, names);
    const int rc = call_pon_ctrl_function_common(GET_PARAM_VALUES, &args, ret);

    amxc_var_clean(&args);
    return rc;
}

/**
 * Ask vendor module to handle (a message arriving on) a file descriptor.
 *
 * @param[in] fd  the file descriptor to handle
 *
 * Examples for the file descriptor:
 * - file descriptor of the socket the vendor module uses for IPC with the
 *   vendor specific PON/OMCI stack.
 * - file descriptor of a socket to monitor netlink messages from the kernel.
 * - ...
 */
void pon_ctrl_handle_file_descriptor(int fd) {

    amxc_var_t var;
    amxc_var_init(&var);
    amxc_var_set(fd_t, &var, fd);
    call_pon_ctrl_function_common(HANDLE_FILE_DESCRIPTOR, &var, NULL);
    amxc_var_clean(&var);
}

/**
 * Ask vendor module to apply the password.
 *
 * @param[in] ani_path: path to ANI instance, e.g. "XPON.ONU.1.ANI.1"
 * @param[in] password: new value for the parameter Password of the ANI
 *                      referred to by @a ani_path
 * @param[in] hex: false if @a password is in ASCII format; true if @a password
 *                 is in hexadecimal format
 */
void pon_ctrl_set_password(const char* const ani_path,
                           const char* const password,
                           bool hex) {
    amxc_var_t args;
    amxc_var_init(&args);

    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &args, "ani_path", ani_path);
    amxc_var_add_key(cstring_t, &args, "password", password);
    amxc_var_add_key(bool, &args, "is_hexadecimal_password", hex);

    SAH_TRACEZ_DEBUG(ME, "ani_path='%s' password='%s' hex=%d", ani_path, password, hex);
    call_pon_ctrl_function_common(SET_PASSWORD, &args, NULL);
    amxc_var_clean(&args);
}

