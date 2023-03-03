/*! *********************************************************************************
* Copyright (c) 2015, Freescale Semiconductor, Inc.
* Copyright 2016-2021 NXP
* All rights reserved.
*
* \file
*
* SPDX-License-Identifier: BSD-3-Clause
********************************************************************************** */


#include "fsl_device_registers.h"
#include "fsl_common.h"
#include "fsl_clock.h"

#if USE_SDK_OSA
#include "frm_os_abstraction.h"
#else
    extern void FRM_OSA_InterruptEnable(void);
    extern void FRM_OSA_InterruptDisable(void);
    extern void FRM_OSA_InstallIntHandler(uint32_t IRQNumber, void (*handler)(void));
#endif

#include "UART_Serial_Adapter.h"
#include "pin_mux.h"
#include "board.h"

#if FSL_FEATURE_SOC_UART_COUNT
#include "fsl_uart.h"
#endif

#if FSL_FEATURE_SOC_LPUART_COUNT
#include "fsl_lpuart.h"
#endif

#if FSL_FEATURE_SOC_LPSCI_COUNT
#include "fsl_lpsci.h"
#endif

#if FSL_FEATURE_SOC_USART_COUNT
#ifndef gUartDMA_d
#define gUartDMA_d (0u)
#endif
#include "fsl_usart.h"
#if gUartDMA_d
#include "fsl_usart_dma.h"
#include "fsl_dma.h"
#include "TimersManager.h"
#endif
#endif

/*! *********************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
********************************************************************************** */
#if FSL_FEATURE_SOC_LPUART_COUNT
static LPUART_Type * mLpuartBase[] = LPUART_BASE_PTRS;
static uartState_t * pLpuartStates[FSL_FEATURE_SOC_LPUART_COUNT];
static void LPUART_Common_ISR(uint32_t instance);
#endif

#if FSL_FEATURE_SOC_UART_COUNT
static UART_Type * mUartBase[] = UART_BASE_PTRS;
static uartState_t * pUartStates[FSL_FEATURE_SOC_UART_COUNT];
static void UART_Common_ISR(uint32_t instance);
#endif

#if FSL_FEATURE_SOC_LPSCI_COUNT
static UART0_Type * mLpsciBase[] = UART0_BASE_PTRS;
static uartState_t * pLpsciStates[FSL_FEATURE_SOC_LPSCI_COUNT];
static void LPSCI_Common_ISR(uint32_t instance);
#endif

#if FSL_FEATURE_SOC_USART_COUNT
static USART_Type  *mUsartBase[]    = USART_BASE_PTRS;
static uartState_t *pUsartStates[FSL_FEATURE_SOC_USART_COUNT];
#if gUartDMA_d
static const IRQn_Type s_dmaIRQNumber[] = DMA_IRQS;
#else
static IRQn_Type    mUsartIrqs[]    = USART_IRQS;
static void USART_ISR(void);
#endif
#endif

/*! *********************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
********************************************************************************** */

/************************************************************************************/
/*                                     LPUART                                       */
/************************************************************************************/
uint32_t LPUART_Initialize(uint32_t instance, uartState_t *pState)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_LPUART_COUNT
    LPUART_Type * base;
    lpuart_config_t config;
    IRQn_Type irqs[] = LPUART_RX_TX_IRQS;

    if( (instance >= FSL_FEATURE_SOC_LPUART_COUNT) || (NULL == pState) )
    {
        status = gUartInvalidParameter_c;
    }
   else
   {
       base = mLpuartBase[instance];
       pLpuartStates[instance] = pState;
       pState->rxCbParam = 0;
       pState->txCbParam = 0;
       pState->pRxData = NULL;
       pState->pTxData = NULL;
       pState->rxSize = 0;
       pState->txSize = 0;

       BOARD_InitLPUART();
       LPUART_GetDefaultConfig(&config);
       config.enableRx = 1;
       config.enableTx = 1;
       LPUART_Init(base, &config, BOARD_GetLpuartClock(instance));
       LPUART_EnableInterrupts(base, kLPUART_RxDataRegFullInterruptEnable);
       //FRM_OSA_InstallIntHandler(irqs[instance], LPUART_ISR);
#if defined(FSL_FEATURE_SOC_INTMUX_COUNT) && FSL_FEATURE_SOC_INTMUX_COUNT
       if( irqs[instance] < FSL_FEATURE_INTMUX_IRQ_START_INDEX )
#endif
       {
           NVIC_SetPriority(irqs[instance], gUartIsrPrio_c >> (8 - __NVIC_PRIO_BITS));
           NVIC_EnableIRQ(irqs[instance]);
       }
   }
#endif
    return status;
}

