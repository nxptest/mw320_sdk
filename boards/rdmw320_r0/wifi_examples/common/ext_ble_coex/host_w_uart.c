/*! *********************************************************************************
* \addtogroup CCC Digital Key Applications
* @{
********************************************************************************** */
/*! *********************************************************************************
* Copyright 2021 NXP
* All rights reserved.
*
* \file ccc_host.c
*
* SPDX-License-Identifier: BSD-3-Clause
********************************************************************************** */

/************************************************************************************
*************************************************************************************
* Include
*************************************************************************************
************************************************************************************/
/* Framework / Drivers */
#include <stdio.h>

#include "frm_os_abstraction.h"
#include "MemManager.h"
#include "Messaging.h"
#include "TimersManager.h"
#include "SerialManager.h"
#include "FsciInterface.h"
#include "hsdk_interface.h"
#include "Panic.h"
#include "host_w_uart.h"

/*  Application */
#include "w_uart_app.h"

//#include "board.h"
//#include "RNG_Interface.h"

extern gapRole_t role;

/************************************************************************************
*************************************************************************************
* Private macros
*************************************************************************************
************************************************************************************/
#ifndef mAppTaskWaitTime_c
#define mAppTaskWaitTime_c (osaWaitForever_c)
#endif

/* Application Events */
#define gAppEvtMsgFromHSDK_c            (1U << 0U)
#define gAppEvtAppCallback_c            (1U << 1U)

/************************************************************************************
*************************************************************************************
* Private type definitions
*************************************************************************************
************************************************************************************/
typedef struct appMsgCallback_tag{
    appCallbackHandler_t   handler;
    appCallbackParam_t     param;
}appMsgCallback_t;
/************************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
************************************************************************************/

static osaEventId_t  mAppEvent;

/* Application input queues */
static anchor_t mHSDKAppInputQueue;
static anchor_t mAppCbInputQueue;

/* FSCI Interface Configuration structure */
static const gFsciSerialConfig_t fsciConfigStruct[] = {
    /* Baudrate,                        interface type,                  channel No,                virtual interface */
    {(uint32_t)APP_SERIAL_INTERFACE_SPEED, APP_SERIAL_INTERFACE_TYPE, APP_SERIAL_INTERFACE_INSTANCE,           gFsciInterface_c}
};

/************************************************************************************
*************************************************************************************
* Private functions prototypes
*************************************************************************************
************************************************************************************/
void App_Thread (osaTaskParam_t param);
void adv_task(void* argument);
static void HSDKCallback(bleEvtContainer_t *container);


/************************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
************************************************************************************/

void adv_task(void* argument)
{
  FRM_OSA_TimeDelay(5000);

    while (1)
    {
        FRM_OSA_TimeDelay(500);

        if (role == gGapCentral_c || role == gGapPeripheral_c)
        {
            BleApp_Start(role);
            break;
        }
    }

  for(;;)
  {
    FRM_OSA_TimeDelay(8000);
  }
}
void ble_task(osaTaskParam_t param)
{
    static bool_t platformInitialized = FALSE;

    if (FALSE == platformInitialized)
    {
        platformInitialized = TRUE;

#if !defined(CPU_QN908X) && !defined (CPU_JN518X) && defined(gDCDC_Enabled_d) && (gDCDC_Enabled_d)
        /* Init DCDC module */
        BOARD_DCDCInit();
#endif
#if defined (CPU_JN518X)
        BOARD_SetFaultBehaviour();
#endif

        (void)MEM_Init();

        TMR_Init();


        /* RNG software initialization and PRNG initial seeding (from hardware) */
        //(void)RNG_Init();
        //RNG_SetPseudoRandomNoSeed(NULL);

        /* Initialize peripheral drivers specific to the application */
        
        /* Init serial manager */
        Serial_InitManager();
        
        /* Init FSCI */
        FSCI_Init((const void*)fsciConfigStruct);
        
        /* Init HSDK with FSCI Interface Id */
        HsdkInit(0);
        
        (void)RegistergHSDKCallback(HSDKCallback);
        (void)RegistergAppCallbackHSDK(App_HandleHSDKMessageInput);
        
        BleApp_Init();
        
        /* Create application event */
        mAppEvent = FRM_OSA_EventCreate(TRUE);
        if( NULL == mAppEvent )
        {
            panic(0,0,0,0);
            return;
        }
        
        /* Prepare application input queue.*/
        MSG_InitQueue(&mHSDKAppInputQueue);

        /* Prepare callback input queue.*/
        MSG_InitQueue(&mAppCbInputQueue);
       
        /* Reset Black Box to start the configure process  */
        FRM_OSA_TimeDelay(5000);
        (void)FSCICPUResetRequest(NULL, gFsciInterface_c);
            
    }

    /* Call application task */
    App_Thread( param );

    
}

