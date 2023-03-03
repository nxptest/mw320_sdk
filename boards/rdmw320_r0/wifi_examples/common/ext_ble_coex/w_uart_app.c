/*! *********************************************************************************
* \addtogroup CCC Digital Key Applications
* @{
********************************************************************************** */
/*! *********************************************************************************
* Copyright 2021 NXP
* All rights reserved.
*
* \file
*
* This file is the source file for the CCC Digital Key Applications
*
* SPDX-License-Identifier: BSD-3-Clause
********************************************************************************** */

/************************************************************************************
*************************************************************************************
* Include
*************************************************************************************
************************************************************************************/
/* Framework / Drivers */
//#include "shell.h"
//#include "Flash_Adapter.h"
#include "Panic.h"
//#include "Reset.h"

/* Host SDK */
#include "hsdk_interface.h"
#include <fsl_debug_console.h>
/* App */
#include "w_uart_app.h"
#include "host_w_uart.h"
#include "SecLib.h"
//#include "board.h"
#if gKeyBoardSupported_d && (gKBD_KeysCount_c > 0)
#include "Keyboard.h"
#endif


extern gapConnectionRequestParameters_t gConnReqParams;
extern gapScanningParameters_t          gScanParams;
extern gapPairingParameters_t           gPairingParameters;
extern gapAdvertisingData_t             gAppAdvertisingData;
extern gapScanResponseData_t            gAppScanRspData;
extern gapAdvertisingParameters_t       gAdvParams;
extern gapSmpKeys_t                     gSmpKeys;
extern gapDeviceSecurityRequirements_t deviceSecurityRequirements;

extern uint8_t gaGattDbDynamic_W_UartServiceUUID[];
extern uint8_t gaGattDbDynamic_W_UartStream[];
/************************************************************************************
*************************************************************************************
* Private macros
*************************************************************************************
************************************************************************************/
#define mAppUartBufferSize_c            100 /* Local Buffer Size */
#define mAppUartFlushIntervalInMs_c     (7)     /* Flush Timeout in Ms */

/************************************************************************************
*************************************************************************************
* Private type definitions
*************************************************************************************
************************************************************************************/

/************************************************************************************
*************************************************************************************
* Public memory declarations
*************************************************************************************
************************************************************************************/

/************************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
************************************************************************************/
/* Application variables */
static appBBConfigState_t mBbConfigState = mBBConfigIdle_c;
static appPeerInfo_t gaPeerInformation[gAppMaxConnections_c];
static bleDeviceAddress_t  gaDeviceAddress;
static bool_t mDeviceFoundDeviceToConnect = FALSE;

#if defined(gAppUseBonding_d) && (gAppUseBonding_d)
/* Save peerDeviceId in GAPConnectionEventConnectedIndication because 
*GAPCheckIfBondedIndication and GAPLoadCustomPeerInformationIndication
*don't return a peerDeviceId */
static uint8_t mDeviceIdSaved;
#endif

/* GATTDB data:*/
static uint8_t maGattDbDynamic_GattServiceChangedInitValue[4]   = {0x00, 0x00, 0x00, 0x00};
static uint8_t maGattDbDynamic_GapServiceDeviceNameInitValue[17] = "NXP_Wireless_UART";
static uint16_t mUARTCharHandleForWrite = INVALID_HANDLE;

/* advertising/scanning local state */
static bool_t mAdvStateOn = FALSE;
static bool_t mScanningOn = FALSE;

/* UART interface related variables */
static uint8_t gAppSerMgrIf;
static volatile bool_t mAppUartNewLine = FALSE;
static volatile bool_t mAppDapaPending = FALSE;

//static tmrTimerID_t mUartStreamFlushTimerId = gTmrInvalidTimerID_c;
/************************************************************************************
*************************************************************************************
* Private functions prototypes
*************************************************************************************
************************************************************************************/
/* Callbacks for indications/Confirms received from HSDK. These are FSCI Indications/Confirms. */
static bool_t BBConfig_HandleHSDKMessageInput(bleEvtContainer_t* pMsg);
static void BBConfig_StateMachineHandler(appBBConfigEvent_t event, bleEvtContainer_t* pMsg);
static void BBConfig_RegisterDeviceSecurityRequirements(void);
static void BBConfig_GATTConfirmHandler(appBBConfigEvent_t event, bleEvtContainer_t* pMsg);
static void BBConfig_GATTDBConfirmHandler(appBBConfigEvent_t event, bleEvtContainer_t* pMsg);
static void BBConfig_GATTDBFindServiceHandle(appBBConfigEvent_t event, bleEvtContainer_t* pMsg);
static void BBConfig_GATTDBFindCharHandle(appBBConfigEvent_t event, bleEvtContainer_t* pMsg);
static void BBConfig_AddPrimaryServiceDeclarationHandler(appBBConfigEvent_t event, bleEvtContainer_t* pMsg);
static void BBConfig_AddCharacteristicDeclarationAndValueIndication(appBBConfigEvent_t event, bleEvtContainer_t* pMsg);
static void BBConfig_AddCccdIndicationHandler(appBBConfigEvent_t event, bleEvtContainer_t* pMsg);
static void BBConfig_GattDb_AddPrimaryServiceDecl(UuidType_t uuidType, uint8_t size, uint8_t *pServiceUuid);
static memStatus_t BBConfig_GattDb_DynamicAddCharacteristicDeclarationAndValue(UuidType_t uuidType, bleUuid_t uuid,
                                                                               GATTDBDynamicAddCharacteristicDeclarationAndValueRequest_CharacteristicProperties_t characteristicProperties,
                                                                               uint16_t maxValueLength, uint16_t initialValueLength, uint8_t* pInitialValue,
                                                                               GATTDBDynamicAddCharacteristicDeclarationAndValueRequest_ValueAccessPermissions_t valueAccessPermissions);
/* Advertising APIs */
static bool_t BleApp_AdvertisingCb(bleEvtContainer_t* pMsg);
static void BleApp_GAPAdvertisingEventAdvertisingStateChangedIndication(bleEvtContainer_t* pMsg);
static memStatus_t BleApp_GAPSetAdvertisingData(gapAdvertisingData_t* appAdvertisingData);
static memStatus_t BleApp_GAPSetAdvertisingParameters(gapAdvertisingParameters_t* pAdvertisingParameters);

/*scanning APIs */
static bool_t BleApp_ScanningCb(bleEvtContainer_t* pMsg);
static void BleApp_GAPScanningEventDeviceScannedIndication(bleEvtContainer_t* pMsg);
static void BleApp_GAPScanningEventStateChangedIndication(bleEvtContainer_t* pMsg);
static memStatus_t BleApp_GAPStartScanning(uint16_t duration, uint16_t period, 
                                    GAPStartScanningRequest_FilterDuplicates_t filterDuplicates, 
                                    gapScanningParameters_t* gScanParams);

/* connection APIs */
static bool_t BleApp_ConnectionCb(bleEvtContainer_t* pMsg);
static void BleApp_ConnectionEventConnected(bleEvtContainer_t* pMsg);
static void BleApp_ConnectionEventDisconnected(bleEvtContainer_t* pMsg);
static memStatus_t BleApp_GAPConnect(gapConnectionRequestParameters_t* pParameters);
#if defined(gAppUseBonding_d) && (gAppUseBonding_d)
memStatus_t BleApp_GAPCheckIfBondedReq(uint8_t deviceId);
static void BleApp_CheckIfBondedHandle(bleEvtContainer_t* pMsg);
static void BleeApp_GAPLoadCustomPeerInformationIndication(bleEvtContainer_t* pMsg);
memStatus_t BleApp_GAPSaveCustomPeerInformation(uint8_t deviceId, uint16_t offset, uint16_t infoSize,  const uint8_t *Info);
#endif
#if defined(gAppUsePairing_d) && (gAppUsePairing_d)
memStatus_t BleApp_GAPEncryptLinkReq(uint8_t deviceId);
memStatus_t BleApp_GAPPairReq(uint8_t deviceId, gapPairingParameters_t *pPairingParam);
memStatus_t BleApp_GapSendSlaveSecurityReq(uint8_t deviceId, gapPairingParameters_t *pPairingParam);
memStatus_t BleApp_GAPAcceptPairingRequest(uint8_t deviceId, gapPairingParameters_t *pPairingParam);
static void BleApp_GAPConnectionEventPairingRequestIndication(bleEvtContainer_t* pMsg);
static void BleApp_GAPConnectionEventLeScDisplayNumericValueIndication(bleEvtContainer_t* pMsg);
static void BleApp_GAPConnectionEventPasskeyRequestIndication(bleEvtContainer_t* pMsg);
static void BleApp_GAPConnectionEventKeyExchangeRequestIndication(bleEvtContainer_t* pMsg);
static void BleApp_GAPConnectionEventLongTermKeyRequestIndication(bleEvtContainer_t* pMsg);
static void BleApp_GAPConnectionEventAuthenticationRejected(bleEvtContainer_t* pMsg);
static void BleApp_GAPConnectionEventPairingCompleteIndication(bleEvtContainer_t* pMsg);
#endif


/* service discovery API */
static bool_t BleApp_ServiceDiscoveryCb(bleEvtContainer_t* pMsg);
static void ServiceDiscovery_StateMachineHandler(deviceId_t peerDeviceId, appServiceDiscoveryEvent_t event, 
                                                 bleEvtContainer_t *pMsg);
static void BleApp_StartServiceDisc(deviceId_t peerDeviceId);
static void BleApp_HandleServiceDiscState(deviceId_t peerDeviceId, appEvent_t event, bleEvtContainer_t* pMsg);
static void BleApp_HandleServiceDiscRetryState(deviceId_t peerDeviceId, appEvent_t event, bleEvtContainer_t* pMsg);
static memStatus_t ServiceDiscovery_GATTClientDiscoverAllCharacteristicsOfService(uint8_t deviceId, uint16_t startHandle, uint16_t endHandle,
                                                                                  UuidType_t uuidType, bleUuid_t uuid);

/* GATT APIs */
static bool_t BleApp_GattServerCallback(bleEvtContainer_t* pMsg);
static void BleApp_GATTClientProcedureExchangeMtuIndication(bleEvtContainer_t* pMsg);
static void BleApp_GATTServerAttributeWrittenWithoutResponseIndication(bleEvtContainer_t* pMsg);
memStatus_t BleApp_GATTClientExchangeMtu(uint8_t deviceId, uint16_t mtu);

/* functions parts of the StateMachineHandler */
static void BleApp_HandleIdleState(deviceId_t peerDeviceId, appEvent_t event, bleEvtContainer_t* pMsg);
static void BleApp_HandleExchangeMtuState(deviceId_t peerDeviceId, appEvent_t event, bleEvtContainer_t* pMsg);
memStatus_t BleApp_GAPDisconnect(uint8_t deviceId);

static bool_t BleApp_GenericCallback(bleEvtContainer_t* pMsg);
static void BleApp_MCUInfoToSmpKeys(void);

/* Uart Tx/Rx Callbacks*/
static void Uart_TxCallBack(void *pBuffer);
static void BleApp_ReceivedUartStream(deviceId_t peerDeviceId, uint8_t *pStream, uint16_t streamLength);
#if gKeyBoardSupported_d && (gKBD_KeysCount_c > 0) && !defined (DUAL_MODE_APP)
static void App_KeyboardCallBack(uint8_t   event);
#endif

/************************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
************************************************************************************/

/*! *********************************************************************************
* \brief    Initializes application specific functionality.
*
********************************************************************************** */
void BleApp_Init(void)
{
    uint8_t mPeerId = 0;
   
#if gKeyBoardSupported_d && (gKBD_KeysCount_c > 0)
    KBD_Init(App_KeyboardCallBack);
#endif

    for (mPeerId = 0; mPeerId < (uint8_t)gAppMaxConnections_c; mPeerId++)
    {
        gaPeerInformation[mPeerId].deviceId = gInvalidDeviceId_c;
        gaPeerInformation[mPeerId].appState = mAppIdle_c;
    }
    
    (void)PRINTF("\n\rWireless UART Example\n\r");
}


/*****************************************************************************
* Handles all messages received from the HSDK.
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
*
* Return value: None
*****************************************************************************/
void App_HandleHSDKMessageInput(bleEvtContainer_t* pMsg)
{
    bool_t matchFound = FALSE;
    
    //(void)PRINTF("\n\reEvent: 0x%x\r\n", pMsg->id);
    
    /* Verify if Black Box config is underway */
    if(mBbConfigState != mBBConfigDone_c)
    {
         matchFound = BBConfig_HandleHSDKMessageInput(pMsg);
    }
    else
    {
        matchFound = BleApp_AdvertisingCb(pMsg);
        
        if (matchFound == FALSE)
        {
            matchFound = BleApp_ScanningCb(pMsg);
        }       
        
        if (matchFound == FALSE)
        {
            matchFound = BleApp_ConnectionCb(pMsg);
        }
        
        if (matchFound == FALSE)
        {
            matchFound = BleApp_ServiceDiscoveryCb(pMsg);
        }
        
        if (matchFound == FALSE)
        {
            matchFound = BleApp_GattServerCallback(pMsg);
        }
        
        if (matchFound == FALSE)
        {
            matchFound = BleApp_GenericCallback(pMsg);
        }
                  
        
    }
    
    (void)matchFound;
}