/************************************************************************************/
uint32_t LPUART_SetBaudrate(uint32_t instance, uint32_t baudrate)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_LPUART_COUNT
    if( instance >= FSL_FEATURE_SOC_LPUART_COUNT )
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        LPUART_SetBaudRate(mLpuartBase[instance], baudrate, BOARD_GetLpuartClock(instance));
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t LPUART_SendData(uint32_t instance, uint8_t* pData, uint32_t size)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_LPUART_COUNT
    LPUART_Type * base;

    if( (instance >= FSL_FEATURE_SOC_LPUART_COUNT) || (0 == size) || (NULL == pData) )
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        base = mLpuartBase[instance];

        FRM_OSA_InterruptDisable();
        if( pLpuartStates[instance]->txSize )
        {
            FRM_OSA_InterruptEnable();
            status = gUartBusy_c;
        }
        else
        {
            while( !(kLPUART_TxDataRegEmptyFlag & LPUART_GetStatusFlags(base)) ) {}

            LPUART_WriteByte(base, *pData);
            pLpuartStates[instance]->pTxData = pData+1;
            pLpuartStates[instance]->txSize = size-1;

            LPUART_ClearStatusFlags(base, kLPUART_TxDataRegEmptyFlag);
            LPUART_EnableInterrupts(base, kLPUART_TxDataRegEmptyInterruptEnable);

            FRM_OSA_InterruptEnable();
        }
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t LPUART_ReceiveData(uint32_t instance, uint8_t* pData, uint32_t size)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_LPUART_COUNT
    if( (instance >= FSL_FEATURE_SOC_LPUART_COUNT)  || (0 == size) || (NULL == pData) )
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        FRM_OSA_InterruptDisable();
        if( pLpuartStates[instance]->rxSize )
        {
            status = gUartBusy_c;
        }
        else
        {
            pLpuartStates[instance]->rxSize = size;
            pLpuartStates[instance]->pRxData = pData;
        }
        FRM_OSA_InterruptEnable();
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t  LPUART_InstallRxCalback(uint32_t instance, uartCallback_t cb, uint32_t cbParam)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_LPUART_COUNT
    if( instance >= FSL_FEATURE_SOC_LPUART_COUNT )
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        pLpuartStates[instance]->rxCb = cb;
        pLpuartStates[instance]->rxCbParam = cbParam;
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t  LPUART_InstallTxCalback(uint32_t instance, uartCallback_t cb, uint32_t cbParam)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_LPUART_COUNT
    if( instance >= FSL_FEATURE_SOC_LPUART_COUNT )
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        pLpuartStates[instance]->txCb = cb;
        pLpuartStates[instance]->txCbParam = cbParam;
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t LPUART_IsTxActive(uint32_t instance)
{
    uint32_t status = 0;
#if FSL_FEATURE_SOC_LPUART_COUNT
    if( instance < FSL_FEATURE_SOC_LPUART_COUNT )
    {
        if( pLpuartStates[instance]->txSize )
        {
            status = 1;
        }
        else
        {
            status = !(LPUART_GetStatusFlags(mLpuartBase[instance]) & kLPUART_TransmissionCompleteFlag);
        }
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t LPUART_EnableLowPowerWakeup(uint32_t instance)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_LPUART_COUNT
    status = LPUART_DisableLowPowerWakeup(instance);
    if( gUartSuccess_c == status )
    {
        LPUART_EnableInterrupts(mLpuartBase[instance], kLPUART_RxActiveEdgeInterruptEnable);
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t LPUART_DisableLowPowerWakeup(uint32_t instance)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_LPUART_COUNT
    LPUART_Type * base;

    if( instance >= FSL_FEATURE_SOC_LPUART_COUNT )
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        base = mLpuartBase[instance];
        LPUART_DisableInterrupts(base, kLPUART_RxActiveEdgeInterruptEnable);
        LPUART_ClearStatusFlags(base, kLPUART_RxActiveEdgeFlag);
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t LPUART_IsWakeupSource(uint32_t instance)
{
    uint32_t status = 0;
#if FSL_FEATURE_SOC_LPUART_COUNT
    if( instance < FSL_FEATURE_SOC_LPUART_COUNT )
    {
        status = !!(LPUART_GetStatusFlags(mLpuartBase[instance]) & kLPUART_RxActiveEdgeFlag);
    }
#endif
    return status;
}

/************************************************************************************/
/*                                      UART                                        */
/************************************************************************************/
uint32_t UART_Initialize(uint32_t instance, uartState_t *pState)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_UART_COUNT
    UART_Type * base;
    uart_config_t config;
    IRQn_Type irqs[] = UART_IRQS;

    if( (instance >= FSL_FEATURE_SOC_UART_COUNT) || (NULL == pState) )
    {
        status = gUartInvalidParameter_c;
    }
   else
   {
       base = mUartBase[instance];
       pUartStates[instance] = pState;
       pState->rxCbParam = 0;
       pState->txCbParam = 0;
       pState->pRxData = NULL;
       pState->pTxData = NULL;
       pState->rxSize = 0;
       pState->txSize = 0;

       UART_GetDefaultConfig(&config);
       config.enable = 1;
       UART_Init(base, &config, CLOCK_GetUartClkFreq(instance));
       UART_EnableInterrupts(base, kUART_RxDataReadyInterruptEnable | kUART_RxStatusInterruptEnable);
//       FRM_OSA_InstallIntHandler(irqs[instance], UART_ISR);
#if defined(FSL_FEATURE_SOC_INTMUX_COUNT) && FSL_FEATURE_SOC_INTMUX_COUNT
       if( irqs[instance] < FSL_FEATURE_INTMUX_IRQ_START_INDEX )
#endif
       {
           NVIC_SetPriority(irqs[instance], gUartIsrPrio_c >> (8 - __NVIC_PRIO_BITS));
           NVIC_EnableIRQ(irqs[instance]);
       }
   }
#endif
    return status;
}

/************************************************************************************/
uint32_t UART_SetBaudrate(uint32_t instance, uint32_t baudrate)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_UART_COUNT
    if( instance >= FSL_FEATURE_SOC_UART_COUNT )
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        UART_SetBaudRate(mUartBase[instance], baudrate, CLOCK_GetUartClkFreq(instance));
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t UART_SendData(uint32_t instance, uint8_t* pData, uint32_t size)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_UART_COUNT
    UART_Type * base;

    if( instance >= FSL_FEATURE_SOC_UART_COUNT || !size || !pData )
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        base = mUartBase[instance];

        FRM_OSA_InterruptDisable();
        if( pUartStates[instance]->txSize )
        {
            FRM_OSA_InterruptEnable();
            status = gUartBusy_c;
        }
        else
        {
            while( !(kUART_TxEmptyInterruptFlag & UART_GetStatusFlags(base)) ) {}

            pUartStates[instance]->txSize = size-1;
            pUartStates[instance]->pTxData = pData+1;
            UART_WriteByte(base, *pData);

            UART_EnableInterrupts(base, kUART_TxDataRequestInterruptEnable);

            FRM_OSA_InterruptEnable();
        }
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t UART_ReceiveData(uint32_t instance, uint8_t* pData, uint32_t size)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_UART_COUNT
    if( instance >= FSL_FEATURE_SOC_UART_COUNT  || !size || !pData )
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        FRM_OSA_InterruptDisable();
        if( pUartStates[instance]->rxSize )
        {
            status = gUartBusy_c;
        }
        else
        {
            pUartStates[instance]->rxSize = size;
            pUartStates[instance]->pRxData = pData;
        }
        FRM_OSA_InterruptEnable();
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t UART_InstallRxCalback(uint32_t instance, uartCallback_t cb, uint32_t cbParam)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_UART_COUNT
    if( instance >= FSL_FEATURE_SOC_UART_COUNT )
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        pUartStates[instance]->rxCb = cb;
        pUartStates[instance]->rxCbParam = cbParam;
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t UART_InstallTxCalback(uint32_t instance, uartCallback_t cb, uint32_t cbParam)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_UART_COUNT
    if( instance >= FSL_FEATURE_SOC_UART_COUNT )
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        pUartStates[instance]->txCb = cb;
        pUartStates[instance]->txCbParam = cbParam;
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t UART_IsTxActive(uint32_t instance)
{
    uint32_t status = 0;
#if FSL_FEATURE_SOC_UART_COUNT
    if( instance < FSL_FEATURE_SOC_UART_COUNT )
    {
        if( pUartStates[instance]->txSize )
        {
            status = 1;
        }
        else
        {
            status = !(UART_GetStatusFlags(mUartBase[instance]) & kUART_TxEmptyInterruptFlag);
        }
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t UART_EnableLowPowerWakeup(uint32_t instance)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_UART_COUNT
    status = UART_DisableLowPowerWakeup(instance);
    if( gUartSuccess_c == status )
    {
        UART_EnableInterrupts(mUartBase[instance], kUART_RxDataReadyInterruptEnable);
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t UART_DisableLowPowerWakeup(uint32_t instance)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_UART_COUNT
    UART_Type * base;

    if( instance >= FSL_FEATURE_SOC_UART_COUNT )
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        base = mUartBase[instance];
        UART_DisableInterrupts(base, kUART_RxDataReadyInterruptEnable);
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t UART_IsWakeupSource(uint32_t instance)
{
    uint32_t status = 0;
#if FSL_FEATURE_SOC_UART_COUNT
    if( instance < FSL_FEATURE_SOC_UART_COUNT )
    {
        status = !!(UART_GetStatusFlags(mUartBase[instance]) & kUART_RxDataReadyInterruptFlag);
    }
#endif
    return status;
}


/************************************************************************************/
/*                                      LPSCI                                       */
/************************************************************************************/
uint32_t LPSCI_Initialize(uint32_t instance, uartState_t *pState)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_LPSCI_COUNT
    UART0_Type * base;
    lpsci_config_t config;
    IRQn_Type irqs[] = UART0_RX_TX_IRQS;

    if( (instance >= FSL_FEATURE_SOC_LPSCI_COUNT) || (NULL == pState) )
    {
        status = gUartInvalidParameter_c;
    }
   else
   {
       base = mLpsciBase[instance];
       pLpsciStates[instance] = pState;
       pState->rxCbParam = 0;
       pState->txCbParam = 0;
       pState->pRxData = NULL;
       pState->pTxData = NULL;
       pState->rxSize = 0;
       pState->txSize = 0;

       BOARD_InitLPSCI();
       LPSCI_GetDefaultConfig(&config);
       config.enableRx = 1;
       config.enableTx = 1;
       LPSCI_Init(base, &config, BOARD_GetLpsciClock(instance));
       LPSCI_EnableInterrupts(base, kLPSCI_RxDataRegFullInterruptEnable);
//       FRM_OSA_InstallIntHandler(irqs[instance], LPSCI_ISR);
#if defined(FSL_FEATURE_SOC_INTMUX_COUNT) && FSL_FEATURE_SOC_INTMUX_COUNT
       if( irqs[instance] < FSL_FEATURE_INTMUX_IRQ_START_INDEX )
#endif
       {
           NVIC_SetPriority(irqs[instance], gUartIsrPrio_c >> (8 - __NVIC_PRIO_BITS));
           NVIC_EnableIRQ(irqs[instance]);
       }
   }
#endif
    return status;
}

/************************************************************************************/
uint32_t LPSCI_SetBaudrate(uint32_t instance, uint32_t baudrate)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_LPSCI_COUNT
    if( instance >= FSL_FEATURE_SOC_LPSCI_COUNT )
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        LPSCI_SetBaudRate(mLpsciBase[instance], baudrate, BOARD_GetLpsciClock(instance));
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t LPSCI_SendData(uint32_t instance, uint8_t* pData, uint32_t size)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_LPSCI_COUNT
    UART0_Type * base;

    if( instance >= FSL_FEATURE_SOC_LPSCI_COUNT || !size || !pData )
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        base = mLpsciBase[instance];

        FRM_OSA_InterruptDisable();
        if( pLpsciStates[instance]->txSize )
        {
            FRM_OSA_InterruptEnable();
            status = gUartBusy_c;
        }
        else
        {
            while( !(kLPSCI_TxDataRegEmptyFlag & LPSCI_GetStatusFlags(base)) ) {}

            pLpsciStates[instance]->txSize = size-1;
            pLpsciStates[instance]->pTxData = pData+1;
            LPSCI_WriteByte(base, *pData);

            LPSCI_ClearStatusFlags(base, kLPSCI_TxDataRegEmptyFlag);
            LPSCI_EnableInterrupts(base, kLPSCI_TxDataRegEmptyInterruptEnable);

            FRM_OSA_InterruptEnable();
        }
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t LPSCI_ReceiveData(uint32_t instance, uint8_t* pData, uint32_t size)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_LPSCI_COUNT
    if( instance >= FSL_FEATURE_SOC_LPSCI_COUNT  || !size || !pData )
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        FRM_OSA_InterruptDisable();
        if( pLpsciStates[instance]->rxSize )
        {
            status = gUartBusy_c;
        }
        else
        {
            pLpsciStates[instance]->rxSize = size;
            pLpsciStates[instance]->pRxData = pData;
        }
        FRM_OSA_InterruptEnable();
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t LPSCI_InstallRxCalback(uint32_t instance, uartCallback_t cb, uint32_t cbParam)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_LPSCI_COUNT
    if( instance >= FSL_FEATURE_SOC_LPSCI_COUNT )
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        pLpsciStates[instance]->rxCb = cb;
        pLpsciStates[instance]->rxCbParam = cbParam;
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t LPSCI_InstallTxCalback(uint32_t instance, uartCallback_t cb, uint32_t cbParam)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_LPSCI_COUNT
    if( instance >= FSL_FEATURE_SOC_LPSCI_COUNT )
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        pLpsciStates[instance]->txCb = cb;
        pLpsciStates[instance]->txCbParam = cbParam;
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t LPSCI_IsTxActive(uint32_t instance)
{
    uint32_t status = 0;
#if FSL_FEATURE_SOC_LPSCI_COUNT
    if( instance < FSL_FEATURE_SOC_LPSCI_COUNT )
    {
        if( pLpsciStates[instance]->txSize )
        {
            status = 1;
        }
        else
        {
            status = !(LPSCI_GetStatusFlags(mLpsciBase[instance]) & kLPSCI_TransmissionCompleteFlag);
        }
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t LPSCI_EnableLowPowerWakeup(uint32_t instance)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_LPSCI_COUNT
    status = LPSCI_DisableLowPowerWakeup(instance);
    if( gUartSuccess_c == status )
    {
        LPSCI_EnableInterrupts(mLpsciBase[instance], kLPSCI_RxActiveEdgeInterruptEnable);
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t LPSCI_DisableLowPowerWakeup(uint32_t instance)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_LPSCI_COUNT
    UART0_Type * base;

    if( instance >= FSL_FEATURE_SOC_LPSCI_COUNT )
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        base = mLpsciBase[instance];
        LPSCI_DisableInterrupts(base, kLPSCI_RxActiveEdgeInterruptEnable);
        LPSCI_ClearStatusFlags(base, kLPSCI_RxActiveEdgeFlag);
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t LPSCI_IsWakeupSource(uint32_t instance)
{
    uint32_t status = 0;
#if FSL_FEATURE_SOC_LPSCI_COUNT
    if( instance < FSL_FEATURE_SOC_LPSCI_COUNT )
    {
        status = !!(LPSCI_GetStatusFlags(mLpsciBase[instance]) & kLPSCI_RxActiveEdgeFlag);
    }
#endif
    return status;
}

/************************************************************************************/
/*                                      USART                                        */
/************************************************************************************/

/*! *********************************************************************************
* \brief   Initialize pins for usart.
*
* \param[in] instance       the instance of the HW module (ex: if UART1 is used, this value should be 1)
*
* \note For JN518x uart, felxcomm initialization must be done before pin configuration,
*         or uart will output incorrect data ('\0') after chip reset
*
********************************************************************************** */
#if FSL_FEATURE_SOC_USART_COUNT
void USART_PinInitialize(uint32_t instance)
{
    BOARD_Init_UART_Pins(instance);
}
#endif

#if FSL_FEATURE_SOC_USART_COUNT
#if gUartDMA_d
#define USART_TX_DMA_CHANNEL 1
#define USART_RX_DMA_CHANNEL 0

#ifndef USART_DMA_TIMEOUT
#define USART_DMA_TIMEOUT (1U)
#endif

/*! @brief Static table of descriptors */
#if defined(__ICCARM__)
#pragma data_alignment              = 16
dma_descriptor_t g_descriptorPtr[1] = {0};
#elif defined(__CC_ARM) || defined(__ARMCC_VERSION)
__attribute__((aligned(16))) dma_descriptor_t g_descriptorPtr[1] = {0};
#elif defined(__GNUC__)
__attribute__((aligned(16))) dma_descriptor_t g_descriptorPtr[1] = {0};
#endif

typedef struct uartDma_handle {
        usart_dma_handle_t uartDmaHandle;
        dma_handle_t uartTxDmaHandle;
        dma_handle_t uartRxDmaHandle;
}uartDma_handle_t;

static uartDma_handle_t mUsartDmaHndle[FSL_FEATURE_SOC_USART_COUNT];
static tmrTimerID_t mUARTTimerID[FSL_FEATURE_SOC_USART_COUNT] = {gTmrInvalidTimerID_c};
static void USART_StartRingBufferDMA(uint32_t instance)
{
    dma_transfer_config_t transferConfig;
    uartState_t *pState =  pUsartStates[instance];
    serialRingState_t  * rx_ring = &pState->rx_ring;

    USART_Type *base = mUsartBase[instance];
    DMA_PrepareTransfer(&transferConfig, (void *)&base->FIFORD, rx_ring->buffer, 1, rx_ring->max, kDMA_PeripheralToMemory,
                        &g_descriptorPtr[0]);
    DMA_SubmitTransfer(&mUsartDmaHndle[instance].uartRxDmaHandle, &transferConfig);
    transferConfig.xfercfg.intA = true;
    transferConfig.xfercfg.intB = false;
    DMA_CreateDescriptor(&g_descriptorPtr[0], &transferConfig.xfercfg, (void *)&base->FIFORD, rx_ring->buffer,
                         &g_descriptorPtr[0]);
     DMA_StartTransfer(&mUsartDmaHndle[instance].uartRxDmaHandle);
}
/* Get how many characters stored in ring buffer. */
static uint32_t USART_GetRingBufferLengthDMA(uint32_t instance)
{
    uartState_t *pState =  pUsartStates[instance];
    serialRingState_t  * rx_ring = &pState->rx_ring; 
    uint32_t regPrimask     = 0U;
    uint32_t RemainingBytes = 0U;
    uint32_t receivedBytes  = 0U;

    /* Disable IRQ, protect ring buffer. */
    regPrimask     = DisableGlobalIRQ();
    RemainingBytes = DMA_GetRemainingBytes(DMA0, USART_RX_DMA_CHANNEL + 2*instance);

    /* When all transfers are completed, XFERCOUNT value changes from 0 to 0x3FF. The last value
     * 0x3FF does not mean there are 1024 transfers left to complete.It means all data transfer
     * has completed.
     */
    if (RemainingBytes == 1024)
    {
        RemainingBytes = 0U;
    }

    receivedBytes = rx_ring->max - RemainingBytes;

    /* If the received bytes is less than index value, it means the ring buffer has reached it boundary. */
    if (receivedBytes < rx_ring->in)
    {
        receivedBytes += rx_ring->max;
    }

    receivedBytes -= rx_ring->in;
    rx_ring->in += receivedBytes;
    if (rx_ring->in >= rx_ring->max)
    {
      rx_ring->in -= rx_ring->max;
    }
    rx_ring->space_left -= receivedBytes;

    /* Enable IRQ if previously enabled. */
    EnableGlobalIRQ(regPrimask);

    return receivedBytes;
}
static void UART_DataRxTimeout
(
    uint32_t instance
)
{
    uartState_t *pState =  pUsartStates[instance];
    uint32_t rxcount = 0;

    rxcount = USART_GetRingBufferLengthDMA(instance);
    if (rxcount != 0) 
    {
        if (NULL != pState->rxCb)
        {
            pState->rxCb(pState);
        }
    }
    TMR_StartSingleShotTimer(mUARTTimerID[instance], USART_DMA_TIMEOUT, (pfTmrCallBack_t)UART_DataRxTimeout, (void *)(instance));

}
static void USART_UserCallback(USART_Type *base, usart_dma_handle_t *handle, status_t status, void *userData)
{
    userData = userData;
    uint32_t instance = ~0UL;
    uartState_t *pState = NULL;

    do {
            /* Get instance */
            for (instance = 0; instance < FSL_FEATURE_SOC_USART_COUNT; instance++)
            {
                if (base == mUsartBase[instance])
                    break;
            }
            if (instance >= FSL_FEATURE_SOC_USART_COUNT)
                break;
            pState = pUsartStates[instance];
            if (kStatus_USART_TxIdle == status)
            {
                pState->txSize = 0;
                if (NULL != pState->txCb)
                {
                    pState->txCb(pState);
                }
            }
        }while(0);
}
#endif
#define IS_USART_HW_FLOW_CONTROL_SET(base)  (base->CFG & USART_CFG_CTSEN(1))
#endif

uint32_t USART_Initialize(uint32_t instance, uartState_t *pState)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_USART_COUNT
    USART_Type *base;
    usart_config_t config;

    if ((instance >= FSL_FEATURE_SOC_USART_COUNT) || (NULL == pState))
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        base = mUsartBase[instance];
        pUsartStates[instance] = pState;
        pState->rxCbParam = 0;
        pState->txCbParam = 0;
        pState->pRxData = NULL;
        pState->pTxData = NULL;
        pState->rxSize = 0;
        pState->txSize = 0;

        USART_GetDefaultConfig(&config);
        config.enableRx = 1;
        config.enableTx = 1;
#if gUartHwFlowControl_d
        config.enableHardwareFlowControl = true;
#endif
#if gUartDMA_d
        static uint8_t dmaInited = 0;
        if (dmaInited == 0)
        {
            /* Configure DMA. */
            DMA_Init(DMA0);
            dmaInited =  1U;
        }
        DMA_EnableChannel(DMA0, USART_TX_DMA_CHANNEL + 2*instance);
        DMA_EnableChannel(DMA0, USART_RX_DMA_CHANNEL + 2*instance);

        DMA_CreateHandle(&mUsartDmaHndle[instance].uartTxDmaHandle, DMA0, USART_TX_DMA_CHANNEL + 2*instance);
        DMA_CreateHandle(&mUsartDmaHndle[instance].uartRxDmaHandle, DMA0, USART_RX_DMA_CHANNEL + 2*instance);

        /* Create UART DMA handle. */
        USART_TransferCreateHandleDMA(base, &mUsartDmaHndle[instance].uartDmaHandle, USART_UserCallback, NULL, &mUsartDmaHndle[instance].uartTxDmaHandle,
                                     NULL);
        USART_Init(base, &config, BOARD_GetUsartClock(instance));
        USART_EnableRxDMA(base, true);
        USART_PinInitialize(instance);
        mUARTTimerID[instance] = TMR_AllocateTimer();
        NVIC_SetPriority(s_dmaIRQNumber[0], gUartIsrPrio_c >> (8 - __NVIC_PRIO_BITS));
#else
        USART_Init(base, &config, BOARD_GetUsartClock(instance));

        USART_PinInitialize(instance);
        USART_EnableInterrupts(base, kUSART_RxLevelInterruptEnable);
        FRM_OSA_InstallIntHandler(mUsartIrqs[instance], USART_ISR);
        NVIC_SetPriority(mUsartIrqs[instance], gUartIsrPrio_c >> (8 - __NVIC_PRIO_BITS));
        NVIC_EnableIRQ(mUsartIrqs[instance]);
#endif
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t USART_SetBaudrate(uint32_t instance, uint32_t baudrate)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_USART_COUNT
    if (instance >= FSL_FEATURE_SOC_USART_COUNT)
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        status = USART_SetBaudRate(mUsartBase[instance], baudrate, BOARD_GetUsartClock(instance));
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t USART_SendData(uint32_t instance, uint8_t *pData, uint32_t size)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_USART_COUNT
#if gUartDMA_d 
    usart_transfer_t sendXfer;
#endif
  
    
    USART_Type *base;

    if (instance >= FSL_FEATURE_SOC_USART_COUNT || !size || !pData)
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        base = mUsartBase[instance];

        FRM_OSA_InterruptDisable();
        if (pUsartStates[instance]->txSize)
        {
            FRM_OSA_InterruptEnable();
            status = gUartBusy_c;
        }
        else
        {
#if gUartDMA_d  
            sendXfer.data        = pData;
            sendXfer.dataSize    = size;
            USART_TransferSendDMA(base, &mUsartDmaHndle[instance].uartDmaHandle, &sendXfer); 
            FRM_OSA_InterruptEnable();
#else
            while (!(kUSART_TxFifoEmptyFlag & USART_GetStatusFlags(base)))
            {
            }

            pUsartStates[instance]->txSize = size - 1;
            pUsartStates[instance]->pTxData = pData + 1;
            USART_WriteByte(base, *pData);
            FRM_OSA_InterruptEnable();

            USART_ClearStatusFlags(base, kUSART_TxFifoEmptyFlag);
            USART_EnableInterrupts(base, kUSART_TxLevelInterruptEnable);
#endif
        }
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t USART_ReceiveData(uint32_t instance, uint8_t *pData, uint32_t size)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_USART_COUNT

    if (instance >= FSL_FEATURE_SOC_USART_COUNT || !size || !pData)
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        FRM_OSA_InterruptDisable();

        if (pUsartStates[instance]->rxSize)
        {
            status = gUartBusy_c;
        }
        else
        {
#if gUartDMA_d  

#else
            pUsartStates[instance]->rxSize = size;
            pUsartStates[instance]->pRxData = pData;
#endif

        }

        FRM_OSA_InterruptEnable();
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t USART_InstallRxCalback(uint32_t instance, uartCallback_t cb, uint32_t cbParam)
{
    uint32_t status = gUartSuccess_c;
    do
    {
#if FSL_FEATURE_SOC_USART_COUNT
        if (instance >= FSL_FEATURE_SOC_USART_COUNT)
        {
            status = gUartInvalidParameter_c;
            break;
        }
        pUsartStates[instance]->rxCb = cb;
        pUsartStates[instance]->rxCbParam = cbParam;
#if gUartDMA_d
        USART_StartRingBufferDMA(instance);
        TMR_StartSingleShotTimer(mUARTTimerID[instance], USART_DMA_TIMEOUT, (pfTmrCallBack_t)UART_DataRxTimeout, (void *)instance);
#endif
#endif
    }while(0);
    return status;
}

/************************************************************************************/
uint32_t USART_InstallTxCalback(uint32_t instance, uartCallback_t cb, uint32_t cbParam)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_USART_COUNT
    if (instance >= FSL_FEATURE_SOC_USART_COUNT)
    {
        status = gUartInvalidParameter_c;
    }
    else
    {
        pUsartStates[instance]->txCb = cb;
        pUsartStates[instance]->txCbParam = cbParam;
    }
#endif
    return status;
}

void USART_RxIntCtrl(uint32_t instance, bool_t fc_on)
{
#if FSL_FEATURE_SOC_USART_COUNT
    do {
        USART_Type *base;
        base = mUsartBase[instance];
        SERIAL_DBG_LOG("fc_on=%d", fc_on);

        if (!IS_USART_HW_FLOW_CONTROL_SET(base))
        {
            /* Would make no sense if the USART does not support HW Flow Control
             */
            break;
        }
        if (fc_on)
        {
            USART_EnableInterrupts(base, kUSART_RxLevelInterruptEnable);
        }
        else
        {
            USART_DisableInterrupts(base, kUSART_RxLevelInterruptEnable);
        }
   } while (0);
#endif
}


/************************************************************************************/
uint32_t USART_IsTxActive(uint32_t instance)
{
    uint32_t status = 0;
#if FSL_FEATURE_SOC_USART_COUNT
    if (instance < FSL_FEATURE_SOC_USART_COUNT)
    {
        if (pUsartStates[instance]->txSize)
        {
            status = 1;
        }
        else
        {
            status = !(USART_GetStatusFlags(mUsartBase[instance]) & kUSART_TxFifoEmptyFlag);
        }
    }
#endif
    return status;
}

/************************************************************************************/
uint32_t USART_EnableLowPowerWakeup(uint32_t instance)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_USART_COUNT
    // TODO
#endif
    return status;
}

/************************************************************************************/
uint32_t USART_DisableLowPowerWakeup(uint32_t instance)
{
    uint32_t status = gUartSuccess_c;
#if FSL_FEATURE_SOC_USART_COUNT
    // TODO
#endif
    return status;
}

/************************************************************************************/
uint32_t USART_IsWakeupSource(uint32_t instance)
{
    uint32_t status = 0;
#if FSL_FEATURE_SOC_USART_COUNT
    if( instance < FSL_FEATURE_SOC_USART_COUNT )
    {
        status = ((mUsartBase[instance]->STAT & USART_STAT_START_MASK) >> USART_STAT_START_SHIFT);
    }
#endif
    return status;
}

/*! *********************************************************************************
*************************************************************************************
* Private functions
*************************************************************************************
********************************************************************************* */
#if FSL_FEATURE_SOC_LPUART_COUNT
static void LPUART_Common_ISR(uint32_t instance)
{
    uint32_t interrupts;
    LPUART_Type * base;
    uartState_t * pState;

    base = mLpuartBase[instance];
    pState = pLpuartStates[instance];
    interrupts = LPUART_GetEnabledInterrupts(base);

    /* Check if data was received */
    if( (kLPUART_RxDataRegFullFlag) & LPUART_GetStatusFlags(base) )
    {
        uint8_t data = LPUART_ReadByte(base);
        LPUART_ClearStatusFlags(base, kLPUART_RxDataRegFullFlag);

        if( pState->rxSize )
        {
            pState->rxSize--;
        }

        if( pState->pRxData )
        {
            *(pState->pRxData) = data;
            pState->pRxData++;
        }

        if( (0 == pState->rxSize) && (NULL != pState->rxCb) )
        {
            pState->rxCb(pState);
        }
    }

    /* Check if data Tx has end */
    if( (kLPUART_TxDataRegEmptyFlag & LPUART_GetStatusFlags(base)) &&
       (kLPUART_TxDataRegEmptyInterruptEnable & interrupts) )
    {
        if( pState->txSize )
        {
            pState->txSize--;

            LPUART_WriteByte(base, *(pState->pTxData++));
        }
        else if( 0 == pState->txSize )
        {
            LPUART_DisableInterrupts(base, kLPUART_TxDataRegEmptyInterruptEnable);

            if( NULL != pState->txCb )
            {
                pState->txCb(pState);
            }
        }
    }

    if( kLPUART_RxOverrunFlag & LPUART_GetStatusFlags(base) )
    {
        LPUART_ClearStatusFlags(base, kLPUART_RxOverrunFlag);
    }
}

#if FSL_FEATURE_LPUART_HAS_SHARED_IRQ0_IRQ1
void LPUART0_LPUART1_IRQHandler(void)
{
    const clock_ip_name_t lpuartClock[] = LPUART_CLOCKS;

    if (CLOCK_isEnabledClock(lpuartClock[0]))
    {
        if ((LPUART_STAT_OR_MASK & LPUART0->STAT) ||
            ((LPUART_STAT_RDRF_MASK & LPUART0->STAT) && (LPUART_CTRL_RIE_MASK & LPUART0->CTRL)) ||
            ((LPUART0->STAT & LPUART_STAT_TDRE_MASK) && (LPUART0->CTRL & LPUART_CTRL_TIE_MASK)))
        {
             LPUART_Common_ISR(0);
        }
    }

    if (CLOCK_isEnabledClock(lpuartClock[1]))
    {
        if ((LPUART_STAT_OR_MASK & LPUART1->STAT) ||
            ((LPUART_STAT_RDRF_MASK & LPUART1->STAT) && (LPUART_CTRL_RIE_MASK & LPUART1->CTRL)) ||
            ((LPUART1->STAT & LPUART_STAT_TDRE_MASK) && (LPUART1->CTRL & LPUART_CTRL_TIE_MASK)))
        {
             LPUART_Common_ISR(1);
        }
    }
}
#endif

#if defined(LPUART0)
void LPUART0_IRQHandler(void)
{
    LPUART_Common_ISR(0);
}
#endif

#if defined(LPUART1)
void LPUART1_IRQHandler(void)
{
    LPUART_Common_ISR(1);
}
#endif

#if defined(LPUART2)
void LPUART2_IRQHandler(void)
{
    LPUART_Common_ISR(2);
}
#endif

#if defined(LPUART3)
void LPUART3_IRQHandler(void)
{
    LPUART_Common_ISR(3);
}
#endif

#if defined(LPUART4)
void LPUART4_IRQHandler(void)
{
    LPUART_Common_ISR(4);
}
#endif

#if defined(LPUART5)
void LPUART5_IRQHandler(void)
{
    LPUART_Common_ISR(5);
}
#endif
#endif /* #if FSL_FEATURE_SOC_LPUART_COUNT */

/************************************************************************************/
#if FSL_FEATURE_SOC_UART_COUNT
static void UART_Common_ISR(uint32_t instance)
{
    UART_Type * base;
    uartState_t * pState;

    base = mUartBase[instance];
    pState = pUartStates[instance];
    /* Check if data was received */
    if( (kUART_RxDataReadyInterruptFlag) & UART_GetStatusFlags(base) )
    {
        uint8_t data = UART_ReadByte(base);

        if( pState->rxSize )
        {
            pState->rxSize--;
        }

        if( pState->pRxData )
        {
            *(pState->pRxData) = data;
            pState->pRxData++;
        }

        if( (0 == pState->rxSize) && (NULL != pState->rxCb) )
        {
            pState->rxCb(pState);
        }
    }
    /* Check if data Tx has end */
    if( (kUART_TxEmptyInterruptFlag) & UART_GetStatusFlags(base) &&
       (kUART_TxDataRequestInterruptEnable) & UART_GetEnabledInterrupts(base) )
    {
        if( pState->txSize )
        {
            pState->txSize--;

            UART_WriteByte(base, *(pState->pTxData++));
        }
        else if( 0 == pState->txSize )
        {
            UART_DisableInterrupts(base, kUART_TxDataRequestInterruptEnable);

            if( NULL != pState->txCb )
            {
                pState->txCb(pState);
            }
        }
    }
}


void UART0_IRQHandler(void)
{
    UART_Common_ISR(0);
}

#if defined(UART1)
void UART1_RX_TX_IRQHandler(void)
{
    UART_Common_ISR(1);
}
#endif

void UART2_IRQHandler(void)
{
    UART_Common_ISR(2);
}

#if defined(UART3)
void UART3_RX_TX_IRQHandler(void)
{
    UART_Common_ISR(3);
}
#endif

#if defined(UART4)
void UART4_RX_TX_IRQHandler(void)
{
    UART_Common_ISR(4);
}
#endif

#if defined(UART5)
void UART5_RX_TX_IRQHandler(void)
{
    UART_Common_ISR(5);
}
#endif
#endif /* #if FSL_FEATURE_SOC_UART_COUNT */

/************************************************************************************/
#if FSL_FEATURE_SOC_LPSCI_COUNT
static void LPSCI_Common_ISR(uint32_t instance)
{
    UART0_Type * base;
    uartState_t * pState;

    base = mLpsciBase[instance];
    pState = pLpsciStates[instance];
    /* Check if data was received */
    if( (kLPSCI_RxDataRegFullFlag) & LPSCI_GetStatusFlags(base) )
    {
        uint8_t data = LPSCI_ReadByte(base);
        LPSCI_ClearStatusFlags(base, kLPSCI_RxDataRegFullFlag);

        if( pState->rxSize )
        {
            pState->rxSize--;
        }

        if( pState->pRxData )
        {
            *(pState->pRxData) = data;
            pState->pRxData++;
        }

        if( (0 == pState->rxSize) && (NULL != pState->rxCb) )
        {
            pState->rxCb(pState);
        }
    }
    /* Check if data Tx has end */
    if( (kLPSCI_TxDataRegEmptyFlag) & LPSCI_GetStatusFlags(base) &&
       (kLPSCI_TxDataRegEmptyInterruptEnable) & LPSCI_GetEnabledInterrupts(base) )
    {
        if( pState->txSize )
        {
            pState->txSize--;

            LPSCI_WriteByte(base, *(pState->pTxData++));
        }
        else if( 0 == pState->txSize )
        {
            LPSCI_DisableInterrupts(base, kLPSCI_TxDataRegEmptyInterruptEnable);

            if( NULL != pState->txCb )
            {
                pState->txCb(pState);
            }
        }
    }

    if( kLPSCI_RxOverrunFlag & LPSCI_GetStatusFlags(base) )
    {
        LPSCI_ClearStatusFlags(base, kLPSCI_RxOverrunFlag);
    }
}

void UART0_IRQHandler(void)
{
    LPSCI_Common_ISR(0);
}
#endif

/************************************************************************************/
#if FSL_FEATURE_SOC_USART_COUNT
#if !gUartDMA_d
static void USART_ISR(void)
{
    do {
        uint32_t irq = __get_IPSR() - 16;
        uint32_t instance = ~0UL;
        USART_Type *base;
        uartState_t *pState;

        /* Get instance */
        for (instance = 0; instance < FSL_FEATURE_SOC_USART_COUNT; instance++)
        {
            if (irq == mUsartIrqs[instance])
                break;
        }
        if (instance >= FSL_FEATURE_SOC_USART_COUNT)
            break;

        base = mUsartBase[instance];
        pState = pUsartStates[instance];

        volatile uint32_t stat;
        uint8_t data;
        stat = USART_GetStatusFlags(base);
        /* Check if data was received */
        if (kUSART_RxError & stat)
        {
            data = USART_ReadByte(base); /* char doomed to be dropped anyway */
            SERIAL_DBG_LOG("stat=0x%x dropping char 0x%x", stat, data);

            USART_ClearStatusFlags(base, kUSART_RxError);
        }
        if ( (kUSART_RxFifoNotEmptyFlag & stat) &&
             (kUSART_RxLevelInterruptEnable & USART_GetEnabledInterrupts(base)))
        {
            uint32_t rx_lvl =  (stat & USART_FIFOSTAT_RXLVL_MASK) >> USART_FIFOSTAT_RXLVL_SHIFT;

            if (NULL != pState->rxCb)
            {
                serialRingState_t  * rx_ring = &pState->rx_ring;

                /* is there still room in the ring buffer */
                while (rx_ring->space_left > 0)
                {
                    data = USART_ReadByte(base);
                    //pState->rxSize--;
                    Serial_RingProduceCharProtected(rx_ring, data);
                    rx_lvl--;

                    if (rx_lvl == 0)
                        break;
                }
                if (pState->rxSize == 0)
                    pState->rxCb(pState);

                /* If we have exited the above loop with the Rx Buffer full,
                 *  disable Rx interrupt */
                if (rx_ring->space_left == 0)
                {
                    if (IS_USART_HW_FLOW_CONTROL_SET(base))
                    {
                        USART_DisableInterrupts(base, kUSART_RxLevelInterruptEnable);
                    }
                    else
                    {
                        if (rx_lvl > 0)
                        {
                            data = USART_ReadByte(base);
                            SERIAL_DBG_LOG("Losing byte %x", data);
                            pState->overrun_cnt++;
                        }
                    }
                }
            }
        }

        /* Check if data Tx has end */
        if ((kUSART_TxFifoEmptyFlag & stat) &&
            (kUSART_TxLevelInterruptEnable & USART_GetEnabledInterrupts(base)))
        {
            if (pState->txSize)
            {
                pState->txSize--;
                USART_WriteByte(base, *(pState->pTxData++));
            }
            else if (0 == pState->txSize)
            {
                USART_DisableInterrupts(base, kUSART_TxLevelInterruptEnable);

                if (NULL != pState->txCb)
                {
                    pState->txCb(pState);
                }
            }
        }

    } while (0);

}
#endif
#endif
