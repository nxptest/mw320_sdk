/** @file threading_alt.h
 *
 *  @brief This file contains header for threading alt
 *
 *  Copyright 2008-2022 NXP
 *
 *  Licensed under the LA_OPT_NXP_Software_License.txt (the "Agreement")
 *
 */

#ifndef THREADING_ALT_H
#define THREADING_ALT_H

#include <wm_os.h>

/* Note:
 * MBEDTLS requires mutex structure with name 'mbedtls_threading_mutex_t'
 * Do not change.
 */
typedef os_mutex_t mbedtls_threading_mutex_t;

#endif /* THREADING_ALT_H */
