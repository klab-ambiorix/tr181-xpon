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
#include "ani.h"

/* Own headers */
#include "xpon_trace.h"

#define DOT_TC_AUTHENTICATION ".TC.Authentication"

/**
 * Strip ".TC.Authentication" from object path
 *
 * @param[in] ani_auth_path: path to the TC.Authentication object of an ANI,
 *                           e.g. "XPON.ONU.1.ANI.1.TC.Authentication"
 * @param[in,out] ani_path: the caller must pass an initialized
 *     amxc_string_t object. The function takes the value of @a ani_auth_path,
 *     strips ".TC.Authentication" from it, and assigns the resulting value to
 *     @a ani_path. The caller must call amxc_string_clean() on
 *     @a ani_path (when finished using it).
 *
 * This function does the inverse of ani_append_tc_authentication().
 *
 * @return true on success, else false
 */
bool ani_strip_tc_authentication(const char* const ani_auth_path,
                                 amxc_string_t* const ani_path) {

    bool rv = false;
    amxc_string_set(ani_path, ani_auth_path);
    const int replacements =
        amxc_string_replace(ani_path, DOT_TC_AUTHENTICATION, "", /*max=*/ 1);
    when_false_trace(replacements == 1, exit, ERROR,
                     "Failed to strip %s from %s", DOT_TC_AUTHENTICATION,
                     ani_auth_path);
    rv = true;
exit:
    return rv;
}

/**
 * Append ".TC.Authentication" to object path.
 *
 * @param[in] ani_path: path to ANI instance, e.g. "XPON.ONU.1.ANI.1"
 * @param[in,out] ani_auth_path: the caller must pass an initialized
 *     amxc_string_t object. The function takes the value of @a ani_path,
 *     appends ".TC.Authentication", and assigns the resulting value to
 *     @a ani_auth_path. The caller must call amxc_string_clean() on
 *     @a ani_auth_path (when finished using it).
 *
 * This function does the inverse of ani_append_tc_authentication().
 */
void ani_append_tc_authentication(const char* const ani_path,
                                  amxc_string_t* const ani_auth_path) {

    amxc_string_setf(ani_auth_path, "%s%s", ani_path, DOT_TC_AUTHENTICATION);
}

