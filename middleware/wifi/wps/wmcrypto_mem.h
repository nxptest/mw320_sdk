/* @file wmcrypto_mem.h
 *
 *  @brief This file provides crypto  memory routine interface
 *
 *  Copyright 2008-2022 NXP
 *
 *  Licensed under the LA_OPT_NXP_Software_License.txt (the "Agreement")
 *
 */

/** Malloc memory
 */
void *crypto_mem_malloc(size_t size);

/** Free previously allocated memory
 */
void crypto_mem_free(void *ptr);

/** Calloc memory
 */
void *crypto_mem_calloc(size_t nmemb, size_t size);
