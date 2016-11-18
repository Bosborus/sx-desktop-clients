/*
 *  Copyright (C) 2012-2016 Skylable Ltd. <info-copyright@skylable.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Special exception for linking this software with OpenSSL:
 *
 *  In addition, as a special exception, Skylable Ltd. gives permission to
 *  link the code of this program with the OpenSSL library and distribute
 *  linked combinations including the two. You must obey the GNU General
 *  Public License in all respects for all of the code used other than
 *  OpenSSL. You may extend this exception to your version of the program,
 *  but you are not obligated to do so. If you do not wish to do so, delete
 *  this exception statement from your version.
 */

#include "fake_misc.h"
#include "crypt_blowfish.h"

#define BCRYPT_ITERATIONS_LOG2 14

static const char hexchar[] = "0123456789abcdef";
void sxi_bin2hex(const void *bin, unsigned int len, char *hex) {
    const uint8_t *s = (const uint8_t *)bin;
    while(len--) {
        uint8_t c = *s;
        s++;
        hex[0] = hexchar[c >> 4];
        hex[1] = hexchar[c & 0xf];
        hex += 2;
    }
    *hex = '\0';
}

static const int hexchars[256] = {
    -1, -1, -1, -1,  -1, -1, -1, -1,    -1, -1, -1, -1,  -1, -1, -1, -1,
    -1, -1, -1, -1,  -1, -1, -1, -1,    -1, -1, -1, -1,  -1, -1, -1, -1,
    -1, -1, -1, -1,  -1, -1, -1, -1,    -1, -1, -1, -1,  -1, -1, -1, -1,
     0,  1,  2,  3,   4,  5,  6,  7,     8,  9, -1, -1,  -1, -1, -1, -1,
    -1, 10, 11, 12,  13, 14, 15, -1,    -1, -1, -1, -1,  -1, -1, -1, -1,
    -1, -1, -1, -1,  -1, -1, -1, -1,    -1, -1, -1, -1,  -1, -1, -1, -1,
    -1, 10, 11, 12,  13, 14, 15, -1,    -1, -1, -1, -1,  -1, -1, -1, -1,
    -1, -1, -1, -1,  -1, -1, -1, -1,    -1, -1, -1, -1,  -1, -1, -1, -1,

    -1, -1, -1, -1,  -1, -1, -1, -1,    -1, -1, -1, -1,  -1, -1, -1, -1,
    -1, -1, -1, -1,  -1, -1, -1, -1,    -1, -1, -1, -1,  -1, -1, -1, -1,
    -1, -1, -1, -1,  -1, -1, -1, -1,    -1, -1, -1, -1,  -1, -1, -1, -1,
    -1, -1, -1, -1,  -1, -1, -1, -1,    -1, -1, -1, -1,  -1, -1, -1, -1,
    -1, -1, -1, -1,  -1, -1, -1, -1,    -1, -1, -1, -1,  -1, -1, -1, -1,
    -1, -1, -1, -1,  -1, -1, -1, -1,    -1, -1, -1, -1,  -1, -1, -1, -1,
    -1, -1, -1, -1,  -1, -1, -1, -1,    -1, -1, -1, -1,  -1, -1, -1, -1,
    -1, -1, -1, -1,  -1, -1, -1, -1,    -1, -1, -1, -1,  -1, -1, -1, -1,
};

int sxi_hex2bin(const char *src, uint32_t src_len, uint8_t *dst, uint32_t dst_len)
{
    uint32_t i;
    if((src_len % 2) || (dst_len < src_len / 2))
        return -1;
    for (i = 0; i < src_len; i += 2) {
        int32_t h = (hexchars[(unsigned int)src[i]] << 4) | hexchars[(unsigned int)src[i+1]];
        if (h < 0)
            return -1;
        dst[i >> 1] = h;
    }
    return 0;
}

int sxi_derive_key(const char *pass, const char *salt, unsigned salt_size, char *out, unsigned int len)
{
    char settingbuf[30];
    const char *genkey, *setting;

    setting = _crypt_gensalt_blowfish_rn("$2b$", BCRYPT_ITERATIONS_LOG2, salt, salt_size, settingbuf, sizeof(settingbuf));
    if (!setting)
        return -1;
    genkey = _crypt_blowfish_rn(pass, setting, out, len);
    if (!genkey)
        return -1;

    return 0;
}

#ifdef WORDS_BIGENDIAN
uint32_t sxi_swapu32(uint32_t v)
{
    v = ((v << 8) & 0xff00ff00) | ((v >> 8) & 0xff00ff);
    return (v << 16) | (v >> 16);
}
uint64_t sxi_swapu64(uint64_t v)
{
    v = ((v << 8) & 0xff00ff00ff00ff00ULL) | ((v >> 8) & 0x00ff00ff00ff00ffULL);
    v = ((v << 16) & 0xffff0000ffff0000ULL) | ((v >> 16) & 0x0000ffff0000ffffULL);
    return (v << 32) | (v >> 32);
}
#else
uint64_t sxi_swapu64(uint64_t v) {
    return v;
}
uint32_t sxi_swapu32(uint32_t v)
{
    return v;
}
#endif

