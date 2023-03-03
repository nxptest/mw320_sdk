/** @file timing_alt.h
 *
 *  @brief This file contains header for timing alt
 *
 *  Copyright 2008-2022 NXP
 *
 *  Licensed under the LA_OPT_NXP_Software_License.txt (the "Agreement")
 *
 */

#ifndef TIMING_ALT_H
#define TIMING_ALT_H

#if defined(__arm__)
#include <sys/time.h>
#else
#include <lwip/sockets.h>
#endif
#include <time.h>

#define mbedtls_timing_hr_time timeval

typedef struct
{
    struct mbedtls_timing_hr_time timer;
    uint32_t int_ms;
    uint32_t fin_ms;
} mbedtls_timing_delay_context;

#endif /* TIMING_ALT_H */
