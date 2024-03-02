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
#include "file_utils.h"

/**
 * To avoid following error:
 * file_utils.c:25:8: error: implicit declaration of function ‘fdatasync’ [-Werror=implicit-function-declaration]
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* System headers */
#include <errno.h>
#include <fcntl.h>  /* open() */
#include <stdio.h>  /* rename() */
#include <string.h> /* strerror() */
#include <unistd.h> /* fdatasync() */

/* Own headers */
#include "xpon_trace.h"

#define LINE_LEN 256

static bool rename_file(const char* const oldpath, const char* const newpath) {
    int fd = 0;
    if(rename(oldpath, newpath) == -1) {
        SAH_TRACEZ_ERROR(ME, "Failed to rename %s to %s: %s", oldpath, newpath,
                         strerror(errno));
        return false;
    }
    if((fd = open(newpath, O_RDWR)) == -1) {
        SAH_TRACEZ_ERROR(ME, "Failed to open %s: %s", newpath, strerror(errno));
        return false;
    }
    if(fdatasync(fd) == -1) {
        SAH_TRACEZ_ERROR(ME, "Failed to sync %s: %s", newpath, strerror(errno));
    }
    close(fd);
    return true;
}

/**
 * Set @a has_value to true if file @a path has value @a value as contents.
 *
 * @param[in] path: file path
 * @param[in] value: function must check if @a path has @a value as contents
 * @param[in,out] has_value: function sets this parameter to true if @a path
 *                           has @a value as contents
 *
 * @return true on success, else false
 */
static bool check_file_has_value(const char* const path, const char* const value,
                                 bool* has_value) {

    char line[LINE_LEN] = { 0 };
    if(!read_first_line_from_file(path, line, LINE_LEN)) {
        return false;
    }
    SAH_TRACEZ_DEBUG2(ME, "line='%s' (%zd) value='%s' (%zd)", line, strlen(line),
                      value, strlen(value));

    const size_t line_len = strlen(line);
    const size_t value_len = strlen(value);
    if(line_len != value_len) {
        *has_value = false;
        return true;
    }

    if(strncmp(line, value, strlen(value)) == 0) {
        SAH_TRACEZ_DEBUG2(ME, "file already has %s", value);
        *has_value = true;
    } else {
        SAH_TRACEZ_DEBUG2(ME, "line='%s' != value='%s'", line, value);
    }
    return true;
}

/**
 * Write the string @a value to the file @a path.
 *
 * @param[in] path: file path
 * @param[in] value: value to write to @a path
 *
 * If @a path already exists and has @a value as contents, the function
 * immediately returns true to avoid an unneeded write.
 *
 * The function first writes the value to the file @a path with ".tmp"
 * appended. Then it renames the file to @a path.
 *
 * @return true on success, else false
 */
bool write_file(const char* const path, const char* const value) {

    SAH_TRACEZ_DEBUG2(ME, "path='%s' value='%s'", path, value);

    const size_t value_len = strlen(value);
    if(value_len >= LINE_LEN) {
        SAH_TRACEZ_ERROR(ME, "strlen(value) = %zd >= %d", value_len, LINE_LEN);
        return false;
    }
    /**
     * Avoid writing the file if the file already exists and already has the
     * value of the parameter 'value' as contents.
     */
    if(access(path, F_OK) == 0) {
        SAH_TRACEZ_DEBUG2(ME, "%s already exists", path);
        bool has_value = false;
        if((check_file_has_value(path, value, &has_value) == true) && has_value) {
            return true;
        }
    }

    char tmpfile[256];
    if(snprintf(tmpfile, 256, "%s.tmp", path) < 0) {
        SAH_TRACEZ_ERROR(ME, "%s: failed to create path with .tmp", path);
        return false;
    }

    FILE* f = fopen(tmpfile, "w");
    if(!f) {
        SAH_TRACEZ_ERROR(ME, "Failed to open %s: %s", tmpfile, strerror(errno));
        return false;
    }

    bool rv = true;
    if(fputs(value, f) == EOF) {
        SAH_TRACEZ_ERROR(ME, "Failed to write to %s", tmpfile);
        rv = false;
    }

    if(fclose(f) == EOF) {
        SAH_TRACEZ_ERROR(ME, "Failed to close %s: %s", tmpfile, strerror(errno));
    }
    if(!rv) {
        return false;
    }
    return rename_file(tmpfile, path);
}

/**
 * Read the first line from a file into a buffer passed by caller.
 *
 * @param[in] path: file path
 * @param[in,out] line: function reads 1st line from @a path and puts it in this
 *                      parameter
 * @param[in] len: length of @a line
 *
 * @return true on success, else false
 */
bool read_first_line_from_file(const char* const path, char* line, size_t len) {
    FILE* f = fopen(path, "r");
    if(!f) {
        SAH_TRACEZ_ERROR(ME, "Failed to open %s: %s", path, strerror(errno));
        return false;
    }
    bool rv = true;
    if(!fgets(line, (int) len, f)) {
        SAH_TRACEZ_ERROR(ME, "Failed to read from %s", path);
        rv = false;
    }
    if(fclose(f) == EOF) {
        SAH_TRACEZ_ERROR(ME, "Failed to close %s: %s", path, strerror(errno));
    }
    return rv;
}