/*! *********************************************************************************
* \brief        State machine handler of the Digital Key Device application.
*
* \param[in]    peerDeviceId        Peer device ID.
* \param[in]    event               Event type.
* \param[in]    pMsg                Message received form Black Box.
********************************************************************************** */
void BleApp_StateMachineHandler(deviceId_t peerDeviceId, appEvent_t event, bleEvtContainer_t*  pMsg)
{

    /* invalid client information */
    if (gInvalidDeviceId_c == gaPeerInformation[peerDeviceId].deviceId)
    {
        return;
    }

    switch (gaPeerInformation[peerDeviceId].appState)
    {
        case mAppIdle_c:
        {
            (void)PRINTF("\n\rIdle\n\r");
            BleApp_HandleIdleState(peerDeviceId, event, pMsg);
        }
        break;

        case mAppExchangeMtu_c:
        {
            (void)PRINTF("\n\rExchangeMTU\n\r");
            BleApp_HandleExchangeMtuState(peerDeviceId, event, pMsg);
        }
        break;

        case mAppServiceDisc_c:
        {
            (void)PRINTF("\n\rServiceDiscovery\n\r");
            BleApp_HandleServiceDiscState(peerDeviceId, event, pMsg);
        }
        break;

        case mAppServiceDiscRetry_c:
        {
            (void)PRINTF("\n\rDiscoveryRetry\n\r");
            BleApp_HandleServiceDiscRetryState(peerDeviceId, event, pMsg);
        }
        break;

        case mAppRunning_c:
        {
            (void)PRINTF("\n\rRunning\n\r");
            if ( event == mAppEvt_PeerDisconnected_c )
            {
                gaPeerInformation[peerDeviceId].deviceId = gInvalidDeviceId_c;
                gaPeerInformation[peerDeviceId].appState = mAppIdle_c;
            }

        }
        break;

        default:
        {
            ; /* No action required */
        }
        break;
    }
}


/*! *********************************************************************************
 * \brief    Starts the BLE application.
 *
 * \param[in]    gapRole    GAP Start Role (Central or Peripheral).
 ********************************************************************************** */
void BleApp_Start(gapRole_t gapRole)
{
    switch (gapRole)
    {
        case gGapCentral_c:
        {
            if (!mScanningOn)
            {
                (void)PRINTF("\n\rScanning...\n\r");
                gPairingParameters.localIoCapabilities = gIoKeyboardDisplay_c;
                /* Start scanning */
                (void)BleApp_GAPStartScanning(gGapScanContinuously_d, gGapScanPeriodicDisabled_d,
                                       GAPStartScanningRequest_FilterDuplicates_Enable,
                                       &gScanParams);
            }
            break;
        }

        case gGapPeripheral_c:
        {
            if (!mAdvStateOn)
            {
                (void)PRINTF("\n\rAdvertising...\n\r");
                gPairingParameters.localIoCapabilities = gIoDisplayOnly_c;
                /* Set advertising parameters, advertising to start on gAdvertisingParametersSetupComplete_c */
                (void)BleApp_GAPSetAdvertisingParameters(&gAdvParams);
            }
            break;
        }

        default:
        {
            ; /* No action required */
            break;
        }
    }
}

/************************************************************************************
*************************************************************************************
* Private functions
*************************************************************************************
************************************************************************************/

////////////////////////////////////////////////////////////////////////////////////
///////////////////////// BackBox - APP configuration  /////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

/*****************************************************************************
*\brief        Handles all messages received from the Black Box that are
*              used by BBConfig_StateMachineHandler.
*
*\param[in]    pMsg    Pointer to bleEvtContainer_t.
*
*\retval       TRUE    If the message type matched and a handler function 
*              was called
*\retval       FALSE   Message was not BBConfig_StateMachineHandler client
*              related
*****************************************************************************/
static bool_t BBConfig_HandleHSDKMessageInput(bleEvtContainer_t* pMsg)
{
    bool_t matchFound = TRUE;
    switch (pMsg->id)
    {
        case (uint16_t)GAPGenericEventInitializationCompleteIndication_FSCI_ID:
        {
            BBConfig_StateMachineHandler(appBBConfigEvent_EventInitializationCompleteIndication_c, pMsg);
        }
        break;
        
        case (uint16_t)GAPGenericEventPublicAddressReadIndication_FSCI_ID:
        {
            BBConfig_StateMachineHandler(appBBConfigEvent_GAPGenericEventPublicAddressReadIndication_c, pMsg);
        }
        break;
        
        case (uint16_t)GAPGetBondedDevicesIdentityInformationIndication_FSCI_ID:
        {
            BBConfig_StateMachineHandler(appBBConfigEvent_BondedDevicesIdentityInformation_c, pMsg);
        }
        break;

        case (uint16_t)GAPGenericEventHostPrivacyStateChangedIndication_FSCI_ID:
        {
            BBConfig_AddCccdIndicationHandler(appBBConfigEvent_HostPrivacyStateChangedIndication_c, pMsg);
        }
        break;

        case (uint16_t)GATTDBDynamicAddPrimaryServiceDeclarationIndication_FSCI_ID:
        {
            BBConfig_StateMachineHandler(appBBConfigEvent_GenAttProfileAddPrimaryServiceDeclarationIndication_c, pMsg);
        }
        break;

        case (uint16_t)GATTDBDynamicAddCharacteristicDeclarationAndValueIndication_FSCI_ID:
        {
            BBConfig_StateMachineHandler(appBBConfigEvent_GenAttProfileAddCharacteristicDeclarationAndValueIndication_c, pMsg);
        }
        break;

        case (uint16_t)GATTDBDynamicAddCccdIndication_FSCI_ID:
        {
            BBConfig_StateMachineHandler(appBBConfigEvent_GenAttProfileAddCccdIndication_c, pMsg);
        }
        break;

        case (uint16_t)GAPGenericEventControllerPrivacyStateChangedIndication_FSCI_ID:
        {
            BBConfig_StateMachineHandler(appBBConfigEvent_GAPGenericEventControllerPrivacyStateChangedIndication_c, pMsg);
        }
        break;
        

        case (uint16_t)GATTConfirm_FSCI_ID:
        {
            BBConfig_StateMachineHandler(appBBConfigEvent_GATTConfirm_c, pMsg);
        }
        break;         
        
        case (uint16_t)GATTDBConfirm_FSCI_ID:
        {
            BBConfig_StateMachineHandler(appBBConfigEvent_GATTDBConfirm_c, pMsg);
        }
        break;    
        
        case (uint16_t)GATTDBFindServiceHandleIndication_FSCI_ID:
        {
              BBConfig_StateMachineHandler(appBBConfigEvent_GATTDBFindServiceHandleIndication_c, pMsg);
        }
        break;

        case (uint16_t)GATTDBFindCharValueHandleInServiceIndication_FSCI_ID:
        {
              BBConfig_StateMachineHandler(appBBConfigEvent_GATTDBFindCharValueHandleInServiceIndication_c, pMsg);
        }
        break;        
        
        default:
        {
            matchFound = FALSE;
        }
        break; 
    }
    
    return matchFound;
}

/*! *********************************************************************************
* \brief        State machine handler of the Black Box configuration.
*
* \param[in]    event               Event type.
* \param[in]    pMsg                Message received form Black Box.
********************************************************************************** */
static void BBConfig_StateMachineHandler(appBBConfigEvent_t event, bleEvtContainer_t* pMsg)
{     
    if(mBbConfigState == mBBConfigDone_c)
    {
        return;
    }
    
    switch(event)
    {
        case appBBConfigEvent_EventInitializationCompleteIndication_c:    
        {
#if (defined(gAppUsePairing_d) && (gAppUsePairing_d == 1U))
            /* Generates LTK, IRK, CSRK, ediv and rand */
            BleApp_MCUInfoToSmpKeys();
#endif
            /* register device security requirements */
            BBConfig_RegisterDeviceSecurityRequirements();          
            /* read public address*/
            (void)GAPReadPublicDeviceAddressRequest(NULL, gFsciInterface_c);
        }
        break;
        
        case appBBConfigEvent_GAPGenericEventPublicAddressReadIndication_c:
        {
            FLib_MemCpy(gaDeviceAddress, 
                        pMsg->Data.GAPGenericEventPublicAddressReadIndication.Address, 
                        sizeof(bleDeviceAddress_t));
 #if (defined(gAppUsePairing_d) && (gAppUsePairing_d == 1U))
            gSmpKeys.addressType = gBleAddrTypePublic_c;
            gSmpKeys.aAddress = gaDeviceAddress;
#endif           
                
            /* move to the next state - add profiles */
            
            /* Start Configure Database */
            /* configure GenericAttributeProfile_d       0x1801U */
            uint16_t uuid  = gBleSig_GenericAttributeProfile_d;

            mBbConfigState = mBBConfigGenericAttributeProfile_c;
            BBConfig_GattDb_AddPrimaryServiceDecl(Uuid16Bits, 2, (uint8_t *)&uuid);
            
        }
        break;

        case appBBConfigEvent_GenAttProfileAddPrimaryServiceDeclarationIndication_c:
           BBConfig_AddPrimaryServiceDeclarationHandler(event, pMsg);
        break;

        case appBBConfigEvent_GenAttProfileAddCharacteristicDeclarationAndValueIndication_c:
           BBConfig_AddCharacteristicDeclarationAndValueIndication(event, pMsg);
        break;
        
        case appBBConfigEvent_GenAttProfileAddCccdIndication_c:
           BBConfig_AddCccdIndicationHandler(event, pMsg);
        break;
        
        
        case appBBConfigEvent_GATTConfirm_c:
           BBConfig_GATTConfirmHandler(event, pMsg);
        break;
        
        case appBBConfigEvent_GATTDBConfirm_c:
           BBConfig_GATTDBConfirmHandler(event, pMsg);
        break;
        
        case appBBConfigEvent_GATTDBFindServiceHandleIndication_c:
            BBConfig_GATTDBFindServiceHandle(event, pMsg);
          break;
              
        case appBBConfigEvent_GATTDBFindCharValueHandleInServiceIndication_c:   
            BBConfig_GATTDBFindCharHandle(event, pMsg);
          break;
          
        default:
        {
            ; /* No action required */
        }
        break;
    }
}

/*! *********************************************************************************
* \brief        Register Device Security Requirements
*
********************************************************************************** */
static void BBConfig_RegisterDeviceSecurityRequirements(void)
{
#if (defined(gAppUsePairing_d) && (gAppUsePairing_d == 1U))
    /* Register security requirements if pairing is used */
    GAPRegisterDeviceSecurityRequirementsRequest_t req = {0};
    
    
    req.SecurityRequirementsIncluded = TRUE;
    req.SecurityRequirements.MasterSecurityRequirements.SecurityModeLevel = 
      (GAPRegisterDeviceSecurityRequirementsRequest_SecurityRequirements_MasterSecurityRequirements_SecurityModeLevel_t)deviceSecurityRequirements.pMasterSecurityRequirements->securityModeLevel;
    req.SecurityRequirements.MasterSecurityRequirements.Authorization = deviceSecurityRequirements.pMasterSecurityRequirements->authorization;
    req.SecurityRequirements.MasterSecurityRequirements.MinimumEncryptionKeySize = deviceSecurityRequirements.pMasterSecurityRequirements->minimumEncryptionKeySize;
    req.SecurityRequirements.NbOfServices = deviceSecurityRequirements.cNumServices;
    req.SecurityRequirements.GapServiceSecurityRequirements = NULL; //TBD with valid data
      
    (void)GAPRegisterDeviceSecurityRequirementsRequest(&req, NULL, gFsciInterface_c);
    /* Set local passkey. If not defined, passkey will be generated random in SMP */
    GAPSetLocalPasskeyRequest_t reqSet = {0};
    reqSet.Passkey = gPasskeyValue_c;   
    (void)GAPSetLocalPasskeyRequest(&reqSet, NULL, gFsciInterface_c);
#endif
}

/*! *********************************************************************************
* \brief        Add a primary service declaration
*               state for BBConfig_StateMachineHandler.
*
* \param[in]    serviceUuid         serviceUUID.
********************************************************************************** */
static void BBConfig_GattDb_AddPrimaryServiceDecl(UuidType_t uuidType, uint8_t size, uint8_t *pServiceUuid)
{
    GATTDBDynamicAddPrimaryServiceDeclarationRequest_t req = { 0 };
    req.UuidType = uuidType;
    
    FLib_MemCpy(req.Uuid.Uuid16Bits, pServiceUuid, size);

   (void)GATTDBDynamicAddPrimaryServiceDeclarationRequest(&req, NULL, gFsciInterface_c);

}
   
/*! *********************************************************************************
* \brief        Handler of appBBConfigEvent_GenAttProfileAddPrimaryServiceDeclarationIndication_c
*               state for BBConfig_StateMachineHandler.
*
* \param[in]    event               Event type.
* \param[in]    pMsg                Message received form Black Box.
********************************************************************************** */
static void BBConfig_AddPrimaryServiceDeclarationHandler(appBBConfigEvent_t event, bleEvtContainer_t* pMsg)
{
    bleUuid_t attrProfile;

    if (pMsg != NULL)
    {
        if(mBbConfigState == mBBConfigGenericAttributeProfile_c)
        {
            /* Add gBleSig_GattServiceChanged_d            0x2A05U */
            attrProfile.uuid16 = gBleSig_GattServiceChanged_d;
            (void)BBConfig_GattDb_DynamicAddCharacteristicDeclarationAndValue(Uuid16Bits,attrProfile, 
                                                                    GATTDBDynamicAddCharacteristicDeclarationAndValueRequest_CharacteristicProperties_gIndicate_c,
                                                                    (uint16_t)sizeof(maGattDbDynamic_GattServiceChangedInitValue), 
                                                                    (uint16_t)sizeof(maGattDbDynamic_GattServiceChangedInitValue),
                                                                    maGattDbDynamic_GattServiceChangedInitValue,
                                                                    GATTDBDynamicAddCharacteristicDeclarationAndValueRequest_ValueAccessPermissions_gPermissionNone_c);  
        }   
        else if(mBbConfigState == mBBConfigGenericAccessProfile_c)
        {
            /* Add gBleSig_GapDeviceName_d                 0x2A00U */
            attrProfile.uuid16 = gBleSig_GapDeviceName_d;
            (void)BBConfig_GattDb_DynamicAddCharacteristicDeclarationAndValue(Uuid16Bits,attrProfile, 
                                                                    GATTDBDynamicAddCharacteristicDeclarationAndValueRequest_CharacteristicProperties_gRead_c,
                                                                    (uint16_t)sizeof(maGattDbDynamic_GapServiceDeviceNameInitValue),
                                                                    (uint16_t)sizeof(maGattDbDynamic_GapServiceDeviceNameInitValue),
                                                                    maGattDbDynamic_GapServiceDeviceNameInitValue,
                                                                    GATTDBDynamicAddCharacteristicDeclarationAndValueRequest_ValueAccessPermissions_gPermissionFlagReadable_c);
        }
        else if (mBbConfigState == mBBConfigWirelessUartProfile_c)
        {
            uint8_t initialValue[1] = {0};
            FLib_MemCpy(attrProfile.uuid128, gaGattDbDynamic_W_UartStream, mUuidType128_size);
            (void)BBConfig_GattDb_DynamicAddCharacteristicDeclarationAndValue(Uuid128Bits,attrProfile, 
                                                                    GATTDBDynamicAddCharacteristicDeclarationAndValueRequest_CharacteristicProperties_gWriteWithoutRsp_c,
                                                                    (uint16_t)250,
                                                                    (uint16_t)1,
                                                                    initialValue,
                                                                    GATTDBDynamicAddCharacteristicDeclarationAndValueRequest_ValueAccessPermissions_gPermissionFlagWritable_c);
        }
        else
        {
              /* For MISRA compliance */
        }
    }
}


