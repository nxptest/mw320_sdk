/*! *********************************************************************************
 * \defgroup app
 * @{
 ********************************************************************************** */
/*!
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2019 NXP
 *
 * \file
 *
 * This file is the app configuration file which is pre included.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of Freescale Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _EXT_BLE_COEX_H_
#define _EXT_BLE_COEX_H_

/*!
 *  Application specific configuration file only
 *  Board Specific Configuration shall be added to board.h file directly such as :
 *  - Number of button on the board,
 *  - Number of LEDs,
 *  - etc...
 */
/*! *********************************************************************************
 *     Board Configuration
 ********************************************************************************** */

/* Specifies the number of physical LEDs on the target board */
#if !defined(gLEDsOnTargetBoardCnt_c)
  #define gLEDsOnTargetBoardCnt_c         0
#endif
#if !defined(gLEDSupported_d)
  #define gLEDSupported_d                 0
#endif

/* Use FRO32K instead of XTAL32K in active and power down modes - XTAL32K no longer required */
#if !defined(gClkUseFro32K)
  #define gClkUseFro32K                   0
#endif

/* Disable uart app console */
#if !defined(gUartAppConsole_d)
  #define gUartAppConsole_d               0
#endif

 /* Defines the number of required keys for the keyboard module */
#if !defined(gKBD_KeysCount_c)
  #define gKBD_KeysCount_c        		0
#endif

/* Enables/disables the switches based keyboard  */
#if !defined(gKeyBoardSupported_d)
  #define gKeyBoardSupported_d           	0
#endif

/*! *********************************************************************************
 *  App Configuration
 ********************************************************************************** */
/*! Maximum number of connections supported for this application. It's up to 8. */
#if !defined(gAppMaxConnections_c)
  #define gAppMaxConnections_c            8
#endif

/* Note: for tests with Android version of IOT Toolbox must define gAppUseBonding_d and gAppUsePairing_d to 0 */
/* Enable/disable use of bonding capability */
#if !defined(gAppUseBonding_d)
  #define gAppUseBonding_d                1
#endif

/* Enable/disable use of pairing procedure */
#if !defined(gAppUsePairing_d)
  #define gAppUsePairing_d                1
#endif

/*! Enable/disable use of privacy */
#if !defined(gAppUsePrivacy_d)
  #define gAppUsePrivacy_d                0
#endif

#if !defined(gPasskeyValue_c)
  #define gPasskeyValue_c                 999999
#endif

#if !defined(gUsePanic_c)
  #define gUsePanic_c     0
#endif

/*! Disable board resetting */
#define gFSCI_ResetCpu_c 0

/*! *********************************************************************************
 *     Framework Configuration
 ********************************************************************************** */
 /* enable NVM to be used as non volatile storage management by the host stack */
#if !defined(gAppUseNvm_d)
  #define gAppUseNvm_d                    0
#endif
/*! *********************************************************************************
 *     HSDK Definition
 ********************************************************************************** */
/* Defines Rx Buffer Size for Serial Manager */
#if !defined(gSerialMgrRxBufSize_c)
#define gSerialMgrRxBufSize_c   320
#endif

/* Defines Tx Queue Size for Serial Manager */
#if !defined(gSerialMgrTxQueueSize_c)
  #define gSerialMgrTxQueueSize_c 5
#endif

#if !defined(gSerialTaskStackSize_c)
/* Defines Size for Serial Manager Task*/
  #define gSerialTaskStackSize_c  700
#endif

#if !defined(gSerialMgrUseUart_c)
/* Enable/Disable UART ussage */
  #define gSerialMgrUseUart_c     1
#endif

#if !defined(gSerialMgrUseSPI_c)
/* Enable/Disable SPI ussage */
  #define gSerialMgrUseSPI_c      0
#endif

#if !defined(gSerialMgrUseIIC_c)
/* Enable/Disable IIC ussage */
  #define gSerialMgrUseIIC_c      0
#endif

/* FSCI config */
#if gSerialMgrUseUart_c
     #define APP_SERIAL_INTERFACE_SPEED         gUARTBaudRate115200_c
     #define APP_SERIAL_INTERFACE_TYPE          gSerialMgrUart_c
     #define APP_SERIAL_INTERFACE_INSTANCE              2
#endif

/* The number of serial interfaces to be used.
0 - means that Serial Manager is disabled */
#define gSerialManagerMaxInterfaces_c       (2)

#define gFsciMaxInterfaces_c      1

/* Defines pools by block size and number of blocks. Must be aligned to 4 bytes.*/
#define AppPoolsDetails_c \
         _block_size_  32  _number_of_blocks_    60 _eol_  \
         _block_size_  80  _number_of_blocks_    30 _eol_  \
		 _block_size_ 128  _number_of_blocks_    30 _eol_  \
		 _block_size_ 256  _number_of_blocks_    30 _eol_  \
         _block_size_ 512  _number_of_blocks_    20 _eol_

/* Defines number of timers needed by the application */
#define gTmrApplicationTimers_c         8

/* Defines number of timers needed by the protocol stack */
#define gTmrStackTimers_c               32

