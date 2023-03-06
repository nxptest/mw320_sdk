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

#define BOARD_PUSH_SW1_PIN                                                   24   /*!< Routed pin */
#define BOARD_PUSH_SW1_PIN_FUNCTION_ID                   PINMUX_GPIO24_GPIO24     /*!< Pin function id */

#define BOARD_UART2_TX_PIN                                                   48   /*!< Routed pin */
#define BOARD_UART2_TX_PIN_FUNCTION_ID                   PINMUX_GPIO48_UART2_TXD  /*!< Pin function id */
#define BOARD_UART2_TX_PULL_STATE                        PINMUX_MODE_DEFAULT

#define BOARD_UART2_RX_PIN                                                   49   /*!< Routed pin */
#define BOARD_UART2_RX_PIN_FUNCTION_ID                   PINMUX_GPIO49_UART2_RXD  /*!< Pin function id */
#define BOARD_UART2_RX_PULL_STATE                        PINMUX_MODE_DEFAULT

#define BOARD_PUSH_SW2_PIN                                                   26   /*!< Routed pin */
#define BOARD_PUSH_SW2_PIN_FUNCTION_ID                   PINMUX_GPIO26_GPIO26     /*!< Pin function id */

#define BOARD_PUSH_SW4_PIN                                                   22   /*!< Routed pin */
#define BOARD_PUSH_SW4_PIN_FUNCTION_ID                   PINMUX_GPIO22_GPIO22     /*!< Pin function id */

#define BOARD_REQ_PIN                                                        39   /*!< Routed pin */

#define BOARD_PRI_PIN                                                        40   /*!< Routed pin */

#define BOARD_GRANT_PIN                                                      41   /*!< Routed pin */
#define BOARD_COEX_PIN_FUNCTION_ID                       PINMUX_FUNC7             /*!< Pin function id */

#define BOARD_LED_YELLOW_PIN 41                               /*!< Routed pin */
#define BOARD_LED_YELLOW_PIN_FUNCTION_ID PINMUX_GPIO41_GPIO41 /*!< Pin function id */

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
