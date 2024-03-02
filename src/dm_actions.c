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
#include "dm_actions.h"

/* System headers */
#include <stdbool.h>
#ifdef _DEBUG_
#include <stdio.h>
#endif
#include <stdlib.h> /* free() */
#include <string.h> /* strlen() */

/* Other libraries' headers */
#include <amxc/amxc_macros.h>
#include <amxc/amxc.h>

#include <amxp/amxp.h>        /* required by amxd_action.h */
#include <amxd/amxd_action.h> /* amxd_action_param_read() */
#include <amxd/amxd_object.h>

/* Own headers */
#include "ani.h"              /* ani_strip_tc_authentication() */
#include "object_intf_priv.h" /* object_intf_priv_t */
#include "onu_priv.h"         /* onu_priv_t */
#include "password.h"         /* passwd_check_password() */
#include "pon_ctrl.h"         /* pon_ctrl_get_param_values() */
#include "utils_time.h"       /* time_get_system_uptime() */
#include "xpon_trace.h"

/**
 * Do not ask vendor module for param value if this setting is true.
 *
 * When deleting a Transceiver instance, the custom read handler
 * _read_trx_param() is called many times for the parameters RxPower, TxPower,
 * etc. This seems not ok. It does not make sense to query the parameters
 * of an instance being deleted. See:
 *
 * ST-826 - [amx] custom param read handler called more often than expected
 *
 * If the vendor module calls dm_remove_instance() to delete a Transceiver
 * instance, the plugin querying the parameter values in the vendor module when
 * deleting the instance causes errors: the vendor module already considers the
 * Transceiver instance deleted.
 *
 * To avoid those errors, the plugin sets s_ignore_param_reads to true just
 * before deleting an instance, and sets it back to false immediately
 * afterwards. The plugin still calls the custom read handler while deleting a
 * Transceiver instance, but the handler will not query the value in the vendor
 * module.
 *
 * The plugin sets s_ignore_param_reads to true for each instance it deletes,
 * not only for Transceiver instances. This makes the code more future proof. We
 * might reuse _read_trx_param() in the future for the volatile parameters of
 * other template objects.
 */
static bool s_ignore_param_reads = false;

/**
 * Set 's_ignore_param_reads' to @a ignore.
 *
 * See doc in front of s_ignore_param_reads.
 */
void dm_actions_set_ignore_param_reads(bool ignore) {
    s_ignore_param_reads = ignore;
}

static bool process_param_value(const char* const param_name,
                                const amxc_var_t* const ret,
                                amxc_var_t* const retval) {
    bool rv = false;

    const amxc_var_t* const params =
        amxc_var_get_key(ret, "parameters", AMXC_VAR_FLAG_DEFAULT);
    when_null_trace(params, exit, ERROR, "Failed to extract 'parameters'");

    const amxc_var_t* const param_value =
        amxc_var_get_key(params, param_name, AMXC_VAR_FLAG_DEFAULT);
    when_null_trace(param_value, exit, ERROR, "Failed to get value for '%s'", param_name);

    const int rc = amxc_var_copy(retval, param_value);
    when_failed_trace(rc, exit, ERROR, "Failed to copy value for '%s'", param_name);

    rv = true;

exit:
    return rv;
}


/**
 * Read a parameter value.
 *
 * The Ambiorix framework calls this function every time a parameter is read
 * which has this function as custom read handler.
 *
 * The function has 'trx' in its name because for now it's only used to query
 * the values of the volatile params of an XPON.ONU.{i}.ANI.1.Transceiver.{i}
 * instance. But the function is generic in the sense that it can be used for
 * other parameters (of other objects) in the future.
 */