/*! *********************************************************************************
* \brief        Handler of appBBConfigEvent_GenAttProfileAddCharacteristicDeclarationAndValueIndication_c
*               state for BBConfig_StateMachineHandler.
*
* \param[in]    event               Event type.
* \param[in]    pMsg                Message received form Black Box.
********************************************************************************** */
static void BBConfig_AddCharacteristicDeclarationAndValueIndication(appBBConfigEvent_t event, bleEvtContainer_t* pMsg)
{
    if (pMsg != NULL)
    {
        if(mBbConfigState == mBBConfigGenericAttributeProfile_c)
        {
            (void)GATTDBDynamicAddCccdRequest(NULL, gFsciInterface_c);  
        }
        else if(mBbConfigState == mBBConfigGenericAccessProfile_c)
        {
            bleUuid_t attrProfile;
            /* Add gBleSig_GapAppearance_d                 0x2A01U */
            attrProfile.uuid16 = gBleSig_GapAppearance_d;
            mBbConfigState = mBBConfigGenericAccessProfileDone_c;
            (void)BBConfig_GattDb_DynamicAddCharacteristicDeclarationAndValue(Uuid16Bits,attrProfile, 
                                                                    GATTDBDynamicAddCharacteristicDeclarationAndValueRequest_CharacteristicProperties_gRead_c,
                                                                    (uint16_t)sizeof(maGattDbDynamic_GapServiceDeviceNameInitValue),
                                                                    (uint16_t)sizeof(maGattDbDynamic_GapServiceDeviceNameInitValue),
                                                                    maGattDbDynamic_GapServiceDeviceNameInitValue,
                                                                    GATTDBDynamicAddCharacteristicDeclarationAndValueRequest_ValueAccessPermissions_gPermissionFlagReadable_c);          
        }
        else if(mBbConfigState == mBBConfigGenericAccessProfileDone_c)
        {
            (void)GATTDBDynamicAddCccdRequest(NULL, gFsciInterface_c);
        }
        else if(mBbConfigState == mBBConfigWirelessUartProfile_c)
        {
            /* GATTDB configuration done */
          
            /* move to the next state */
            mBbConfigState = mBBConfigRegisterGattServerCallback_c;
            /* Installs the application callback for the GATT Client module Procedures */
            (void)GATTServerRegisterCallbackRequest(NULL, gFsciInterface_c);
        }
    }
}


/*! *********************************************************************************
* \brief        Handler of appBBConfigEvent_GenAttProfileAddCccdIndication_c state
*               for BBConfig_StateMachineHandler.
*
* \param[in]    event               Event type.
* \param[in]    pMsg                Message received form Black Box.
********************************************************************************** */
static void BBConfig_AddCccdIndicationHandler(appBBConfigEvent_t event, bleEvtContainer_t* pMsg)
{
   if (pMsg != NULL)
   {  
        if(mBbConfigState == mBBConfigGenericAttributeProfile_c)
        {
            /* #define gBleSig_GenericAccessProfile_d          0x1800U */
            uint16_t uuid  = gBleSig_GenericAccessProfile_d;
            mBbConfigState = mBBConfigGenericAccessProfile_c;
            BBConfig_GattDb_AddPrimaryServiceDecl(Uuid16Bits, 2, (uint8_t *)&uuid);
        }
        else if(mBbConfigState == mBBConfigGenericAccessProfileDone_c)
        {
            /* w_uart profile */          
            mBbConfigState = mBBConfigWirelessUartProfile_c;
            BBConfig_GattDb_AddPrimaryServiceDecl(Uuid128Bits, 16, gaGattDbDynamic_W_UartServiceUUID);
        }
        else
        {
              /* For MISRA compliance */
        }
   } 
}

/*! *********************************************************************************
* \brief        Handler of appBBConfigEvent_GATTConfirm_c
*               state for BBConfig_StateMachineHandler.
*
* \param[in]    event               Event type.
* \param[in]    pMsg                Message received form Black Box.
********************************************************************************** */
static void BBConfig_GATTConfirmHandler
(
    appBBConfigEvent_t event,
    bleEvtContainer_t* pMsg
)
{
    if(mBbConfigState == mBBConfigRegisterGattServerCallback_c)
    {
        /* Registers callbacks for GATT client procedure */
        mBbConfigState = mBBConfigRegisterGattClientProcedureCallback_c;
        (void)GATTClientRegisterProcedureCallbackRequest(NULL, gFsciInterface_c);      
    }
    else if(mBbConfigState == mBBConfigRegisterGattClientProcedureCallback_c)
    {
        /* find w_uart handle */
        GATTDBFindServiceHandleRequest_t req;
        req.StartHandle = 0x0001;  /* should be 0x0001 on the first call */
        req.UuidType = Uuid128Bits;
        FLib_MemCpy(&req.Uuid.Uuid128Bits[0], gaGattDbDynamic_W_UartServiceUUID, 16);
        mBbConfigState = mBBConfigFindWUARTServiceHandle_c;
        GATTDBFindServiceHandleRequest(&req, NULL, gFsciInterface_c);
    }
    else if(mBbConfigState == mBBConfigRegisterWUuartCharHandleForWriteNotifications_c)
    {
        /* configuration completed */
        mBbConfigState = mBBConfigDone_c;
        
        /* start peripheral role */
        //BleApp_Start(gGapPeripheral_c);
    }
    else
    {
        /* do nothing */
    }
}


/*! *********************************************************************************
* \brief        Handler of appBBConfigEvent_GATTDBConfirm_c
*               state for BBConfig_StateMachineHandler.
*
* \param[in]    event               Event type.
* \param[in]    pMsg                Message received form Black Box.
********************************************************************************** */
static void BBConfig_GATTDBConfirmHandler
(
    appBBConfigEvent_t event,
    bleEvtContainer_t* pMsg
)
{
 
}

/*! *********************************************************************************
* \brief        Handler of appBBConfigEvent_GATTDBFindServiceHandleIndication_c
*               state for BBConfig_StateMachineHandler.
*
* \param[in]    event               Event type.
* \param[in]    pMsg                Message received form Black Box.
********************************************************************************** */
static void BBConfig_GATTDBFindServiceHandle
(
    appBBConfigEvent_t event,
    bleEvtContainer_t* pMsg
)
{
    if(mBbConfigState == mBBConfigFindWUARTServiceHandle_c)
    {       
        uint16_t wUartHandleService= pMsg->Data.GATTDBFindServiceHandleIndication.ServiceHandle;      
        GATTDBFindCharValueHandleInServiceRequest_t req;
        
        req.ServiceHandle = wUartHandleService;
        req.UuidType = Uuid128Bits;
        FLib_MemCpy(req.Uuid.Uuid128Bits, gaGattDbDynamic_W_UartStream, 16);
        
        /* Find w_uart char in service request */
        mBbConfigState = mBBConfigFindWUARTCharHandle_c;
        GATTDBFindCharValueHandleInServiceRequest(&req, NULL, gFsciInterface_c);
    }  
}

/*! *********************************************************************************
* \brief        Handler of appBBConfigEvent_GATTDBFindServiceHandleIndication_c
*               state for BBConfig_StateMachineHandler.
*
* \param[in]    event               Event type.
* \param[in]    pMsg                Message received form Black Box.
********************************************************************************** */
static void BBConfig_GATTDBFindCharHandle
(
    appBBConfigEvent_t event,
    bleEvtContainer_t* pMsg
)
{
    if(mBbConfigState == mBBConfigFindWUARTCharHandle_c)
    {       
        mUARTCharHandleForWrite = pMsg->Data.GATTDBFindCharValueHandleInServiceIndication.CharValueHandle;
      
        GATTServerRegisterHandlesForWriteNotificationsRequest_t req;
        req.HandleCount = 1;
        req.AttributeHandles = &mUARTCharHandleForWrite;
        
        /* Register handle for write notification */
        mBbConfigState = mBBConfigRegisterWUuartCharHandleForWriteNotifications_c;
        GATTServerRegisterHandlesForWriteNotificationsRequest(&req, NULL, gFsciInterface_c);
    }  
}

/*! *********************************************************************************
* \brief        Sends GATTDBDynamicAddCharacteristicDeclarationAndValueRequest command to the Black Box.
*
* \param[in]    uuidType                    UUID type.
* \param[in]    uuid                        UUID value.
* \param[in]    characteristicProperties    Characteristic properties. 
* \param[in]    maxValueLength              If the Characteristic Value length is variable,
*                                           this is the maximum length; for fixed lengths this must be set to 0.
* \param[in]    initialValueLength          Value length at initialization; remains fixed if maxValueLength 
*                                           is set to 0, otherwise cannot be greater than maxValueLength
* \param[in]    initialValue                Contains the initial value of the Characteristic. 
* \param[in]    valueAccessPermissions      Access permissions for the value attribute. 
*
* \return       MEM_SUCCESS_c is commands is correctly sent to FSCI to be transmitted
*               to the Black Box
********************************************************************************** */
static memStatus_t BBConfig_GattDb_DynamicAddCharacteristicDeclarationAndValue(
    UuidType_t          uuidType,
    bleUuid_t           uuid,
    GATTDBDynamicAddCharacteristicDeclarationAndValueRequest_CharacteristicProperties_t characteristicProperties,
    uint16_t            maxValueLength,
    uint16_t            initialValueLength,
    uint8_t             *pInitialValue,
    GATTDBDynamicAddCharacteristicDeclarationAndValueRequest_ValueAccessPermissions_t valueAccessPermissions
)
{
    GATTDBDynamicAddCharacteristicDeclarationAndValueRequest_t req;
    FLib_MemSet(&req, 0x00U, sizeof(GATTDBDynamicAddCharacteristicDeclarationAndValueRequest_t));
    req.UuidType = uuidType;
    switch (req.UuidType)
    {
        case Uuid16Bits:
            Utils_PackTwoByteValue(uuid.uuid16, req.Uuid.Uuid16Bits);
        break;

        case Uuid128Bits:
            FLib_MemCpy(req.Uuid.Uuid128Bits, uuid.uuid128, 16);
        break;

        case Uuid32Bits:
            Utils_PackFourByteValue(uuid.uuid32, req.Uuid.Uuid32Bits);
        break;
        
        default:
        {
                ; /* No action required */
        }
        break;
    }
    req.CharacteristicProperties = characteristicProperties;
    req.MaxValueLength = maxValueLength;
    req.InitialValue = MEM_BufferAlloc(sizeof(initialValueLength));
    if(NULL == req.InitialValue)
    {
        panic( 0, (uint32_t)BBConfig_GattDb_DynamicAddCharacteristicDeclarationAndValue, 0, 0 );
        return MEM_ALLOC_ERROR_c;
    }
    FLib_MemCpy(req.InitialValue, pInitialValue, initialValueLength);
    req.InitialValueLength = initialValueLength;
    req.ValueAccessPermissions = valueAccessPermissions;
    memStatus_t result = GATTDBDynamicAddCharacteristicDeclarationAndValueRequest(&req, NULL, gFsciInterface_c);
    (void)MEM_BufferFree(req.InitialValue);
    req.InitialValue = NULL;
    return result;
}

////////////////////////////////////////////////////////////////////////////////////
///////////////////////// BackBox - APP configuration end //////////////////////////
////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////
///////////////////////// Advertising APIs //////////////////////////
/////////////////////////////////////////////////////////////////////

/*****************************************************************************
* \brief        Handles all messages received from the Black Box that are
*               used by BLE application for advertising
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
*
*\retval       TRUE    If the message type matched and a handler function 
*              was called
*\retval       FALSE   Message was not Digital Key Device application related
*****************************************************************************/
static bool_t BleApp_AdvertisingCb(bleEvtContainer_t* pMsg)
{
    bool_t matchFound = TRUE;

    switch (pMsg->id)
    {
        case (uint16_t)GAPGenericEventAdvertisingDataSetupCompleteIndication_FSCI_ID:
        {        
            (void)GAPStartAdvertisingRequest(NULL, gFsciInterface_c);
        }
        break;

        case (uint16_t)GAPGenericEventAdvertisingParametersSetupCompleteIndication_FSCI_ID:
        {
            (void)BleApp_GAPSetAdvertisingData(&gAppAdvertisingData);
        }
        break;           
        
        case (uint16_t)GAPAdvertisingEventStateChangedIndication_FSCI_ID:
        {
            BleApp_GAPAdvertisingEventAdvertisingStateChangedIndication(pMsg);
        }
        break;
                      
        case (uint16_t)GAPGenericEventAdvertisingSetupFailedIndication_FSCI_ID:
            /* Advertising Setup Failed received */
            (void)PRINTF("\n\rAdvertising setup failed \n\r");
            panic(0,(uint32_t)BleApp_AdvertisingCb,0,0);
        break;

        case (uint16_t)GAPAdvertisingEventCommandFailedIndication_FSCI_ID:
        {
            /* Advertising Command Failed received */
            (void)PRINTF("\n\rAdvertising command failed \n\r");
            panic(0,(uint32_t)BleApp_AdvertisingCb,0,0);
        }
        break;
        
        case GAPAdvertisingEventAdvertisingSetTerminatedIndication_FSCI_ID:
        {
            (void)PRINTF("\n\rAdvertising stopped \n\r");
            mAdvStateOn = FALSE;
        }
        break;

        default:
        {
            matchFound = FALSE;
        }
        break;
    }

    
    return matchFound;
}

