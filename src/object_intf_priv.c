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
#include "object_intf_priv.h"

/* System headers */
#include <stdint.h>
#include <stdlib.h> /* calloc(), free() */

/* Other libraries' headers */

/**
 * Include amxp.h to avoid following compile error:
 *
 * In file included from /usr/include/amxd/amxd_object_hierarchy.h:70,
 *                  from private_object.c:7:
 * /usr/include/amxd/amxd_types.h:266:5: error: unknown type name ‘amxp_signal_mngr_t’
 */
#include <amxp/amxp.h>
#include <amxd/amxd_object_hierarchy.h> /* amxd_object_get_instance() */

/* Own headers */
#include "dm_xpon_mngr.h"               /* xpon_mngr_get_dm() */
#include "utils_time.h"                 /* time_get_system_uptime() */
#include "xpon_trace.h"


static object_intf_priv_t* create_priv(void) {
    object_intf_priv_t* priv = calloc(1, sizeof(object_intf_priv_t));
    when_null_trace(priv, exit, ERROR, "Failed to create object_intf_priv_t");
    priv->last_change = time_get_system_uptime();

exit:
    return priv;
}

/**
 * Attach private data to interface object referred by @a data.
 *
 * @param[in] data: event data. It has the path and the index of the instance
 *                  just added.
 *
 * tr181-xpon calls this function if an EthernetUNI or ANI instance is added
 * to attach private data to the instance.
 */
void oipriv_attach_private_data(const amxc_var_t* const data) {

    const char* const path = GETP_CHAR(data, "path");
    const uint32_t index = GET_UINT32(data, "index");
    SAH_TRACEZ_DEBUG(ME, "path='%s' index=%d", path, index);

    amxd_dm_t* const dm = xpon_mngr_get_dm();
    const amxd_object_t* const obj = amxd_dm_signal_get_object(dm, data);
    amxd_object_t* const inst = amxd_object_get_instance(obj, NULL, index);
    when_null_trace(inst, exit, ERROR, "Failed to get instance");
    object_intf_priv_t* const priv = create_priv();
    when_null_trace(priv, exit, ERROR, "Failed to create object_intf_priv_t");
    inst->priv = priv;

exit:
    return;
}

void oipriv_delete_private_data(object_intf_priv_t* priv) {
    if(priv) {
        free(priv);
    }
}

/**
 * Update the last_change field in the private data of an interface object.
 *
 * @param[in] data: event data. It has the path of the interface object for
 *                  which this function must update the last_change field.
 *
 * tr181-xpon calls this function if the Status parameter of an interface object
 * is changed.
 */
void oipriv_update_last_change(const amxc_var_t* const data) {

    const char* const path = GETP_CHAR(data, "path");
    SAH_TRACEZ_DEBUG(ME, "path='%s'", path);

    amxd_dm_t* const dm = xpon_mngr_get_dm();
    const amxd_object_t* const obj = amxd_dm_signal_get_object(dm, data);
    when_null_trace(obj->priv, exit, ERROR, "object %s has no private data", path);

    object_intf_priv_t* const priv = (object_intf_priv_t* const) obj->priv;
    priv->last_change = time_get_system_uptime();

exit:
    return;
}

