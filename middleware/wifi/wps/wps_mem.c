/** @file wps_mem.c
 *
 *  @brief WPS memory routines
 *
 *  Copyright 2008-2022 NXP
 *
 *  Licensed under the LA_OPT_NXP_Software_License.txt (the "Agreement")
 *
 */

#include <string.h>
#include <ctype.h>
#include <wm_os.h>
#include "wps.h"

void *wps_mem_malloc(size_t size)
{
    void *buffer_ptr = 0;

    if (size == 0)
        return NULL;

    buffer_ptr = os_mem_alloc(size);

    if (!buffer_ptr)
    {
        wps_d("Failed to allocate mem: Size: %d", size);
        return NULL;
    }

    return buffer_ptr;
}

void *wps_mem_calloc(size_t nmemb, size_t size)
{
    void *buffer_ptr = NULL;

    buffer_ptr = wps_mem_malloc(nmemb * size);

    if (!buffer_ptr)
    {
        wps_d("Failed to allocate mem: Size: %d", size);
        return NULL;
    }

    memset(buffer_ptr, 0, nmemb * size);
    return buffer_ptr;
}

void wps_mem_free(void *buffer_ptr)
{
    if (buffer_ptr)
        os_mem_free(buffer_ptr);
}