/*! *********************************************************************************
* \brief        Handles GAPAdvertisingEventExtAdvertisingStateChangedIndication 
*               callback from Black Box.
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
********************************************************************************** */
static void BleApp_GAPAdvertisingEventAdvertisingStateChangedIndication(bleEvtContainer_t* pMsg)
{
    mAdvStateOn = (mAdvStateOn)?FALSE:TRUE;
    if(mAdvStateOn)
    {
        (void)PRINTF("\n\rAdvertising Started\n\r");
    }
    else
    {
        (void)PRINTF("\n\rAdvertising Stopped\n\r");       
    }
}

/*! *********************************************************************************
* \brief        Sends GAPSetAdvertisingDataRequest command to the Black Box.
*
* \param[in]    Handle                  Advertising handle
* \param[in]    appAdvertisingData      Pointer to gapAdvertisingData_t structure.
*
* \return       MEM_SUCCESS_c is commands is correctly sent to FSCI to be transmitted
*               to the Black Box
********************************************************************************** */
static memStatus_t BleApp_GAPSetAdvertisingData(gapAdvertisingData_t* appAdvertisingData)
{
    uint8_t i = 0;
  
    GAPSetAdvertisingDataRequest_t req = { 0 };
    req.ScanResponseDataIncluded = FALSE;
    req.ScanResponseData.NbOfAdStructures = 0;
    req.ScanResponseData.AdStructures = NULL;
    req.AdvertisingDataIncluded=TRUE;
    req.AdvertisingData.NbOfAdStructures = appAdvertisingData->cNumAdStructures;
    
    if(appAdvertisingData->cNumAdStructures > 0U)
    {
        req.AdvertisingData.AdStructures = MEM_BufferAlloc(appAdvertisingData->cNumAdStructures * sizeof(*req.AdvertisingData.AdStructures));
        if(NULL == req.AdvertisingData.AdStructures)
        {
            panic( 0, (uint32_t)BleApp_GAPSetAdvertisingData, 0, 0 );
            return MEM_ALLOC_ERROR_c;
        }
        
        for(i=0;i<appAdvertisingData->cNumAdStructures;i++)
        {
            req.AdvertisingData.AdStructures[i].Length = appAdvertisingData->aAdStructures[i].length - 0x01U;
            req.AdvertisingData.AdStructures[i].Type = (GAPSetAdvertisingDataRequest_AdvertisingData_AdStructures_Type_t)appAdvertisingData->aAdStructures[i].adType;
            req.AdvertisingData.AdStructures[i].Data = MEM_BufferAlloc((uint32_t)appAdvertisingData->aAdStructures[i].length - 0x01U);
            if(NULL == req.AdvertisingData.AdStructures[i].Data)
            {
                panic( 0, (uint32_t)BleApp_GAPSetAdvertisingData, 0, 0 );
                return MEM_ALLOC_ERROR_c;
            }
            FLib_MemCpy(req.AdvertisingData.AdStructures[i].Data, appAdvertisingData->aAdStructures[i].aData, (uint32_t)appAdvertisingData->aAdStructures[i].length - 0x01U);
        }
    }
    else
    {
        req.AdvertisingData.AdStructures = NULL;
    }
     
    memStatus_t result = GAPSetAdvertisingDataRequest(&req, NULL, gFsciInterface_c);
    
    if(appAdvertisingData->cNumAdStructures > 0U)
    {
        for(i=0;i<appAdvertisingData->cNumAdStructures;i++)
        {
            (void)MEM_BufferFree(req.AdvertisingData.AdStructures[i].Data);
            req.AdvertisingData.AdStructures[i].Data = NULL;
        }
        (void)MEM_BufferFree(req.AdvertisingData.AdStructures);
        req.AdvertisingData.AdStructures = NULL;
    }
    return result;
}

/*! *********************************************************************************
* \brief        Sends GAPSetAdvertisingParameters command to the Black Box.
*
* \param[in]    pAdvertisingParameters  Pointer to gapAdvertisingParameters_t structure.
*
* \return       MEM_SUCCESS_c is commands is correctly sent to FSCI to be transmitted
*               to the Black Box
********************************************************************************** */
static memStatus_t BleApp_GAPSetAdvertisingParameters(gapAdvertisingParameters_t* pAdvertisingParameters)
{
    GAPSetAdvertisingParametersRequest_t req = { 0 };
    req.MaxInterval = pAdvertisingParameters->maxInterval;
    req.MinInterval = pAdvertisingParameters->minInterval;
    req.AdvertisingType = (GAPSetAdvertisingParametersRequest_AdvertisingType_t)pAdvertisingParameters->advertisingType;
    req.OwnAddressType = (GAPSetAdvertisingParametersRequest_OwnAddressType_t)pAdvertisingParameters->ownAddressType;
    req.PeerAddressType = (GAPSetAdvertisingParametersRequest_PeerAddressType_t)pAdvertisingParameters->peerAddressType;
    FLib_MemCpy(req.PeerAddress, pAdvertisingParameters->peerAddress, sizeof(bleDeviceAddress_t));
    req.ChannelMap = (uint8_t)pAdvertisingParameters->channelMap;
    req.FilterPolicy = (GAPSetAdvertisingParametersRequest_FilterPolicy_t)pAdvertisingParameters->filterPolicy;
    return GAPSetAdvertisingParametersRequest(&req, NULL, gFsciInterface_c);
}


/////////////////////////////////////////////////////////////////////
///////////////////// Advertising APIs end //////////////////////////
/////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////
///////////////////// Scanning APIs /////////////////////////////////
/////////////////////////////////////////////////////////////////////
/*****************************************************************************
* \brief        Handles all messages received from the Black Box that are
*               used by BLE application for scanning
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
*
*\retval       TRUE    If the message type matched and a handler function 
*              was called
*\retval       FALSE   Message was not Digital Key Device application related
*****************************************************************************/
static bool_t BleApp_ScanningCb(bleEvtContainer_t* pMsg)
{
    bool_t matchFound = TRUE;

    switch (pMsg->id)
    {
 
        case (uint16_t)GAPScanningEventStateChangedIndication_FSCI_ID:
        {
            BleApp_GAPScanningEventStateChangedIndication(pMsg);
        }
        break;   
        
        case (uint16_t)GAPScanningEventDeviceScannedIndication_FSCI_ID:
        {
            BleApp_GAPScanningEventDeviceScannedIndication(pMsg);
        }
        break;    
        
        case (uint16_t)GAPScanningEventCommandFailedIndication_FSCI_ID:
        {
            /* Advertising Command Failed received */
            (void)PRINTF("\n\rScanning command failed \n\r");
            panic(0,(uint32_t)BleApp_ScanningCb,0,0);          
        }

        default:
        {
            matchFound = FALSE;
        }
        break;
    }

    
    return matchFound;
}

/*! *********************************************************************************
* \brief        Process scanning events to search for the DK Ranging Service.
*
* \param[in]    pElement                   pointer to element entru
* \param[in]    pElement                   pointer to data
* \param[in]    iDataLen                   data lenght
*
* \return       TRUE if matching
                FALSE otherwise
********************************************************************************** */
static bool_t MatchDataInAdvElementList(gapAdStructure_t *pElement,
                                        void *pData,
                                        uint8_t iDataLen)
{
    uint8_t i;
    bool_t status = FALSE;

    for (i = 0; i < (pElement->length - 1U); i += iDataLen)
    {
        if (FLib_MemCmp(pData, &pElement->aData[i], iDataLen))
        {
            status = TRUE;
            break;
        }
    }

    return status;
}

/*! *********************************************************************************
* \brief        Checks Scan data for a device to connect.
*
* \param[in]    pMsg                   Pointer to bleEvtContainer_t.
*
* \return       TRUE if the scanned device implements the W_UART profile,
                FALSE otherwise
********************************************************************************** */
static bool_t BleApp_CheckScanEventLegacy(bleEvtContainer_t* pMsg)
{
    uint8_t index = 0;
    bool_t foundMatch = FALSE;

    while (index < pMsg->Data.GAPScanningEventDeviceScannedIndication.DataLength)
    {
        gapAdStructure_t adElement;
        
        adElement.length = pMsg->Data.GAPScanningEventDeviceScannedIndication.Data[index];
        adElement.adType = (gapAdType_t)pMsg->Data.GAPScanningEventDeviceScannedIndication.Data[index + 1U];
        adElement.aData = &pMsg->Data.GAPScanningEventDeviceScannedIndication.Data[index + 2U];
        
        /* Search for Wireless UART Service */
        if ((adElement.adType == gAdIncomplete128bitServiceList_c)
            || (adElement.adType == gAdComplete128bitServiceList_c))
        {
            foundMatch = MatchDataInAdvElementList(&adElement,
                                                   gaGattDbDynamic_W_UartServiceUUID, 16);
        }

        /* Move on to the next AD element type */
        index += adElement.length + sizeof(uint8_t);
    }

    return foundMatch;  
}


/*! *********************************************************************************
* \brief        Handles BLE GAPScanningEventDeviceScannedIndication from Black Box.
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
********************************************************************************** */
static void BleApp_GAPScanningEventDeviceScannedIndication(bleEvtContainer_t* pMsg)
{
    if (pMsg == NULL)
    {
        return;
    }
    
    if(mDeviceFoundDeviceToConnect == TRUE)
    {
        return;
    }
                          
    /*  check the payload if we do not have a bond */
    if(BleApp_CheckScanEventLegacy(pMsg))
    {
        mDeviceFoundDeviceToConnect = TRUE;
        /* Set connection parameters and stop scanning. Connect on gScanStateChanged_c. */
        gConnReqParams.peerAddressType = (uint8_t)pMsg->Data.GAPScanningEventDeviceScannedIndication.AddressType;
        FLib_MemCpy(gConnReqParams.peerAddress,
                    pMsg->Data.GAPScanningEventDeviceScannedIndication.Address,
                    sizeof(bleDeviceAddress_t));

         (void)GAPStopScanningRequest(NULL, gFsciInterface_c);
#if defined(gAppUsePrivacy_d) && (gAppUsePrivacy_d)
         gConnReqParams.usePeerIdentityAddress = pMsg->Data.GAPScanningEventDeviceScannedIndication.advertisingAddressResolved;
#endif
    }
}
    
/*! *********************************************************************************
* \brief        Handles GAPAdvertisingEventExtAdvertisingStateChangedIndication 
*               callback from Black Box.
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
********************************************************************************** */
static void BleApp_GAPScanningEventStateChangedIndication(bleEvtContainer_t* pMsg)
{
    mScanningOn = (mScanningOn)?FALSE:TRUE;
    if(mScanningOn)
    {
        (void)PRINTF("\n\rScanning Started\n\r");
    }
    else
    {
        (void)PRINTF("\n\rScanning Stopped\n\r");    
        
        if(mDeviceFoundDeviceToConnect)
        {
            mDeviceFoundDeviceToConnect = FALSE;
            BleApp_GAPConnect(&gConnReqParams);
        }
        
    }
}


/*! *********************************************************************************
* \brief        Sends GAPStartScanningRequest command to the Black Box.
*
*
* \return       MEM_SUCCESS_c is commands is correctly sent to FSCI to be transmitted
*               to the Black Box
********************************************************************************** */
static memStatus_t BleApp_GAPStartScanning
(
    uint16_t duration,
    uint16_t period,
    GAPStartScanningRequest_FilterDuplicates_t filterDuplicates,
    gapScanningParameters_t* gScanParams
)
{
    GAPStartScanningRequest_t req = { 0 };
    req.Duration = duration;
    req.FilterDuplicates = filterDuplicates;
    req.Period = period;
    req.ScanningParametersIncluded = TRUE;
    req.ScanningPHYs = gScanParams->scanningPHYs;
    req.ScanningParameters.FilterPolicy = (GAPStartScanningRequest_ScanningParameters_FilterPolicy_t)gScanParams->filterPolicy;
    req.ScanningParameters.Interval = gScanParams->interval;
    req.ScanningParameters.OwnAddressType = (GAPStartScanningRequest_ScanningParameters_OwnAddressType_t)gScanParams->ownAddressType;
    req.ScanningParameters.Type = (GAPStartScanningRequest_ScanningParameters_Type_t)gScanParams->type;
    req.ScanningParameters.Window = gScanParams->window;
    
    return GAPStartScanningRequest(&req, NULL, gFsciInterface_c);
}

