/*! *********************************************************************************
 * \defgroup CCC Digital Key Applications
 * @{
 ********************************************************************************** */
/*! *********************************************************************************
* Copyright 2021 NXP
* All rights reserved.
* 
* \file ccc_host.h
*
* SPDX-License-Identifier: BSD-3-Clause
********************************************************************************** */


#ifndef CCC_HOST_H
#define CCC_HOST_H
/*! *********************************************************************************
*************************************************************************************
* Public type definitions
*************************************************************************************
********************************************************************************** */
typedef void* appCallbackParam_t;
typedef void (*appCallbackHandler_t)(appCallbackParam_t param);
/*************************************************************************************
**************************************************************************************
* Public macros
**************************************************************************************
*************************************************************************************/

/************************************************************************************
*************************************************************************************
* Public prototypes
*************************************************************************************
************************************************************************************/
uint8_t App_PostCallbackMessage
(
    appCallbackHandler_t   handler,
    appCallbackParam_t     param
);

#ifdef __cplusplus
extern "C" {
#endif

void ble_task(void* argument);
void adv_task(void* argument);

#ifdef __cplusplus
}
#endif 


#endif /* CCC_HOST_H */

/*! *********************************************************************************
 * @}
 ********************************************************************************** */