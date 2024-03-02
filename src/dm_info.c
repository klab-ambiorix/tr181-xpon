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
#include "dm_info.h"

/* System headers */
#include <string.h> /* strncmp() */

/* Other libraries' headers */
#include <amxc/amxc.h>
#include <amxc/amxc_macros.h>

/* Own headers */
#include "xpon_trace.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

static const param_info_t ONU_PARAMS[] = {
    { .name = ENABLE_PARAM, .type = AMXC_VAR_ID_BOOL    },
    { .name = "Version", .type = AMXC_VAR_ID_CSTRING },
    { .name = "EquipmentID", .type = AMXC_VAR_ID_CSTRING },
    { .name = "UsePPTPEthernetUNIasIFtoNonOmciDomain", .type = AMXC_VAR_ID_BOOL }
};

static const param_info_t SOFTWARE_IMAGE_PARAMS[] = {
    { .name = "Version", .type = AMXC_VAR_ID_CSTRING },
    { .name = "IsCommitted", .type = AMXC_VAR_ID_BOOL },
    { .name = "IsActive", .type = AMXC_VAR_ID_BOOL },
    { .name = "IsValid", .type = AMXC_VAR_ID_BOOL }
};

static const param_info_t ETHERNET_UNI_PARAMS[] = {
    { .name = "Enable", .type = AMXC_VAR_ID_BOOL       },
    { .name = "Status", .type = AMXC_VAR_ID_CSTRING    },
    { .name = "LastChange", .type = AMXC_VAR_ID_UINT32     },
    { .name = "ANIs", .type = AMXC_VAR_ID_CSV_STRING },
    { .name = "InterdomainID", .type = AMXC_VAR_ID_CSTRING    },
    { .name = "InterdomainName", .type = AMXC_VAR_ID_CSTRING    },
};

static const param_info_t ANI_PARAMS[] = {
    { .name = ENABLE_PARAM, .type = AMXC_VAR_ID_BOOL    },
    { .name = "Status", .type = AMXC_VAR_ID_CSTRING },
    { .name = "LastChange", .type = AMXC_VAR_ID_UINT32  },
    { .name = "PONMode", .type = AMXC_VAR_ID_CSTRING }
};

static const param_info_t GEM_PORT_PARAMS[] = {
    { .name = "Direction", .type = AMXC_VAR_ID_CSTRING },
    { .name = "PortType", .type = AMXC_VAR_ID_CSTRING }
};

static const param_info_t TRANSCEIVER_PARAMS[] = {
    { .name = "Identifier", .type = AMXC_VAR_ID_UINT32  },
    { .name = "VendorName", .type = AMXC_VAR_ID_CSTRING },
    { .name = "VendorPartNumber", .type = AMXC_VAR_ID_CSTRING },
    { .name = "VendorRevision", .type = AMXC_VAR_ID_CSTRING },
    { .name = "PONMode", .type = AMXC_VAR_ID_CSTRING },
    { .name = "Connector", .type = AMXC_VAR_ID_CSTRING },
    { .name = "NominalBitRateDownstream", .type = AMXC_VAR_ID_UINT32  },
    { .name = "NominalBitRateUpstream", .type = AMXC_VAR_ID_UINT32  },
    { .name = "RxPower", .type = AMXC_VAR_ID_INT32   },
    { .name = "TxPower", .type = AMXC_VAR_ID_INT32   },
    { .name = "Voltage", .type = AMXC_VAR_ID_UINT32  },
    { .name = "Bias", .type = AMXC_VAR_ID_UINT32  },
    { .name = "Temperature", .type = AMXC_VAR_ID_INT32   }
};

static const param_info_t ONU_ACTIVATION_PARAMS[] = {
    { .name = "ONUState", .type = AMXC_VAR_ID_CSTRING },
    { .name = "VendorID", .type = AMXC_VAR_ID_CSTRING },
    { .name = "SerialNumber", .type = AMXC_VAR_ID_CSTRING },
    { .name = "ONUID", .type = AMXC_VAR_ID_UINT32  }
};

static const param_info_t AUTHENTICATION_PARAMS[] = {
    { .name = "Password", .type = AMXC_VAR_ID_CSTRING },
    { .name = "HexadecimalPassword", .type = AMXC_VAR_ID_BOOL }
};

static const param_info_t PERFORMANCE_THRESHOLDS_PARAMS[] = {
    { .name = "SignalFail", .type = AMXC_VAR_ID_UINT32 },
    { .name = "SignalDegrade", .type = AMXC_VAR_ID_UINT32 }
};

