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

#ifndef _MISC_H
#define _MISC_H

#include "fake_sx.h"
#include <sys/types.h>

void sxi_bin2hex(const void *bin, unsigned int len, char *hex);
int sxi_hex2bin(const char *src, uint32_t src_len, uint8_t *dst, uint32_t dst_len);
int sxi_derive_key(const char *pass, const char *salt, unsigned salt_size, char *out, unsigned int len);
uint64_t sxi_swapu64(uint64_t v);
uint32_t sxi_swapu32(uint32_t v);

#endif

