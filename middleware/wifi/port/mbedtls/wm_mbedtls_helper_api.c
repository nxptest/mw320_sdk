/** @file wm_mbedtls_helper_api.c
 *
 *  @brief This file provides helper APIs to use mbedtls on marvell platform
 *
 *  Copyright 2008-2022 NXP
 *
 *  Licensed under the LA_OPT_NXP_Software_License.txt (the "Agreement")
 *
 */

#include "wm_mbedtls_mem.h"
#include "wm_mbedtls_threading.h"
#include "wm_mbedtls_entropy.h"
#include "wm_mbedtls_helper_api.h"
#include "timing_alt.h"
#include <mbedtls/debug.h>

#ifdef MBEDTLS_DEBUG_C
#include <string.h>
#endif /* MBEDTLS_DEBUG_C */

static bool wm_mbedtls_lib_inited;

int wm_mbedtls_lib_init()
{
    int ret = 0;
    if (wm_mbedtls_lib_inited)
        return 0;

    wm_mbedtls_set_mem_alloc();
    wm_mbedtls_set_threading_alt();

    if ((ret = wm_mbedtls_entropy_ctr_drbg_setup()) != 0)
        return ret;

    wm_mbedtls_lib_inited = 1;
    return 0;
}

bool is_wm_mbedtls_lib_init()
{
    return wm_mbedtls_lib_inited;
}
