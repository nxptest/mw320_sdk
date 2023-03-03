/** @file wm_mbedtls_entropy.c
 *
 *  @brief This file ports mbedtls entropy functions on marvell platform
 *
 *  Copyright 2008-2022 NXP
 *
 *  Licensed under the LA_OPT_NXP_Software_License.txt (the "Agreement")
 *
 */

#include "wm_mbedtls_entropy.h"

#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/platform.h>
#include <mbedtls/entropy_poll.h>
#include <stdlib.h>
#include <wm_utils.h>
#include <wlan.h>
#include "wm_mbedtls_debug.h"

static struct
{
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
} wm_mbedtls_gdata;

static void get_random_sequence_1(uint8_t *buf, unsigned int size)
{
    int i;
    uint32_t tsf_high, tsf_low;
    wlan_get_tsf(&tsf_high, &tsf_low);

    srand(tsf_low);
    for (i = 0; i < size; i++)
    {
        buf[i] = rand();
    }
}

static int wm_wrap_entropy_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
    ((void)data);
    get_random_sequence_1(output, len);
    *olen = len;
    return 0;
}

mbedtls_ctr_drbg_context *wm_mbedtls_get_ctr_drbg_ctx()
{
    return &wm_mbedtls_gdata.ctr_drbg;
}

int wm_mbedtls_entropy_ctr_drbg_setup()
{
    int ret                          = 0;
    const unsigned char *custom_name = (const unsigned char *)"WMSDK_4.0";
    size_t custom_name_len           = strlen((const char *)custom_name);

    mbedtls_entropy_init(&wm_mbedtls_gdata.entropy);

    mbedtls_entropy_add_source(&wm_mbedtls_gdata.entropy, wm_wrap_entropy_poll, NULL, MBEDTLS_ENTROPY_MIN_PLATFORM,
                               MBEDTLS_ENTROPY_SOURCE_STRONG);

    mbedtls_ctr_drbg_init(&wm_mbedtls_gdata.ctr_drbg);

    if ((ret = mbedtls_ctr_drbg_seed(&wm_mbedtls_gdata.ctr_drbg, mbedtls_entropy_func, &wm_mbedtls_gdata.entropy,
                                     custom_name, custom_name_len)) != 0)
    {
        wm_mbedtls_e(
            "mbedtls_ctr_drbg_seed returned, "
            "ret = -0x%02X",
            -ret);
        return -1;
    }
    return 0;
}
