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
 * @file dm_events.c
 *
 * Handles functions called by the Ambiorix framework upon certain events
 * triggered by changes to the XPON DM.
 *
 * The file odl/tr181-xpon_definition.odl specifies which functions the Ambiorix
 * framework should call upon certain events. E.g., the file specifies that
 * Ambiorix should call the function
 * - onu_enable_changed() or
 * - ani_enable_changed()
 * if the Enable parameter of an XPON.ONU or XPON.ONU.x.ANI instance is changed
 * respectively.
 */

/* System headers */
#include <amxc/amxc.h>        /* amxc_var_t */
#include <amxc/amxc_macros.h> /* UNUSED */

/* Own headers */
#include "ani.h"              /* ani_strip_tc_authentication() */
#include "object_intf_priv.h" /* oipriv_attach_private_data() */
#include "password.h"         /* passwd_set_password() */
#include "persistency.h"      /* persistency_enable() */
#include "pon_ctrl.h"         /* pon_ctrl_set_enable() */
#include "restore_to_hal.h"   /* rth_disable() */
#include "xpon_trace.h"

static int isdot(int c) {
    return (c == '.') ? 1 : 0;
}

/**
 * The Enable param of an XPON.ONU or XPON.ONU.x.ANI instance changed value.
 *
 * @param[in] event_data  has path and new value of the Enable param which
 *                        changed value.
 * @param[in] onu If true, Enable of an ONU instance changed, else Enable of
 *                an ANI instance changed
 *
 * The function forwards the value to:
 * - the vendor module
 * - the persistency part of this plugin
 */
static void onu_or_ani_enable_changed(const amxc_var_t* const event_data, bool onu) {

    const char* const path = GETP_CHAR(event_data, "path");
    const bool enable = GETP_BOOL(event_data, "parameters.Enable.to");

    when_null_trace(path, exit, ERROR, "path is NULL");

    SAH_TRACEZ_INFO(ME, "path='%s' enable=%d", path, enable);

    amxc_string_t path_no_dot; /* path with dot at the end trimmed */
    amxc_string_init(&path_no_dot, 0);
    amxc_string_set(&path_no_dot, path);
    amxc_string_trimr(&path_no_dot, isdot);

    const char* const path_no_dot_cstr = amxc_string_get(&path_no_dot, 0);

    /**
     * When enabling an ONU, avoid enabling the ONU while tr181-xpon is still
     * busy querying whether the ONU has EthernetUNI and ANI instances. Schedule
     * a task to enable the ONU in the near future.
     */
    if(enable && onu) {
        rth_schedule_enable(path_no_dot_cstr);
    } else {
        pon_ctrl_set_enable(path_no_dot_cstr, enable);
    }
    persistency_enable(path_no_dot_cstr, enable);

    /**
     * restore_to_hal part maybe still has task to enable the object towards
     * the hal. Remove that task if this function asks to disable the object.
     */
    if(!enable) {
        rth_disable(path_no_dot_cstr);
    }

    amxc_string_clean(&path_no_dot);

exit:
    return;
}


/**
 * The Enable param of an XPON.ONU instance changed value.
 *
 * @param[in] event_data  has path and new value of the Enable param which
 *                        changed value.
 */
void _onu_enable_changed(UNUSED const char* const event_name,
                         const amxc_var_t* const event_data,
                         UNUSED void* const priv) {

    onu_or_ani_enable_changed(event_data, /*onu=*/ true);
}

/**
 * The Enable param of an XPON.ONU.x.ANI instance changed value.
 *
 * @param[in] event_data  has path and new value of the Enable param which
 *                        changed value.
 */
void _ani_enable_changed(UNUSED const char* const event_name,
                         const amxc_var_t* const event_data,
                         UNUSED void* const priv) {

    onu_or_ani_enable_changed(event_data, /*onu=*/ false);
}

/**
 * The component added an instance of an interface object.
 *
 * @param[in] event_data  has path and index of instance added
 *
 * The function attaches private data to the instance to support the LastChange
 * parameter.
 */
void _interface_object_added(UNUSED const char* const event_name,
                             const amxc_var_t* const event_data,
                             UNUSED void* const priv) {
    oipriv_attach_private_data(event_data);
}

/**
 * The Status parameter of an interface object changed value.
 *
 * @param[in] event_data has path of object whose Status parameter changed value
 *
 * The function updates the 'last_change' timestamp in the private data attached
 * to the object.
 */
void _status_changed(UNUSED const char* const event_name,
                     const amxc_var_t* const event_data,
                     UNUSED void* const priv) {

    oipriv_update_last_change(event_data);
}

/**
 * The Password parameter of an ANI instance changed value.
 *
 * @param[in] event_data: contains both the path of TC.Authentication object of
 *     the ANI instance whose Password parameter changed value, and the new
 *     value for that Password
 *
 * The function does not need to check if the password is valid:
 * _check_password() already did that.
 *
 * The function (indirectly) forwards the value to:
 * - the vendor module
 * - the part of this plugin responsible for the upgrade persistent settings
 */
void _password_changed(UNUSED const char* const event_name,
                       const amxc_var_t* const event_data,
                       UNUSED void* const priv) {

    amxc_string_t path_no_dot; /* path with dot at the end trimmed */
    amxc_string_t ani_instance;
    amxc_string_init(&path_no_dot, 0);
    amxc_string_init(&ani_instance, 0);

    const char* const path = GETP_CHAR(event_data, "path");
    const char* const password = GETP_CHAR(event_data, "parameters.Password.to");

    when_null_trace(path, exit, ERROR, "path is NULL");
    when_null_trace(password, exit, ERROR, "password is NULL");

    SAH_TRACEZ_DEBUG(ME, "path='%s' password='%s'", path, password);

    amxc_string_set(&path_no_dot, path);
    amxc_string_trimr(&path_no_dot, isdot);

    const char* const path_no_dot_cstr = amxc_string_get(&path_no_dot, 0);

    if(!ani_strip_tc_authentication(path_no_dot_cstr, &ani_instance)) {
        SAH_TRACEZ_ERROR(ME, "%s: failed to check password", path_no_dot_cstr);
        goto exit;
    }

    const char* const ani_instance_cstr = amxc_string_get(&ani_instance, 0);
    passwd_set_password(ani_instance_cstr, password);

exit:
    amxc_string_clean(&path_no_dot);
    amxc_string_clean(&ani_instance);
}