/////////////////////////////////////////////////////////////////////
///////////////////// Scanning APIs end//////////////////////////////
/////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////
///////////////////// Connection APIs ///////////////////////////////
/////////////////////////////////////////////////////////////////////
/*****************************************************************************
* \brief        Handles all messages received from the Black Box that are
*               used by BLE application for connection
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
*
*\retval       TRUE    If the message type matched and a handler function 
*              was called
*\retval       FALSE   Message was not Digital Key Device application related
*****************************************************************************/
static bool_t BleApp_ConnectionCb(bleEvtContainer_t* pMsg)
{
    bool_t matchFound = TRUE;

    switch (pMsg->id)
    {
 
        case (uint16_t)GAPConnectionEventConnectedIndication_FSCI_ID:
        {
            BleApp_ConnectionEventConnected(pMsg);
        }
        break; 
        
        case (uint16_t)GAPConnectionEventDisconnectedIndication_FSCI_ID:
        {
            BleApp_ConnectionEventDisconnected(pMsg);
        }
        break;
        
#if defined(gAppUseBonding_d) && (gAppUseBonding_d)
        /*case (uint16_t)GAPCheckIfBondedIndication_FSCI_ID:
        {
            BleApp_CheckIfBondedHandle(pMsg);
        }
        break;*/ 
        
        case (uint16_t)GAPLoadCustomPeerInformationIndication_FSCI_ID:
        {
            BleeApp_GAPLoadCustomPeerInformationIndication(pMsg);
        }
        break;
#endif
   
#if defined(gAppUsePairing_d) && (gAppUsePairing_d)
        case (uint16_t)GAPConnectionEventAuthenticationRejectedIndication_FSCI_ID:
        {
            BleApp_GAPConnectionEventAuthenticationRejected(pMsg);
        }
        break;  
        
        case (uint16_t)GAPConnectionEventPairingCompleteIndication_FSCI_ID:
        {
            BleApp_GAPConnectionEventPairingCompleteIndication(pMsg);
        }
        break;

        case (uint16_t)GAPConnectionEventPairingRequestIndication_FSCI_ID:
        {
            BleApp_GAPConnectionEventPairingRequestIndication(pMsg);
        }
        break;
   
        case (uint16_t)GAPConnectionEventPasskeyDisplayIndication_FSCI_ID:
        {
            ; /* Display on a screen or simply ignore */
        }
        break;
        
        case (uint16_t)GAPConnectionEventLeScDisplayNumericValueIndication_FSCI_ID:
        {
            BleApp_GAPConnectionEventLeScDisplayNumericValueIndication(pMsg);
        }
        break;
        
        case (uint16_t)GAPConnectionEventPasskeyRequestIndication_FSCI_ID:
        {
            BleApp_GAPConnectionEventPasskeyRequestIndication(pMsg);
        }
        break;    
        
        case (uint16_t)GAPConnectionEventKeyExchangeRequestIndication_FSCI_ID: 
        {
            BleApp_GAPConnectionEventKeyExchangeRequestIndication(pMsg);
        }
        break;
        
        case (uint16_t)GAPConnectionEventLongTermKeyRequestIndication_FSCI_ID:
        {
            BleApp_GAPConnectionEventLongTermKeyRequestIndication(pMsg);
        }
        break;
#endif        
        default:
        {
            matchFound = FALSE;
        }
        break;
    }

    
    return matchFound;
}


/*! *********************************************************************************
* \brief        Sends GAPConnectRequest command to the Black Box.
*
* \param[in]    pParameters            Pointer to GAP Connect parameters structure.
*
* \return       MEM_SUCCESS_c is commands is correctly sent to FSCI to be transmitted
*               to the Black Box
********************************************************************************** */
static memStatus_t BleApp_GAPConnect(gapConnectionRequestParameters_t* pParameters)
{  
    GAPConnectRequest_t req = { 0 };
    req.ConnEventLengthMax = pParameters->connEventLengthMax;
    req.ConnEventLengthMin = pParameters->connEventLengthMin;
    req.ConnIntervalMax = pParameters->connIntervalMax;
    req.ConnIntervalMin = pParameters->connIntervalMin;
    req.ConnLatency = pParameters->connLatency;
    req.FilterPolicy = GAPConnectRequest_FilterPolicy_gUseDeviceAddress_c;
    req.Initiating_PHYs = pParameters->initiatingPHYs;
    req.OwnAddressType = GAPConnectRequest_OwnAddressType_gPublic_c;
    FLib_MemCpy(req.PeerAddress, pParameters->peerAddress, sizeof(bleDeviceAddress_t));
    if(pParameters->peerAddressType == gBleAddrTypePublic_c)
    {
        req.PeerAddressType = GAPConnectRequest_PeerAddressType_gPublic_c;
    }
    else if (pParameters->peerAddressType == gBleAddrTypeRandom_c)
    {
        req.PeerAddressType = GAPConnectRequest_PeerAddressType_gRandom_c;
    }
    else
    {
	/* For MISRA compliance */
    }
    req.ScanInterval = pParameters->scanInterval;
    req.ScanWindow = pParameters->scanWindow;
    req.SupervisionTimeout = pParameters->supervisionTimeout;
    req.usePeerIdentityAddress = pParameters->usePeerIdentityAddress;
    return GAPConnectRequest(&req, NULL, gFsciInterface_c);
}

/*****************************************************************************
* \brief        Handler of GAPConnectionEventConnectedIndication_FSCI_ID
*               event for App_HandleHSDKMessageInput.
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
*
* Return value: None
*****************************************************************************/
static void BleApp_ConnectionEventConnected(bleEvtContainer_t* pMsg)
{
    uint8_t peerDeviceId = pMsg->Data.GAPConnectionEventConnectedIndication.DeviceId;
    
    gaPeerInformation[peerDeviceId].deviceId = peerDeviceId;
    gaPeerInformation[peerDeviceId].isBonded = FALSE;
    gaPeerInformation[peerDeviceId].gapRole = (gapRole_t)pMsg->Data.GAPConnectionEventConnectedIndication.connectionRole;
    
    (void)PRINTF("Connected to device %d", peerDeviceId);

     if ( gaPeerInformation[peerDeviceId].gapRole == gGapCentral_c)
     {
        (void)PRINTF(" as master.\n\r");
        BleApp_StateMachineHandler(peerDeviceId, mAppEvt_PeerConnected_c, pMsg);
     }
     else
     {
        (void)PRINTF(" as slave.\n\r");
        //(void)BleApp_GapSendSlaveSecurityReq(mDeviceIdSaved, &gPairingParameters);
     }    
    
#if defined(gAppUseBonding_d) && (gAppUseBonding_d)
    /*
    Save peerDeviceId because GAPCheckIfBondedIndication and
    GAPLoadCustomPeerInformationIndication don't return a peerDeviceId 
    */
    mDeviceIdSaved = peerDeviceId;
    (void)BleApp_GAPCheckIfBondedReq(peerDeviceId); 
#else
    BleApp_StateMachineHandler(peerDeviceId, mAppEvt_PeerConnected_c, pMsg);
#endif
}


/*****************************************************************************
* \brief        Handler of GAPConnectionEventDisconnectedIndication_FSCI_ID
*               event for App_HandleHSDKMessageInput.
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
*
* Return value: None
*****************************************************************************/
static void BleApp_ConnectionEventDisconnected(bleEvtContainer_t* pMsg)
{
    uint8_t peerDeviceId = pMsg->Data.GAPConnectionEventConnectedIndication.DeviceId;
    
    (void)PRINTF("Disconnected from device %d.\n\r", peerDeviceId);
    
    gaPeerInformation[peerDeviceId].appState = mAppIdle_c;
    gaPeerInformation[peerDeviceId].clientInfo.hService = INVALID_HANDLE;
    gaPeerInformation[peerDeviceId].clientInfo.hUartStream = INVALID_HANDLE;    

    /* mark device id as invalid */
    gaPeerInformation[peerDeviceId].deviceId = gInvalidDeviceId_c;    
}

#if defined(gAppUseBonding_d) && (gAppUseBonding_d)
/*! *********************************************************************************
* \brief        Sends GAPLoadCustomPeerInformationRequest command to the Black Box.
*
* \param[in]    deviceId               Device ID of the GAP peer.
* \param[in]    offset                 Data offset (offset from the beginning).
* \param[in]    infoSize               Data size.
*
* \return       MEM_SUCCESS_c is commands is correctly sent to FSCI to be transmitted
*               to the Black Box
********************************************************************************** */
static memStatus_t BleApp_GAPLoadCustomPeerInformation
(
    uint8_t deviceId,
    uint16_t offset,
    uint16_t infoSize
)
{
    GAPLoadCustomPeerInformationRequest_t req = { 0 };
    req.DeviceId = deviceId;
    req.Offset = offset;
    req.InfoSize = infoSize;
    return GAPLoadCustomPeerInformationRequest(&req, NULL, gFsciInterface_c);
}

/*****************************************************************************
* \brief        Handler of GAPCheckIfBondedIndication_FSCI_ID event for
*               App_HandleHSDKMessageInput.
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
*
* Return value: None
*****************************************************************************/
static void BleApp_CheckIfBondedHandle(bleEvtContainer_t* pMsg)
{
    if(pMsg->Data.GAPCheckIfBondedIndication.IsBonded == TRUE)
    {
        gaPeerInformation[mDeviceIdSaved].isBonded = TRUE;
        (void)BleApp_GAPLoadCustomPeerInformation(mDeviceIdSaved, 0,
                                           (uint16_t)sizeof(wucConfig_t));
    }
    else
    { 
#if defined(gAppUsePairing_d) && (gAppUsePairing_d)      
        if(gaPeerInformation[mDeviceIdSaved].gapRole == gGapCentral_c)
        {
            (void)BleApp_GAPPairReq(mDeviceIdSaved, &gPairingParameters);
        }
#else
        BleApp_StateMachineHandler(mDeviceIdSaved, mAppEvt_PeerConnected_c, NULL);
#endif        
    }
}


/*! *********************************************************************************
* \brief        Handles GAPLoadCustomPeerInformationIndication callback from Black Box.
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
********************************************************************************** */
static void BleeApp_GAPLoadCustomPeerInformationIndication(bleEvtContainer_t* pMsg)
{       
    if(pMsg->Data.GAPLoadCustomPeerInformationIndication.InfoSize > 0x00U)
    {
        FLib_MemCpy((void*) &gaPeerInformation[mDeviceIdSaved].clientInfo,
                    pMsg->Data.GAPLoadCustomPeerInformationIndication.Info,
                    pMsg->Data.GAPLoadCustomPeerInformationIndication.InfoSize);
#if defined(gAppUsePairing_d) && (gAppUsePairing_d)      
        if(gaPeerInformation[mDeviceIdSaved].gapRole == gGapCentral_c)
        {
            (void)BleApp_GAPEncryptLinkReq(mDeviceIdSaved);
        }
        else
        {
            (void)BleApp_GAPPairReq(mDeviceIdSaved, &gPairingParameters);
        }
#else
        BleApp_StateMachineHandler(mDeviceIdSaved, mAppEvt_PeerConnected_c, NULL);
#endif
    }
    else
    {
        BleApp_StateMachineHandler(mDeviceIdSaved, mAppEvt_PeerConnected_c, NULL);
    }
    
}
#endif /* defined(gAppUseBonding_d) && (gAppUseBonding_d) */

#if defined(gAppUsePairing_d) && (gAppUsePairing_d)
/*! *********************************************************************************
* \brief        Sends GAPEncryptLinkRequest command to the Black Box.
*
* \param[in]    deviceId               Device ID of the GAP peer.
*
* \return       MEM_SUCCESS_c is commands is correctly sent to FSCI to be transmitted
*               to the Black Box
********************************************************************************** */
memStatus_t BleApp_GAPEncryptLinkReq(uint8_t deviceId)
{
    GAPEncryptLinkRequest_t req = { 0 };
    req.DeviceId = deviceId;
    return GAPEncryptLinkRequest(&req, NULL, gFsciInterface_c);
}

/*! *********************************************************************************
* \brief        Sends GAPPairRequest command to the Black Box.
*
* \param[in]    deviceId               Device ID of the GAP peer.
*
* \return       MEM_SUCCESS_c is commands is correctly sent to FSCI to be transmitted
*               to the Black Box
********************************************************************************** */
memStatus_t BleApp_GAPPairReq(uint8_t deviceId, gapPairingParameters_t *pPairingParam)
{
    GAPPairRequest_t req = { 0 };
    req.DeviceId = deviceId;
    req.PairingParameters.WithBonding = pPairingParam->withBonding;
    req.PairingParameters.SecurityModeAndLevel = (GAPPairRequest_PairingParameters_SecurityModeAndLevel_t)pPairingParam->securityModeAndLevel;
    req.PairingParameters.MaxEncryptionKeySize = pPairingParam->maxEncryptionKeySize;
    req.PairingParameters.LocalIoCapabilities = (GAPPairRequest_PairingParameters_LocalIoCapabilities_t)pPairingParam->localIoCapabilities;
    req.PairingParameters.OobAvailable = pPairingParam->oobAvailable;
    req.PairingParameters.CentralKeys = pPairingParam->centralKeys;
    req.PairingParameters.PeripheralKeys = pPairingParam->peripheralKeys;
    req.PairingParameters.LeSecureConnectionSupported = pPairingParam->leSecureConnectionSupported;
    req.PairingParameters.UseKeypressNotifications = pPairingParam->useKeypressNotifications;      
      
    return GAPPairRequest(&req, NULL, gFsciInterface_c);
}


/*! *********************************************************************************
* \brief        Sends GAPSendSlaveSecurityRequestRequest command to the Black Box.
*
* \param[in]    deviceId               Device ID of the GAP peer.
*
* \return       MEM_SUCCESS_c is commands is correctly sent to FSCI to be transmitted
*               to the Black Box
********************************************************************************** */
memStatus_t BleApp_GapSendSlaveSecurityReq(uint8_t deviceId, gapPairingParameters_t *pPairingParam)
{
    GAPSendSlaveSecurityRequestRequest_t req = { 0 };
    req.DeviceId = deviceId;
    req.PairingParameters.WithBonding = pPairingParam->withBonding;
    req.PairingParameters.SecurityModeAndLevel = (GAPSendSlaveSecurityRequestRequest_PairingParameters_SecurityModeAndLevel_t)pPairingParam->securityModeAndLevel;
    req.PairingParameters.MaxEncryptionKeySize = pPairingParam->maxEncryptionKeySize;
    req.PairingParameters.LocalIoCapabilities = (GAPSendSlaveSecurityRequestRequest_PairingParameters_LocalIoCapabilities_t)pPairingParam->localIoCapabilities;
    req.PairingParameters.OobAvailable = pPairingParam->oobAvailable;
    req.PairingParameters.CentralKeys = pPairingParam->centralKeys;
    req.PairingParameters.PeripheralKeys = pPairingParam->peripheralKeys;
    req.PairingParameters.LeSecureConnectionSupported = pPairingParam->leSecureConnectionSupported;
    req.PairingParameters.UseKeypressNotifications = pPairingParam->useKeypressNotifications;      
      
    return GAPSendSlaveSecurityRequestRequest(&req, NULL, gFsciInterface_c);
}



