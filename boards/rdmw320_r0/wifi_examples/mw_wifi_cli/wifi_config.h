/*
 *  Copyright 2020-2021 NXP
 *  All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _WIFI_CONFIG_H_
#define _WIFI_CONFIG_H_

#include "ext_ble_coex.h"

#define SD8801

#define CONFIG_WIFI_MAX_PRIO (configMAX_PRIORITIES - 1)

#define CONFIG_MAX_AP_ENTRIES 10

#define CONFIG_SDIO_MULTI_PORT_RX_AGGR 1

#define CONFIG_FLASH_PARTITION_COUNT 16

//#define CONFIG_WPS2
#define CONFIG_IPV6 1

#define CONFIG_EXT_COEX

#ifdef CONFIG_IPV6
#define CONFIG_MAX_IPV6_ADDRESSES 3
#endif

/* Logs */
#define CONFIG_ENABLE_ERROR_LOGS   1
#define CONFIG_ENABLE_WARNING_LOGS 1

/* WLCMGR debug */
#undef CONFIG_WLCMGR_DEBUG

/*
 * Wifi extra debug options
 */
#undef CONFIG_WIFI_EXTRA_DEBUG
#undef CONFIG_WIFI_EVENTS_DEBUG
#undef CONFIG_WIFI_CMD_RESP_DEBUG
#undef CONFIG_WIFI_SCAN_DEBUG
#undef CONFIG_WIFI_IO_INFO_DUMP
#undef CONFIG_WIFI_IO_DEBUG
#undef CONFIG_WIFI_IO_DUMP
#undef CONFIG_WIFI_MEM_DEBUG
#undef CONFIG_WIFI_AMPDU_DEBUG
#undef CONFIG_WIFI_TIMER_DEBUG
#undef CONFIG_WIFI_SDIO_DEBUG
#undef CONFIG_WIFI_FW_DEBUG
#undef CONFIG_WPS_DEBUG

/*
 * Heap debug options
 */
#undef CONFIG_HEAP_DEBUG
#undef CONFIG_HEAP_STAT

#endif /* _WIFI_CONFIG_H_ */
