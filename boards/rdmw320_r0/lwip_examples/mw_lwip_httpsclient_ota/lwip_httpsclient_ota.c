/*
 *  Copyright 2008-2021 NXP
 *
 *  NXP CONFIDENTIAL
 *  The source code contained or described herein and all documents related to
 *  the source code ("Materials") are owned by NXP, its
 *  suppliers and/or its licensors. Title to the Materials remains with NXP,
 *  its suppliers and/or its licensors. The Materials contain
 *  trade secrets and proprietary and confidential information of NXP, its
 *  suppliers and/or its licensors. The Materials are protected by worldwide copyright
 *  and trade secret laws and treaty provisions. No part of the Materials may be
 *  used, copied, reproduced, modified, published, uploaded, posted,
 *  transmitted, distributed, or disclosed in any way without NXP's prior
 *  express written permission.
 *
 *  No license under any patent, copyright, trade secret or other intellectual
 *  property right is granted to or conferred upon you by disclosure or delivery
 *  of the Materials, either expressly, by implication, inducement, estoppel or
 *  otherwise. Any license under such intellectual property rights must be
 *  express and approved by NXP in writing.
 */

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"

#include "mflash_drv.h"
#include "mflash_params.h"
#include "ksdk_mbedtls.h"

#include "wifi.h"
#include "wm_net.h"
#include <wm_os.h>
#include "cli.h"
#include "partition.h"
#include "boot_flags.h"

#include "fsl_sdmmc_host.h"
#include "fsl_aes.h"


/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define APP_AES AES
#define CONNECTION_INFO_FILENAME "connection_info.dat"

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
int fw_update_cli_init(void);
void test_wlan_add(int argc, char **argv);

/*******************************************************************************
 * Variables
 ******************************************************************************/
static SemaphoreHandle_t aesLock;

static struct wlan_network sta_network;

const int TASK_MAIN_PRIO       = OS_PRIO_3;
const int TASK_MAIN_STACK_SIZE = 2048;

portSTACK_TYPE *task_main_stack = NULL;
TaskHandle_t task_main_task_handler;

/*******************************************************************************
 * Code
 ******************************************************************************/
static void printSeparator(void)
{
    PRINTF("----------------------------------------\r\n");
}

static status_t APP_AES_Lock(void)
{
    if (pdTRUE == xSemaphoreTakeRecursive(aesLock, portMAX_DELAY))
    {
        return kStatus_Success;
    }
    else
    {
        return kStatus_Fail;
    }
}

static void APP_AES_Unlock(void)
{
    xSemaphoreGiveRecursive(aesLock);
}

void wlan_remove_profile(int argc, char **argv)
{
    int ret;
        
    if (argc < 2)
    {   
        PRINTF("Usage: %s <profile_name>\r\n", argv[0]);
        PRINTF("Error: specify network to remove\r\n");
        return;
    }
    
    ret = wlan_remove_network(argv[1]);
    switch (ret)
    {
        case WM_SUCCESS:
            PRINTF("Removed \"%s\"\r\n", argv[1]);
            break;
        case -WM_E_INVAL:
            PRINTF("Error: network not found\r\n");
            break;
        case WLAN_ERROR_STATE:
            PRINTF("Error: can't remove network in this state\r\n");
            break;
        default:
            PRINTF("Error: unable to remove network\r\n");
            break;
    }
}

void wlan_connect_profile(int argc, char **argv)
{           
    int ret = wlan_connect(argc >= 2 ? argv[1] : NULL);
            
    if (ret == WLAN_ERROR_STATE)
    {   
        PRINTF("Error: connect manager not running\r\n");
        return;
    }   

    if (ret == -WM_E_INVAL)
    {
        PRINTF("Usage: %s <profile_name>\r\n", argv[0]);
        PRINTF("Error: specify a network to connect\r\n");
        return;
    }
    PRINTF(
        "Connecting to network...\r\nUse 'wlan-stat' for "
        "current connection status.\r\n");
}

void wlan_disconnect_profile(int argc, char **argv)
{   
    if (wlan_disconnect() != WM_SUCCESS)
        PRINTF("Error: unable to disconnect\r\n");
}

static struct cli_command wifi_commands[] = {
    {"wlan-add", "<profile_name> ssid <ssid> bssid...", test_wlan_add},
    {"wlan-remove", "<profile_name>", wlan_remove_profile},
    {"wlan-connect", "<profile_name>", wlan_connect_profile},
    {"wlan-disconnect", NULL, wlan_disconnect_profile},
};  

int wifi_cli_init(void)
{   
    if (cli_register_commands(wifi_commands, sizeof(wifi_commands) / sizeof(struct cli_command)))
        return -WM_FAIL;

    return WM_SUCCESS;
}
/* Callback Function passed to WLAN Connection Manager. The callback function
 * gets called when there are WLAN Events that need to be handled by the
 * application.
 */
