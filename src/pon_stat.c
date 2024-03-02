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
#include "pon_stat.h"

/* System headers */
#include <amxc/amxc_macros.h> /* UNUSED */

/* Other libraries' headers */
/**
 * amxo.h includes amxo_types.h, which requires amxd/amxd_types.h. That header
 * needs amxp_signal_mngr_t.
 */
#include <amxp/amxp.h>
#include <amxc/amxc.h>
#include <amxd/amxd_types.h>
#include <amxo/amxo.h>    /* amxo_connection_add() */

/* Own headers */
#include "data_model.h"   /* dm_add_instance() */
#include "dm_xpon_mngr.h" /* xpon_mngr_get_parser() */
#include "pon_ctrl.h"     /* handle_file_descriptor() */
#include "xpon_trace.h"


/**
 * Add an instance to the XPON DM.
 *
 * @param[in] args : must be htable with the keys 'path', 'index' and 'keys'.
 *                   The htable normally also has an entry with the key
 *                   'parameters'.
 *
 * @return 0 on success, else -1.
 */
int dm_instance_added(UNUSED const char* function_name,
                      amxc_var_t* args,
                      UNUSED amxc_var_t* ret) {

    SAH_TRACEZ_INFO(ME, "called");
    return dm_add_instance(args);
}

/**
 * Remove an instance from the XPON DM.
 *
 * @param[in] args : must be htable with the keys 'path' and 'index'
 *
 * @return 0 on success, else -1.
 */
int dm_instance_removed(UNUSED const char* function_name,
                        amxc_var_t* args,
                        UNUSED amxc_var_t* ret) {

    SAH_TRACEZ_INFO(ME, "called");
    return dm_remove_instance(args);
}

/**
 * Update one of more params of an object in the XPON DM.
 *
 * @param[in] args : must be htable with the keys 'path' and 'parameters'
 *
 * @return 0 on success, else -1.
 */
int dm_object_changed(UNUSED const char* function_name,
                      amxc_var_t* args,
                      UNUSED amxc_var_t* ret) {

    SAH_TRACEZ_INFO(ME, "called");
    return dm_change_object(args);
}

/**
 * Add or update an instance in the XPON DM.
 *
 * @param[in] args : must be htable with the keys 'path' and 'index'. It must
 *                   also have the key 'keys' if the instance does not yet
 *                   exist, else it must have the key 'parameters'. It's
 *                   recommended that 'args' has all those 4 keys. See below
 *                   for more info.
 *
 * The function checks if the instance exists. If the instance does not exist,
 * this component ends up calling 'dm_add_instance()', else it ends up calling
 * 'dm_change_object()'.
 *
 * More info about the parameter @a args:
 *
 * The path in @a args must be the template path, e.g.
 * "XPON.ONU.1.SoftwareImage".
 *
 * If the function finds out the instance does not exist yet, it demands the
 * same from @a args as dm_instance_added(): it must be an htable with the keys
 * 'path', 'index' and 'keys'.
 *
 * If the function finds out the instance already exists, it demands the same
 * from @a args as dm_object_changed(): it must be htable with the keys 'path'
 * and 'parameters'.
 *
 * Hence when calling this function, it's recommended @a args is htable with
 * the keys 'path', 'index', 'keys' and 'parameters'. Then the function finds
 * the required keys, independent from whether the instance already exists or
 * not.
 *
 * @return 0 on success, else -1.
 */
int dm_add_or_change_instance(UNUSED const char* function_name,
                              amxc_var_t* args,
                              UNUSED amxc_var_t* ret) {
    SAH_TRACEZ_INFO(ME, "called");
    return dm_add_or_change_instance_impl(args);
}

/**
 * Notify plugin an OMCI reset MIB message was received for an ONU.
 *
 * @param[in] args : must be htable with the key 'index'. The 'index' must
 *                   indicate for which XPON.ONU instance an OMCI reset MIB is
 *                   received.
 *
 * @return 0 on success, else -1.
 */
int omci_reset_mib(UNUSED const char* function_name,
                   amxc_var_t* args,
                   UNUSED amxc_var_t* ret) {

    SAH_TRACEZ_INFO(ME, "called");
    return dm_omci_reset_mib(args);
}

static void handle_fd(int fd, UNUSED void* priv) {
    when_false_trace(fd > 0, exit, ERROR, "Invalid fd [%d]", fd);

    SAH_TRACEZ_DEBUG(ME, "fd=%d readable", fd);
    pon_ctrl_handle_file_descriptor(fd);

exit:
    return;
}


static int watch_file_descriptor_common(amxc_var_t* args,
                                        bool start) {
    int rc = -1;
    amxo_parser_t* const parser = xpon_mngr_get_parser();
    when_null(parser, exit);
    when_null(args, exit);

    const uint32_t type = amxc_var_type_of(args);
    when_false_trace(type == AMXC_VAR_ID_FD, exit, ERROR,
                     "Type of 'args' = %d != FD", type);
    const int fd = amxc_var_constcast(fd_t, args);
    when_false_trace(fd > 0, exit, ERROR, "Invalid fd [%d]", fd);

    if(start) {
        if(amxo_connection_add(parser, fd, handle_fd, NULL, AMXO_CUSTOM, NULL) != 0) {
            SAH_TRACEZ_ERROR(ME, "Failed to start monitoring fd=%d", fd);
            goto exit;
        }
    } else {
        if(amxo_connection_remove(parser, fd) != 0) {
            SAH_TRACEZ_ERROR(ME, "Failed to stop monitoring fd=%d", fd);
            goto exit;
        }
    }

    rc = 0;

exit:
    return rc;
}

/**
 * Start monitoring a file descriptor.
 *
 * @param[in] args : must be of type AMXC_VAR_ID_FD.
 *
 * Examples for the file descriptor:
 * - file descriptor of the socket the vendor module uses for IPC with the
 *   vendor specific PON/OMCI stack.
 * - file descriptor of a socket to monitor netlink messages from the kernel.
 * - ...
 *
 * @return 0 on success, else -1.
 */
int watch_file_descriptor_start(UNUSED const char* function_name,
                                amxc_var_t* args,
                                UNUSED amxc_var_t* ret) {
    return watch_file_descriptor_common(args, /*start=*/ true);
}

/**
 * Stop monitoring a file descriptor.
 *
 * @param[in] args : must be of type AMXC_VAR_ID_FD.
 *
 * Examples for the file descriptor:
 * - file descriptor of the socket the vendor module uses for IPC with the
 *   vendor specific PON/OMCI stack.
 * - file descriptor of a socket to monitor netlink messages from the kernel.
 * - ...
 *
 * @return 0 on success, else -1.
 */
int watch_file_descriptor_stop(UNUSED const char* function_name,
                               amxc_var_t* args,
                               UNUSED amxc_var_t* ret) {
    return watch_file_descriptor_common(args, /*start=*/ false);
}

/**
 * Update a parameter of the XPON object.
 *
 * @param[in] args: must be htable with the keys 'name' and 'value'.
 *
 * @return 0 on success, else -1.
 */
int dm_set_xpon_parameter(UNUSED const char* function_name,
                          amxc_var_t* args,
                          UNUSED amxc_var_t* ret) {

    SAH_TRACEZ_INFO(ME, "called");
    return dm_set_xpon_parameter_impl(args);
}

