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
#include "data_model.h"

/* System headers */
#include <stdio.h>  /* snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strncmp() */

/* Other libraries' headers */
#include <amxc/amxc.h>

/**
 * Include amxp.h to include amxp_signal.h, which defines amxp_signal_mngr_t.
 * amxd_object.h and amxd_transaction.h need it: they include amxd_types.h,
 * which uses amxp_signal_mngr_t.
 */
#include <amxp/amxp.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_transaction.h>

/* Own headers */
#include "ani.h"            /* ani_append_tc_authentication() */
#include "dm_actions.h"     /* dm_actions_set_ignore_param_reads() */
#include "dm_info.h"
#include "dm_xpon_mngr.h"   /* xpon_mngr_get_dm() */
#include "onu_priv.h"       /* onu_priv_attach_private_data() */
#include "persistency.h"
#include "password.h"
#include "restore_to_hal.h" /* rth_schedule_enable() */
#include "xpon_trace.h"

#define ADD_INST_N_ARGS_REQUIRED 3
static const char* ADD_INST_PARAMS_REQUIRED[ADD_INST_N_ARGS_REQUIRED] = {
    "path", "index", "keys"
};

#define REMOVE_INST_N_ARGS_REQUIRED 2
static const char* REMOVE_INST_ARGS_REQUIRED[REMOVE_INST_N_ARGS_REQUIRED] = {
    "path", "index"
};

#define CHANGE_OBJ_N_ARGS_REQUIRED 2
static const char* CHANGE_OBJ_ARGS_REQUIRED[CHANGE_OBJ_N_ARGS_REQUIRED] = {
    "path", "parameters"
};

#define ADD_OR_CHANGE_INST_N_ARGS_REQUIRED 2
static const char* ADD_OR_CHANGE_INST_ARGS_REQUIRED[ADD_OR_CHANGE_INST_N_ARGS_REQUIRED] = {
    "path", "index"
};


/**
 * Info related to actions done on the DM.
 *
 * The actions are:
 * - add an instance
 * - remove an instance
 * - update/change an object
 *
 * Meaning of the fields:
 * - path:      object path of a singleton or template object
 * - index:     instance index for the instance of a template object. 0 if not
 *              applicable.
 * - obj_id:    object ID (derived from 'path' with dm_get_object_id())
 * - key_value: for an instance of a template object, it is the value of the
 *              unique key for that instance. The variant normally has a string
 *              or an uint32_t.
 * - params:    values for the params of the object
 */
typedef struct _dm_action_info {
    const char* path;
    uint32_t index;
    object_id_t obj_id;
    amxc_var_t* key_value;
    const amxc_var_t* params;
} dm_action_info_t;

static void init_dm_action_info(dm_action_info_t* const info) {

    info->path = NULL;
    info->index = 0;
    info->obj_id = obj_id_unknown;
    info->key_value = NULL;
    info->params = NULL;
}

/**
 * Return the "XPON" object from the XPON DM.
 */
static amxd_object_t* dm_get_xpon_object(void) {

    amxd_dm_t* const dm = xpon_mngr_get_dm();
    if(!dm) {
        SAH_TRACEZ_ERROR(ME, "Failed to get DM");
        return NULL;
    }


    amxd_object_t* xpon = amxd_dm_get_object(dm, "XPON");
    if(!xpon) {
        SAH_TRACEZ_ERROR(ME, "Failed to get XPON object");
        return NULL;
    }
    return xpon;
}

/**
 * Set XPON.ModuleName to @a name.
 *
 * @param[in] name: name of the vendor module, e.g. "mod-xpon-prpl".
 *
 * The component should call this function once at startup, after determining
 * which vendor module it should load.
 */
void dm_set_vendor_module(const char* name) {

    amxd_object_t* xpon = dm_get_xpon_object();
    when_null(xpon, exit);
    if(amxd_object_set_value(cstring_t, xpon, "ModuleName", name)) {
        SAH_TRACEZ_ERROR(ME, "Failed to update XPON.ModuleName");
    }
exit:
    return;
}

/**
 * Set XPON.ModuleError to true.
 */
void dm_set_module_error(void) {

    amxd_object_t* xpon = dm_get_xpon_object();
    when_null(xpon, exit);
    if(amxd_object_set_value(bool, xpon, "ModuleError", true)) {
        SAH_TRACEZ_ERROR(ME, "Failed to set XPON.ModuleError to true");
    }
exit:
    return;
}

static bool get_ref_to_params(const amxc_var_t* const args,
                              const amxc_var_t** params) {
    bool rv = false;

    const amxc_var_t* const params_var =
        amxc_var_get_key(args, "parameters", AMXC_VAR_FLAG_DEFAULT);
    when_null_trace(params_var, exit, ERROR, "Failed to get 'parameters' from 'args'");

    const uint32_t params_type = amxc_var_type_of(params_var);
    when_false_trace(AMXC_VAR_ID_HTABLE == params_type, exit, ERROR,
                     "Type of 'parameters' = %d != htable", params_type);

    *params = params_var;

    rv = true;
exit:
    return rv;
}

