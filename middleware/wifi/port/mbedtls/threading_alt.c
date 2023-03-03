/** @file threading_alt.c
 *
 *  @brief This file contains threading alt and related functions
 *
 *  Copyright 2008-2022 NXP
 *
 *  Licensed under the LA_OPT_NXP_Software_License.txt (the "Agreement")
 *
 */

#include <stdio.h>
#include "threading_alt.h"
#include <mbedtls/threading.h>
#include "wm_mbedtls_debug.h"

static void wmos_wrap_mutex_init(mbedtls_threading_mutex_t *mutex)
{
    char mutex_name_buf[12];
    static int mutex_count = 1;

    snprintf(mutex_name_buf, sizeof(mutex_name_buf), "mbd_mtx-%d", mutex_count++);

    if (WM_SUCCESS != os_mutex_create((os_mutex_t *)mutex, mutex_name_buf, OS_MUTEX_INHERIT))
    {
        wm_mbedtls_e("%s: mutex creation failed", __func__);
    }
}

static void wmos_wrap_mutex_free(mbedtls_threading_mutex_t *mutex)
{
    os_mutex_delete((os_mutex_t *)mutex);
}

static int wmos_wrap_mutex_lock(mbedtls_threading_mutex_t *mutex)
{
    return os_mutex_get((os_mutex_t *)mutex, OS_WAIT_FOREVER);
}

static int wmos_wrap_mutex_unlock(mbedtls_threading_mutex_t *mutex)
{
    return os_mutex_put((os_mutex_t *)mutex);
}

void wm_mbedtls_set_threading_alt()
{
    mbedtls_threading_set_alt(wmos_wrap_mutex_init, wmos_wrap_mutex_free, wmos_wrap_mutex_lock, wmos_wrap_mutex_unlock);
}
