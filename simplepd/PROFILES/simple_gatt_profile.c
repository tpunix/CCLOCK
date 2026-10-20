/******************************************************************************

 @file  simple_gatt_profile.c

 @brief This file contains the Simple GATT profile sample GATT service profile
        for use with the BLE sample application.

 Group: WCS, BTS
 Target Device: cc2640r2

 ******************************************************************************
 
 Copyright (c) 2010-2021, Texas Instruments Incorporated
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 *  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 *  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 *  Neither the name of Texas Instruments Incorporated nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ******************************************************************************
 
 
 *****************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include <string.h>
#include <icall.h>
#include "util.h"
/* This Header file contains all BLE API and icall structure definition */
#include "icall_ble_api.h"

#include "simple_gatt_profile.h"

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

#define SERVAPP_NUM_ATTR_SUPPORTED        7

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

// BLEPaper-compatible service UUID: 0xFF00
uint8_t simpleProfileServUUID[ATT_BT_UUID_SIZE] =
{
  LO_UINT16(SIMPLEPROFILE_SERV_UUID), HI_UINT16(SIMPLEPROFILE_SERV_UUID)
};


// Control Point UUID: 0xFF03
CONST uint8_t simpleProfilecontrolPointUUID[ATT_BT_UUID_SIZE] =
{
  LO_UINT16(SIMPLEPROFILE_CONTROL_POINT_UUID), HI_UINT16(SIMPLEPROFILE_CONTROL_POINT_UUID)
};

// ADC Value UUID: 0xFF02
CONST uint8_t simpleProfileAdcValueUUID[ATT_BT_UUID_SIZE] =
{
  LO_UINT16(SIMPLEPROFILE_ADC_VALUE_UUID), HI_UINT16(SIMPLEPROFILE_ADC_VALUE_UUID)
};

// Long Value UUID: 0xFF01
CONST uint8_t simpleProfileLongValueUUID[ATT_BT_UUID_SIZE] =
{
  LO_UINT16(SIMPLEPROFILE_LONG_VALUE_UUID), HI_UINT16(SIMPLEPROFILE_LONG_VALUE_UUID)
};

static simpleProfileCBs_t *simpleProfile_AppCBs = NULL;



/*********************************************************************
 * Profile Attributes - variables
 */

// Simple Profile Service attribute
static CONST gattAttrType_t simpleProfileService = { ATT_BT_UUID_SIZE, simpleProfileServUUID };

static uint8_t simpleProfileControlPointProps = GATT_PROP_WRITE;
uint8_t simpleProfileControlPoint = 0;

static uint8_t simpleProfileAdcValueProps = GATT_PROP_READ;
uint8_t simpleProfileAdcValue[SIMPLEPROFILE_ADC_VALUE_LEN] = {0};

static uint8_t simpleProfileLongValueProps = GATT_PROP_READ | GATT_PROP_WRITE;
uint8_t simpleProfileLongValue[SIMPLEPROFILE_LONG_VALUE_LEN] = {0};

/*********************************************************************
 * Profile Attributes - Table
 */

static gattAttribute_t simpleProfileAttrTbl[SERVAPP_NUM_ATTR_SUPPORTED] =
{
  // Simple Profile Service
  {
    { ATT_BT_UUID_SIZE, primaryServiceUUID }, /* type */
    GATT_PERMIT_READ,                         /* permissions */
    0,                                        /* handle */
    (uint8 *)&simpleProfileService            /* pValue */
  },

  // Control Point characteristic declaration
  {
    { ATT_BT_UUID_SIZE, characterUUID },
    GATT_PERMIT_READ,
    0,
    &simpleProfileControlPointProps
  },

  // Control Point characteristic value
  {
    { ATT_BT_UUID_SIZE, simpleProfilecontrolPointUUID },
    GATT_PERMIT_WRITE,
    0,
    &simpleProfileControlPoint
  },

  // ADC Value characteristic declaration
  {
    { ATT_BT_UUID_SIZE, characterUUID },
    GATT_PERMIT_READ,
    0,
    &simpleProfileAdcValueProps
  },

  // ADC Value characteristic value
  {
    { ATT_BT_UUID_SIZE, simpleProfileAdcValueUUID },
    GATT_PERMIT_READ,
    0,
    simpleProfileAdcValue
  },

  // Long Value characteristic declaration
  {
    { ATT_BT_UUID_SIZE, characterUUID },
    GATT_PERMIT_READ,
    0,
    &simpleProfileLongValueProps
  },

  // Long Value characteristic value
  {
    { ATT_BT_UUID_SIZE, simpleProfileLongValueUUID },
    GATT_PERMIT_READ | GATT_PERMIT_WRITE,
    0,
    simpleProfileLongValue
  },
};

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static bStatus_t simpleProfile_ReadAttrCB(uint16_t connHandle,
                                          gattAttribute_t *pAttr,
                                          uint8_t *pValue, uint16_t *pLen,
                                          uint16_t offset, uint16_t maxLen,
                                          uint8_t method);
