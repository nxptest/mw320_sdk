/*! *********************************************************************************
 * \defgroup CCC Digital Key Applications
 * @{
 ********************************************************************************** */
/*! *********************************************************************************
* Copyright 2021 NXP
* All rights reserved.
*
* \file
*
* This file is the interface file for the CCC Digital Key Applications
*
* SPDX-License-Identifier: BSD-3-Clause
********************************************************************************** */

#ifndef DIGITAL_KEY_APPLICATION_H
#define DIGITAL_KEY_APPLICATION_H

/************************************************************************************
*************************************************************************************
* Include
*************************************************************************************
************************************************************************************/
#include "cmd_ble.h"
#include "ble_general.h"
#include "gap_types.h"
#include "TimersManager.h"
#include "ble_constants.h"

/*************************************************************************************
**************************************************************************************
* Public macros
**************************************************************************************
*************************************************************************************/

/* FSCI interface to be used to comunicate with BB */
#ifndef gFsciInterface_c
#define gFsciInterface_c        0
#endif

#define gScanningTime_c         30

#define mUuidType16_size        2
#define mUuidType128_size       16


#define INVALID_HANDLE          0xFFFF


/* Wireless UART App Configuration */
#ifndef APP_W_UART_INTERFACE_TYPE
#define APP_W_UART_INTERFACE_TYPE (gSerialMgrUsart_c)
#endif

/* EVAL use LPUART0 for HCI */
#ifndef APP_W_UART_INTERFACE_INSTANCE
#define APP_W_UART_INTERFACE_INSTANCE (0)
#endif

#ifndef APP_W_UART_INTERFACE_SPEED
#define APP_W_UART_INTERFACE_SPEED (115200)
#endif

/************************************************************************************
*************************************************************************************
* Public type definitions
*************************************************************************************
************************************************************************************/
typedef enum appEvent_tag{
    mAppEvt_PeerConnected_c,
    mAppEvt_PairingComplete_c,
    mAppEvt_ServiceDiscoveryComplete_c,
    mAppEvt_ServiceDiscoveryNotFound_c,
    mAppEvt_ServiceDiscoveryFailed_c,
    mAppEvt_GattProcComplete_c,
    mAppEvt_GattProcError_c,
    mAppEvt_PeerDisconnected_c
}appEvent_t;

typedef enum appState_tag{
    mAppIdle_c,
    mAppExchangeMtu_c,
    mAppServiceDisc_c,
    mAppServiceDiscRetry_c,
    mAppRunning_c
}appState_t;

typedef struct wucConfig_tag
{
    uint16_t    hService;
    uint16_t    hUartStream;
} wucConfig_t;

typedef struct appPeerInfo_tag
{
    deviceId_t  deviceId;
    bool_t      isBonded;
    wucConfig_t clientInfo;
    appState_t  appState;
    gapRole_t   gapRole;
}appPeerInfo_t;


typedef enum appBBConfigEvent_tag{
    appBBConfigEvent_Idle_c,
    appBBConfigEvent_EventInitializationCompleteIndication_c,
    appBBConfigEvent_BondedDevicesIdentityInformation_c,
    appBBConfigEvent_HostPrivacyStateChangedIndication_c,
    appBBConfigEvent_GAPGenericEventControllerPrivacyStateChangedIndication_c,
    appBBConfigEvent_GenAttProfileAddPrimaryServiceDeclarationIndication_c,
    appBBConfigEvent_GenAttProfileAddCharacteristicDeclarationAndValueIndication_c,
    appBBConfigEvent_GenAttProfileAddCccdIndication_c,
    appBBConfigEvent_GAPGenericEventPublicAddressReadIndication_c,
    appBBConfigEvent_GATTConfirm_c,
    appBBConfigEvent_GATTDBConfirm_c,    
    cappBBConfigEvent_GATTDBFindServiceHandle_c,
    appBBConfigEvent_GATTDBFindServiceHandleIndication_c,
    appBBConfigEvent_GATTDBFindCharValueHandleInServiceIndication_c
}appBBConfigEvent_t;

typedef enum appBBConfigState_tag{
    mBBConfigIdle_c,
    mBBConfigDone_c,
    mBBConfigGenericAttributeProfile_c,
    mBBConfigGenericAccessProfile_c,
    mBBConfigGenericAccessProfileDone_c,
    mBBConfigWirelessUartProfile_c,
    mBBConfigRegisterGattServerCallback_c,
    mBBConfigRegisterGattClientProcedureCallback_c,
    mBBConfigFindWUARTServiceHandle_c,
    mBBConfigFindWUARTCharHandle_c,
    mBBConfigRegisterWUuartCharHandleForWriteNotifications_c,
}appBBConfigState_t;

typedef enum appServiceDiscoveryEvent_tag{
    appServiceDiscoveryEvent_GATTDiscoverPrimaryServicesByUuidIndication_c,
    appServiceDiscoveryEvent_GATTDiscoverAllCharacteristicsIndication_c,
    appServiceDiscoveryEvent_GATTReadCharacteristicValuesIndication_c,
}appServiceDiscoveryEvent_t;
/************************************************************************************
*************************************************************************************
* Public memory declarations
*************************************************************************************
********************************************************************************** */

/************************************************************************************
*************************************************************************************
* Public prototypes
*************************************************************************************
************************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif
  
/*! *********************************************************************************
* \brief    Initializes application specific functionality.
*
********************************************************************************** */
void BleApp_Init(void);
/*****************************************************************************
* Handles all messages received from the HSDK.
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
*
* Return value: None
*****************************************************************************/
void App_HandleHSDKMessageInput(bleEvtContainer_t* pMsg);
/*! *********************************************************************************
 * \brief    Starts the BLE application.
 *
 * \param[in]    gapRole    GAP Start Role (Central or Peripheral).
 ********************************************************************************** */
void BleApp_Start(gapRole_t gapRole);

/*! *********************************************************************************
* \brief        Enable Host/Controller privacy based on the available bonding
*               information.
*  
********************************************************************************** */
void EnablePrivacy(void);

#ifdef __cplusplus
}
#endif


#endif /* DIGITAL_KEY_APPLICATION_H */
