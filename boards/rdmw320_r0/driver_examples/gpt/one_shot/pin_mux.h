/*
 * Copyright 2020 NXP.
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PIN_MUX_H_
#define _PIN_MUX_H_


/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/

#define BOARD_UART0_TX_PIN                                                    2   /*!< Routed pin */
#define BOARD_UART0_TX_PIN_FUNCTION_ID                   PINMUX_GPIO2_UART0_TXD   /*!< Pin function id */

#define BOARD_UART0_RX_PIN                                                    3   /*!< Routed pin */
#define BOARD_UART0_RX_PIN_FUNCTION_ID                   PINMUX_GPIO3_UART0_RXD   /*!< Pin function id */

#define BOARD_AMBER_LED_PIN                                                  40   /*!< Routed pin */
#define BOARD_AMBER_LED_PIN_FUNCTION_ID                    PINMUX_GPIO40_GPIO40   /*!< Pin function id */

#define BOARD_GPT0_CH1_PIN                                                    1   /*!< Routed pin */
#define BOARD_GPT0_CH1_PIN_FUNCTION_ID                    PINMUX_GPIO1_GPT0_CH1   /*!< Pin function id */

/*!
 * @addtogroup pin_mux
 * @{
 */

/***********************************************************************************************************************
 * API
 **********************************************************************************************************************/

#if defined(__cplusplus)
extern "C" {
#endif


/*!
 * @brief Calls initialization functions.
 *
 */
void BOARD_InitBootPins(void);

/*!
 * @brief Configures pin routing and optionally pin electrical features.
 *
 */
void BOARD_InitPins(void);                                 /*!< Function assigned for the core: Cortex-M4[cm4] */

#if defined(__cplusplus)
}
#endif

/*!
 * @}
 */
#endif /* _PIN_MUX_H_ */

/***********************************************************************************************************************
 * EOF
 **********************************************************************************************************************/
