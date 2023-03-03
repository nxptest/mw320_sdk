/*
 *  Copyright 2018-2022 NXP
 *
 *  Licensed under the LA_OPT_NXP_Software_License.txt (the "Agreement")
 *
 */

/** @file wm_mbedtls_helper_api.h
 *
 *  @brief This header file provides abstraction layer for mbedTLS stack
 */

#ifndef WM_MBEDTLS_HELPER_H
#define WM_MBEDTLS_HELPER_H

#include <wm_mbedtls_debug.h>

/**
 * Initialize MBEDTLS library pre-requisites as following:
 *
 * Initialize time subsystem including RTC.
 * Set memory callback functions for alloc, free
 * Set threading callback function for mutex free, lock, unlock
 * Setup global entropy, CTR_DRBG context.
 *
 * @return 0    Success
 * @return -1   Failed in setup for entropy, CTR_DRBG context.
 */
int wm_mbedtls_lib_init();

/**
 * Get wm_mbedtls library initialization status
 *
 * return true if initialized, false if not.
 */
bool is_wm_mbedtls_lib_init();

#endif /* WM_MBEDTLS_HELPER_H */
