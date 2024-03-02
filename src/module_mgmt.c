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
#include "module_mgmt.h"

/* System headers */
#include <dlfcn.h>  /* dlerror() */
#include <stdlib.h> /* free() */
#include <unistd.h> /* access() */

/* Other libraries' headers */
#include <amxc/amxc_macros.h>
#include <amxc/amxc.h>
#include <amxm/amxm.h>
#include <amxp/amxp_dir.h> /* amxp_dir_scan() */

/* Own headers */
#include "data_model.h"    /* dm_get_vendor_module() */
#include "pon_stat.h"      /* dm_instance_added() */
#include "pon_cfg.h"       /* pon_cfg_get_param_value() */
#include "xpon_trace.h"

#define MOD_PON_STAT "pon_stat"
#define MOD_PON_CFG "pon_cfg"

static const char* const MODULES_PATH = "/usr/lib/amx/tr181-xpon/modules/";

static amxm_module_t* s_pon_stat_module = NULL;
static amxm_module_t* s_pon_cfg_module = NULL;
static amxc_string_t s_module_name;              /* name of vendor module */
static amxm_shared_object_t* s_module_so = NULL; /* vendor module loaded */

typedef struct _pon_stat_function {
    const char* name;
    amxm_callback_t impl;
} pon_stat_function_t;

static const pon_stat_function_t PON_STAT_FUNCTIONS[] = {

    { .name = "dm_instance_added", .impl = dm_instance_added   },
    { .name = "dm_instance_removed", .impl = dm_instance_removed },
    { .name = "dm_object_changed", .impl = dm_object_changed   },
    { .name = "dm_add_or_change_instance", .impl = dm_add_or_change_instance },
    { .name = "omci_reset_mib", .impl = omci_reset_mib      },
    { .name = "watch_file_descriptor_start", .impl = watch_file_descriptor_start },
    { .name = "watch_file_descriptor_stop", .impl = watch_file_descriptor_stop },
    { .name = "dm_set_xpon_parameter", .impl = dm_set_xpon_parameter },
    { .name = NULL, .impl = NULL } /* sentinel */
};

static const pon_stat_function_t PON_CFG_FUNCTIONS[] = {

    { .name = "pon_cfg_get_param_value", .impl = pon_cfg_get_param_value },
    { .name = NULL, .impl = NULL } /* sentinel */
};

static bool register_pon_stat_module(void) {

    bool rv = false;
    amxm_shared_object_t* so = amxm_get_so("self");
    if(!so) {
        SAH_TRACEZ_ERROR(ME, "Failed to get amxm_shared_object_t for self");
        goto exit;
    }

    if(amxm_module_register(&s_pon_stat_module, so, MOD_PON_STAT)) {
        SAH_TRACEZ_ERROR(ME, "Failed to register %s module namespace", MOD_PON_STAT);
        goto exit;
    }

    int i;
    for(i = 0; PON_STAT_FUNCTIONS[i].name != NULL; ++i) {
        if(amxm_module_add_function(s_pon_stat_module, PON_STAT_FUNCTIONS[i].name,
                                    PON_STAT_FUNCTIONS[i].impl)) {
            SAH_TRACEZ_ERROR(ME, "Failed to register self.%s.%s()", MOD_PON_STAT,
                             PON_STAT_FUNCTIONS[i].name);
        }
    }
    rv = true;

exit:
    return rv;
}

static bool register_pon_cfg_module(void) {

    bool rv = false;
    amxm_shared_object_t* so = amxm_get_so("self");
    if(!so) {
        SAH_TRACEZ_ERROR(ME, "Failed to get amxm_shared_object_t for self");
        goto exit;
    }

    if(amxm_module_register(&s_pon_cfg_module, so, MOD_PON_CFG)) {
        SAH_TRACEZ_ERROR(ME, "Failed to register %s module namespace", MOD_PON_CFG);
        goto exit;
    }

    int i;
    for(i = 0; PON_CFG_FUNCTIONS[i].name != NULL; ++i) {
        if(amxm_module_add_function(s_pon_cfg_module, PON_CFG_FUNCTIONS[i].name,
                                    PON_CFG_FUNCTIONS[i].impl)) {
            SAH_TRACEZ_ERROR(ME, "Failed to register self.%s.%s()", MOD_PON_CFG,
                             PON_CFG_FUNCTIONS[i].name);
        }
    }
    rv = true;

exit:
    return rv;
}


/**
 * Handle a module found by amxp_dir_scan() in find_vendor_module().
 *
 * @param[in] name: path to the vendor module, e.g.,
 *                  "/usr/lib/amx/tr181-xpon/modules/mod-xpon-prpl.so"
 *
 * @return 0 on success: amxp_dir_scan() should continue scanning
 * @return 1 on error: amxp_dir_scan() should stop scanning
 */
