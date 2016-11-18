/*
 *  Copyright (C) 2012-2016 Skylable Ltd. <info-copyright@skylable.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef __SX_H
#define __SX_H

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define SXLIMIT_MAX_FILENAME_LEN 1024
#define SXF_ABI_VERSION	9

typedef enum {
    SXC_INPUT_PLAIN,	    /* Plain text input */
    SXC_INPUT_SENSITIVE,    /* Sensitive input (password, etc.) */
    SXC_INPUT_YN	    /* Y/N question */
} sxc_input_t;

typedef struct {
    uint64_t size;
} sxc_file_t;

uint64_t sxc_file_get_size(const sxc_file_t *file);
int sxi_file_set_size(sxc_file_t *file, uint64_t size);

/** Defines a filter's type
 * This is used to prioritize filters, for example
 * an encryption filter must always be run last on upload.
 */
typedef enum {
    SXF_TYPE_NONE = 0,
    SXF_TYPE_COMPRESS,/**< compression filter */
    SXF_TYPE_CRYPT,/**< encryption filter */
    SXF_TYPE_GENERIC /**< generic filter */
} sxf_type_t;

/** Defines the direction of the transfer
 */
typedef enum {
    SXF_MODE_UPLOAD = 0,/**< file upload */
    SXF_MODE_DOWNLOAD,/**< file download */
    SXF_MODE_RCOPY, /**< remote-to-remote copy (fast mode) */
    SXF_MODE_DELETE /**< file delete */
} sxf_mode_t;

/** EOF and looping control
 */
typedef enum {
    SXF_ACTION_NORMAL = 0,/**< first time a new block is processed */
    SXF_ACTION_REPEAT,/**< repeat call with same 'in' and 'insize' parameters */
    SXF_ACTION_DATA_END/**< marks the file's last block */
} sxf_action_t;

typedef enum {
    SXF_FILEMETA_LOCAL = 0,/**< file meta is local */
    SXF_FILEMETA_REMOTE /**< file meta is remote */
} sxf_filemeta_type_t;

typedef void sxf_handle_t;
typedef void sxc_meta_t;

typedef struct {
    int abi_version;
    const char *shortname;
    const char *shortdesc;
    const char *summary;
    const char *options;
    const char *uuid;
    sxf_type_t type;
    int version[2];
    int (*init)(const sxf_handle_t *handle, void **ctx);
    int (*shutdown)(const sxf_handle_t *handle, void *ctx);
    int (*configure)(const sxf_handle_t *handle, const char *cfgstr, const char *cfgdir, void **cfgdata, unsigned int *cfgdata_len);
    int (*data_prepare)(const sxf_handle_t *handle, void **ctx, const char *filename, const char *cfgdir, const void *cfgdata, unsigned int cfgdata_len, sxc_meta_t *custom_meta, sxf_mode_t mode);
    ssize_t (*data_process)(const sxf_handle_t *handle, void *ctx, const void *in, size_t insize, void *out, size_t outsize, sxf_mode_t mode, sxf_action_t *action);
    int (*data_finish)(const sxf_handle_t *handle, void **ctx, sxf_mode_t mode);
    int (*file_process)(const sxf_handle_t *handle, void *ctx, const char *filename, sxc_meta_t *meta, const char *cfgdir, const void *cfgdata, unsigned int cfgdata_len, sxf_mode_t mode);
    void (*file_notify)(const sxf_handle_t *handle, void *ctx, const void *cfgdata, unsigned int cfgdata_len, sxf_mode_t mode, const char *source_cluster, const char *source_volume, const char *source_path, const char *dest_cluster, const char *dest_volume, const char *dest_path);
    int (*file_update)(const sxf_handle_t *handle, void *ctx, const void *cfgdata, unsigned int cfgdata_len, sxf_mode_t mode, sxc_file_t *source, sxc_file_t *dest, int recursive);
    int (*filemeta_process)(const sxf_handle_t *handle, void **ctx, const char *cfgdir, const void *cfgdata, unsigned int cfgdata_len, sxc_file_t *file, sxf_filemeta_type_t filemeta_type, const char *filename, char **new_filename, sxc_meta_t *file_meta, sxc_meta_t *custom_volume_meta);
    const char *tname;
} sxc_filter_t;

enum sxc_log_level {
    SX_LOG_ALERT=1,
    SX_LOG_CRIT,
    SX_LOG_ERR,
    SX_LOG_WARNING,
    SX_LOG_NOTICE,
    SX_LOG_INFO,
    SX_LOG_DEBUG
};

int sxc_filter_msg(const sxf_handle_t *handle, int level, const char *format, ...);
int sxc_filter_get_input(const sxf_handle_t *h, sxc_input_t type, const char *prompt, const char *def, char *in, unsigned int insize);
int sxc_meta_getval(sxc_meta_t *meta, const char *key, const void **value, unsigned int *value_len);
int sxc_meta_setval(sxc_meta_t *meta, const char *key, const void *value, unsigned int value_len);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