/**
 * Process the arguments of a function implemented in this file.
 *
 * @param[in] args             must be htable with keys specified by
 *                             @a keys_required
 * @param[in,out] info         the function extracts info from @a args and
 *                             assigns it to the fields of this parameter
 * @param[in] keys_required    list of keys which must be present in @a args
 * @param[in] n_keys_required  number of elements in @a keys_required
 *
 * @a args must always have an entry with key 'path'. The function assigns its
 * value to info->path. It derives the object ID from the path with
 * dm_get_object_id() and assigns it to info->obj_id.
 *
 * If @a args contains entries with the key 'index' and/or 'parameters', the
 * function assigns their values to info->index and info->params respectively.
 *
 * The function returns false if one of the keys specified in @a keys_required
 * does not occur in @a args.
 *
 * @return true on success, else false
 */
static bool process_args_common(const amxc_var_t* const args,
                                dm_action_info_t* info,
                                const char** keys_required,
                                int n_keys_required) {
    bool rv = false;
    uint32_t index = 0;
    const amxc_var_t* params = NULL;

    SAH_TRACEZ_DEBUG2(ME, "called");

    when_null_trace(args, exit, ERROR, "args is NULL");

#ifdef _DEBUG_
    printf("tr181-xpon: process_args_common(): dump(args):\n");
    amxc_var_dump(args, STDOUT_FILENO);
#endif

    init_dm_action_info(info);

    const amxc_htable_t* const htable = amxc_var_constcast(amxc_htable_t, args);
    when_null_trace(htable, exit, ERROR, "args is not an htable");

    int i;
    for(i = 0; i < n_keys_required; ++i) {
        if(!amxc_htable_contains(htable, keys_required[i])) {
            SAH_TRACEZ_ERROR(ME, "args does not contain key '%s'", keys_required[i]);
            goto exit;
        }
        if((index == 0) && (strncmp(keys_required[i], "index", 5) == 0)) {
            index = GET_UINT32(args, "index");
            if(0 == index) {
                SAH_TRACEZ_ERROR(ME, "Failed to get valid index");
                goto exit;
            }
        }
    }

    const char* const path = GET_CHAR(args, "path");
    when_null_trace(path, exit, ERROR, "Failed to get path");

    const object_id_t obj_id = dm_get_object_id(path);
    when_false_trace(obj_id_unknown != obj_id, exit, ERROR,
                     "path='%s': failed to get ID", path);

    /* Get 'index' if it's in 'args' even it's not required */
    if((index == 0) && amxc_htable_contains(htable, "index")) {
        index = GET_UINT32(args, "index");
        if(0 == index) {
            SAH_TRACEZ_ERROR(ME, "Failed to get valid index");
        }
    }

    info->path = path;
    info->index = index;
    info->obj_id = obj_id;

    if(amxc_htable_contains(htable, "parameters")) {
        if(get_ref_to_params(args, &params)) {
            info->params = params;
        }
    }
    rv = true;
exit:
    return rv;
}

/**
 * Process the arguments of dm_add_instance() function call.
 *
 * @param[in] args       must be htable with keys specified by
 *                       ADD_INST_PARAMS_REQUIRED. The htable normally also has
 *                       an entry with the key 'parameters'.
 * @param[in,out] info   the function extracts info from @a args and assigns it
 *                       to the fields of this parameter
 *
 * It calls process_args_common() with args = ADD_INST_PARAMS_REQUIRED.
 *
 * In addition it extracts the key value from @a args and assigns it
 * info->key_value.
 *
 * @return true on success, else false
 */
static bool process_add_instance_args(const amxc_var_t* const args,
                                      dm_action_info_t* info) {

    bool rv = false;
    SAH_TRACEZ_DEBUG2(ME, "called");

    if(!process_args_common(args, info, ADD_INST_PARAMS_REQUIRED,
                            ADD_INST_N_ARGS_REQUIRED)) {
        goto exit;
    }

    const object_id_t obj_id = info->obj_id;
    const object_info_t* const obj_info = dm_get_object_info(obj_id);
    when_null_trace(obj_info, exit, ERROR, "obj_info is NULL for obj_id=%d", obj_id);

    const char* key_name = obj_info->key_name; /* alias */

    const amxc_var_t* const keys_var = amxc_var_get_key(args, "keys", AMXC_VAR_FLAG_DEFAULT);
    when_null_trace(keys_var, exit, ERROR, "%s.%d: failed to get 'keys' from args",
                    info->path, info->index);

    if(amxc_var_type_of(keys_var) != AMXC_VAR_ID_HTABLE) {
        SAH_TRACEZ_ERROR(ME, "%s.%d: variant 'keys' is not an htable",
                         info->path, info->index);
        goto exit;
    }
    amxc_var_t* key_value =
        amxc_var_get_key(keys_var, key_name, AMXC_VAR_FLAG_DEFAULT);
    when_null_trace(key_value, exit, ERROR, "%s.%d: failed to get value for key='%s'",
                    info->path, info->index, key_name);

    /* If the key name is "Name", its value is of type string, else its value
     * is of type uint32_t. */
    const bool key_is_uint32 =
        strncmp(key_name, NAME_PARAM, NAME_PARAM_LEN) != 0;

    if(key_is_uint32) {
        const uint32_t key_uint32 = amxc_var_constcast(uint32_t, key_value);
        if(key_uint32 > obj_info->key_max_value) {
            SAH_TRACEZ_ERROR(ME, "%s.%d: key: %s: value=%d > max=%d",
                             info->path, info->index, key_name, key_uint32,
                             obj_info->key_max_value);
            goto exit;
        }
    }

    info->key_value = key_value;

    rv = true;
exit:
    return rv;
}