amxd_status_t _read_trx_param(amxd_object_t* object,
                              amxd_param_t* param,
                              amxd_action_t reason,
                              const amxc_var_t* const args,
                              amxc_var_t* const retval,
                              void* priv) {

    amxd_status_t rv = amxd_status_unknown_error;

    SAH_TRACEZ_DEBUG2(ME, "called");

    when_null_status(object, exit, rv = amxd_status_invalid_function_argument);
    when_null_status(param, exit, rv = amxd_status_invalid_function_argument);
    when_null_status(retval, exit, rv = amxd_status_invalid_function_argument);

    rv = amxd_action_param_read(object, param, reason, args, retval, priv);

    when_failed_trace(rv, exit, ERROR, "amxd_action_param_read() failed");

    when_true(s_ignore_param_reads, exit);

    /**
     * Go to 'exit' if function is called for a template object.
     *
     * It does not make sense to read the param values of a template object.
     * Only read them for a singleton or an instance.
     *
     * See also:
     * ST-826 - [amx] custom param read handler called more often than expected
     */
    const amxd_object_type_t type = amxd_object_get_type(object);
    when_true(amxd_object_template == type, exit);

    const char* const param_name = amxd_param_get_name(param);
    when_null_trace(param_name, exit, ERROR, "param_name is NULL");

    char* path = amxd_object_get_path(object, AMXD_OBJECT_INDEXED);
    when_null_trace(path, exit, ERROR, "path is NULL");

    SAH_TRACEZ_DEBUG(ME, "path=%s param=%s", path, param_name);

    amxc_var_t ret;
    amxc_var_init(&ret);

    if(pon_ctrl_get_param_values(path, param_name, &ret) != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to query %s.%s", path, param_name);
        goto exit_cleanup;
    }

#ifdef _DEBUG_
    printf("tr181-xpon: read_trx_param():\n");
    amxc_var_dump(&ret, STDOUT_FILENO);
#endif

    /* Extract the parameter value from 'ret' and assign it to 'retval'. */
    if(!process_param_value(param_name, &ret, retval)) {
        goto exit_cleanup;
    }

    rv = 0;

exit_cleanup:
    free(path);
    amxc_var_clean(&ret);

exit:
    return rv;
}

/**
 * Called if a LastChange parameter is read.
 *
 * @param[in] object: object whose LastChange parameter is read
 * @param[in] action: this must be 'action_param_read'
 * @param[in,out] retval: the function returns the value for the LastChange
 *                        parameter via this 'retval' parameter
 *
 * @return amxd_status_ok (=0) upon success, another value in case of an error
 */
amxd_status_t _lastchange_on_read(amxd_object_t* const object,
                                  UNUSED amxd_param_t* const param,
                                  amxd_action_t reason,
                                  UNUSED const amxc_var_t* const args,
                                  amxc_var_t* const retval,
                                  UNUSED void* priv_unused) {
    amxd_status_t rv = amxd_status_unknown_error;
    object_intf_priv_t* priv = NULL;
    uint32_t since_change = 0;
    uint32_t uptime = 0;
    char* path = NULL;

    if(reason != action_param_read) {
        SAH_TRACEZ_WARNING(ME, "wrong reason, expected action_param_read(%d) got %d",
                           action_param_read, reason);
        rv = amxd_status_invalid_action;
        goto exit;
    }

    when_null_trace(object, skip, ERROR, "object can not be NULL");
    path = amxd_object_get_path(object, AMXD_OBJECT_INDEXED);
    /**
     * This function is called when the object with the LastChange parameter is
     * is being created. Then 'object->priv' is still NULL. Therefore do not log
     * an error if 'object->priv' is NULL.
     */
    when_null_trace(object->priv, skip, DEBUG, "object %s has no private data", path);
    priv = (object_intf_priv_t*) object->priv;
    uptime = time_get_system_uptime();
    when_true_trace(uptime < priv->last_change, skip, ERROR,
                    "uptime is smaller than LastChange");
    since_change = uptime - priv->last_change;
    rv = amxd_status_ok;

skip:
    when_failed_trace(amxc_var_set_uint32_t(retval, since_change),
                      exit, ERROR, "Failed to set parameter LastChange");
exit:
    if(path) {
        free(path);
    }
    return rv;
}

typedef enum _private_data_type {
    private_data_for_onu = 0,
    private_data_for_object_interface
} private_data_type_t;


/**
 * Helper function called when certain instances are deleted.
 *
 * @param[in] object: the object/instance being destroyed
 * @param[in] reason: this must be 'action_object_destroy'
 * @param[in] private_data_type: type of the private data to be destroyed
 *
 * The function deletes the private data attached to the instance.
 *
 * @return amxd_status_ok (=0) upon success, another value in case of an error
 */