/* Set this define TRUE if the PIT frequency is an integer number of MHZ */
#define gTMR_PIT_FreqMultipleOfMHZ_d    0

/* Enables / Disables the precision timers platform component */
#define gTimestamp_Enabled_d            0

/* Use Lowpower timers - switch timers from CTIMERS to RTC 16bit timer - 1ms resolution*/
#define gTimerMgrLowPowerTimers         0

/* Enable/Disable FSCI */
#define gFsciIncluded_c                 1

/* Defines FSCI length - set this to FALSE is FSCI length has 1 byte */
#define gFsciLenHas2Bytes_c             1

/* Defines FSCI maximum payload length */
#define gFsciMaxPayloadLen_c            600

/* Enable/Disable Ack transmission */
#define gFsciTxAck_c                    0

/* Enable/Disable Ack reception */
#define gFsciRxAck_c                    0

/* Enable FSCI Rx restart with timeout */
#define gFsciRxTimeout_c                1
#define mFsciRxTimeoutUsePolling_c      1

/* Enable/Disable FSCI Low Power Commands*/
#define gFSCI_IncludeLpmCommands_c      0

/* Use Misra Compliant version of FSCI module */
#define gFSCI_BootloaderCommands_c      0
           
/* Use Misra Compliant version of FSCI module */
#define gFsciUseDedicatedTask_c         1
#define gLoggingActive_d                0
//#define DBG_SERIAL 1

#define gTMR_Enabled_d 0

/*! *********************************************************************************
 *     RTOS Configuration
 ********************************************************************************** */
/* Defines number of OS events used */
#define osNumberOfEvents        6

//#define FSL_RTOS_FREE_RTOS 1

/*! *********************************************************************************
 *     BLE Stack Configuration
 ********************************************************************************** */

#define gUseHciTransport_d          0

/* Enable/Disable Dynamic GattDb functionality */
#define gGattDbDynamic_d            1

#define gFsciBleBBox_d              1


#define gGapSimultaneousEAChainedReports_c     2

/*! *********************************************************************************
 *  NVM Module Configuration - gAppUseNvm_d shall be defined aboved as 1 or 0
 ********************************************************************************** */
/* USER DO NOT MODIFY THESE MACROS DIRECTLY. */
#define gAppMemPoolId_c 0
#if gAppUseNvm_d
  #define gNvmMemPoolId_c 1
  #if gUsePdm_d
    #define gPdmMemPoolId_c 2
  #endif
#else
  #if gUsePdm_d
    #define gPdmMemPoolId_c 1
  #endif
#endif

#if gAppUseNvm_d
    #define gNvmOverPdm_d               1
    /* Defines NVM pools by block size and number of blocks. Must be aligned to 4 bytes.*/
   #define NvmPoolsDetails_c \
         _block_size_   32   _number_of_blocks_  20 _pool_id_(gNvmMemPoolId_c) _eol_ \
         _block_size_ 60   _number_of_blocks_    10 _pool_id_(gNvmMemPoolId_c) _eol_ \
         _block_size_ 80   _number_of_blocks_    10 _pool_id_(gNvmMemPoolId_c) _eol_ \
         _block_size_ 100  _number_of_blocks_    2 _pool_id_(gNvmMemPoolId_c) _eol_

    /* configure NVM module */
    #define  gNvStorageIncluded_d                (1)
    #define  gNvFragmentation_Enabled_d          (1)
    #define  gUnmirroredFeatureSet_d             (0)
    #define  gNvRecordsCopiedBufferSize_c        (512)
#else
#define NvmPoolsDetails_c
#endif

#if gUsePdm_d
   #define gPdmNbSegments             63 /* number of sectors contained in PDM storage */

   #define PdmInternalPoolsDetails_c \
        _block_size_ 512                   _number_of_blocks_  2 _pool_id_(gPdmMemPoolId_c) _eol_ \
        _block_size_ (gPdmNbSegments*12)  _number_of_blocks_  1 _pool_id_(gPdmMemPoolId_c) _eol_
#else
#define PdmInternalPoolsDetails_c
#endif


/*! *********************************************************************************
 *     Memory Pools Configuration
 ********************************************************************************** */

/* Defines pools by block size and number of blocks. Must be aligned to 4 bytes.
 * DO NOT MODIFY THIS DIRECTLY. CONFIGURE AppPoolsDetails_c
 * If gMaxBondedDevices_c increases, adjust NvmPoolsDetails_c
*/

#if gAppUseNvm_d
    #define PoolsDetails_c \
         AppPoolsDetails_c \
         NvmPoolsDetails_c \
         PdmInternalPoolsDetails_c
#elif gUsePdm_d /* Radio drivers uses PDM but no NVM over PDM */
    #define PoolsDetails_c \
         AppPoolsDetails_c \
         PdmInternalPoolsDetails_c
#else
    #define PoolsDetails_c \
         AppPoolsDetails_c
#endif


#endif /* _EXT_BLE_COEX_H_ */

/*! *********************************************************************************
 * @}
 ********************************************************************************** */