/**
 * Prepare adding an instance to the XPON DM.
 *
 * @param[in] path       object path of a template object, e.g. "XPON.ONU"
 * @param[in] index      index of instance to be added
 * @param[in,out] templ  function assigns pointer to template object if instance
 *                       does not yet exist
 *
 * @return true on success and if instance does not yet exist, else false
 */
static bool prepare_adding_instance(const char* const path, uint32_t index,
                                    amxd_object_t** templ) {

    bool rv = false;
    when_null(path, exit);
    when_null(templ, exit);

    amxd_dm_t* const dm = xpon_mngr_get_dm();
    when_null(dm, exit);

    amxd_object_t* templ_candidate = amxd_dm_findf(dm, "%s", path);
    when_null_trace(templ_candidate, exit, ERROR, "%s does not exist", path);

    const amxd_object_type_t type = amxd_object_get_type(templ_candidate);
    when_false_trace(amxd_object_template == type, exit, ERROR,
                     "%s is not a template object", path);

    if(amxd_object_get_instance(templ_candidate, NULL, index) != NULL) {
        SAH_TRACEZ_ERROR(ME, "%s.%d already exists", path, index);
        goto exit;
    }

    *templ = templ_candidate;
    rv = true;

exit:
    return rv;
}

/**
 * Add set value actions to a transaction to set params of an object.
 *
 * @param[in,out] transaction  transaction to add set value actions to
 * @param[in] params           htable with values for one or more params of an object
 * @param[in] id               object ID
 *
 * The function iterates over all elements in @a params. It adds a set value
 * action to @a transaction for each known element.
 *
 * @return true on success, else false
 */
static bool add_params_to_transaction(amxd_trans_t* transaction,
                                      const amxc_var_t* const params,
                                      object_id_t id) {
    bool rv = false;

    const amxc_htable_t* const params_table = amxc_var_constcast(amxc_htable_t, params);
    when_null(params_table, exit);

    const object_info_t* const obj_info = dm_get_object_info(id);
    when_null(obj_info, exit);

    const param_info_t* param_info;
    const param_info_t* single_param;
    uint32_t n_params;

    if(!dm_get_object_param_info(id, &param_info, &n_params)) {
        SAH_TRACEZ_ERROR(ME, "%s: failed to get info about its params", obj_info->name);
        goto exit;
    }

    amxc_var_t* param_value = NULL;
    uint32_t i;
    const char* key;

    amxc_htable_for_each(it, params_table) {
        single_param = NULL;
        key = amxc_htable_it_get_key(it);
        if(!key) {
            continue;
        }
        for(i = 0; i < n_params; ++i) {
            if((strncmp(key, param_info[i].name, strlen(key)) == 0) &&
               (strlen(key) == strlen(param_info[i].name))) {
                single_param = &param_info[i];
                break; /* out of for loop */
            }
        }
        if(NULL == single_param) {
            SAH_TRACEZ_WARNING(ME, "%s: unknown param name: %s", obj_info->name, key);
            continue;
        }
        param_value = GET_ARG(params, key);
        if(NULL == param_value) {
            SAH_TRACEZ_ERROR(ME, "%s: failed to get value for %s", obj_info->name, key);
            continue;
        }
        if(amxc_var_type_of(param_value) != single_param->type) {
            SAH_TRACEZ_ERROR(ME, "%s: type of parameter %s: %d != expected=%d",
                             obj_info->name, key, amxc_var_type_of(param_value),
                             single_param->type);
            continue;
        }
        if(amxd_trans_set_param(transaction, key, param_value)) {
            SAH_TRACEZ_ERROR(ME, "%s: failed to add value for %s to transaction",
                             obj_info->name, key);
        } else {
            SAH_TRACEZ_DEBUG(ME, "%s: added value for %s to transaction",
                             obj_info->name, key);
        }
    }
    rv = true;

exit:
    return rv;
}

/**
 * Set the Enable param to true if the object is enabled according to persistency.
 *
 * Also call rth_schedule_enable().
 *
 * @param[in] transaction  transaction which is going to add an instance
 * @param[in] path         object path, e.g. "XPON.ONU"
 * @param[in] index        the index of the object instance
 */
static void update_enable(amxd_trans_t* transaction,
                          const char* const path, uint32_t index) {

    when_false_trace(index != 0, exit, ERROR, "index is 0");

    amxc_string_t full_path;
    amxc_string_init(&full_path, 0);
    amxc_string_set(&full_path, path);
    amxc_string_appendf(&full_path, ".%d", index);

    const char* const full_path_cstr = amxc_string_get(&full_path, 0);

    if(persistency_is_enabled(full_path_cstr)) {
        amxc_var_t enabled;
        amxc_var_init(&enabled);
        amxc_var_set(bool, &enabled, true);
        amxd_trans_set_param(transaction, ENABLE_PARAM, &enabled);
        amxc_var_clean(&enabled);

        rth_schedule_enable(full_path_cstr);
    }
    amxc_string_clean(&full_path);

exit:
    return;
}

/**
 * Create and attach private data to the ONU instance.
 *
 * @param[in] path: path of the XPON.ONU template. This must be "XPON.ONU".
 * @param[in] index: index of the ONU instance
 */
