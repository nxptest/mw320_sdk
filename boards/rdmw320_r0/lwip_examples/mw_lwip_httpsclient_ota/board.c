/*
 * Copyright 2020 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include "fsl_common.h"
#include "fsl_debug_console.h"
#include "fsl_clock.h"
#include "board.h"
#include "fsl_gpio.h"
#include "fsl_pinmux.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
/* Initialize debug console. */
void BOARD_InitDebugConsole(void)
{
    uint32_t uartClkSrcFreq;

    /* attach FAST clock to UART0 (debug console) */
    CLOCK_AttachClk(BOARD_DEBUG_UART_CLK_ATTACH);

    uartClkSrcFreq = BOARD_DEBUG_UART_CLK_FREQ;

    DbgConsole_Init(BOARD_DEBUG_UART_INSTANCE, BOARD_DEBUG_UART_BAUDRATE, BOARD_DEBUG_UART_TYPE, uartClkSrcFreq);
}

void BOARD_InitRfCtrl(int ant)
{
    gpio_pin_config_t rf_config = {
        kGPIO_DigitalOutput,
        0,
    };

    PINMUX_PinMuxSet(BOARD_WLAN_RADIO_CTRL_1, PINMUX_GPIO44_GPIO44 | PINMUX_MODE_DEFAULT);
    PINMUX_PinMuxSet(BOARD_WLAN_RADIO_CTRL_0, PINMUX_GPIO45_GPIO45 | PINMUX_MODE_DEFAULT);

    GPIO_PinInit(GPIO, BOARD_WLAN_RADIO_CTRL_1, &rf_config);
    GPIO_PinInit(GPIO, BOARD_WLAN_RADIO_CTRL_0, &rf_config);

    switch (ant)
    {
        case 2:
            GPIO_PinWrite(GPIO, BOARD_WLAN_RADIO_CTRL_0, LOGIC_RF_HIGH);
            break;
        case 1:
        default:
            GPIO_PinWrite(GPIO, BOARD_WLAN_RADIO_CTRL_1, LOGIC_RF_HIGH);
            break;
    }
}