static const param_info_t TC_ALARMS_PARAMS[] = {
    { .name = "LOS", .type = AMXC_VAR_ID_BOOL },
    { .name = "LOF", .type = AMXC_VAR_ID_BOOL },
    { .name = "SF", .type = AMXC_VAR_ID_BOOL },
    { .name = "SD", .type = AMXC_VAR_ID_BOOL },
    { .name = "LCDG", .type = AMXC_VAR_ID_BOOL },
    { .name = "TF", .type = AMXC_VAR_ID_BOOL },
    { .name = "SUF", .type = AMXC_VAR_ID_BOOL },
    { .name = "MEM", .type = AMXC_VAR_ID_BOOL },
    { .name = "DACT", .type = AMXC_VAR_ID_BOOL },
    { .name = "DIS", .type = AMXC_VAR_ID_BOOL },
    { .name = "MIS", .type = AMXC_VAR_ID_BOOL },
    { .name = "PEE", .type = AMXC_VAR_ID_BOOL },
    { .name = "RDI", .type = AMXC_VAR_ID_BOOL },
    { .name = "LODS", .type = AMXC_VAR_ID_BOOL },
    { .name = "ROGUE", .type = AMXC_VAR_ID_BOOL }
};

/**
 * Array with info about objects in the XPON DM.
 *
 * The 'id' of the element at index 'i' must have 'i' as value.
 */
const object_info_t OBJECT_INFO[obj_id_nbr] = {
    {
        .id = obj_id_onu,
        .name = "ONU",
        .generic_path = "XPON.ONU",
        .key_name = NAME_PARAM,
        .singletons = NULL,
        .templates = "SoftwareImage,EthernetUNI,ANI",
        .params = ONU_PARAMS,
        .n_params = ARRAY_SIZE(ONU_PARAMS),
        .has_rw_enable = true
    },
    {
        .id = obj_id_software_image,
        .name = "SoftwareImage",
        .generic_path = "XPON.ONU.x.SoftwareImage",
        .key_name = "ID",
        .key_max_value = 1,
        .singletons = NULL,
        .templates = NULL,
        .params = SOFTWARE_IMAGE_PARAMS,
        .n_params = ARRAY_SIZE(SOFTWARE_IMAGE_PARAMS),
        .has_rw_enable = false
    },
    {
        .id = obj_id_ethernet_uni,
        .name = "EthernetUNI",
        .generic_path = "XPON.ONU.x.EthernetUNI",
        .key_name = NAME_PARAM,
        .singletons = NULL,
        .templates = NULL,
        .params = ETHERNET_UNI_PARAMS,
        .n_params = ARRAY_SIZE(ETHERNET_UNI_PARAMS),
        .has_rw_enable = false
    },
    {
        .id = obj_id_ani,
        .name = "ANI",
        .generic_path = "XPON.ONU.x.ANI",
        .key_name = NAME_PARAM,
        .singletons = "TC.ONUActivation,TC.PerformanceThresholds,TC.Alarms",
        .templates = "TC.GEM.Port,Transceiver",
        .params = ANI_PARAMS,
        .n_params = ARRAY_SIZE(ANI_PARAMS),
        .has_rw_enable = true
    },
    {
        .id = obj_id_gem_port,
        .name = "GEMPort",
        .generic_path = "XPON.ONU.x.ANI.x.TC.GEM.Port",
        .key_name = "PortID",
        .key_max_value = 65534,
        .singletons = NULL,
        .templates = NULL,
        .params = GEM_PORT_PARAMS,
        .n_params = ARRAY_SIZE(GEM_PORT_PARAMS),
        .has_rw_enable = false
    },
    {
        .id = obj_id_transceiver,
        .name = "Transceiver",
        .generic_path = "XPON.ONU.x.ANI.x.Transceiver",
        .key_name = "ID",
        .key_max_value = 1,
        .singletons = NULL,
        .templates = NULL,
        .params = TRANSCEIVER_PARAMS,
        .n_params = ARRAY_SIZE(TRANSCEIVER_PARAMS),
        .has_rw_enable = false
    },
    {
        .id = obj_id_ani_tc_onu_activation,
        .name = "ONUActivation",
        .generic_path = "XPON.ONU.x.ANI.x.TC.ONUActivation",
        .key_name = NULL,
        .singletons = NULL,
        .templates = NULL,
        .params = ONU_ACTIVATION_PARAMS,
        .n_params = ARRAY_SIZE(ONU_ACTIVATION_PARAMS),
        .has_rw_enable = false
    },
    {
        .id = obj_id_ani_tc_authentication,
        .name = "Authentication",
        .generic_path = "XPON.ONU.x.ANI.x.TC.Authentication",
        .key_name = NULL,
        .singletons = NULL,
        .templates = NULL,
        .params = AUTHENTICATION_PARAMS,
        .n_params = ARRAY_SIZE(AUTHENTICATION_PARAMS),
        .has_rw_enable = false
    },
    {
        .id = obj_id_ani_tc_performance_thresholds,
        .name = "PerformanceThresholds",
        .generic_path = "XPON.ONU.x.ANI.x.TC.PerformanceThresholds",
        .key_name = NULL,
        .singletons = NULL,
        .templates = NULL,
        .params = PERFORMANCE_THRESHOLDS_PARAMS,
        .n_params = ARRAY_SIZE(PERFORMANCE_THRESHOLDS_PARAMS),
        .has_rw_enable = false
    },
    {
        .id = obj_id_ani_tc_alarms,
        .name = "TC.Alarms",
        .generic_path = "XPON.ONU.x.ANI.x.TC.Alarms",
        .key_name = NULL,
        .singletons = NULL,
        .templates = NULL,
        .params = TC_ALARMS_PARAMS,
        .n_params = ARRAY_SIZE(TC_ALARMS_PARAMS),
        .has_rw_enable = false
    }
};