int wlan_event_callback(enum wlan_event_reason reason, void *data)
{
    int ret;
    struct wlan_ip_config addr;
    char ip[16];
    static int auth_fail = 0;

    printSeparator();
    PRINTF("app_cb: WLAN: received event %d\r\n", reason);
    printSeparator();

    switch (reason)
    {
        case WLAN_REASON_INITIALIZED:
            ret = wifi_cli_init();
            if (ret != WM_SUCCESS)
            {
                PRINTF("Failed to initialize WLAN CLIs\r\n");
                return 0;
            }
            ret = fw_update_cli_init();
            if (ret != WM_SUCCESS)
            {
                PRINTF("Failed to initialize FW UPGRADE CLI\r\n");
                return 0;
            }
            break;
        case WLAN_REASON_INITIALIZATION_FAILED:
            PRINTF("app_cb: WLAN: initialization failed\r\n");
            break;
        case WLAN_REASON_SUCCESS:
            PRINTF("app_cb: WLAN: connected to network\r\n");
            ret = wlan_get_address(&addr);
            if (ret != WM_SUCCESS)
            {
                PRINTF("failed to get IP address\r\n");
                return 0;
            }

            net_inet_ntoa(addr.ipv4.address, ip);

            ret = wlan_get_current_network(&sta_network);
            if (ret != WM_SUCCESS)
            {
                PRINTF("Failed to get External AP network\r\n");
                return 0;
            }

            PRINTF("Connected to following BSS:\r\n");
            PRINTF("SSID = [%s], IP = [%s]\r\n", sta_network.ssid, ip);
            auth_fail = 0;
            break;
        case WLAN_REASON_CONNECT_FAILED:
            PRINTF("app_cb: WLAN: connect failed\r\n");
            break;
        case WLAN_REASON_NETWORK_NOT_FOUND:
            PRINTF("app_cb: WLAN: network not found\r\n");
            break;
        case WLAN_REASON_NETWORK_AUTH_FAILED:
            PRINTF("app_cb: WLAN: network authentication failed\r\n");
            auth_fail++;
            if (auth_fail >= 3)
            {
                PRINTF("Authentication Failed. Disconnecting ... \r\n");
                wlan_disconnect();
                auth_fail = 0;
            }
            break;
        case WLAN_REASON_ADDRESS_SUCCESS:
            PRINTF("network mgr: DHCP new lease\r\n");
            break;
        case WLAN_REASON_ADDRESS_FAILED:
            PRINTF("app_cb: failed to obtain an IP address\r\n");
            break;
        case WLAN_REASON_USER_DISCONNECT:
            PRINTF("app_cb: disconnected\r\n");
            auth_fail = 0;
            break;
        default:
            PRINTF("app_cb: WLAN: Unknown Event: %d\r\n", reason);
    }
    return 0;
}

void task_main(void *param)
{
    int32_t result = 0;
    struct partition_entry *p;
    short history = 0;
    struct partition_entry *f1, *f2;
    flash_desc_t fl;
    uint32_t *wififw;

    boot_init();

    mflash_drv_init();

    printSeparator();

    result = cli_init();
    if (WM_SUCCESS != result)
    {
        assert(false);
    }

    PRINTF("Initialize WLAN Driver\r\n");
    printSeparator();

    result = part_init();
    if (WM_SUCCESS != result)
    {
        assert(false);
    }

    f1 = part_get_layout_by_id(FC_COMP_WLAN_FW, &history);
    f2 = part_get_layout_by_id(FC_COMP_WLAN_FW, &history);

    if (f1 && f2)
    {
        p = part_get_active_partition(f1, f2);
    }
    else if (!f1 && f2)
    {
        p = f2;
    }
    else if (!f2 && f1)
    {
        p = f1;
    }
    else
    {
        PRINTF(" Wi-Fi Firmware not detected\r\n");
        p = NULL;
    }

    if (p != NULL)
    {
        part_to_flash_desc(p, &fl);

        wififw = (uint32_t *)mflash_drv_phys2log(fl.fl_start, fl.fl_size);
        assert(wififw != NULL);
        /* First word in WIFI firmware is magic number. */
        assert(*wififw == (('W' << 0) | ('L' << 8) | ('F' << 16) | ('W' << 24)));

        /* Initialize WIFI Driver */
        /* Second word in WIFI firmware is WIFI firmware length in bytes. */
        /* Real WIFI binary starts from 3rd word. */
        result = wlan_init((const uint8_t *)(wififw + 2U), *(wififw + 1U));
        if (WM_SUCCESS != result)
        {
            assert(false);
        }

        result = wlan_start(wlan_event_callback);
        if (WM_SUCCESS != result)
        {
            assert(false);
        }
    }

    while (1)
    {
        /* wait for interface up */
        os_thread_sleep(os_msec_to_ticks(5000));
    }
}

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

int main(void)
{
    BaseType_t result = 0;
    (void)result;

    uint8_t hash[32];
    uint32_t len = sizeof(hash);

    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();

    BOARD_GetHash(hash, &len);
    assert(len > 0U);
    mbedtls_hardware_init_hash(hash, len);

    CLOCK_EnableXtal32K(kCLOCK_Osc32k_External);
    CLOCK_AttachClk(kXTAL32K_to_RTC);

    printSeparator();
    PRINTF("[client mcufw OTA demo]\r\n");
    printSeparator();
    PRINTF("Build Time: %s--%s \r\n", __DATE__, __TIME__);

    aesLock = xSemaphoreCreateRecursiveMutex();
    assert(aesLock != NULL);

    AES_Init(APP_AES);
    AES_SetLockFunc(APP_AES_Lock, APP_AES_Unlock);

    CRYPTO_InitHardware();

    result =
        xTaskCreate(task_main, "main", TASK_MAIN_STACK_SIZE, task_main_stack, TASK_MAIN_PRIO, &task_main_task_handler);
    assert(pdPASS == result);

    vTaskStartScheduler();
    for (;;)
        ;
}