/*! *********************************************************************************
* \brief        Sends GAPAcceptPairingRequestRequest command to the Black Box.
*
* \param[in]    deviceId               Device ID of the GAP peer.
*
* \return       MEM_SUCCESS_c is commands is correctly sent to FSCI to be transmitted
*               to the Black Box
********************************************************************************** */
memStatus_t BleApp_GAPAcceptPairingRequest(uint8_t deviceId, gapPairingParameters_t *pPairingParam)
{
    GAPAcceptPairingRequestRequest_t req;
    
    req.DeviceId = deviceId;
    req.PairingParameters.WithBonding = pPairingParam->withBonding;
    req.PairingParameters.SecurityModeAndLevel = (GAPAcceptPairingRequestRequest_PairingParameters_SecurityModeAndLevel_t)pPairingParam->securityModeAndLevel;
    req.PairingParameters.MaxEncryptionKeySize = pPairingParam->maxEncryptionKeySize;
    req.PairingParameters.LocalIoCapabilities = (GAPAcceptPairingRequestRequest_PairingParameters_LocalIoCapabilities_t)pPairingParam->localIoCapabilities;
    req.PairingParameters.OobAvailable = pPairingParam->oobAvailable;
    req.PairingParameters.CentralKeys = pPairingParam->centralKeys;
    req.PairingParameters.PeripheralKeys = pPairingParam->peripheralKeys;
    req.PairingParameters.LeSecureConnectionSupported = pPairingParam->leSecureConnectionSupported;
    req.PairingParameters.UseKeypressNotifications = pPairingParam->useKeypressNotifications;      
      
    return GAPAcceptPairingRequestRequest(&req, NULL, gFsciInterface_c);
}


/*! *********************************************************************************
* \brief        Handles BLE GAPConnectionEventPairingRequestIndication_FSCI_ID from Black Box.
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
********************************************************************************** */
static void BleApp_GAPConnectionEventPairingRequestIndication(bleEvtContainer_t* pMsg)
{
    uint8_t deviceId = pMsg->Data.GAPConnectionEventPairingResponseIndication.DeviceId;
    
    if((deviceId < gAppMaxConnections_c) &&
       (gaPeerInformation[deviceId].gapRole == gGapPeripheral_c))
    {
        gPairingParameters.centralKeys = pMsg->Data.GAPConnectionEventPairingRequestIndication.PairingParameters.CentralKeys;       
        
        /* TBD - add functionality for Repeated attempts */
        
        BleApp_GAPAcceptPairingRequest(pMsg->Data.GAPConnectionEventPairingResponseIndication.DeviceId, &gPairingParameters);
    }

}

/*! *********************************************************************************
* \brief        Handles BLE GAPConnectionEventPasskeyRequestIndication_FSCI_ID from Black Box.
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
********************************************************************************** */
static void BleApp_GAPConnectionEventPasskeyRequestIndication(bleEvtContainer_t* pMsg)
{
    uint8_t deviceId = pMsg->Data.GAPConnectionEventPasskeyRequestIndication.DeviceId;
    
    if(deviceId < gAppMaxConnections_c)
    {
        GAPEnterPasskeyRequest_t req;
        
        req.DeviceId = deviceId;
        req.Passkey = gPasskeyValue_c;
        
        /* Display on a screen for user confirmation then validate/invalidate based on value*/
        
        (void)GAPEnterPasskeyRequest(&req, NULL, gFsciInterface_c);
    }
}

/*! *********************************************************************************
* \brief        Handles BLE GAPConnectionEventKeyExchangeRequestIndication_FSCI_ID from Black Box.
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
********************************************************************************** */
static void BleApp_GAPConnectionEventKeyExchangeRequestIndication(bleEvtContainer_t* pMsg)
{
    uint8_t deviceId = pMsg->Data.GAPConnectionEventKeyExchangeRequestIndication.DeviceId;
    
    if(deviceId < gAppMaxConnections_c)
    {
      GAPSendSmpKeysRequest_t req = {0};
        
        req.DeviceId = deviceId;
        FLib_MemCpy(req.Keys.Csrk, gSmpKeys.aCsrk, 16);
        FLib_MemCpy(req.Keys.Irk, gSmpKeys.aIrk, 16);
        req.Keys.LtkInfo.Ltk = gSmpKeys.aLtk;    
        req.Keys.LtkInfo.LtkSize = gSmpKeys.cLtkSize;
        req.Keys.RandEdivInfo.RandSize = gSmpKeys.cRandSize;
        req.Keys.RandEdivInfo.Ediv = gSmpKeys.ediv;
        req.Keys.RandEdivInfo.Rand = gSmpKeys.aRand;
        req.Keys.AddressIncluded = 0;
        if(gSmpKeys.aAddress != NULL)
        {
            req.Keys.AddressIncluded = 1; 
            req.Keys.AddressInfo.DeviceAddressType = (GAPSendSmpKeysRequest_Keys_AddressInfo_DeviceAddressType_t ) gSmpKeys.addressType;
            FLib_MemCpy(req.Keys.AddressInfo.DeviceAddress, gSmpKeys.aAddress, 6);
        }
       
        req.Keys.LtkIncluded = 1;
        if ((pMsg->Data.GAPConnectionEventKeyExchangeRequestIndication.RequestedKeys & (uint8_t)gLtk_c) == 0U)
        {
            req.Keys.LtkIncluded = 0;
            /* When the LTK is NULL EDIV and Rand are not sent and will be ignored. */
        }

        req.Keys.IrkIncluded = 1;
        if ((pMsg->Data.GAPConnectionEventKeyExchangeRequestIndication.RequestedKeys & (uint8_t)gIrk_c) == 0U)
        {
            req.Keys.IrkIncluded = 0;
            /* When the IRK is NULL the Address and Address Type are not sent and will be ignored. */
        }
        req.Keys.CsrkIncluded = 1;
        if ((pMsg->Data.GAPConnectionEventKeyExchangeRequestIndication.RequestedKeys & (uint8_t)gCsrk_c) == 0U)
        {
            req.Keys.CsrkIncluded = 0;
        }
        
        (void)GAPSendSmpKeysRequest(&req, NULL, gFsciInterface_c);
    }
}

/*! *********************************************************************************
* \brief        Handles BLE GAPConnectionEventLongTermKeyRequestIndication_FSCI_ID from Black Box.
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
********************************************************************************** */
static void BleApp_GAPConnectionEventLongTermKeyRequestIndication(bleEvtContainer_t* pMsg)
{
    uint8_t deviceId = pMsg->Data.GAPConnectionEventLongTermKeyRequestIndication.DeviceId;
    
    if(deviceId < gAppMaxConnections_c)
    {
        if((pMsg->Data.GAPConnectionEventLongTermKeyRequestIndication.Ediv == gSmpKeys.ediv) &&
          (pMsg->Data.GAPConnectionEventLongTermKeyRequestIndication.RandSize == gSmpKeys.cRandSize) &&
            (TRUE == FLib_MemCmp(pMsg->Data.GAPConnectionEventLongTermKeyRequestIndication.Rand, gSmpKeys.aRand, gSmpKeys.cRandSize)))
        {
            GAPProvideLongTermKeyRequest_t req;
            
            req.DeviceId = deviceId;
            req.LtkSize = gSmpKeys.cLtkSize;
            req.Ltk = gSmpKeys.aLtk;
                         
            (void)GAPProvideLongTermKeyRequest(&req, NULL, gFsciInterface_c);
        }
        else
        {
            GAPDenyLongTermKeyRequest_t req;
            req.DeviceId = deviceId;
            (void)GAPDenyLongTermKeyRequest(&req, NULL, gFsciInterface_c);
        }
    }
}


/*! *********************************************************************************
* \brief        Handles BLE GAPConnectionEventLeScDisplayNumericValueIndication_FSCI_ID from Black Box.
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
********************************************************************************** */
static void BleApp_GAPConnectionEventLeScDisplayNumericValueIndication(bleEvtContainer_t* pMsg)
{
    uint8_t deviceId = pMsg->Data.GAPConnectionEventPasskeyDisplayIndication.DeviceId;
    
    if(deviceId < gAppMaxConnections_c)
    {
        GAPLeScValidateNumericValueRequest_t req;
        
        req.DeviceId = deviceId;
        req.Valid = TRUE;
        
        /* Display on a screen for user confirmation then validate/invalidate based on value*/
        
        (void)GAPLeScValidateNumericValueRequest(&req, NULL, gFsciInterface_c);
    }

}

/*****************************************************************************
* \brief        Handler of GAPConnectionEventAuthenticationRejectedIndication_FSCI_ID
*               event for App_HandleHSDKMessageInput.
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
*
* Return value: None
*****************************************************************************/
static void BleApp_GAPConnectionEventAuthenticationRejected(bleEvtContainer_t* pMsg)
{
    uint8_t deviceId = pMsg->Data.GAPConnectionEventAuthenticationRejectedIndication.DeviceId;
    if((deviceId < gAppMaxConnections_c) && 
       (gaPeerInformation[deviceId].gapRole == gGapCentral_c))
    {
        BleApp_GAPPairReq(mDeviceIdSaved, &gPairingParameters);
    }
}

/*****************************************************************************
* \brief        Handler of GAPConnectionEventPairingCompleteIndication_FSCI_ID
*               event for App_HandleHSDKMessageInput.
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
*
* Return value: None
*****************************************************************************/
static void BleApp_GAPConnectionEventPairingCompleteIndication(bleEvtContainer_t* pMsg)
{
    if(pMsg->Data.GAPConnectionEventPairingCompleteIndication.PairingStatus ==
         GAPConnectionEventPairingCompleteIndication_PairingStatus_PairingSuccessful)
    {
        uint8_t peerDeviceId = pMsg->Data.GAPConnectionEventPairingCompleteIndication.DeviceId;
        (void)PRINTF("Pairing - Success");
        
        if (gaPeerInformation[peerDeviceId].gapRole == gGapPeripheral_c)
        {
            /* call the state machine for peripheral role to start the discovery procedure */  
            BleApp_StateMachineHandler(peerDeviceId, mAppEvt_PairingComplete_c, NULL);     
        }
    }
    else
    {
        (void)PRINTF("Pairing Failed");         
    }
}
        
#endif /* defined(gAppUsePairing_d) && (gAppUsePairing_d)*/

/////////////////////////////////////////////////////////////////////
///////////////////// Connection APIs end////////////////////////////
/////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////
///////////////////// Service Discovery APIs ////////////////////////
/////////////////////////////////////////////////////////////////////
/*****************************************************************************
* \brief        Handles all messages received from the Black Box that are
*               used by BLE application for service discovery
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
*
*\retval       TRUE    If the message type matched and a handler function 
*              was called
*\retval       FALSE   Message was not Digital Key Device application related
*****************************************************************************/
static bool_t BleApp_ServiceDiscoveryCb(bleEvtContainer_t* pMsg)        
{
    bool_t matchFound = TRUE;
    
    switch (pMsg->id)
    {    
       
        case (uint16_t)GATTClientProcedureDiscoverPrimaryServicesByUuidIndication_FSCI_ID:
        {
            ServiceDiscovery_StateMachineHandler(pMsg->Data.GATTClientProcedureDiscoverPrimaryServicesByUuidIndication.DeviceId,
                                                                    appServiceDiscoveryEvent_GATTDiscoverPrimaryServicesByUuidIndication_c, pMsg);
        }
        break;

        case (uint16_t)GATTClientProcedureDiscoverAllCharacteristicsIndication_FSCI_ID:
        {
            ServiceDiscovery_StateMachineHandler(pMsg->Data.GATTClientProcedureDiscoverAllCharacteristicsIndication.DeviceId,
                                                                    appServiceDiscoveryEvent_GATTDiscoverAllCharacteristicsIndication_c, pMsg);
        }
        break;
        
        
        default:
        {
            matchFound = FALSE;
        }
        break;
    }
    
    return matchFound;
}
        
