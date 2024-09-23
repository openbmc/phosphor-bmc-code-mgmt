/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2015-present Facebook. All Rights Reserved.
 */

#include "kv.hpp"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

int kv_get(const char* key, char* value, size_t* len, unsigned int flags)
{
    (void)key;
    (void)value;
    (void)len;
    (void)flags;
    return 0;
}

int kv_set(const char* key, const char* value, size_t len, unsigned int flags)
{
    (void)key;
    (void)value;
    (void)len;
    (void)flags;
    return 0;
}

int kv_del(const char* key, unsigned int flags)
{
    (void)key;
    (void)flags;
    return 0;
}

#ifdef __cplusplus
}
#endif
