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

#ifndef __dm_info_h__
#define __dm_info_h__

/**
 * @file dm_info.h
 *
 * Info related to the TR-181 XPON DM known at compile time.
 */
#include <stdbool.h>
#include <stdint.h>

#define ENABLE_PARAM "Enable"

#define NAME_PARAM "Name"
#define NAME_PARAM_LEN  4

typedef enum _xpon_object_id {
    obj_id_onu = 0,
    obj_id_software_image,
    obj_id_ethernet_uni,
    obj_id_ani,
    obj_id_gem_port,
    obj_id_transceiver,
    obj_id_ani_tc_onu_activation,
    obj_id_ani_tc_authentication,
    obj_id_ani_tc_performance_thresholds,
    obj_id_ani_tc_alarms,
    obj_id_nbr,
    obj_id_unknown = obj_id_nbr
} object_id_t;

typedef struct _param_info {
    const char* name;
    uint32_t type; /* One of the AMXC_VAR_ID values */
} param_info_t;


/**
 * Info which is known at compile time about an object in the XPON DM.
 *
 * - id: unique ID identifying an object in the DM
 * - name: symbolic name for 'id'. Mainly intended for log messages. E.g.,
 *     "SoftwareImage".
 * - generic_path: object path with all instance indexes replaced by 'x', e.g.,
 *     "XPON.ONU.x.SoftwareImage"
 * - key_name: the name of the parameter which is the unique key of the
 *     template object. Only applicable for template objects.
 * - key_max_value: if the key referred to by 'key_name' is an uint32, this
 *     field indicates its max value.
 * - singletons: string of comma-separated strings identifying the children
 *     which are singletons.
 * - templates: string of comma-separated strings identifying the children
 *     which are templates.
 * - params: pointer to array with info about the params of this object.
 * - n_params: number of params in the array pointed to by 'params'
 * - has_rw_enable: true if the object has a read-write Enable parameter. The
 *       code only looks at this field when creating a new instance of a
 *       template object.
 */
typedef struct _object_info {
    object_id_t id;
    const char* name;
    const char* generic_path;
    const char* key_name;
    uint32_t key_max_value;
    const char* singletons;
    const char* templates;
    const param_info_t* params;
    uint32_t n_params;
    bool has_rw_enable;
} object_info_t;

bool dm_info_init(void);

object_id_t dm_get_object_id(const char* path);

const object_info_t* dm_get_object_info(object_id_t id);

bool dm_get_object_param_info(object_id_t id, const param_info_t** param_info,
                              uint32_t* size);

#endif