/*! *********************************************************************************
* \brief        State machine handler for the Service Discovery process.
*
* \param[in]    peerDeviceId        Peer device ID.
* \param[in]    pMsg                Message received form Black Box.
* \param[in]    event               Event type.
********************************************************************************** */
static void ServiceDiscovery_StateMachineHandler(
    deviceId_t                  peerDeviceId,
    appServiceDiscoveryEvent_t  event,
    bleEvtContainer_t*          pMsg
)
{
    /* If the application state is Service Discovery State, consume the event. */
    if(gaPeerInformation[peerDeviceId].appState == mAppServiceDisc_c)
    {
        switch(event)
        {     
            case appServiceDiscoveryEvent_GATTDiscoverPrimaryServicesByUuidIndication_c:
            {
                if (pMsg != NULL)
                {
                    if(pMsg->Data.GATTClientProcedureDiscoverPrimaryServicesByUuidIndication.ProcedureResult ==
                       GATTClientProcedureDiscoverPrimaryServicesByUuidIndication_ProcedureResult_gGattProcSuccess_c)
                    {
                        uint8_t index;
                        for(index = 0; index < pMsg->Data.GATTClientProcedureDiscoverPrimaryServicesByUuidIndication.NbOfDiscoveredServices; index++)
                        {
                            if ((pMsg->Data.GATTClientProcedureDiscoverPrimaryServicesByUuidIndication.DiscoveredServices[index].UuidType == Uuid128Bits) 
                                && FLib_MemCmp(pMsg->Data.GATTClientProcedureDiscoverPrimaryServicesByUuidIndication.DiscoveredServices[index].Uuid.Uuid128Bits, 
                                               gaGattDbDynamic_W_UartServiceUUID, mUuidType128_size))
                            {
                                /* Found W_uart Service */
                                /* Found Wireless UART Service */
                                gaPeerInformation[peerDeviceId].clientInfo.hService =
                                    pMsg->Data.GATTClientProcedureDiscoverPrimaryServicesByUuidIndication.DiscoveredServices[index].StartHandle;

                                if (pMsg->Data.GATTClientProcedureDiscoverPrimaryServicesByUuidIndication.DiscoveredServices[index].NbOfCharacteristics > 0U &&
                                    pMsg->Data.GATTClientProcedureDiscoverPrimaryServicesByUuidIndication.DiscoveredServices[index].Characteristics != NULL)
                                {
                                    /* Found Uart Characteristic */
                                    gaPeerInformation[peerDeviceId].clientInfo.hUartStream =
                                        pMsg->Data.GATTClientProcedureDiscoverPrimaryServicesByUuidIndication.DiscoveredServices[index].Characteristics[0].Value.Handle;
                                }
                                else
                                {
                                     /* Found w_uart_char */
                                    bleUuid_t serviceUuid;
                                    FLib_MemCpy(serviceUuid.uuid128, gaGattDbDynamic_W_UartStream, mUuidType128_size);
                                    (void)ServiceDiscovery_GATTClientDiscoverAllCharacteristicsOfService(peerDeviceId,
                                                                                    pMsg->Data.GATTClientProcedureDiscoverPrimaryServicesByUuidIndication.DiscoveredServices[index].StartHandle,
                                                                                    pMsg->Data.GATTClientProcedureDiscoverPrimaryServicesByUuidIndication.DiscoveredServices[index].EndHandle,
                                                                                    Uuid128Bits,
                                                                                    serviceUuid);                               
                                }
                                break;
                            }
                        }
                        if(index == pMsg->Data.GATTClientProcedureDiscoverPrimaryServicesByUuidIndication.NbOfDiscoveredServices)
                        {
                            BleApp_StateMachineHandler(peerDeviceId,  mAppEvt_ServiceDiscoveryNotFound_c, NULL);
                        }
                    }
                    else
                    {
                        BleApp_StateMachineHandler(peerDeviceId,  mAppEvt_ServiceDiscoveryFailed_c, NULL);
                    }
                }
            }
            break;
            
            case appServiceDiscoveryEvent_GATTDiscoverAllCharacteristicsIndication_c:
                if (pMsg != NULL)
                {
                    if(pMsg->Data.GATTClientProcedureDiscoverAllCharacteristicsIndication.ProcedureResult ==
                       GATTClientProcedureDiscoverAllCharacteristicsIndication_ProcedureResult_gGattProcSuccess_c)
                    {
                        uint8_t index;
                        for(index = 0; index < pMsg->Data.GATTClientProcedureDiscoverAllCharacteristicsIndication.Service.NbOfCharacteristics; index++)
                        {
                            if (((uint8_t)pMsg->Data.GATTClientProcedureDiscoverAllCharacteristicsIndication.Service.Characteristics[index].Value.UuidType == gBleUuidType128_c) &&
                                FLib_MemCmp(pMsg->Data.GATTClientProcedureDiscoverAllCharacteristicsIndication.Service.Characteristics[index].Value.Uuid.Uuid128Bits, 
                                            gaGattDbDynamic_W_UartStream, mUuidType128_size))
                            {
                                /* Found w_uart Char */
                                gaPeerInformation[peerDeviceId].clientInfo.hUartStream = 
                                      pMsg->Data.GATTClientProcedureDiscoverAllCharacteristicsIndication.Service.Characteristics[index].Value.Handle; 
                                BleApp_StateMachineHandler(peerDeviceId,  mAppEvt_ServiceDiscoveryComplete_c, NULL);
                            }
                            else
                            {
                                    /* For MISRA compliance */
                            }
                        }
                        if(index == pMsg->Data.GATTClientProcedureDiscoverAllCharacteristicsIndication.Service.NbOfCharacteristics)
                        {
                            BleApp_StateMachineHandler(peerDeviceId,  mAppEvt_ServiceDiscoveryNotFound_c, NULL);
                        }                       
                        
                    }
                    else
                    {
                        BleApp_StateMachineHandler(peerDeviceId,  mAppEvt_ServiceDiscoveryFailed_c, NULL);
                    }
                }
            break;            
            
            default:
            {
                ; /* No action required */
            }
            break;
        }
    }
    
}
        
        
/*! *********************************************************************************
* \brief        Sends GATTClientDiscoverAllCharacteristicsOfServiceRequest command to the Black Box.
*
* \param[in]    deviceId               Device ID of the connected peer.
* \param[in]    startHandle            The handle of the Service Declaration attribute.
* \param[in]    endHandle              The last handle belonging to this Service 
*                                      (followed by another Service declaration of the end of the database).
* \param[in]    uuidType               UUID type.
* \param[in]    uuid                   UUID value.
*
* \return       MEM_SUCCESS_c is commands is correctly sent to FSCI to be transmitted
*               to the Black Box
********************************************************************************** */
static memStatus_t ServiceDiscovery_GATTClientDiscoverAllCharacteristicsOfService(
    uint8_t         deviceId, 
    uint16_t        startHandle, 
    uint16_t        endHandle, 
    UuidType_t      uuidType, 
    bleUuid_t       uuid
)
{
    GATTClientDiscoverAllCharacteristicsOfServiceRequest_t req = { 0 };
    req.DeviceId = deviceId;
    req.MaxNbOfCharacteristics = 0x0A;
    req.Service.Characteristics = NULL;
    req.Service.EndHandle = endHandle;
    req.Service.StartHandle = startHandle;
    req.Service.IncludedServices = NULL;
    req.Service.NbOfIncludedServices = 0x00;
    req.Service.NbOfCharacteristics = 0x00;
    req.Service.UuidType = uuidType;
    
    switch (req.Service.UuidType)
    {
        case Uuid16Bits:
            Utils_PackTwoByteValue(uuid.uuid16, req.Service.Uuid.Uuid16Bits);
        break;

        case Uuid128Bits:
            FLib_MemCpy(req.Service.Uuid.Uuid128Bits, uuid.uuid128, 16);
        break;

        case Uuid32Bits:
            Utils_PackFourByteValue(uuid.uuid32, req.Service.Uuid.Uuid32Bits);
        break;
        
        default:
        {
                ; /* No action required */
        }
        break;
    }
    return GATTClientDiscoverAllCharacteristicsOfServiceRequest(&req, NULL, gFsciInterface_c);
}
 

/*****************************************************************************
* \brief       Handler of mAppExchangeMtu_c state for BleApp_StateMachineHandler
*
* \param[in]    peerDeviceId        Peer device ID.
* \param[in]    event               Event type.
* \param[in]    pMsg                Message received form Black Box.
*
* Return value: None
*****************************************************************************/
static void BleApp_HandleServiceDiscState
(
    deviceId_t peerDeviceId,
    appEvent_t event,
    bleEvtContainer_t* pMsg
)
{
    if (event == mAppEvt_ServiceDiscoveryComplete_c)
    {
        /* Moving to Running State*/
      gaPeerInformation[peerDeviceId].appState = mAppRunning_c;
      
#if gAppUseBonding_d
      /* Write data in NVM */
      (void)BleApp_GAPSaveCustomPeerInformation(gaPeerInformation[peerDeviceId].deviceId,
                                          0, sizeof(wucConfig_t), (void *) &gaPeerInformation[peerDeviceId].clientInfo);
#endif
    }
    else if (event == mAppEvt_ServiceDiscoveryNotFound_c)
    {
        /* Moving to Service discovery Retry State*/
        gaPeerInformation[peerDeviceId].appState = mAppServiceDiscRetry_c;
        /* Restart Service Discovery for all services */
        (void)BleApp_StartServiceDisc(peerDeviceId);
    }
    else if (event == mAppEvt_ServiceDiscoveryFailed_c)
    {
        GAPDisconnectRequest_t req = { 0 };
        req.DeviceId = peerDeviceId;
        (void)GAPDisconnectRequest(&req, NULL, gFsciInterface_c);
    }
    else
    {
        /* ignore other event types */
    } 
}

#if gAppUseBonding_d    
/*! *********************************************************************************
* \brief        Sends GAPSaveCustomPeerInformationRequest command to the Black Box.
*
* \param[in]    deviceId               Device ID of the GAP peer.
* \param[in]    offset                 Data offset (offset from the beginning). 
* \param[in]    infoSize               Data size. 
* \param[in]    Info                   Data. 
*
* \return       MEM_SUCCESS_c is commands is correctly sent to FSCI to be transmitted
*               to the Black Box
********************************************************************************** */
memStatus_t BleApp_GAPSaveCustomPeerInformation
(
    uint8_t deviceId, 
    uint16_t offset, 
    uint16_t infoSize, 
    const uint8_t *Info
)
{
    GAPSaveCustomPeerInformationRequest_t req = { 0 };
    req.DeviceId = deviceId;
    req.InfoSize = infoSize;
    req.Offset = offset;
    req.Info = MEM_BufferAlloc(infoSize);
    if(NULL == req.Info)
    {
        panic( 0, (uint32_t)BleApp_GAPSaveCustomPeerInformation, 0, 0 );
        return MEM_ALLOC_ERROR_c;
    }
    FLib_MemCpy(req.Info, Info, infoSize);
      
    memStatus_t result = GAPSaveCustomPeerInformationRequest(&req, NULL, gFsciInterface_c);
    (void)MEM_BufferFree(req.Info);
    req.Info = NULL;
    return result;
}
#endif /*#if gAppUseBonding_d */

/*****************************************************************************
* \brief       Handler of mAppExchangeMtu_c state for BleApp_StateMachineHandler
*
* \param[in]    peerDeviceId        Peer device ID.
* \param[in]    event               Event type.
* \param[in]    pMsg                Message received form Black Box.
*
* Return value: None
*****************************************************************************/
static void BleApp_HandleServiceDiscRetryState
(
    deviceId_t peerDeviceId,
    appEvent_t event,
    bleEvtContainer_t* pMsg
)
{

    if (event == mAppEvt_ServiceDiscoveryComplete_c)
    {
        /* Moving to Running State*/
      gaPeerInformation[peerDeviceId].appState = mAppRunning_c;
      
#if gAppUseBonding_d
      /* Write data in local table/NVM */
      (void)BleApp_GAPSaveCustomPeerInformation(gaPeerInformation[peerDeviceId].deviceId, 0, sizeof(wucConfig_t),
                                          (void *) &gaPeerInformation[peerDeviceId].clientInfo);
#endif
    }
    else if ((event == mAppEvt_ServiceDiscoveryNotFound_c) ||
             (event == mAppEvt_ServiceDiscoveryFailed_c))
    {
         (void)BleApp_GAPDisconnect(peerDeviceId);
    }
    else
    {
        /* ignore other event types */
    } 
}        

/*! *********************************************************************************
* \brief        Sends GATTClientDiscoverPrimaryServicesByUuidRequest command to the Black Box.
*
* \param[in]    deviceId               Device ID of the connected peer.
* \param[in]    uuidType               UUID type.
* \param[in]    uuid                   UUID value.
* \param[in]    maxNbOfServices        Maximum number of services to be filled.
*
* \return       MEM_SUCCESS_c is commands is correctly sent to FSCI to be transmitted
*               to the Black Box
********************************************************************************** */
static memStatus_t BleApp_GATTClientDiscoverPrimaryServicesByUuid(
    uint8_t         deviceId,
    UuidType_t      uuidType,
    bleUuid_t       uuid,
    uint8_t         maxNbOfServices
)
{  
    GATTClientDiscoverPrimaryServicesByUuidRequest_t req = { 0 };
    req.DeviceId = deviceId;
    req.MaxNbOfServices = maxNbOfServices;
    req.UuidType = uuidType;
    switch (req.UuidType)
    {
        case Uuid16Bits:
            Utils_PackTwoByteValue(uuid.uuid16, req.Uuid.Uuid16Bits);
        break;

        case Uuid128Bits:
            FLib_MemCpy(req.Uuid.Uuid128Bits, uuid.uuid128, 16);
        break;

        case Uuid32Bits:
            Utils_PackFourByteValue(uuid.uuid32, req.Uuid.Uuid32Bits);
        break;
        
        default:
        {
                ; /* No action required */
        }
        break;
    }
    
    return GATTClientDiscoverPrimaryServicesByUuidRequest(&req, NULL, gFsciInterface_c);
}


/*****************************************************************************
* \brief        Start Service discovery
*
* \param[in]    peerDeviceId        Peer device ID.
* \param[in]    event               Event type.
* \param[in]    pMsg                Message received form Black Box.
*
* Return value: None
*****************************************************************************/
static void BleApp_StartServiceDisc
(
    deviceId_t peerDeviceId
)
{
    bleUuid_t bleUuid;
    
    FLib_MemCpy(&bleUuid.uuid128[0], gaGattDbDynamic_W_UartServiceUUID, 16);
    (void)BleApp_GATTClientDiscoverPrimaryServicesByUuid(peerDeviceId, Uuid128Bits, bleUuid, 0x01);
    
}
        
        
/////////////////////////////////////////////////////////////////////
///////////////////// Service Discovery APIs end ////////////////////
///////////////////////////////////////////////////////////////////// 

