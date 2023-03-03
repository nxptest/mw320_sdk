/** @file mlan_sdio.c
 *
 *  @brief This file provides mlan driver for SDIO
 *
 *  Copyright 2008-2020 NXP
 *
 *  Licensed under the LA_OPT_NXP_Software_License.txt (the "Agreement")
 *
 */

#include <mlan_sdio_api.h>
#include <wm_os.h>
#include <board.h>
/* MW320 Specific Code Starts here */
#include <sdmmc_config.h>
/* MW320 Specific Code Ends here */
#include <fsl_common.h>
#include <fsl_clock.h>
#include <fsl_sdio.h>
#include <fsl_sdmmc_spec.h>
/* MW320 Specific Code Starts here */
//#include <fsl_usdhc.h>
/* MW320 Specific Code Ends here */
#include <wifi.h>

#define SDIO_CMD_TIMEOUT 2000

static sdio_card_t wm_g_sd;

static os_mutex_t sdio_mutex;

int sdio_drv_creg_read(int addr, int fn, uint32_t *resp)
{
    int ret;

    ret = os_mutex_get(&sdio_mutex, OS_WAIT_FOREVER);
    if (ret == -WM_FAIL)
    {
        sdio_e("failed to get mutex\r\n");
        return 0;
    }

    if (SDIO_IO_Read_Direct(&wm_g_sd, (sdio_func_num_t)fn, addr, (uint8_t *)resp) != kStatus_Success)
    {
        (void)os_mutex_put(&sdio_mutex);
        return 0;
    }

    (void)os_mutex_put(&sdio_mutex);

    return 1;
}

int sdio_drv_creg_write(int addr, int fn, uint8_t data, uint32_t *resp)
{
    int ret;

    ret = os_mutex_get(&sdio_mutex, OS_WAIT_FOREVER);
    if (ret == -WM_FAIL)
    {
        sdio_e("failed to get mutex\r\n");
        return 0;
    }

    if (SDIO_IO_Write_Direct(&wm_g_sd, (sdio_func_num_t)fn, addr, &data, true) != kStatus_Success)
    {
        (void)os_mutex_put(&sdio_mutex);
        return 0;
    }

    *resp = data;

    (void)os_mutex_put(&sdio_mutex);

    return 1;
}

int sdio_drv_read(uint32_t addr, uint32_t fn, uint32_t bcnt, uint32_t bsize, uint8_t *buf, uint32_t *resp)
{
    int ret;
    uint32_t flags = 0;
    uint32_t param;

    ret = os_mutex_get(&sdio_mutex, OS_WAIT_FOREVER);
    if (ret == -WM_FAIL)
    {
        sdio_e("failed to get mutex\r\n");
        return 0;
    }

    if (bcnt > 1U)
    {
        flags |= SDIO_EXTEND_CMD_BLOCK_MODE_MASK;
        param = bcnt;
    }
    else
    {
        param = bsize;
    }

/* MW320 Specific Code Starts here */
    uint32_t argument = ((uint32_t)fn << SDIO_CMD_ARGUMENT_FUNC_NUM_POS) |
                        ((addr & SDIO_CMD_ARGUMENT_REG_ADDR_MASK) << SDIO_CMD_ARGUMENT_REG_ADDR_POS) |
                        (param & SDIO_EXTEND_CMD_COUNT_MASK) |
                        ((bcnt > 1 ? 1U : 0U) << SDIO_EXTEND_CMD_ARGUMENT_BLOCK_MODE_POS);

    if (SDIO_IO_Transfer(&wm_g_sd, kSDIO_RWIOExtended, argument, bsize, NULL, buf, bsize * bcnt, resp) !=
        kStatus_Success)
/* MW320 Specific Code Ends here */
    {
        (void)os_mutex_put(&sdio_mutex);
        return 0;
    }

    (void)os_mutex_put(&sdio_mutex);

    return 1;
}

bool sdio_drv_write(uint32_t addr, uint32_t fn, uint32_t bcnt, uint32_t bsize, uint8_t *buf, uint32_t *resp)
{
    int ret;
    uint32_t flags = 0;
    uint32_t param;

    ret = os_mutex_get(&sdio_mutex, OS_WAIT_FOREVER);
    if (ret == -WM_FAIL)
    {
        sdio_e("failed to get mutex\r\n");
        return false;
    }

    if (bcnt > 1U)
    {
        flags |= SDIO_EXTEND_CMD_BLOCK_MODE_MASK;
        param = bcnt;
    }
    else
    {
        param = bsize;
    }

/* MW320 Specific Code Starts here */
    uint32_t argument = ((uint32_t)fn << SDIO_CMD_ARGUMENT_FUNC_NUM_POS) |
                        ((addr & SDIO_CMD_ARGUMENT_REG_ADDR_MASK) << SDIO_CMD_ARGUMENT_REG_ADDR_POS) |
                        (param & SDIO_EXTEND_CMD_COUNT_MASK) | (1U << SDIO_CMD_ARGUMENT_RW_POS) |
                        ((bcnt > 1 ? 1U : 0U) << SDIO_EXTEND_CMD_ARGUMENT_BLOCK_MODE_POS);

    if (SDIO_IO_Transfer(&wm_g_sd, kSDIO_RWIOExtended, argument, bsize, buf, NULL, bsize * bcnt, resp) !=
        kStatus_Success)
/* MW320 Specific Code Ends here */
    {
        (void)os_mutex_put(&sdio_mutex);
        return false;
    }

    (void)os_mutex_put(&sdio_mutex);

    return true;
}

