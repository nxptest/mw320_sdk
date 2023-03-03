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
}

/***********************************************************************************************************************
 * EOF
 **********************************************************************************************************************/