/////////////////////////////////////////////////////
///////////////////// GATT APIs  ////////////////////
///////////////////////////////////////////////////// 
/*****************************************************************************
* \brief        Handles all messages received from the Black Box that are
*               used by BLE applications from GATT layer.
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
*
*\retval       TRUE    If the message type matched and a handler function 
*              was called
*\retval       FALSE   Message was not Digital Key Car Anchor or 
*              Digital Key Device GATT leyer related          
*****************************************************************************/
static bool_t BleApp_GattServerCallback(bleEvtContainer_t* pMsg)
{
    bool_t matchFound = TRUE;
    
    switch (pMsg->id)
    {
        case (uint16_t)GATTClientProcedureExchangeMtuIndication_FSCI_ID:
        {
            BleApp_GATTClientProcedureExchangeMtuIndication(pMsg);
        }       
        break;
               
        case (uint16_t)GATTServerAttributeWrittenWithoutResponseIndication_FSCI_ID:
        {
            BleApp_GATTServerAttributeWrittenWithoutResponseIndication(pMsg);
        }
        break;
      
      
        default:
        {
            matchFound = FALSE;
        }
        break;
    }
    
    return matchFound;
}

/*****************************************************************************
* \brief        Handler of GATTClientProcedureExchangeMtuIndication_FSCI_ID
*               event for App_HandleHSDKMessageInput.
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
*
* Return value: None
*****************************************************************************/
static void BleApp_GATTClientProcedureExchangeMtuIndication(bleEvtContainer_t* pMsg)
{
    if (pMsg->Data.GATTClientProcedureExchangeMtuIndication.ProcedureResult ==
        GATTClientProcedureExchangeMtuIndication_ProcedureResult_gGattProcSuccess_c)
    {
        BleApp_StateMachineHandler(pMsg->Data.GATTClientProcedureExchangeMtuIndication.DeviceId,
                                             mAppEvt_GattProcComplete_c, pMsg);
    }
    else
    {
        BleApp_StateMachineHandler(pMsg->Data.GATTClientProcedureExchangeMtuIndication.DeviceId,
                                             mAppEvt_GattProcError_c, pMsg);
    }  
}
        
/*****************************************************************************
* \brief        Handler of GATTServerAttributeWrittenWithoutResponseIndication_FSCI_ID
*               event for App_HandleHSDKMessageInput.
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
*
* Return value: None
*****************************************************************************/
static void BleApp_GATTServerAttributeWrittenWithoutResponseIndication(bleEvtContainer_t* pMsg)
{
    uint8_t deviceId = pMsg->Data.GATTServerAttributeWrittenWithoutResponseIndication.DeviceId;
    
    if(deviceId < gAppMaxConnections_c)
    {
        if(pMsg->Data.GATTServerAttributeWrittenWithoutResponseIndication.AttributeWrittenEvent.Handle == mUARTCharHandleForWrite)
        {
            BleApp_ReceivedUartStream(deviceId,
                                      pMsg->Data.GATTServerAttributeWrittenWithoutResponseIndication.AttributeWrittenEvent.Value,
                                      pMsg->Data.GATTServerAttributeWrittenWithoutResponseIndication.AttributeWrittenEvent.ValueLength);
        }
    }
 
}

/*! *********************************************************************************
* \brief        Sends GATTClientExchangeMtuRequest command to the Black Box.
*
* \param[in]    deviceId               Device ID of the connected peer.
* \param[in]    mtu                    Desired MTU size for the connected peer
*
* \return       MEM_SUCCESS_c is commands is correctly sent to FSCI to be transmitted
*               to the Black Box
********************************************************************************** */
memStatus_t BleApp_GATTClientExchangeMtu(uint8_t deviceId, uint16_t mtu)
{
    GATTClientExchangeMtuRequest_t req = { 0 };
    req.DeviceId = deviceId;
    req.Mtu = mtu;
    return GATTClientExchangeMtuRequest(&req, NULL, gFsciInterface_c);
}        

//////////////////////////////////////////////////////////
///////////////////// GATT APIs end  ////////////////////
///////////////////////////////////////////////////////// 

/*****************************************************************************
* \brief        Handles all messages received from the Black Box 
*
* \param[in]    pMsg    Pointer to bleEvtContainer_t.
*
*\retval       TRUE    If the message type matched and a handler function 
*              was called
*\retval       FALSE   Message was not Digital Key Car Anchor or 
*              Digital Key Device GATT leyer related          
*****************************************************************************/
static bool_t BleApp_GenericCallback(bleEvtContainer_t* pMsg)
{
    bool_t matchFound = TRUE;
    
    switch (pMsg->id)
    {
        case (uint16_t)GAPGenericEventInternalErrorIndication_FSCI_ID:
        {
            panic( 0, (uint32_t)BleApp_GenericCallback, 0, 0 );
        }       
        break;
              
        case GAPConfirm_FSCI_ID:
        {
            if(pMsg->Data.GAPConfirm.Status != 0x00)
            {
                panic( 0, (uint32_t)BleApp_GenericCallback, 0, 0 );
            }
        }
        break;
      
        default:
        {
            matchFound = FALSE;
        }
        break;
    }
    
    return matchFound;
}


/*! *********************************************************************************
* \brief        Sends GAPCheckIfBondedRequest command to the Black Box.
*
* \param[in]    deviceId               Device ID of the GAP peer.
*
* \return       MEM_SUCCESS_c is commands is correctly sent to FSCI to be transmitted
*               to the Black Box
********************************************************************************** */
#if defined(gAppUseBonding_d) && (gAppUseBonding_d)
memStatus_t BleApp_GAPCheckIfBondedReq(uint8_t deviceId)
{
    GAPCheckIfBondedRequest_t req = { 0 };
    req.DeviceId = deviceId;
    
    (void)RegisterRemovableObserver(GAPCheckIfBondedIndication_FSCI_ID, BleApp_CheckIfBondedHandle);    
    return GAPCheckIfBondedRequest(&req, NULL, gFsciInterface_c);
}
#endif
/*! *********************************************************************************
* \brief        Sends GAPDisconnectRequest command to the Black Box.
*
* \param[in]    deviceId               The connected peer to disconnect from.
*
* \return       MEM_SUCCESS_c is commands is correctly sent to FSCI to be transmitted
*               to the Black Box
********************************************************************************** */
memStatus_t BleApp_GAPDisconnect(uint8_t deviceId)
{
    GAPDisconnectRequest_t req = { 0 };
    req.DeviceId = deviceId;
    return GAPDisconnectRequest(&req, NULL, gFsciInterface_c);
}

/*****************************************************************************
* \brief        Handler of mAppIdle_c state for void BleApp_StateMachineHandler
*
* \param[in]    peerDeviceId        Peer device ID.
* \param[in]    event               Event type.
* \param[in]    pMsg                Message received form Black Box.
*
* Return value: None
*****************************************************************************/
static void  BleApp_HandleIdleState
(
    deviceId_t peerDeviceId,
    appEvent_t event,
    bleEvtContainer_t* pMsg
)
{
  
    if (event == mAppEvt_PeerConnected_c)
    {
        /* Let the central device initiate the Exchange MTU procedure*/
        if (gaPeerInformation[peerDeviceId].gapRole == gGapCentral_c)
        {
            /* Moving to Exchange MTU State */
            gaPeerInformation[peerDeviceId].appState = mAppExchangeMtu_c;
            (void)BleApp_GATTClientExchangeMtu(peerDeviceId, gAttMaxMtu_c);
        }
        else
        {
            /* Moving to Service Discovery State*/
            gaPeerInformation[peerDeviceId].appState = mAppServiceDisc_c;

            /* Start Service Discovery*/
            (void)BleApp_StartServiceDisc(peerDeviceId);
        }
    }  
    else if (event == mAppEvt_PairingComplete_c) 
    {
         /* Let the central device initiate the Exchange MTU procedure*/
        if (gaPeerInformation[peerDeviceId].gapRole == gGapPeripheral_c)
        {
            /* Moving to Service Discovery State*/
            gaPeerInformation[peerDeviceId].appState = mAppServiceDisc_c;

            /* Start Service Discovery*/
            (void)BleApp_StartServiceDisc(peerDeviceId);
        }       
    }
   
}

/*****************************************************************************
* \brief       Handler of mAppExchangeMtu_c state for BleApp_StateMachineHandler
*
* \param[in]    peerDeviceId        Peer device ID.
* \param[in]    event               Event type.
* \param[in]    pMsg                Message received form Black Box.
*
* Return value: None
*****************************************************************************/
static void BleApp_HandleExchangeMtuState
(
    deviceId_t peerDeviceId,
    appEvent_t event,
    bleEvtContainer_t* pMsg
)
{
    if (event == mAppEvt_GattProcComplete_c)
    {
        /* Moving to Service Discovery State*/
        gaPeerInformation[peerDeviceId].appState = mAppServiceDisc_c;
                
        /* Start Service Discovery */
        BleApp_StartServiceDisc(peerDeviceId);
    }
    else
    {
        if (event == mAppEvt_GattProcError_c)
        {
            (void)BleApp_GAPDisconnect(peerDeviceId);
        }
    }
}        


/*! *********************************************************************************
* \brief  Generates LTK, IRK, CSRK, ediv and rand
*
* \param[in] none
*
* \return none
*
********************************************************************************** */
static void BleApp_MCUInfoToSmpKeys(void)
{
#if 0
    uint8_t uid[16] = {0};
    uint8_t len = 0;
    uint8_t sha256Output[SHA256_HASH_SIZE];

    BOARD_GetMCUUid (uid, &len);

    if(len > 0U)
    {
        /* generate LTK. LTK size always smaller than SHA1 hash size */
        uid[len - 1U]++;
        SHA256_Hash (uid, len, sha256Output);
        FLib_MemCpy (gSmpKeys.aLtk, sha256Output, gSmpKeys.cLtkSize);

#ifndef gUseCustomIRK_d
        /* generate IRK */
        uid[len - 1U]++;
        SHA256_Hash (uid, len, sha256Output);
        FLib_MemCpy (gSmpKeys.aIrk, sha256Output, gcSmpIrkSize_c);
#endif

        /* generate CSRK */
        uid[len - 1U]++;
        SHA256_Hash (uid, len, sha256Output);
        FLib_MemCpy (gSmpKeys.aCsrk, sha256Output, gcSmpCsrkSize_c);

        /* generate ediv and rand */
        uid[len - 1U]++;
        SHA256_Hash (uid, len, sha256Output);
        gSmpKeys.ediv = (uint16_t)sha256Output[0];
        FLib_MemCpy (&(gSmpKeys.ediv), &(sha256Output[0]), sizeof(gSmpKeys.ediv));
        FLib_MemCpy (gSmpKeys.aRand, &(sha256Output[sizeof(gSmpKeys.ediv)]), gSmpKeys.cRandSize);
    }
#endif
}

///////////////////////////////////////////////////////////////////
///////////////////// UART COMMUNICATION APIs  ////////////////////
///////////////////////////////////////////////////////////////////  
        
/*! *********************************************************************************
* \brief        Function to print received data
*
* \param[in]    pData        Parameters.
********************************************************************************** */          
static void BleApp_ReceivedUartStream(deviceId_t peerDeviceId, uint8_t *pStream, uint16_t streamLength)
{
    static deviceId_t previousDeviceId = gInvalidDeviceId_c;

    char additionalInfoBuff[10] = { '\r', '\n', '[', '0', '0', '-', 'M', ']', ':', ' '};
    uint8_t *pBuffer = NULL;
    uint32_t messageHeaderSize = 0;

    if (mAppUartNewLine || (previousDeviceId != peerDeviceId))
    {
        streamLength += (uint16_t)sizeof(additionalInfoBuff);
    }

    /* Allocate buffer for asynchronous write */
    pBuffer = MEM_BufferAlloc(streamLength);

    if (pBuffer != NULL)
    {
        /* if this is a message from a previous device, print device ID */
        if (mAppUartNewLine || (previousDeviceId != peerDeviceId))
        {
            messageHeaderSize = sizeof(additionalInfoBuff);

            if (mAppUartNewLine)
            {
                mAppUartNewLine = FALSE;
            }

            additionalInfoBuff[3] = '0' + (peerDeviceId / 10U);
            additionalInfoBuff[4] = '0' + (peerDeviceId % 10U);

            if (gGapCentral_c != gaPeerInformation[peerDeviceId].gapRole)
            {
                additionalInfoBuff[6] = 'S';
            }

            FLib_MemCpy(pBuffer, additionalInfoBuff, sizeof(additionalInfoBuff));
        }

        FLib_MemCpy(pBuffer + messageHeaderSize, pStream, (uint32_t)streamLength - messageHeaderSize);
        (void)Serial_AsyncWrite(gAppSerMgrIf, pBuffer, streamLength, Uart_TxCallBack, pBuffer);
    }

    /* update the previous device ID */
    previousDeviceId = peerDeviceId;
}

/*! *********************************************************************************
* \brief        Handles UART Transmit callback.
*
* \param[in]    pData        Parameters.
********************************************************************************** */
static void Uart_TxCallBack(void *pBuffer)
{
    (void)MEM_BufferFree(pBuffer);
}

#if gKeyBoardSupported_d && (gKBD_KeysCount_c > 0) && !defined (DUAL_MODE_APP)
/*! *********************************************************************************
* \brief        Handles Keyboard events
*
* \param[in]    events        Events from keyboard module.
********************************************************************************** */
static void App_KeyboardCallBack(uint8_t   events )
{
    switch (events)
    {
        case gKBD_EventPressPB1_c:
        {
            BleApp_Start(gGapPeripheral_c);
            break;
        }

        case gKBD_EventLongPB1_c:
        {
        	BleApp_Start(gGapCentral_c);
            break;
        }

        default:
        {
            ; /* No action required */
            break;
        }
    }
}
#endif

/*! *********************************************************************************
* @}
********************************************************************************** */
