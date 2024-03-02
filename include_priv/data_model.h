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

#ifndef __data_model_h__
#define __data_model_h__

/**
 * @file data_model.h
 *
 * Functionality related to accessing (set/get) the TR-181 XPON DM.
 */

/* System headers */
#include <stdbool.h>
#include <stddef.h>  /* size_t */

/* Other libraries' headers */
#include <amxc/amxc_variant.h> /* amxc_var_t */

/* Own headers */
#include "pon_mode.h" /* pon_mode_t */

void dm_set_vendor_module(const char* name);
void dm_set_module_error(void);

int dm_add_instance(const amxc_var_t* const args);
int dm_remove_instance(const amxc_var_t* const args);
int dm_change_object(const amxc_var_t* const args);
int dm_add_or_change_instance_impl(const amxc_var_t* const args);
int dm_omci_reset_mib(const amxc_var_t* const args);
int dm_set_xpon_parameter_impl(const amxc_var_t* const args);

bool dm_does_instance_exist(const char* path, uint32_t index);
uint32_t dm_get_nr_of_ethernet_uni_instances(const char* const onu_path);
uint32_t dm_get_nr_of_ani_instances(const char* const onu_path);

bool dm_get_param(const char* path, const char* name, amxc_var_t* resp);
bool dm_is_hex_password(const char* const ani_path, bool* is_hex);
bool dm_get_ani_pon_mode(const char* const ani_path, pon_mode_t* pon_mode);

#endif
