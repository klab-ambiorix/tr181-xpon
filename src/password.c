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

/* Related header */
#include "password.h"

/* System headers */
#include <ctype.h>  /* isxdigit() */
#include <string.h> /* strlen() */

/* Other libraries' headers */
#include <amxc/amxc_macros.h>  /* when_true() */
#include <amxc/amxc_string.h>
#include <amxc/amxc_variant.h>

/* Own headers */
#include "ani.h"                 /* ani_append_tc_authentication() */
#include "data_model.h"          /* dm_is_hex_password() */
#include "password_constants.h"  /* MAX_GPON_PASSWORD_LEN */
#include "pon_ctrl.h"            /* pon_ctrl_set_password() */
#include "upgrade_persistency.h" /* upgr_persistency_set_password() */
#include "xpon_trace.h"


/**
 * Return true if the password is valid, else return false.
 *
 * @param[in] ani_path: path to ANI instance, e.g. "XPON.ONU.1.ANI.1"
 * @param[in] password: possible new value for the parameter Password of the
 *                      ANI instance referred to by @a ani_path.
 *
 * The function looks at other parameters of the ANI referred to by @a ani_path
 * to determine if the password is valid. E.g. if @a ani_path is
 * "XPON.ONU.1.ANI.1", it looks at:
 * - XPON.ONU.1.ANI.1.PONMode
 * - XPON.ONU.1.ANI.1.TC.Authentication.HexadecimalPassword
 *
 * @return true if password is valid, else false
 */
bool passwd_check_password(const char* const ani_path, const char* const password) {

    bool rv = false;
    size_t i;

    SAH_TRACEZ_DEBUG(ME, "ani_path='%s' password='%s'", ani_path, password);

    bool is_hex = false;
    if(!dm_is_hex_password(ani_path, &is_hex)) {
        SAH_TRACEZ_ERROR(ME, "Failed to determine if password is in hex");
        goto exit;
    }

    const size_t len = strlen(password);
    if(is_hex) {
        if((len % 2) != 0) {
            SAH_TRACEZ_ERROR(ME, "hex password does not have even number of chars");
            goto exit;
        }
        for(i = 0; i < len; i++) {
            if(!isxdigit(password[i])) {
                SAH_TRACEZ_ERROR(ME, "hex password has non-hexadecimal digit");
                goto exit;
            }
        }
    }

    pon_mode_t pon_mode = pon_mode_unknown;
    if(!dm_get_ani_pon_mode(ani_path, &pon_mode)) {
        SAH_TRACEZ_ERROR(ME, "Failed to get PON mode");
        goto exit;
    }
    when_true_trace(pon_mode_unknown == pon_mode, exit, ERROR,
                    "Unknown PON mode");

    size_t maxlen =
        (pon_mode_gpon == pon_mode) ?
        MAX_GPON_PASSWORD_LEN : MAX_XPON_PASSWORD_LEN;

    if(is_hex) {
        maxlen *= 2; /* hex value is twice the size of an ASCII value */
        if(len != maxlen) {
            SAH_TRACEZ_ERROR(ME, "Length of password = %zd != %zd [mode='%s', hex=1]",
                             len, maxlen, pon_mode_to_string(pon_mode));
            goto exit;
        }
    } else if(len > maxlen) {
        SAH_TRACEZ_ERROR(ME, "Length of password = %zd > %zd [mode='%s', hex=0]",
                         len, maxlen, pon_mode_to_string(pon_mode));
        goto exit;
    }

    rv = true;

exit:
    return rv;
}

/**
 * Set the password of an ANI instance.
 *
 * @param[in] ani_path: path to ANI instance, e.g. "XPON.ONU.1.ANI.1"
 * @param[in] password: new value for the parameter Password of the ANI
 *                      referred to by @a ani_path
 *
 * The function forwards the value to:
 * - the vendor module
 * - the part of this plugin responsible for the upgrade persistent settings
 */
void passwd_set_password(const char* const ani_path,
                         const char* const password) {

    SAH_TRACEZ_DEBUG(ME, "ani_path='%s' password='%s'", ani_path, password);

    bool is_hex = false;
    if(strlen(password) != 0) {
        if(!dm_is_hex_password(ani_path, &is_hex)) {
            SAH_TRACEZ_ERROR(ME, "%s: failed to determine if password is in "
                             "hex", ani_path);
            goto exit;
        }
    }

    upgr_persistency_set_password(ani_path, password, is_hex);
    pon_ctrl_set_password(ani_path, password, is_hex);

exit:
    return;
}

static void update_password_in_dm(const char* const ani_path,
                                  const char* const password, bool hex) {

    amxc_string_t ani_auth_path;
    amxc_var_t args;

    amxc_string_init(&ani_auth_path, 0);
    amxc_var_init(&args);

    ani_append_tc_authentication(ani_path, &ani_auth_path);
    const char* const ani_auth_path_cstr = amxc_string_get(&ani_auth_path, 0);

    const int rc = amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);
    when_false_trace(0 == rc, exit, ERROR, "Failed to set type to htable");

    amxc_var_t* const path_var = amxc_var_add_key(cstring_t, &args, "path", ani_auth_path_cstr);
    when_null_trace(path_var, exit, ERROR, "Failed to add 'path' to 'args'");

    amxc_var_t* const params = amxc_var_add_key(amxc_htable_t, &args, "parameters", NULL);
    when_null_trace(params, exit, ERROR, "Failed to add 'parameters' to 'args'");

    amxc_var_t* const password_var = amxc_var_add_key(cstring_t, params, "Password", password);
    when_null_trace(password_var, exit, ERROR, "Failed to add Password to 'parameters'");

    if(hex) {
        amxc_var_t* const hex_var = amxc_var_add_key(bool, params, "HexadecimalPassword", hex);
        when_null_trace(hex_var, exit, ERROR, "Failed to add HexadecimalPassword to 'parameters'");
    }

    dm_change_object(&args);

exit:
    amxc_string_clean(&ani_auth_path);
    amxc_var_clean(&args);
}

/**
 * Restore the PON password for an ANI: in DM and towards vendor module.
 *
 * @param[in] ani_path: path to ANI instance, e.g. "XPON.ONU.1.ANI.1"
 *
 * Check if there is a saved PON password for the ANI. If there is, update its
 * Password parameter (and its HexadecimalPassword parameter) in the DM. The
 * update in the DM causes the Ambiorix framework to call the same functions as
 * when another component or user updates the Password parameter:
 * _check_password() and _password_changed(). The latter function call among
 * others forwards the password to the vendor module.
 */
void passwd_restore_password(const char* const ani_path) {

    SAH_TRACEZ_DEBUG(ME, "ani_path='%s'", ani_path);
    char password[MAX_PASSWORD_LEN_PLUS_ONE] = { 0 };
    bool hex = false;
    if(!upgr_persistency_get_password(ani_path, password, &hex)) {
        SAH_TRACEZ_ERROR(ME, "ani='%s': failed to get password", ani_path);
        goto exit;
    }
    when_true(strlen(password) == 0, exit); /* nothing to do */

    update_password_in_dm(ani_path, password, hex);

exit:
    return;
}

