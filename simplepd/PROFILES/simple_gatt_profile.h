/******************************************************************************

 @file  simple_gatt_profile.h

 @brief This file contains the Simple GATT profile definitions and prototypes
        prototypes.

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

#ifndef SIMPLEGATTPROFILE_H
#define SIMPLEGATTPROFILE_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */

/*********************************************************************
 * CONSTANTS
 */

// BLEPaper-compatible profile parameters
#define SIMPLEPROFILE_CONTROL_POINT           0  // Write uint8 command
#define SIMPLEPROFILE_ADC_VALUE               1  // Read uint16 value
#define SIMPLEPROFILE_LONG_VALUE              2  // Read/write long value

// BLEPaper service and characteristic UUIDs
#define SIMPLEPROFILE_SERV_UUID               0xFF00
#define SIMPLEPROFILE_LONG_VALUE_UUID         0xFF01
#define SIMPLEPROFILE_ADC_VALUE_UUID          0xFF02
#define SIMPLEPROFILE_CONTROL_POINT_UUID      0xFF03

#define SIMPLEPROFILE_SERVICE                 0x00000001

#define SIMPLEPROFILE_CONTROL_POINT_LEN       1
#define SIMPLEPROFILE_ADC_VALUE_LEN           2
#define SIMPLEPROFILE_LONG_VALUE_LEN          16

/*********************************************************************
 * TYPEDEFS
 */

// Callback when a characteristic value has changed
typedef void (*simpleProfileChange_t)( uint8 paramID );

typedef struct
{
  simpleProfileChange_t        pfnSimpleProfileChange;  // Called when characteristic value changes
} simpleProfileCBs_t;

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * Profile Callbacks
 */

/*********************************************************************
 * API FUNCTIONS
 */

extern uint8_t simpleProfileControlPoint;
extern uint8_t simpleProfileAdcValue[];
extern uint8_t simpleProfileLongValue[];

extern bStatus_t SimpleProfile_AddService( uint32 services );
extern bStatus_t SimpleProfile_RegisterAppCBs( simpleProfileCBs_t *appCallbacks );
extern bStatus_t SimpleProfile_SetParameter( uint8 param, uint8 len, void *value );
extern bStatus_t SimpleProfile_GetParameter( uint8 param, void *value );


/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* SIMPLEGATTPROFILE_H */
