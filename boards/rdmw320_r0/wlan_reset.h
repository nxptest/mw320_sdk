/*
 * Copyright 2021 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _WLAN_RESET_H_
#define _WLAN_RESET_H_

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/*******************************************************************************
 * API
 ******************************************************************************/

/*!
 * @brief Power cycle the WLAN core.
 * This API should be called once wlan_stop() and wlan_deinit() APIs have been
 * successfully called.
 */
void BOARD_WLANPowerReset(void);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* _WLAN_RESET_H_ */