/* MW320 Specific Code Starts here */
static void SDIOCARD_DetectCallBack(bool isInserted, void *userData)
{
}
/* MW320 Specific Code Ends here */

static void SDIO_CardInterruptCallBack(void *userData)
{
    SDMMCHOST_EnableCardInt(wm_g_sd.host, false);
    handle_cdint(0);
}

void sdio_enable_interrupt(void)
{
    if (wm_g_sd.isHostReady)
    {
        SDMMCHOST_EnableCardInt(wm_g_sd.host, true);
    }
}

static void sdio_controller_init(void)
{
    (void)memset(&wm_g_sd, 0, sizeof(sdio_card_t));

/* MW320 Specific Code Starts here */
    BOARD_SDIO_Config(&wm_g_sd, SDIOCARD_DetectCallBack, BOARD_SDMMC_SDIO_HOST_IRQ_PRIORITY,
                      SDIO_CardInterruptCallBack);
/* MW320 Specific Code Ends here */

#if defined(SD_TIMING_MAX)
    wm_g_sd.currentTiming = SD_TIMING_MAX;
#endif
#if defined(SD_CLOCK_MAX)
    wm_g_sd.usrParam.maxFreq = SD_CLOCK_MAX;
#endif
}

static int sdio_card_init(void)
{
    int ret = WM_SUCCESS;
    uint32_t resp;

    if (SDIO_HostInit(&wm_g_sd) != kStatus_Success)
    {
        return kStatus_SDMMC_HostNotReady;
    }

#if defined(SDMMCHOST_OPERATION_VOLTAGE_3V3)
    /* Disable switch to 1.8V in SDIO_ProbeBusVoltage() */
    wm_g_sd.usrParam.ioVoltage = NULL;
#elif defined(SDMMCHOST_OPERATION_VOLTAGE_1V8)
    /* Switch to 1.8V */
    if ((wm_g_sd.usrParam.ioVoltage != NULL) && (wm_g_sd.usrParam.ioVoltage->type == kSD_IOVoltageCtrlByGpio))
    {
        if (wm_g_sd.usrParam.ioVoltage->func != NULL)
        {
            wm_g_sd.usrParam.ioVoltage->func(kSDMMC_OperationVoltage180V);
        }
    }
#if SDMMCHOST_SUPPORT_VOLTAGE_CONTROL
    else if ((wm_g_sd.usrParam.ioVoltage != NULL) && (wm_g_sd.usrParam.ioVoltage->type == kSD_IOVoltageCtrlByHost))
    {
        SDMMCHOST_SwitchToVoltage(wm_g_sd.host, kSDMMC_OperationVoltage180V);
    }
#endif
    else
    {
        /* Do Nothing */
    }
    wm_g_sd.operationVoltage = kSDMMC_OperationVoltage180V;
#endif

/* MW320 Specific Code Starts here */
    /* power off card */
    SDIO_SetCardPower(&wm_g_sd, false);
    /* power on card */
    SDIO_SetCardPower(&wm_g_sd, true);
/* MW320 Specific Code Ends here */

    ret = SDIO_CardInit(&wm_g_sd);
    if (ret != WM_SUCCESS)
    {
        return ret;
    }

    (void)sdio_drv_creg_read(0x0, 0, &resp);

    sdio_d("Card Version - (0x%x)", resp & 0xff);

    /* Mask interrupts in card */
    (void)sdio_drv_creg_write(0x4, 0, 0x3, &resp);
    /* Enable IO in card */
    (void)sdio_drv_creg_write(0x2, 0, 0x2, &resp);

    (void)SDIO_SetBlockSize(&wm_g_sd, (sdio_func_num_t)0, 256);
    (void)SDIO_SetBlockSize(&wm_g_sd, (sdio_func_num_t)1, 256);
    (void)SDIO_SetBlockSize(&wm_g_sd, (sdio_func_num_t)2, 256);

    return ret;
}

/* MW320 Specific Code Starts here */
void sdio_drv_deinit(void)
{
    SDIO_HostDeinit(&wm_g_sd);

    os_mutex_delete(&sdio_mutex);
}
/* MW320 Specific Code Ends here */

int sdio_drv_init(void (*cd_int)(int))
{
    int ret;

    ret = os_mutex_create(&sdio_mutex, "sdio-mutex", OS_MUTEX_INHERIT);
    if (ret == -WM_FAIL)
    {
        sdio_e("Failed to create mutex");
        return -WM_FAIL;
    }

    sdio_controller_init();

    if (sdio_card_init() != WM_SUCCESS)
    {
        sdio_e("Card initialization failed");
        return -WM_FAIL;
    }
    else
    {
        sdio_d("Card initialization successful");
    }

    return WM_SUCCESS;
}