static void add_private_data_to_onu(const char* const path, uint32_t index) {

    SAH_TRACEZ_DEBUG(ME, "%s.%d", path, index);

    amxd_dm_t* const dm = xpon_mngr_get_dm();
    when_null(dm, exit);

    amxd_object_t* templ = amxd_dm_findf(dm, "%s", path);
    when_null_trace(templ, exit, ERROR, "Failed to find %s", path);

    amxd_object_t* const obj = amxd_object_get_instance(templ, NULL, index);
    when_null_trace(obj, exit, ERROR, "Instance %s.%d does not exist", path, index);

    onu_priv_attach_private_data(obj);

exit:
    return;
}

/**
 * Add an instance to the XPON DM.
 *
 * @param[in] info  all the info needed to create the instance
 *
 * If the instance to be created has a read-write Enable parameter, and if that
 * instance is enabled according to the persistent data, create the instance
 * with that Enable parameter set to true, and schedule a timer to let this
 * know to the vendor module in 100 ms.
 *
 * If the instance created is an ONU instance, create and attach private data to
 * the instance to indicate the persistency of the RW Enable parameter has been
 * handled. The ANI also a has RW Enable parameter. The approach with the
 * private data is only done for an ONU because the PON vendor modules create an
 * ONU instance in 2 different ways:
 *
 * - via this add_instance() call: then this add_instance() call attaches the
 *     private data to the ONU instance. If dm_change_object() is called
 *     afterwards, it notices the private data is already present, and then it
 *     knows this process already checked if it should enable the ONU
 *     according to the persistent data.
 *
 * - via defaults.odl file: then add_instance() is not triggered, but
 *     dm_change_object() is. dm_change_object() will see that there is no
 *     private data attached to the ONU. Then that function will attach the
 *     private data and check if it should enable the ONU.
 *
 * If the instance created is an ANI instance, restore its PON password if there
 * is one.
 *
 * @return true on success, else false
 */
static bool add_instance(const dm_action_info_t* const info) {

    bool rv = false;
    amxd_object_t* templ = NULL;

    SAH_TRACEZ_INFO(ME, "Create %s.%d", info->path, info->index);

    const object_info_t* const obj_info = dm_get_object_info(info->obj_id);
    when_null_trace(obj_info, exit_no_cleanup, ERROR,
                    "obj_info is NULL for obj_id=%d", info->obj_id);

    if(!prepare_adding_instance(info->path, info->index, &templ)) {
        return false;
    }

    amxd_dm_t* const dm = xpon_mngr_get_dm();
    when_null(dm, exit_no_cleanup);

    amxd_trans_t transaction;
    amxd_trans_init(&transaction);
    amxd_trans_set_attr(&transaction, amxd_tattr_change_ro, true);

    const char* const key_name = obj_info->key_name; /* alias */

    amxd_status_t rc = amxd_trans_select_object(&transaction, templ);
    when_failed_trace(rc, exit, ERROR, "Failed to select %s for transaction (rc=%d)",
                      info->path, rc);

    rc = amxd_trans_add_inst(&transaction, info->index, NULL);
    when_failed_trace(rc, exit, ERROR,
                      "Failed to add creation of %s.%d to transaction (rc=%d)",
                      info->path, info->index, rc);

    rc = amxd_trans_set_param(&transaction, key_name, info->key_value);
    when_failed_trace(rc, exit, ERROR,
                      "Failed to add value for key to transaction (rc=%d)", rc);

    if(info->params) {
        add_params_to_transaction(&transaction, info->params, info->obj_id);
    }

    if(obj_info->has_rw_enable) {
        update_enable(&transaction, info->path, info->index);
    }

    rc = amxd_trans_apply(&transaction, dm);
    when_failed_trace(rc, exit, ERROR, "Failed to create %s.%d (rc=%d)",
                      info->path, info->index, rc);

    SAH_TRACEZ_DEBUG(ME, "Created %s.%d", info->path, info->index);

    if(obj_info->id == obj_id_onu) {
        add_private_data_to_onu(info->path, info->index);
    } else if(obj_info->id == obj_id_ani) {
        amxc_string_t ani_instance;
        amxc_string_init(&ani_instance, 0);
        amxc_string_setf(&ani_instance, "%s.%d", info->path, info->index);
        passwd_restore_password(amxc_string_get(&ani_instance, 0));
        amxc_string_clean(&ani_instance);
    }

    rv = true;

exit:
    amxd_trans_clean(&transaction);

exit_no_cleanup:
    return rv;
}

/**
 * Process the arguments of dm_remove_instance() function call.
 *
 * @param[in] args       must be htable with keys specified by
 *                       REMOVE_INST_ARGS_REQUIRED
 * @param[in,out] info   the function extracts info from @a args and assigns it
 *                       to the fields of this parameter
 *
 * @return true on success, else false
 */
static bool process_remove_instance_args(const amxc_var_t* const args,
                                         dm_action_info_t* info) {

    bool rv = false;
    SAH_TRACEZ_DEBUG2(ME, "called");

    if(!process_args_common(args, info, REMOVE_INST_ARGS_REQUIRED,
                            REMOVE_INST_N_ARGS_REQUIRED)) {
        goto exit;
    }

    rv = true;

exit:
    return rv;
}

/**
 * Remove an from instance from to the XPON DM.
 *
 * @param[in] info  all the info needed to remove the instance
 *
 * The function logs a warning and returns true if the instance does not exist.
 *
 * @return true on success, else false
 */
