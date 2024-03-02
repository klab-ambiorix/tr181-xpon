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

#ifndef __xpon_trace_h__
#define __xpon_trace_h__

/**
 * @file xpon_trace.h
 *
 * Tracing macros.
 */

/**
 * Trace zone name
 *
 * Unique string to easily grep on in log file, e.g.:
 * grep xpon /ext/messages
 */
#define ME "xpon"

#include <debug/sahtrace.h>
#include <debug/sahtrace_macros.h> /* when_null_trace(), ... */

// Patrick.R: I'm missing SAH_TRACE_DEBUG in sahtrace.h. For now define it here.
//   Can we add it to lib_sahtrace in the future ?

#define SAHTRACE_DEBUG                "", "d", "", ""

/**
 * Tell GCC to consider the rest of this header as a system header.
 *
 * Reason:
 * Without the pragma statement, the following compile error occurs for a statement like:
 * SAH_TRACEZ_DEBUG(ME, "called"):
 *
 * ```
 * error: ISO C99 requires at least one argument for the "..." in a variadic macro [-Werror]
 * 156 |     SAH_TRACEZ_DEBUG(ME, "called");
 * ```
 *
 * The error does not occur for 'SAH_TRACEZ_INFO(ME, "called");'. Reason: GCC treats
 * sahtrace.h as a system header. GCC checks system headers less strictly for conformance.
 * Use next pragma to tell GCC to treat this header in the same way as sahtrace.h and other
 * system headers.
 *
 * See:
 * https://gcc.gnu.org/onlinedocs/cpp/System-Headers.html
 */

#pragma GCC system_header

#if !defined(SAHTRACES_LEVEL) || (SAHTRACES_LEVEL >= 500)
// Set debug level at same level as callstack level.
#define TRACE_LEVEL_DEBUG1 TRACE_LEVEL_CALLSTACK
#define SAH_TRACE_DEBUG(format, ...) sahTrace(TRACE_LEVEL_DEBUG1, "%s%-7.7s%s - %s[%s]%s%s" format "%s - %s(%s@%s:%d)%s", SAHTRACE_ZONE(""), SAHTRACE_DEBUG, ## __VA_ARGS__, SAHTRACE_SOURCE)
#define SAH_TRACEZ_DEBUG(zone, format, ...) sahTraceZ(TRACE_LEVEL_DEBUG1, zone, "%s%-7.7s%s - %s[%s]%s%s" format "%s - %s(%s@%s:%d)%s", SAHTRACE_ZONE(zone), SAHTRACE_DEBUG, ## __VA_ARGS__, SAHTRACE_SOURCE)
#else
#error "Normal debugging should not be compiled away"
#endif

// DEBUG2: typically for debug msgs which can occur regularly.
#if defined(SAHTRACES_LEVEL) && (SAHTRACES_LEVEL >= 600)
#define TRACE_LEVEL_DEBUG2 600
#define SAH_TRACE_DEBUG2(format, ...) sahTrace(TRACE_LEVEL_DEBUG2, "%s%-7.7s%s - %s[%s]%s%s" format "%s - %s(%s@%s:%d)%s", SAHTRACE_ZONE(""), SAHTRACE_DEBUG, ## __VA_ARGS__, SAHTRACE_SOURCE)
#define SAH_TRACEZ_DEBUG2(zone, format, ...) sahTraceZ(TRACE_LEVEL_DEBUG2, zone, "%s%-7.7s%s - %s[%s]%s%s" format "%s - %s(%s@%s:%d)%s", SAHTRACE_ZONE(zone), SAHTRACE_DEBUG, ## __VA_ARGS__, SAHTRACE_SOURCE)
#else
#define SAH_TRACE_DEBUG2(format, ...)       SAH_TRACE_DO_NOTHING
#define SAH_TRACEZ_DEBUG2(zone, format, ...) SAH_TRACE_DO_NOTHING
#endif

#endif
