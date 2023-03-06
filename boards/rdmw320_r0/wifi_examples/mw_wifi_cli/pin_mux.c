/*
 * Copyright 2020 NXP.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_common.h"
#include "fsl_pinmux.h"
#include "pin_mux.h"

/* FUNCTION ************************************************************************************************************
 *
 * Function Name : BOARD_InitBootPins
 * Description   : Calls initialization functions.
 *
 * END ****************************************************************************************************************/
void BOARD_InitBootPins(void)
{
    BOARD_InitPins();
}

/* FUNCTION ************************************************************************************************************
 *
 * Function Name : BOARD_InitPins
 * Description   : Configures pin routing and optionally pin electrical features.
 *
 * END ****************************************************************************************************************/
void BOARD_InitPins(void) {                                /*!< Function assigned for the core: Cortex-M4[cm4] */
    PINMUX_PinMuxSet(BOARD_UART0_TX_PIN, BOARD_UART0_TX_PIN_FUNCTION_ID | PINMUX_MODE_DEFAULT);
    PINMUX_PinMuxSet(BOARD_UART0_RX_PIN, BOARD_UART0_RX_PIN_FUNCTION_ID | PINMUX_MODE_DEFAULT);
    PINMUX_PinMuxSet(BOARD_PUSH_SW1_PIN, BOARD_PUSH_SW1_PIN_FUNCTION_ID | PINMUX_MODE_DEFAULT);

    PINMUX_PinMuxSet(BOARD_PUSH_SW2_PIN, BOARD_PUSH_SW2_PIN_FUNCTION_ID | PINMUX_MODE_DEFAULT);
    PINMUX_PinMuxSet(BOARD_PUSH_SW4_PIN, BOARD_PUSH_SW4_PIN_FUNCTION_ID | PINMUX_MODE_DEFAULT);

    PINMUX_PinMuxSet(BOARD_UART2_TX_PIN, BOARD_UART2_TX_PIN_FUNCTION_ID | BOARD_UART2_TX_PULL_STATE);
    PINMUX_PinMuxSet(BOARD_UART2_RX_PIN, BOARD_UART2_RX_PIN_FUNCTION_ID | BOARD_UART2_RX_PULL_STATE);

    PINMUX_PinMuxSet(BOARD_REQ_PIN,   BOARD_COEX_PIN_FUNCTION_ID | PINMUX_MODE_PULLDOWN);
    PINMUX_PinMuxSet(BOARD_PRI_PIN,   BOARD_COEX_PIN_FUNCTION_ID | PINMUX_MODE_PULLDOWN);
    //PINMUX_PinMuxSet(BOARD_GRANT_PIN, BOARD_COEX_PIN_FUNCTION_ID | PINMUX_MODE_PULLDOWN);
    PINMUX_PinMuxSet(BOARD_LED_YELLOW_PIN, BOARD_LED_YELLOW_PIN_FUNCTION_ID | PINMUX_MODE_DEFAULT);
}

/***********************************************************************************************************************
 * EOF
 **********************************************************************************************************************/