amxd_status_t object_destroyed_common(amxd_object_t* object,
                                      amxd_action_t reason,
                                      private_data_type_t private_data_type) {

    amxd_status_t rv = amxd_status_unknown_error;
    char* path = NULL;

    if(reason != action_object_destroy) {
        SAH_TRACEZ_WARNING(ME, "Wrong reason, expected action_object_destroy(%d) got %d",
                           action_object_destroy, reason);
        rv = amxd_status_invalid_action;
        goto exit;
    }

    when_null_trace(object, exit, ERROR, "object is NULL");
    path = amxd_object_get_path(object, AMXD_OBJECT_INDEXED);
    when_null_trace(object->priv, exit, WARNING,
                    "object %s has no private data", path);
    SAH_TRACEZ_DEBUG(ME, "Delete private data of %s", path);
    switch(private_data_type) {
    case private_data_for_onu:
        onu_priv_delete_private_data((onu_priv_t*) object->priv);
        break;
    case private_data_for_object_interface:
        oipriv_delete_private_data((object_intf_priv_t*) object->priv);
        break;
    default:
        break;
    }
    object->priv = NULL;
    rv = amxd_status_ok;

exit:
    if(path) {
        free(path);
    }
    return rv;
}

/**
 * Called if an ONU instance is destroyed.
 *
 * @param[in] object: the object/instance being destroyed
 * @param[in] reason: this must be 'action_object_destroy'
 *
 * The function deletes the private data attached to the instance.
 *
 * @return amxd_status_ok (=0) upon success, another value in case of an error
 */
amxd_status_t _onu_destroyed(amxd_object_t* object,
                             UNUSED amxd_param_t* param,
                             amxd_action_t reason,
                             UNUSED const amxc_var_t* const args,
                             UNUSED amxc_var_t* const retval,
                             UNUSED void* priv_unused) {

    return object_destroyed_common(object, reason, private_data_for_onu);
}


/**
 * Called if an instance of an object interface is destroyed.
 *
 * @param[in] object: the object/instance being destroyed
 * @param[in] reason: this must be 'action_object_destroy'
 *
 * The function deletes the private data attached to the instance.
 *
 * @return amxd_status_ok (=0) upon success, another value in case of an error
 */
amxd_status_t _interface_object_destroyed(amxd_object_t* object,
                                          UNUSED amxd_param_t* param,
                                          amxd_action_t reason,
                                          UNUSED const amxc_var_t* const args,
                                          UNUSED amxc_var_t* const retval,
                                          UNUSED void* priv_unused) {

    return object_destroyed_common(object, reason,
                                   private_data_for_object_interface);
}


/**
 * Called if Password parameter of an ANI instance is changed.
 *
 * @param[in] object: the TC.Authentication object of an ANI instance whose
 *                    Password parameter is changed
 * @param[in] reason: this must be 'action_param_validate'
 * @param[in] args: variant with cstring_t value for the Password
 *
 * @return amxd_status_ok (=0) upon success, another value in case of an error
 */
amxd_status_t _check_password(amxd_object_t* object,
                              UNUSED amxd_param_t* param,
                              amxd_action_t reason,
                              const amxc_var_t* const args,
                              UNUSED amxc_var_t* const retval,
                              UNUSED void* priv) {
    amxd_status_t status = amxd_status_unknown_error;
    const char* password = NULL;
    char* path = NULL;
    amxc_string_t ani_path;
    amxc_string_init(&ani_path, 0);

    when_null_status(object, exit, status = amxd_status_invalid_function_argument);
    when_null_status(args, exit, status = amxd_status_invalid_function_argument);

    when_true_status(reason != action_param_validate, exit,
                     status = amxd_status_function_not_implemented);

    password = amxc_var_constcast(cstring_t, args);
    when_null_trace(password, exit, ERROR, "password is NULL");

    if(strlen(password) != 0) {

        path = amxd_object_get_path(object, AMXD_OBJECT_INDEXED);
        when_null_trace(path, exit, ERROR, "path is NULL");
        if(!ani_strip_tc_authentication(path, &ani_path)) {
            SAH_TRACEZ_ERROR(ME, "%s: failed to check password", path);
            goto exit;
        }

        SAH_TRACEZ_DEBUG(ME, "path='%s' password='%s'", path, password);

        const char* const ani_path_cstr = amxc_string_get(&ani_path, 0);
        if(!passwd_check_password(ani_path_cstr, password)) {
            status = amxd_status_invalid_value;
            goto exit;
        }
    }

    status = amxd_status_ok;

exit:
    if(path) {
        free(path);
    }
    amxc_string_clean(&ani_path);
    return status;
}