static int handle_vendor_module_match(const char* name, void* priv UNUSED) {
    SAH_TRACEZ_DEBUG(ME, "match: '%s'", name);

    int rc = 0; /* keep scanning */

    amxc_string_t module;
    amxc_string_init(&module, 0);
    amxc_string_setf(&module, "%s", name);
    amxc_string_replace(&module, MODULES_PATH, "", /*max= */ 1);
    amxc_string_replace(&module, ".so", "", /*max= */ 1);
    const char* const module_cstr = amxc_string_get(&module, 0);

    if(amxc_string_is_empty(&s_module_name)) {
        amxc_string_copy(&s_module_name, &module);
        SAH_TRACEZ_DEBUG(ME, "Found vendor module: '%s.so'", module_cstr);
        dm_set_vendor_module(module_cstr);
    } else {
        SAH_TRACEZ_ERROR(ME, "Found more than one vendor module");
        SAH_TRACEZ_ERROR(ME, "  modules found: %s.so, %s.so",
                         amxc_string_get(&s_module_name, 0), module_cstr);
        rc = 1; /* error, stop scanning */
    }

    amxc_string_clean(&module);
    return rc;
}

/**
 * Find vendor module in MODULES_PATH and assign its name to s_module_name.
 */
static bool find_vendor_module(void) {

    const char* const filter = "d_type == DT_REG && d_name matches 'mod-xpon-.*\\.so'";

    if(amxp_dir_scan(MODULES_PATH, filter, false, handle_vendor_module_match, NULL) != 0) {
        /* handle_vendor_module_match() already logged error */
        return false;
    }

    if(amxc_string_is_empty(&s_module_name)) {
        SAH_TRACEZ_ERROR(ME, "Failed to find vendor module");
        return false;
    }
    return true;
}

/**
 * Find module with pattern 'mod-xpon-*.so' in MODULES_PATH and load it.
 *
 * If the function finds a module:
 * - set s_module_name to the name of the module found, e.g. 'mod-xpon-prpl' if
 *   the function found 'mod-xpon-prpl.so' as vendor module in MODULES_PATH.
 * - set XPON.ModuleName in DM to the same name.
 *
 * Set XPON.ModuleError to true if the function encounters one of the following
 * errors:
 * - It fails to find a vendor module.
 * - It finds more than one vendor module. An image must only install 1 vendor
 *   module.
 * - It fails to load the module.
 *
 * @return true on success, else false
 */
static bool load_vendor_module(void) {

    bool rv = false;

    amxc_string_t mod_path;
    amxc_string_init(&mod_path, 0);

    if(!find_vendor_module()) {
        goto exit;
    }
    const char* const module = amxc_string_get(&s_module_name, 0);
    amxc_string_setf(&mod_path, "%s%s.so", MODULES_PATH, module);

    const char* const path_to_so = amxc_string_get(&mod_path, /*offset=*/ 0);

    if(access(path_to_so, F_OK) != 0) {
        SAH_TRACEZ_ERROR(ME, "%s does not exist", path_to_so);
        goto exit;
    }

    if(amxm_so_open(&s_module_so, module, path_to_so)) {
        const char* error = dlerror();
        SAH_TRACEZ_ERROR(ME, "Failed to load %s module", module);
        SAH_TRACEZ_ERROR(ME, "dlerror(): '%s'", error ? error : "No error");
        goto exit;
    }

    rv = true;

exit:
    if(!rv) {
        dm_set_module_error();
    }
    amxc_string_clean(&mod_path);
    return rv;
}

/**
 * Initialize the module_mgmt part of this plugin.
 *
 * The function:
 * - registers the namespaces 'pon_stat' and 'pon_cfg'. The namespaces have
 *   functions which can be called by the vendor module.
 * - loads the vendor module.
 *
 * The plugin must call this function once at startup.
 *
 * @param[in,out] module_error: the function sets it to true if it encounters
 *                              an error regarding (the loading of) the vendor
 *                              module
 *
 * @return true on success, else false
 */
bool mod_module_mgmt_init(bool* module_error) {

    amxc_string_init(&s_module_name, 0);

    if(!register_pon_stat_module()) {
        return false;
    }

    if(!register_pon_cfg_module()) {
        return false;
    }

    if(!load_vendor_module()) {
        if(module_error) {
            *module_error = true;
        }
        return false;
    }
    return true;
}

/**
 * Return the name of the vendor module the plugin loaded at startup.
 *
 * @return the name of that vendor module upon success
 * @return NULL if the plugin failed to load the vendor module at startup
 */
const char* mod_get_vendor_module_loaded(void) {

    return s_module_so ? amxc_string_get(&s_module_name, 0) : NULL;
}

/**
 * Clean up the module_mgmt part of this plugin.
 *
 * The function does the reverse of mod_module_mgmt_init():
 * - unload the vendor module
 * - unregisters the namespaces 'pon_stat' and 'pon_cfg'
 *
 * The plugin must call this function once when stopping.
 */
void mod_module_mgmt_cleanup(void) {

    if(s_module_so) {
        if(amxm_so_close(&s_module_so)) {
            SAH_TRACEZ_ERROR(ME, "Failed to close module");
        }
    }

    if(s_pon_stat_module) {
        amxm_module_deregister(&s_pon_stat_module);
    }
    if(s_pon_cfg_module) {
        amxm_module_deregister(&s_pon_cfg_module);
    }
    amxc_string_clean(&s_module_name);
}