static bool remove_instance(const dm_action_info_t* const info) {

    bool rv = false;
    amxd_object_t* obj;

    SAH_TRACEZ_INFO(ME, "Delete %s.%d", info->path, info->index);

    amxd_dm_t* const dm = xpon_mngr_get_dm();
    when_null(dm, exit_no_cleanup);

    amxd_object_t* templ = amxd_dm_findf(dm, "%s", info->path);
    if(!templ) {
        SAH_TRACEZ_WARNING(ME, "Failed to find %s", info->path);
        return true;
    }

    const amxd_object_type_t type = amxd_object_get_type(templ);
    when_false_trace(amxd_object_template == type, exit_no_cleanup, ERROR,
                     "%s is not a template object", info->path);

    obj = amxd_object_get_instance(templ, NULL, info->index);
    if(!obj) {
        SAH_TRACEZ_WARNING(ME, "Instance %s.%d does not exist", info->path,
                           info->index);
        return true;
    }

    amxd_trans_t transaction;
    amxd_trans_init(&transaction);
    amxd_trans_set_attr(&transaction, amxd_tattr_change_ro, true);

    amxd_status_t rc = amxd_trans_select_object(&transaction, templ);
    when_failed_trace(rc, exit, ERROR, "Failed to select %s for transaction (rc=%d)",
                      info->path, rc);

    rc = amxd_trans_del_inst(&transaction, info->index, NULL);
    when_failed_trace(rc, exit, ERROR,
                      "Failed to add %s.%d for deletion to transaction (rc=%d)",
                      info->path, info->index, rc);

    dm_actions_set_ignore_param_reads(true);
    rc = amxd_trans_apply(&transaction, dm);
    when_failed_trace(rc, exit, ERROR, "Failed to delete %s.%d (rc=%d)",
                      info->path, info->index, rc);

    SAH_TRACEZ_DEBUG(ME, "Deleted %s.%d", info->path, info->index);

    rv = true;

exit:
    amxd_trans_clean(&transaction);
    dm_actions_set_ignore_param_reads(false);

exit_no_cleanup:
    return rv;
}


/**
 * Add an instance to the XPON DM.
 *
 * @param[in] args : must be htable with the keys 'path', 'index' and 'keys'.
 *                   The htable normally also has an entry with the key
 *                   'parameters'.
 *
 * @return 0 on success, else -1.
 */
int dm_add_instance(const amxc_var_t* const args) {

    int rc = -1;
    dm_action_info_t info;

    SAH_TRACEZ_DEBUG2(ME, "called");

    if(!process_add_instance_args(args, &info)) {
        goto exit;
    }
    if(!add_instance(&info)) {
        goto exit;
    }

    rc = 0;
exit:
    return rc;
}

/**
 * Remove an instance from the XPON DM.
 *
 * @param[in] args : must be htable with the keys 'path' and 'index'
 *
 * @return 0 on success, else -1.
 */
int dm_remove_instance(const amxc_var_t* const args) {

    int rc = -1;
    dm_action_info_t info;

    SAH_TRACEZ_DEBUG2(ME, "called");

    if(!process_remove_instance_args(args, &info)) {
        goto exit;
    }
    if(!remove_instance(&info)) {
        goto exit;
    }

    rc = 0;
exit:
    return rc;
}

/**
 * Update one of more params of an object in the XPON DM.
 *
 * @param[in] args : must be htable with the keys 'path' and 'parameters'. If it
 *                   has a non-zero value for 'index', the function assumes the
 *                   caller wants to update the instance 'path'.'index'.
 *
 * If the object is an ONU instance, check if the instance has private data. If
 * it doesn't, attach private data, and call update_enable() for that instance.
 * If the ONU instance has no private data yet, this process did not check yet
 * the persistent data to find out if the ONU instance should be enabled.
 *
 * @return 0 on success, else -1.
 */