static bStatus_t simpleProfile_WriteAttrCB(uint16_t connHandle,
                                           gattAttribute_t *pAttr,
                                           uint8_t *pValue, uint16_t len,
                                           uint16_t offset, uint8_t method);

/*********************************************************************
 * PROFILE CALLBACKS
 */

// Simple Profile Service Callbacks
// Note: When an operation on a characteristic requires authorization and
// pfnAuthorizeAttrCB is not defined for that characteristic's service, the
// Stack will report a status of ATT_ERR_UNLIKELY to the client.  When an
// operation on a characteristic requires authorization the Stack will call
// pfnAuthorizeAttrCB to check a client's authorization prior to calling
// pfnReadAttrCB or pfnWriteAttrCB, so no checks for authorization need to be
// made within these functions.
CONST gattServiceCBs_t simpleProfileCBs =
{
  simpleProfile_ReadAttrCB,  // Read callback function pointer
  simpleProfile_WriteAttrCB, // Write callback function pointer
  NULL                       // Authorization callback function pointer
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      SimpleProfile_AddService
 *
 * @brief   Initializes the Simple Profile service by registering
 *          GATT attributes with the GATT server.
 *
 * @param   services - services to add. This is a bit map and can
 *                     contain more than one service.
 *
 * @return  Success or Failure
 */
bStatus_t SimpleProfile_AddService( uint32 services )
{
  return GATTServApp_RegisterService( simpleProfileAttrTbl,
                                      GATT_NUM_ATTRS( simpleProfileAttrTbl ),
                                      GATT_MAX_ENCRYPT_KEY_SIZE,
                                      &simpleProfileCBs );
}

/*********************************************************************
 * @fn      SimpleProfile_RegisterAppCBs
 *
 * @brief   Registers the application callback function. Only call
 *          this function once.
 *
 * @param   callbacks - pointer to application callbacks.
 *
 * @return  SUCCESS or bleAlreadyInRequestedMode
 */
bStatus_t SimpleProfile_RegisterAppCBs( simpleProfileCBs_t *appCallbacks )
{
  if ( appCallbacks )
  {
    simpleProfile_AppCBs = appCallbacks;
    return ( SUCCESS );
  }

  return ( bleAlreadyInRequestedMode );
}

/*********************************************************************
 * @fn      SimpleProfile_SetParameter
 *
 * @brief   Set a Simple Profile parameter.
 *
 * @param   param - Profile parameter ID
 * @param   len - length of data to write
 * @param   value - pointer to data to write.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16 will be cast to
 *          uint16 pointer).
 *
 * @return  bStatus_t
 */
bStatus_t SimpleProfile_SetParameter( uint8 param, uint8 len, void *value )
{
  bStatus_t ret = SUCCESS;

  switch ( param )
  {
    case SIMPLEPROFILE_CONTROL_POINT:
      if ( len == SIMPLEPROFILE_CONTROL_POINT_LEN )
      {
        simpleProfileControlPoint = *((uint8_t*)value);
      }
      else
      {
        ret = bleInvalidRange;
      }
      break;

    case SIMPLEPROFILE_ADC_VALUE:
      if ( len == SIMPLEPROFILE_ADC_VALUE_LEN )
      {
        memcpy(simpleProfileAdcValue, value, SIMPLEPROFILE_ADC_VALUE_LEN);
      }
      else
      {
        ret = bleInvalidRange;
      }
      break;

    case SIMPLEPROFILE_LONG_VALUE:
      if ( len <= SIMPLEPROFILE_LONG_VALUE_LEN )
      {
        memset(simpleProfileLongValue, 0, SIMPLEPROFILE_LONG_VALUE_LEN);
        memcpy(simpleProfileLongValue, value, len);
      }
      else
      {
        ret = bleInvalidRange;
      }
      break;

    default:
      ret = INVALIDPARAMETER;
      break;
  }

  return ret;
}

/*********************************************************************
 * @fn      SimpleProfile_GetParameter
 *
 * @brief   Get a Simple Profile parameter.
 *
 * @param   param - Profile parameter ID
 * @param   value - pointer to data to put.  This is dependent on
 *          the parameter ID and WILL be cast to the appropriate
 *          data type (example: data type of uint16 will be cast to
 *          uint16 pointer).
 *
 * @return  bStatus_t
 */
bStatus_t SimpleProfile_GetParameter( uint8 param, void *value )
{
  bStatus_t ret = SUCCESS;

  switch ( param )
  {
    case SIMPLEPROFILE_CONTROL_POINT:
      *((uint8_t*)value) = simpleProfileControlPoint;
      break;

    case SIMPLEPROFILE_ADC_VALUE:
      memcpy(value, simpleProfileAdcValue, SIMPLEPROFILE_ADC_VALUE_LEN);
      break;

    case SIMPLEPROFILE_LONG_VALUE:
      memcpy(value, simpleProfileLongValue, SIMPLEPROFILE_LONG_VALUE_LEN);
      break;

    default:
      ret = INVALIDPARAMETER;
      break;
  }

  return ret;
}

/*********************************************************************
 * @fn          simpleProfile_ReadAttrCB
 *
 * @brief       Read an attribute.
 *
 * @param       connHandle - connection message was received on
 * @param       pAttr - pointer to attribute
 * @param       pValue - pointer to data to be read
 * @param       pLen - length of data to be read
 * @param       offset - offset of the first octet to be read
 * @param       maxLen - maximum length of data to be read
 * @param       method - type of read message
 *
 * @return      SUCCESS, blePending or Failure
 */
static bStatus_t simpleProfile_ReadAttrCB(uint16_t connHandle,
                                          gattAttribute_t *pAttr,
                                          uint8_t *pValue, uint16_t *pLen,
                                          uint16_t offset, uint16_t maxLen,
                                          uint8_t method)
{
  bStatus_t status = SUCCESS;
  uint16_t uuid = 0;
  uint16_t valueLen = 0;
  uint8_t *pSrc = NULL;

  if ( pAttr->type.len == ATT_BT_UUID_SIZE )
  {
    uuid = BUILD_UINT16( pAttr->type.uuid[0], pAttr->type.uuid[1] );
  }
  else
  {
    return ATT_ERR_INVALID_HANDLE;
  }

  switch ( uuid )
  {
    case SIMPLEPROFILE_ADC_VALUE_UUID:
      pSrc = simpleProfileAdcValue;
      valueLen = SIMPLEPROFILE_ADC_VALUE_LEN;
      break;

    case SIMPLEPROFILE_LONG_VALUE_UUID:
      pSrc = simpleProfileLongValue;
      valueLen = SIMPLEPROFILE_LONG_VALUE_LEN;
      break;

    default:
      return ATT_ERR_ATTR_NOT_FOUND;
  }

  if ( offset > valueLen )
  {
    status = ATT_ERR_INVALID_OFFSET;
  }
  else
  {
    *pLen = MIN(maxLen, valueLen - offset);
    memcpy(pValue, pSrc + offset, *pLen);
  }

  return status;
}

/*********************************************************************
 * @fn      simpleProfile_WriteAttrCB
 *
 * @brief   Validate attribute data prior to a write operation
 *
 * @param   connHandle - connection message was received on
 * @param   pAttr - pointer to attribute
 * @param   pValue - pointer to data to be written
 * @param   len - length of data
 * @param   offset - offset of the first octet to be written
 * @param   method - type of write message
 *
 * @return  SUCCESS, blePending or Failure
 */
static bStatus_t simpleProfile_WriteAttrCB(uint16_t connHandle,
                                           gattAttribute_t *pAttr,
                                           uint8_t *pValue, uint16_t len,
                                           uint16_t offset, uint8_t method)
{
  bStatus_t status = SUCCESS;
  uint8 notifyApp = 0xFF;
  uint16_t uuid = 0;

  if ( pAttr->type.len == ATT_BT_UUID_SIZE )
  {
    uuid = BUILD_UINT16( pAttr->type.uuid[0], pAttr->type.uuid[1] );
  }
  else
  {
    return ATT_ERR_INVALID_HANDLE;
  }

  switch ( uuid )
  {
    case SIMPLEPROFILE_CONTROL_POINT_UUID:
      if ( offset == 0 && len == SIMPLEPROFILE_CONTROL_POINT_LEN )
      {
        simpleProfileControlPoint = pValue[0];
        notifyApp = SIMPLEPROFILE_CONTROL_POINT;
      }
      else
      {
        status = ATT_ERR_INVALID_VALUE_SIZE;
      }
      break;

    case SIMPLEPROFILE_LONG_VALUE_UUID:
      if ( offset <= SIMPLEPROFILE_LONG_VALUE_LEN && len <= (SIMPLEPROFILE_LONG_VALUE_LEN - offset) )
      {
        memcpy(simpleProfileLongValue + offset, pValue, len);
        notifyApp = SIMPLEPROFILE_LONG_VALUE;
      }
      else
      {
        status = ATT_ERR_INVALID_OFFSET;
      }
      break;

    default:
      status = ATT_ERR_ATTR_NOT_FOUND;
      break;
  }

  if ( (status == SUCCESS) && (notifyApp != 0xFF) && simpleProfile_AppCBs &&
       simpleProfile_AppCBs->pfnSimpleProfileChange )
  {
    simpleProfile_AppCBs->pfnSimpleProfileChange( notifyApp );
  }

  return status;
}

/*********************************************************************
*********************************************************************/
