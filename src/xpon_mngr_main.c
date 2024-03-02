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

#include "dm_info.h"             /* dm_info_init() */
#include "dm_xpon_mngr.h"
#include "module_mgmt.h"         /* mod_module_mgmt_init() */
#include "persistency.h"         /* persistency_init() */
#include "pon_ctrl.h"            /* pon_ctrl_init() */
#include "populate_dm_startup.h" /* pplt_dm_init() */
#include "restore_to_hal.h"      /* rth_init() */
#include "upgrade_persistency.h" /* upgr_persistency_init() */
#include "xpon_trace.h"

typedef struct _xpon_mngr {
    amxd_dm_t* dm;
    amxo_parser_t* parser;
} xpon_mngr_t;


static xpon_mngr_t s_app;

int _xpon_mngr_main(int reason,
                    amxd_dm_t* dm,
                    amxo_parser_t* parser);

amxd_dm_t* PRIVATE xpon_mngr_get_dm(void) {
    return s_app.dm;
}

amxo_parser_t* PRIVATE xpon_mngr_get_parser(void) {
    return s_app.parser;
}

static void do_cleanup(void) {
    pplt_dm_cleanup();
    rth_cleanup();
    mod_module_mgmt_cleanup();
    persistency_cleanup();
    upgr_persistency_cleanup();
}

int _xpon_mngr_main(int reason,
                    amxd_dm_t* dm,
                    amxo_parser_t* parser) {

    SAH_TRACEZ_INFO(ME, "reason=%d", reason);

    bool success = false;
    int rc = 1; /* error */
    bool module_error = false;

    switch(reason) {
    case 0:     // START
        s_app.dm = dm;
        s_app.parser = parser;

        if(!dm_info_init()) {
            return -1;
        }
        persistency_init();
        upgr_persistency_init();
        rth_init();
        if(!mod_module_mgmt_init(&module_error)) {
            if(module_error) {
                /**
                 * The function mod_module_mgmt_init() should have set
                 * XPON.ModuleError to true. Keep running to show this to the
                 * user.
                 */
                rc = 0;
            }
            break;
        }
        pon_ctrl_init();
        if(!pplt_dm_init()) {
            break;
        }
        success = true;
        rc = 0;
        break;
    case 1:     // STOP
        do_cleanup();
        s_app.dm = NULL;
        s_app.parser = NULL;
        success = true;
        rc = 0;
        break;
    }
    if(!success) {
        do_cleanup();
    }

    return rc;
}