/*! *********************************************************************************
* \brief  This function represents the Application task.
*         This task reuses the stack allocated for the MainThread.
*         This function is called to process all events for the task. Events
*         include timers, messages and any other user defined events.
* \param[in]  argument
*
* \remarks  For bare-metal, process only one type of message at a time,
*           to allow other higher priority task to run.
*
********************************************************************************** */
void App_Thread (osaTaskParam_t param)
{
#if !defined(gHybridApp_d) || (!gHybridApp_d)
    osaEventFlags_t event = 0U;

    for (;;)
    {

        (void)FRM_OSA_EventWait(mAppEvent, osaEventFlagsAll_c, FALSE, mAppTaskWaitTime_c , &event);
#else
    {
#endif /* gHybridApp_d */
        /* Check for existing messages in queue */
        if (MSG_Pending(&mHSDKAppInputQueue))
        {
            /* Pointer for storing the messages from host. */
            bleEvtContainer_t *pMsgIn = MSG_DeQueue(&mHSDKAppInputQueue);

            if (pMsgIn != NULL)
            {
                /* Process it */
                App_HandleObservedHSDKMessageInput(pMsgIn);

                /* Messages must always be freed. */
                (void)MSG_Free(pMsgIn);
            }
        }

        /* Check for existing messages in queue */
        if (MSG_Pending(&mAppCbInputQueue))
        {
            /* Pointer for storing the callback messages. */
            appMsgCallback_t *pMsgIn = MSG_DeQueue(&mAppCbInputQueue);

            if (pMsgIn != NULL)
            {
                /* Execute callback handler */
                if (pMsgIn->handler != NULL)
                {
                    pMsgIn->handler(pMsgIn->param);
                }

                /* Messages must always be freed. */
                (void)MSG_Free(pMsgIn);
            }
        }
#if !defined(gHybridApp_d) || (!gHybridApp_d)
        /* Signal the App_Thread again if there are more messages pending */
        event = MSG_Pending(&mHSDKAppInputQueue) ? gAppEvtMsgFromHSDK_c : 0U;
        event |= MSG_Pending(&mAppCbInputQueue) ? gAppEvtAppCallback_c : 0U;

        if (event != 0U)
        {
            (void)FRM_OSA_EventSet(mAppEvent, gAppEvtAppCallback_c);
        }

        /* For BareMetal break the while(1) after 1 run */
        if( gUseRtos_c == 0U )
        {
            break;
        }
#endif /* gHybridApp_d */
    }
}

/*! *********************************************************************************
* \brief  Posts an application event containing a callback handler and parameter.
*
* \param[in] handler Handler function, to be executed when the event is processed.
* \param[in] param   Parameter for the handler function.
*
* \return  gBleSuccess_c or error.
*
********************************************************************************** */
uint8_t App_PostCallbackMessage
(
    appCallbackHandler_t   handler,
    appCallbackParam_t     param
)
{
    appMsgCallback_t *pMsgIn = NULL;

    /* Allocate a buffer with enough space to store the packet */
    pMsgIn = MSG_Alloc(sizeof (appMsgCallback_t));

    if (pMsgIn == NULL)
    {
        return gBleOutOfMemory_c;
    }

    pMsgIn->handler = handler;
    pMsgIn->param = param;

    /* Put message in the Cb App queue */
    (void)MSG_Queue(&mAppCbInputQueue, pMsgIn);

    /* Signal application */
    (void)FRM_OSA_EventSet(mAppEvent, gAppEvtAppCallback_c);

    return gBleSuccess_c;
}

/************************************************************************************
*************************************************************************************
* Private functions
*************************************************************************************
************************************************************************************/

static void HSDKCallback(bleEvtContainer_t *container)
{
    /* Put message in the HSDK Stack to App queue */
    MSG_Queue(&mHSDKAppInputQueue, container);
    
    /* Signal application */
    (void)FRM_OSA_EventSet(mAppEvent, gAppEvtMsgFromHSDK_c);
}




/*! *********************************************************************************
* @}
********************************************************************************** */