int dm_change_object(const amxc_var_t* const args) {

    int rc = -1;

    SAH_TRACEZ_DEBUG2(ME, "called");

    amxd_dm_t* const dm = xpon_mngr_get_dm();
    when_null(dm, exit);

    dm_action_info_t info;
    if(!process_args_common(args, &info, CHANGE_OBJ_ARGS_REQUIRED,
                            CHANGE_OBJ_N_ARGS_REQUIRED)) {
        goto exit;
    }

    amxc_string_t path;
    amxc_string_init(&path, 0);
    amxc_string_set(&path, info.path);

    SAH_TRACEZ_DEBUG(ME, "path='%s' index=%d", info.path, info.index);

    if(info.index) {
        /* Assume caller wants to update 'path'.'index' */
        amxc_string_appendf(&path, ".%d", info.index);
    }
    const char* const path_cstr = amxc_string_get(&path, 0);

    amxd_object_t* object = amxd_dm_findf(dm, "%s", path_cstr);
    if(!object) {
        SAH_TRACEZ_WARNING(ME, "%s does not exist: ignore object-changed", path_cstr);
        amxc_string_clean(&path);
        rc = 0;
        goto exit;
    }

    amxd_trans_t transaction;
    amxd_trans_init(&transaction);
    amxd_trans_set_attr(&transaction, amxd_tattr_change_ro, true);

    amxd_status_t status = amxd_trans_select_object(&transaction, object);
    when_failed_trace(status, exit_cleanup, ERROR,
                      "Failed to select %s for transaction (status=%d)",
                      info.path, status);

    add_params_to_transaction(&transaction, info.params, info.obj_id);

    if(info.obj_id == obj_id_onu) {
        if(object->priv == NULL) {
            SAH_TRACEZ_DEBUG(ME, "%s has no private data", path_cstr);
            onu_priv_attach_private_data(object);
            update_enable(&transaction, info.path, info.index);
        } else {
            SAH_TRACEZ_DEBUG(ME, "%s already has private data", path_cstr);
        }
    }

    status = amxd_trans_apply(&transaction, dm);
    when_failed_trace(status, exit_cleanup, ERROR, "Failed to update %s (status=%d)",
                      info.path, status);

    rc = 0;

exit_cleanup:
    amxc_string_clean(&path);
    amxd_trans_clean(&transaction);
exit:
    return rc;
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
 * the function calls 'dm_add_instance()', else it calls 'dm_change_object()'.
 *
 * More info about the parameter @a args:
 * The path in @a args must be the template path, e.g.
 * "XPON.ONU.1.SoftwareImage".
 *
 * If the function finds out the instance does not exist yet, it demands the
 * same from @a args as dm_instance_added(): it must be an htable with the keys
 * 'path', 'index' and 'keys'.
 *
 * If the function finds out the instance already exists, it demands the same
 * from @a args as dm_object_changed(): it must be an htable with the keys
 * 'path' and 'parameters'. If dm_object_changed() sees that @a args has a value
 * for 'index', it assumes the caller wants to change path.index, not path.
 *
 * Hence when calling this function, it's recommended @a args is htable with
 * the keys 'path', 'index', 'keys' and 'parameters'. Then the function finds
 * the required keys, independent from whether the instance already exists or
 * not.
 *
 * @return 0 on success, else -1.
 */
int dm_add_or_change_instance_impl(const amxc_var_t* const args) {

    int rc = -1;

    SAH_TRACEZ_DEBUG2(ME, "called");

    amxd_dm_t* const dm = xpon_mngr_get_dm();
    when_null(dm, exit);

    dm_action_info_t info;
    if(!process_args_common(args, &info, ADD_OR_CHANGE_INST_ARGS_REQUIRED,
                            ADD_OR_CHANGE_INST_N_ARGS_REQUIRED)) {
        goto exit;
    }

    if(dm_does_instance_exist(info.path, info.index)) {
        SAH_TRACEZ_DEBUG(ME, "Change %s.%d", info.path, info.index);
        return dm_change_object(args);
    } else {
        SAH_TRACEZ_DEBUG(ME, "Add %s.%d", info.path, info.index);
        return dm_add_instance(args);
    }

exit:
    return rc;
}

/**
 * Remove all instances from a template object.
 *
 * @param[in] templ  template object to remove all instances from
 *
 * Example: @a template can refer to "XPON.ONU.1.ANI.1.TC.GEM.Port".
 *
 * @return true on success, else false
 */
static bool remove_all_instances(amxd_object_t* templ) {

    bool rv = false;

    when_null(templ, exit_no_cleanup);

    amxd_dm_t* const dm = xpon_mngr_get_dm();
    when_null(dm, exit_no_cleanup);

    if(amxd_object_get_type(templ) != amxd_object_template) {
        SAH_TRACEZ_ERROR(ME, "Parameter 'templ' is not a template");
        goto exit_no_cleanup;
    }

    const uint32_t nr_instances = amxd_object_get_instance_count(templ);
    if(0 == nr_instances) {
        SAH_TRACEZ_DEBUG(ME, "No instances");
        rv = true;
        goto exit_no_cleanup;
    }

    amxd_trans_t transaction;
    amxd_trans_init(&transaction);
    amxd_trans_set_attr(&transaction, amxd_tattr_change_ro, true);

    int rc = amxd_trans_select_object(&transaction, templ);
    when_failed_trace(rc, exit, ERROR, "Failed to select template object");

    uint32_t index;
    amxd_object_t* obj_inst = NULL;

    amxd_object_iterate(instance, it, templ) {
        obj_inst = amxc_container_of(it, amxd_object_t, it);
        index = amxd_object_get_index(obj_inst);
        rc = amxd_trans_del_inst(&transaction, index, NULL);
        when_failed_trace(rc, exit, ERROR, "Failed to add deletion to transaction");
    }

    rc = amxd_trans_apply(&transaction, dm);
    when_failed_trace(rc, exit, ERROR, "Failed to apply transaction (rc=%d)", rc);

    rv = true;

exit:
    amxd_trans_clean(&transaction);

exit_no_cleanup:
    return rv;
}

/**
 * For all EthernetUNI instances of the ONU, set Status to Down if it is Up now.
 */
static void set_ethernet_uni_status_to_down(const amxd_object_t* const onu_instance,
                                            const char* const path) {

    const amxd_object_t* ethernet_uni = amxd_object_get_child(onu_instance, "EthernetUNI");
    when_null_trace(ethernet_uni, exit, ERROR, "%s.EthernetUNI does not exist", path);

    char* current_status_cstr;
    amxd_object_iterate(instance, it, ethernet_uni) {
        amxd_object_t* const eth_uni_inst = amxc_container_of(it, amxd_object_t, it);
        if(NULL == eth_uni_inst) {
            SAH_TRACEZ_ERROR(ME, "Failed to get EthernetUNI object");
            continue;
        }
        current_status_cstr = amxd_object_get_value(cstring_t, eth_uni_inst, "Status", NULL);
        if(current_status_cstr) {
            if(strncmp(current_status_cstr, "Up", 2) == 0) {
                amxd_object_set_value(cstring_t, eth_uni_inst, "Status", "Down");
            }
            free(current_status_cstr);
        }
    }
exit:
    return;
}

/**
 * Handle an OMCI reset MIB message.
 *
 * @param[in] args : must be htable with the key 'index'. The 'index' must
 *                   indicate for which XPON.ONU instance an OMCI reset MIB is
 *                   received
 *
 * The function does the following:
 * - For all EthernetUNI instances of the ONU, set Status to Down if it is Up now.
 * - For all ANI instances of the ONU, remove all TC.GEM.Port instances.
 *
 * @return 0 on success, else -1
 */
int dm_omci_reset_mib(const amxc_var_t* const args) {

    int rc = -1;

    SAH_TRACEZ_DEBUG2(ME, "called");

    when_null_trace(args, exit, ERROR, "args is NULL");

    amxd_dm_t* const dm = xpon_mngr_get_dm();
    when_null(dm, exit);

    const uint32_t index = GET_UINT32(args, "index");
    when_false_trace(index != 0, exit, ERROR, "Failed to extract valid index from args");

    char path[16];
    snprintf(path, 16, "XPON.ONU.%d", index);
    const amxd_object_t* onu_instance = amxd_dm_findf(dm, "%s", path);
    if(!onu_instance) {
        SAH_TRACEZ_WARNING(ME, "%s does not exist: ignore omci:reset-mib", path);
        rc = 0;
        goto exit;
    }

    set_ethernet_uni_status_to_down(onu_instance, path);

    /* Remove GEM ports */
    const amxd_object_t* ani = amxd_object_get_child(onu_instance, "ANI");
    if(ani) {
        amxd_object_iterate(instance, it, ani) {
            amxd_object_t* const ani_inst = amxc_container_of(it, amxd_object_t, it);
            amxd_object_t* const port_templ = amxd_object_findf(ani_inst, "TC.GEM.Port");
            if(!port_templ) {
                SAH_TRACEZ_ERROR(ME, "%s: failed to find TC.GEM.Port for ANI.%d",
                                 path, amxd_object_get_index(ani_inst));
                continue;
            }
            if(!remove_all_instances(port_templ)) {
                SAH_TRACEZ_ERROR(ME, "%s: failed to delete GEM ports for ANI.%d",
                                 path, amxd_object_get_index(ani_inst));
            }
        }
    } else {
        SAH_TRACEZ_ERROR(ME, "%s.ANI does not exist", path);
    }

    rc = 0;
exit:
    return rc;
}

/**
 * Update a parameter of the XPON object.
 *
 * @param[in] args: must be htable with the keys 'name' and 'value'.
 *
 * @return 0 on success, else -1
 */
int dm_set_xpon_parameter_impl(const amxc_var_t* const args) {
    int rc = -1;
    amxd_object_t* const xpon = dm_get_xpon_object();
    when_null(xpon, exit);

    const amxc_htable_t* const htable = amxc_var_constcast(amxc_htable_t, args);
    when_null_trace(htable, exit, ERROR, "args is not an htable");

    const char* const name = GET_CHAR(args, "name");
    when_null_trace(name, exit, ERROR, "Failed to get name");

    if(strncmp(name, "FsmState", 8) != 0) {
        SAH_TRACEZ_WARNING(ME, "dm_set_xpon_parameter() only supports XPON.FsmState");
        goto exit;
    }

    amxc_var_t* const value = GET_ARG(args, "value");
    when_null_trace(value, exit, ERROR, "Failed to get value");

    const amxd_status_t status = amxd_object_set_param(xpon, name, value);
    when_true_trace(status != amxd_status_ok, exit, ERROR,
                    "Failed to update %s: status=%d", name, status);
    rc = 0;
exit:
    return rc;
}

/**
 * Return true if a certain instance of a template object exists.
 *
 * @param[in] path   object path, e.g., "XPON.ONU"
 * @param[in] index  the instance index, e.g., 1
 *
 * @return true if instance exists, else false
 */
bool dm_does_instance_exist(const char* const path, uint32_t index) {

    bool rv = false;
    amxd_dm_t* const dm = xpon_mngr_get_dm();
    when_null(dm, exit);

    const amxd_object_t* const templ = amxd_dm_findf(dm, "%s", path);
    if(!templ) {
        goto exit;
    }

    if(amxd_object_get_instance(templ, NULL, index)) {
        rv = true;
    }

exit:
    return rv;
}

/**
 * Return the number of instances of a template object.
 *
 * @param[in] path  path of a template object, e.g., "XPON.ONU" or
 *                  "XPON.ONU.1.ANI"
 */
static uint32_t get_nr_of_instances(const char* const path) {

    uint32_t result = 0;

    amxd_dm_t* const dm = xpon_mngr_get_dm();
    when_null(dm, exit);

    const amxd_object_t* const object = amxd_dm_findf(dm, "%s", path);
    when_null_trace(object, exit, ERROR, "%s not found", path);

    result = amxd_object_get_instance_count(object);

exit:
    return result;
}

/**
 * Return the number of EthernetUNI instances for a certain XPON.ONU instance.
 *
 * @param[in] path  path of a certain XPON.ONU instance, e.g. "XPON.ONU.1"
 */
uint32_t dm_get_nr_of_ethernet_uni_instances(const char* const onu_path) {
    char path[64];
    snprintf(path, 64, "%s.EthernetUNI", onu_path);
    return get_nr_of_instances(path);
}

/**
 * Return the number of ANI instances for a certain XPON.ONU instance.
 *
 * @param[in] path  path of a certain XPON.ONU instance, e.g. "XPON.ONU.1"
 */
uint32_t dm_get_nr_of_ani_instances(const char* const onu_path) {
    char path[64];
    snprintf(path, 64, "%s.ANI", onu_path);
    return get_nr_of_instances(path);
}

bool dm_get_param(const char* path, const char* name, amxc_var_t* resp) {
    SAH_TRACEZ_DEBUG(ME, "get value of dm parameter path=%s.%s", path, name);

    amxd_dm_t* const dm = xpon_mngr_get_dm();
    when_null(dm, exit);

    amxd_object_t* const param_object = amxd_dm_findf(dm, "%s", path);
    if(!param_object) {
        SAH_TRACEZ_WARNING(ME, "%s does not exist!", path);
        goto exit;
    }

    if(amxd_status_ok != amxd_object_get_param(param_object, name, resp)) {
        SAH_TRACEZ_WARNING(ME, "Failed to get param from object: %s.%s", path, name);
        goto exit;
    }

    return true;
exit:
    return false;
}

/**
 * Find out if password for an ANI instance is in hex format or not.
 *
 * @param[in] ani_path: path to ANI instance, e.g. "XPON.ONU.1.ANI.1"
 * @param[in,out] is_hex: function sets this to true if the parameter
 *                        'HexadecimalPassword' of the ANI instance referred
 *                        to by @a ani_path is true
 *
 * @return true on success, else false
 */
bool dm_is_hex_password(const char* const ani_path, bool* is_hex) {

    bool rv = false;
    amxc_var_t is_hex_variant;
    amxc_string_t ani_auth_path;
    amxc_var_init(&is_hex_variant);
    amxc_string_init(&ani_auth_path, 0);

    amxd_dm_t* const dm = xpon_mngr_get_dm();
    when_null(dm, exit);
    when_null(is_hex, exit);

    ani_append_tc_authentication(ani_path, &ani_auth_path);
    const char* const ani_auth_path_cstr = amxc_string_get(&ani_auth_path, 0);

    amxd_object_t* const object = amxd_dm_findf(dm, "%s", ani_auth_path_cstr);
    when_null_trace(object, exit, WARNING, "%s does not exist",
                    ani_auth_path_cstr);

    if(amxd_status_ok != amxd_object_get_param(object, "HexadecimalPassword",
                                               &is_hex_variant)) {
        SAH_TRACEZ_ERROR(ME, "Failed to get %s.HexadecimalPassword",
                         ani_auth_path_cstr);
        goto exit;
    }

    *is_hex = amxc_var_constcast(bool, &is_hex_variant);
    rv = true;

exit:
    amxc_var_clean(&is_hex_variant);
    amxc_string_clean(&ani_auth_path);
    return rv;
}

/**
 * Find out the PON Mode of an ANI.
 *
 * @param[in] ani_path: path to ANI instance, e.g. "XPON.ONU.1.ANI.1"
 * @param[in,out] pon_mode: function sets this to the PON mode of the ANI
 *     referred to by @a ani_path
 *
 * @return true on success, else false
 */
bool dm_get_ani_pon_mode(const char* const ani_path, pon_mode_t* pon_mode) {

    bool rv = false;
    amxc_var_t pon_mode_var;
    amxc_var_init(&pon_mode_var);

    amxd_dm_t* const dm = xpon_mngr_get_dm();
    when_null(dm, exit);
    when_null(pon_mode, exit);

    amxd_object_t* const object = amxd_dm_findf(dm, "%s", ani_path);
    when_null_trace(object, exit, WARNING, "%s does not exist", ani_path);

    if(amxd_status_ok != amxd_object_get_param(object, "PONMode",
                                               &pon_mode_var)) {
        SAH_TRACEZ_ERROR(ME, "Failed to get %s.PONMode", ani_path);
        goto exit;
    }

    const char* const pon_mode_cstr = amxc_var_constcast(cstring_t, &pon_mode_var);
    if(strncmp(pon_mode_cstr, "Unknown", 7) == 0) {
        *pon_mode = pon_mode_unknown;
    } else if(strncmp(pon_mode_cstr, "G-PON", 5) == 0) {
        *pon_mode = pon_mode_gpon;
    } else if(strncmp(pon_mode_cstr, "XG-PON", 6) == 0) {
        *pon_mode = pon_mode_xg_pon;
    } else if(strncmp(pon_mode_cstr, "NG-PON2", 7) == 0) {
        *pon_mode = pon_mode_ng_pon2;
    } else if(strncmp(pon_mode_cstr, "XGS-PON", 7) == 0) {
        *pon_mode = pon_mode_xgs_pon;
    } else {
        SAH_TRACEZ_ERROR(ME, "Invalid PON mode: %s", pon_mode_cstr);
        *pon_mode = pon_mode_unknown;
        goto exit;
    }

    rv = true;

exit:
    amxc_var_clean(&pon_mode_var);
    return rv;
}