/**
 * Initialize the dm_info part.
 *
 * The function only runs a sanity check on the OBJECT_INFO array.
 *
 * The plugin must call this function once at startup.
 *
 * @return true on success, else false
 */
bool dm_info_init(void) {
    uint32_t i;
    for(i = 0; i < obj_id_nbr; ++i) {
        if(OBJECT_INFO[i].id != i) {
            SAH_TRACEZ_ERROR(ME, "OBJECT_INFO[%d].id=%d != %d",
                             i, OBJECT_INFO[i].id, i);
            return false;
        }
    }
    return true;
}

/**
 * Convert an actual path to a generic path.
 *
 * @param[in] path  actual path with real instance indexes, e.g.,
 *                  "XPON.ONU.1.ANI.1.Transceiver"
 * @param[in,out] generic_path  the function returns the generic version of
 *     @a path. The generic version of a path is the path with all instance
 *     indexes replaced by 'x', e.g., "XPON.ONU.x.ANI.x.Transceiver".
 *
 * @return true on success, else false
 */
static bool dm_convert_to_generic_path(const char* path, amxc_string_t* generic_path) {

    bool rv = false;
    when_null(path, exit_no_cleanup);
    when_null(generic_path, exit_no_cleanup);

    amxc_llist_t list;
    amxc_llist_init(&list);

    amxc_string_t input;
    amxc_string_init(&input, 0);
    amxc_string_set(&input, path);

    if(AMXC_STRING_SPLIT_OK != amxc_string_split_to_llist(&input, &list, '.')) {
        SAH_TRACEZ_ERROR(ME, "Failed to split '%s'", path);
        goto exit;
    }

    /* Remove the last elem from 'list' if it's numeric */
    amxc_llist_it_t* last_it = amxc_llist_get_last(&list);
    when_null(last_it, exit);
    amxc_string_t* const last_elem = amxc_string_from_llist_it(last_it);
    if(amxc_string_is_numeric(last_elem)) {
        amxc_llist_take_last(&list);
        amxc_string_list_it_free(last_it);
    }

    amxc_llist_iterate(it, &list) {
        amxc_string_t* part = amxc_string_from_llist_it(it);
        if(amxc_string_is_numeric(part)) {
            amxc_string_set(part, "x");
        }
    }

    if(amxc_string_join_llist(generic_path, &list, '.') != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to generate generic path from '%s'", path);
        goto exit;
    }
    rv = true;

exit:
    amxc_llist_clean(&list, amxc_string_list_it_free);
    amxc_string_clean(&input);

exit_no_cleanup:
    return rv;
}


/**
 * Return the ID of an object.
 *
 * @param[in] path  object path, e.g., "XPON.ONU.1.SoftwareImage".
 *
 * Example: if @a path is "XPON.ONU.1.SoftwareImage", the function returns
 *          obj_id_software_image
 *
 * @return one of the value of object_id_t smaller than obj_id_nbr upon
 *         success, else obj_id_unknown
 */
object_id_t dm_get_object_id(const char* path) {

    object_id_t id = obj_id_unknown;

    amxc_string_t generic_path_str;
    amxc_string_init(&generic_path_str, 0);

    if(!dm_convert_to_generic_path(path, &generic_path_str)) {
        goto exit;
    }

    const char* const generic_path = amxc_string_get(&generic_path_str, 0);
    const size_t generic_path_len = strlen(generic_path);

    uint32_t i;
    size_t len;
    for(i = 0; i < obj_id_nbr; ++i) {
        len = strlen(OBJECT_INFO[i].generic_path);
        if((generic_path_len == len) &&
           (strncmp(generic_path, OBJECT_INFO[i].generic_path, len) == 0)) {
            id = OBJECT_INFO[i].id;
            break;
        }
    }

exit:
    amxc_string_clean(&generic_path_str);
    return id;
}

const object_info_t* dm_get_object_info(object_id_t id) {
    if(id < obj_id_nbr) {
        return &OBJECT_INFO[id];
    }

    SAH_TRACEZ_ERROR(ME, "Invalid id [%d]", id);
    return NULL;
}

/**
 * Return info about the params of an object.
 *
 * @param[in] id              object ID
 * @param[in,out] param_info  function passes info about the params of the
 *                            object via this param.
 * @param[in,out] size        nr of elems in @a param_info
 *
 * @return true on success, else false
 */
bool dm_get_object_param_info(object_id_t id,
                              const param_info_t** param_info,
                              uint32_t* size) {
    if(id < obj_id_nbr) {
        *param_info = OBJECT_INFO[id].params;
        *size = OBJECT_INFO[id].n_params;
        return true;
    }
    SAH_TRACEZ_ERROR(ME, "Invalid id [%d]", id);
    return false;
}
