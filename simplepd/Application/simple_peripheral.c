/******************************************************************************

 @file  simple_peripheral.c

 @brief This file contains the Simple Peripheral sample application for use
        with the CC2650 Bluetooth Low Energy Protocol Stack.

 Group: WCS, BTS
 Target Device: cc2640r2

 ******************************************************************************
 
 Copyright (c) 2013-2021, Texas Instruments Incorporated
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

#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Queue.h>

#include <icall.h>
#include "util.h"
#include "att_rsp.h"

/* This Header file contains all BLE API and icall structure definition */
#include "icall_ble_api.h"

#include "simple_gatt_profile.h"
#include "ll_common.h"

#include "peripheral.h"

#include "board.h"

#include "simple_peripheral.h"
#include "epd/epd.h"

/*********************************************************************
 * CONSTANTS
 */

// Advertising interval when device is discoverable (units of 625us, 160=100ms)
#define DEFAULT_ADVERTISING_INTERVAL          800

// General discoverable mode: advertise indefinitely
#define DEFAULT_DISCOVERABLE_MODE             GAP_ADTYPE_FLAGS_LIMITED

// Minimum connection interval (units of 1.25ms, 80=100ms) for automatic
// parameter update request
#define DEFAULT_DESIRED_MIN_CONN_INTERVAL     160

// Maximum connection interval (units of 1.25ms, 800=1000ms) for automatic
// parameter update request
#define DEFAULT_DESIRED_MAX_CONN_INTERVAL     800

// Slave latency to use for automatic parameter update request
#define DEFAULT_DESIRED_SLAVE_LATENCY         0

// Supervision timeout value (units of 10ms, 1000=10s) for automatic parameter
// update request
#define DEFAULT_DESIRED_CONN_TIMEOUT          1000

// After the connection is formed, the peripheral waits until the central
// device asks for its preferred connection parameters
#define DEFAULT_ENABLE_UPDATE_REQUEST         GAPROLE_LINK_PARAM_UPDATE_WAIT_REMOTE_PARAMS

// Connection Pause Peripheral time value (in seconds)
#define DEFAULT_CONN_PAUSE_PERIPHERAL         6

// How often to perform periodic event (in msec)
#define SBP_PERIODIC_EVT_PERIOD               60000

// Application specific event ID for HCI Connection Event End Events
#define SBP_HCI_CONN_EVT_END_EVT              0x0001

// Task configuration
#define SBP_TASK_PRIORITY                     1

#ifndef SBP_TASK_STACK_SIZE
#define SBP_TASK_STACK_SIZE                   644
#endif

// Application events
#define SBP_STATE_CHANGE_EVT                  0x0001
#define SBP_CHAR_CHANGE_EVT                   0x0002
#define SBP_PAIRING_STATE_EVT                 0x0004
#define SBP_PASSCODE_NEEDED_EVT               0x0008
#define SBP_CONN_EVT                          0x0010

// Internal Events for RTOS application
#define SBP_ICALL_EVT                         ICALL_MSG_EVENT_ID // Event_Id_31
#define SBP_QUEUE_EVT                         UTIL_QUEUE_EVENT_ID // Event_Id_30
#define SBP_PERIODIC_EVT                      Event_Id_00

// Bitwise OR of all events to pend on
#define SBP_ALL_EVENTS                        (SBP_ICALL_EVT        | \
                                               SBP_QUEUE_EVT        | \
                                               SBP_PERIODIC_EVT)


// Set the register cause to the registration bit-mask
#define CONNECTION_EVENT_REGISTER_BIT_SET(RegisterCause) (connectionEventRegisterCauseBitMap |= RegisterCause )
// Remove the register cause from the registration bit-mask
#define CONNECTION_EVENT_REGISTER_BIT_REMOVE(RegisterCause) (connectionEventRegisterCauseBitMap &= (~RegisterCause) )
// Gets whether the current App is registered to the receive connection events
#define CONNECTION_EVENT_IS_REGISTERED (connectionEventRegisterCauseBitMap > 0)
// Gets whether the RegisterCause was registered to recieve connection event
#define CONNECTION_EVENT_REGISTRATION_CAUSE(RegisterCause) (connectionEventRegisterCauseBitMap & RegisterCause )


// Dynamic BLE name generated from the device address.
#define DEVICE_NAME_PREFIX                    "DLG-CLOCK-"
#define DEVICE_NAME_LEN                       16
#define SCAN_RSP_NAME_OFFSET                  2


/*********************************************************************
 * TYPEDEFS
 */

// App event passed from profiles.
typedef struct
{
  appEvtHdr_t hdr;  // event header.
  uint8_t *pData;  // event data
} sbpEvt_t;

/*********************************************************************
 * GLOBAL VARIABLES
 */

int clock_interval_ones;
int clock_interval;
int clock_fixup_value;
int clock_fixup_count;
int ble_state;
int adcval;

int g_ticks, g_adj;

/*********************************************************************
 * LOCAL VARIABLES
 */

// Entity ID globally used to check for source and/or destination of messages
static ICall_EntityID selfEntity;

// Event globally used to post local events and pend on system and
// local events.
static ICall_SyncHandle syncEvent;

// Clock instances for internal periodic events.
static Clock_Struct periodicClock;

// Queue object used for app messages
static Queue_Struct appMsg;
static Queue_Handle appMsgQueue;

// Task configuration
Task_Struct sbpTask;
Char sbpTaskStack[SBP_TASK_STACK_SIZE];

// Scan response data (max size = 31 bytes)
static uint8_t scanRspData[] =
{
  // complete name
  DEVICE_NAME_LEN + 1,   // length of this data
  GAP_ADTYPE_LOCAL_NAME_COMPLETE,
  'D', 'L', 'G', '-', 'C', 'L', 'O', 'C', 'K', '-',
  '0', '0', '0', '0', '0', '0',

  // connection interval range
  0x05,   // length of this data
  GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE,
  LO_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL),   // 100ms
  HI_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL),
  LO_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL),   // 1s
  HI_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL),

  // Tx power level
  0x02,   // length of this data
  GAP_ADTYPE_POWER_LEVEL,
  0       // 0dBm
};

// Advertisement data (max size = 31 bytes, though this is
// best kept short to conserve power while advertising)
static uint8_t advertData[] =
{
  // Flags: this field sets the device to use general discoverable
  // mode (advertises indefinitely) instead of general
  // discoverable mode (advertise for 30 seconds at a time)
  0x02,   // length of this data
  GAP_ADTYPE_FLAGS,
  DEFAULT_DISCOVERABLE_MODE | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

  // service UUID, to notify central devices what services are included
  // in this peripheral
  0x03,   // length of this data
  GAP_ADTYPE_16BIT_MORE,      // some of the UUID's, but not all
  LO_UINT16(SIMPLEPROFILE_SERV_UUID),
  HI_UINT16(SIMPLEPROFILE_SERV_UUID)
};

// GAP GATT Attributes
static uint8_t attDeviceName[GAP_DEVICE_NAME_LEN] = DEVICE_NAME_PREFIX "000000";
char *bt_id = (char*)attDeviceName+10;

/*********************************************************************
 * LOCAL FUNCTIONS
 */

static void SimplePeripheral_init( void );
static void SimplePeripheral_taskFxn(UArg a0, UArg a1);

static uint8_t SimplePeripheral_processStackMsg(ICall_Hdr *pMsg);
static uint8_t SimplePeripheral_processGATTMsg(gattMsgEvent_t *pMsg);
static void SimplePeripheral_processAppMsg(sbpEvt_t *pMsg);
static void SimplePeripheral_processStateChangeEvt(gaprole_States_t newState);
static void SimplePeripheral_processCharValueChangeEvt(uint8_t paramID);
static void SimplePeripheral_performPeriodicTask(void);
static void SimplePeripheral_clockHandler(UArg arg);

static void SimplePeripheral_passcodeCB(uint8_t *deviceAddr,
                                        uint16_t connHandle,
                                        uint8_t uiInputs, uint8_t uiOutputs,
                                        uint32_t numComparison);
static void SimplePeripheral_pairStateCB(uint16_t connHandle, uint8_t state,
                                         uint8_t status);
static void SimplePeripheral_processPairState(uint8_t state, uint8_t status);
static void SimplePeripheral_processPasscode(uint8_t uiOutputs);

static void SimplePeripheral_stateChangeCB(gaprole_States_t newState);
static void SimplePeripheral_charValueChangeCB(uint8_t paramID);
static uint8_t SimplePeripheral_enqueueMsg(uint8_t event, uint8_t state,
                                              uint8_t *pData);

static void SimplePeripheral_connEvtCB(Gap_ConnEventRpt_t *pReport);
static void SimplePeripheral_processConnEvt(Gap_ConnEventRpt_t *pReport);

int read_adc(int id);

/*********************************************************************
 * EXTERN FUNCTIONS
 */
extern void AssertHandler(uint8 assertCause, uint8 assertSubcause);

/*********************************************************************
 * PROFILE CALLBACKS
 */

// Peripheral GAPRole Callbacks
static gapRolesCBs_t SimplePeripheral_gapRoleCBs =
{
  SimplePeripheral_stateChangeCB     // GAPRole State Change Callbacks
};

// GAP Bond Manager Callbacks
// These are set to NULL since they are not needed. The application
// is set up to only perform justworks pairing.
static gapBondCBs_t simplePeripheral_BondMgrCBs =
{
  SimplePeripheral_passcodeCB,  // Passcode callback
  SimplePeripheral_pairStateCB  // Pairing / Bonding state Callback
};

// Simple GATT Profile Callbacks
static simpleProfileCBs_t SimplePeripheral_simpleProfileCBs =
{
  SimplePeripheral_charValueChangeCB // Simple GATT Characteristic value change callback
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */


static void SimplePeripheral_setDeviceName(uint8_t *addr)
{
  sprintk((char *)attDeviceName, DEVICE_NAME_PREFIX "%02x%02x%02x",
          addr[2], addr[1], addr[0]);

  memcpy(&scanRspData[SCAN_RSP_NAME_OFFSET], attDeviceName, DEVICE_NAME_LEN);
  GGS_SetParameter(GGS_DEVICE_NAME_ATT, DEVICE_NAME_LEN, attDeviceName);
  GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(scanRspData), scanRspData);
}


/*********************************************************************
 * The following typedef and global handle the registration to connection event
 */
typedef enum
{
   NOT_REGISTER       = 0,
   FOR_AOA_SCAN       = 1,
   FOR_ATT_RSP        = 2,
   FOR_AOA_SEND       = 4,
   FOR_TOF_SEND       = 8
}connectionEventRegisterCause_u;

// Handle the registration and un-registration for the connection event, since only one can be registered.
uint32_t       connectionEventRegisterCauseBitMap = NOT_REGISTER; //see connectionEventRegisterCause_u

/*********************************************************************
 * @fn      SimplePeripheral_RegistertToAllConnectionEvent()
 *
 * @brief   register to receive connection events for all the connection
 *
 * @param connectionEventRegisterCause represents the reason for registration
 *
 * @return @ref SUCCESS
 *
 */
bStatus_t SimplePeripheral_RegistertToAllConnectionEvent (connectionEventRegisterCause_u connectionEventRegisterCause)
{
  bStatus_t status = SUCCESS;

  // in case  there is no registration for the connection event, make the registration
  if (!CONNECTION_EVENT_IS_REGISTERED)
  {
    status = GAP_RegisterConnEventCb(SimplePeripheral_connEvtCB, GAP_CB_REGISTER, LINKDB_CONNHANDLE_ALL);
  }
  if(status == SUCCESS)
  {
    //add the reason bit to the bitamap.
    CONNECTION_EVENT_REGISTER_BIT_SET(connectionEventRegisterCause);
  }

  return(status);
}

/*********************************************************************
 * @fn      SimplePeripheral_UnRegistertToAllConnectionEvent()
 *
 * @brief   Unregister connection events
 *
 * @param connectionEventRegisterCause represents the reason for registration
 *
 * @return @ref SUCCESS
 *
 */
bStatus_t SimplePeripheral_UnRegistertToAllConnectionEvent (connectionEventRegisterCause_u connectionEventRegisterCause)
{
  bStatus_t status = SUCCESS;

  CONNECTION_EVENT_REGISTER_BIT_REMOVE(connectionEventRegisterCause);
  // in case  there is no more registration for the connection event than unregister
  if (!CONNECTION_EVENT_IS_REGISTERED)
  {
    GAP_RegisterConnEventCb(SimplePeripheral_connEvtCB, GAP_CB_UNREGISTER, LINKDB_CONNHANDLE_ALL);
  }

  return(status);
}

 /*********************************************************************
 * @fn      SimplePeripheral_createTask
 *
 * @brief   Task creation function for the Simple Peripheral.
 *
 * @param   None.
 *
 * @return  None.
 */
void SimplePeripheral_createTask(void)
{
  Task_Params taskParams;

  // Configure task
  Task_Params_init(&taskParams);
  taskParams.stack = sbpTaskStack;
  taskParams.stackSize = SBP_TASK_STACK_SIZE;
  taskParams.priority = SBP_TASK_PRIORITY;

  Task_construct(&sbpTask, SimplePeripheral_taskFxn, &taskParams, NULL);
}

/*********************************************************************
 * @fn      SimplePeripheral_init
 *
 * @brief   Called during initialization and contains application
 *          specific initialization (ie. hardware initialization/setup,
 *          table initialization, power up notification, etc), and
 *          profile initialization/setup.
 *
 * @param   None.
 *
 * @return  None.
 */
static void SimplePeripheral_init(void)
{
  // ******************************************************************
  // N0 STACK API CALLS CAN OCCUR BEFORE THIS CALL TO ICall_registerApp
  // ******************************************************************
  // Register the current thread as an ICall dispatcher application
  // so that the application can send and receive messages.
  ICall_registerApp(&selfEntity, &syncEvent);

  // Create an RTOS queue for message from profile to be sent to app.
  appMsgQueue = Util_constructQueue(&appMsg);


  app_hw_init();
  epd_hw_init(122, 250, EPD_BWR|ROTATE_3);

  clock_interval = 60;
  clock_interval_ones = 0;
  clock_fixup_value = 24276;
  clock_fixup_count = 0;
  ble_state = 0;


  // Create one-shot clocks for internal periodic events.
  Util_constructClock(&periodicClock, SimplePeripheral_clockHandler,
                      clock_interval*1000, 0, false, SBP_PERIODIC_EVT);



  // Set GAP Parameters: After a connection was established, delay in seconds
  // before sending when GAPRole_SetParameter(GAPROLE_PARAM_UPDATE_ENABLE,...)
  // uses GAPROLE_LINK_PARAM_UPDATE_INITIATE_BOTH_PARAMS or
  // GAPROLE_LINK_PARAM_UPDATE_INITIATE_APP_PARAMS
  // For current defaults, this has no effect.
  GAP_SetParamValue(TGAP_CONN_PAUSE_PERIPHERAL, DEFAULT_CONN_PAUSE_PERIPHERAL);

  // Setup the Peripheral GAPRole Profile. For more information see the User's Guide:
  // http://software-dl.ti.com/lprf/sdg-latest/html/
  {
    // By setting this to zero, the device will go into the waiting state after
    // being discoverable for 30.72 second, and will not being advertising again
    // until re-enabled by the application
    uint16_t advertOffTime = 0;

    uint8_t enableUpdateRequest = DEFAULT_ENABLE_UPDATE_REQUEST;
    uint16_t desiredMinInterval = DEFAULT_DESIRED_MIN_CONN_INTERVAL;
    uint16_t desiredMaxInterval = DEFAULT_DESIRED_MAX_CONN_INTERVAL;
    uint16_t desiredSlaveLatency = DEFAULT_DESIRED_SLAVE_LATENCY;
    uint16_t desiredConnTimeout = DEFAULT_DESIRED_CONN_TIMEOUT;

    GAPRole_SetParameter(GAPROLE_ADVERT_OFF_TIME, sizeof(uint16_t), &advertOffTime);

    GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(scanRspData), scanRspData);
    GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);

    GAPRole_SetParameter(GAPROLE_PARAM_UPDATE_ENABLE, sizeof(uint8_t), &enableUpdateRequest);
    GAPRole_SetParameter(GAPROLE_MIN_CONN_INTERVAL, sizeof(uint16_t), &desiredMinInterval);
    GAPRole_SetParameter(GAPROLE_MAX_CONN_INTERVAL, sizeof(uint16_t), &desiredMaxInterval);
    GAPRole_SetParameter(GAPROLE_SLAVE_LATENCY, sizeof(uint16_t), &desiredSlaveLatency);
    GAPRole_SetParameter(GAPROLE_TIMEOUT_MULTIPLIER, sizeof(uint16_t), &desiredConnTimeout);
  }

  // Set the Device Name characteristic in the GAP GATT Service
  // For more information, see the section in the User's Guide:
  // http://software-dl.ti.com/lprf/sdg-latest/html
  GGS_SetParameter(GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, attDeviceName);

  // Set GAP Parameters to set the advertising interval
  // For more information, see the GAP section of the User's Guide:
  // http://software-dl.ti.com/lprf/sdg-latest/html
  {
    // Use the same interval for general and limited advertising.
    // Note that only general advertising will occur based on the above configuration
    uint16_t advInt = DEFAULT_ADVERTISING_INTERVAL;

    GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MIN, advInt);
    GAP_SetParamValue(TGAP_LIM_DISC_ADV_INT_MAX, advInt);
    GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MIN, advInt);
    GAP_SetParamValue(TGAP_GEN_DISC_ADV_INT_MAX, advInt);

	GAP_SetParamValue(TGAP_LIM_ADV_TIMEOUT, 30);
  }

  // Setup the GAP Bond Manager. For more information see the section in the
  // User's Guide:
  // http://software-dl.ti.com/lprf/sdg-latest/html/
  {
    // Don't send a pairing request after connecting; the peer device must
    // initiate pairing
    uint8_t pairMode = GAPBOND_PAIRING_MODE_WAIT_FOR_REQ;
    // Use authenticated pairing: require passcode.
    uint8_t mitm = TRUE;
    // This device only has display capabilities. Therefore, it will display the
    // passcode during pairing. However, since the default passcode is being
    // used, there is no need to display anything.
    uint8_t ioCap = GAPBOND_IO_CAP_DISPLAY_ONLY;
    // Request bonding (storing long-term keys for re-encryption upon subsequent
    // connections without repairing)
    uint8_t bonding = TRUE;
    // Whether to replace the least recently used entry when bond list is full,
    // and a new device is bonded.
    // Alternative is pairing succeeds but bonding fails, unless application has
    // manually erased at least one bond.
    uint8_t replaceBonds = FALSE;

    GAPBondMgr_SetParameter(GAPBOND_PAIRING_MODE, sizeof(uint8_t), &pairMode);
    GAPBondMgr_SetParameter(GAPBOND_MITM_PROTECTION, sizeof(uint8_t), &mitm);
    GAPBondMgr_SetParameter(GAPBOND_IO_CAPABILITIES, sizeof(uint8_t), &ioCap);
    GAPBondMgr_SetParameter(GAPBOND_BONDING_ENABLED, sizeof(uint8_t), &bonding);
    GAPBondMgr_SetParameter(GAPBOND_LRU_BOND_REPLACEMENT, sizeof(uint8_t), &replaceBonds);
  }

  // Initialize GATT attributes
  GGS_AddService(GATT_ALL_SERVICES);           // GAP GATT Service
  GATTServApp_AddService(GATT_ALL_SERVICES);   // GATT Service
  SimpleProfile_AddService(GATT_ALL_SERVICES); // Simple GATT Profile

  // Start the Device:
  // Please Notice that in case of wanting to use the GAPRole_SetParameter
  // function with GAPROLE_IRK or GAPROLE_SRK parameter - Perform
  // these function calls before the GAPRole_StartDevice use.
  // (because Both cases are updating the gapRole_IRK & gapRole_SRK variables).
  VOID GAPRole_StartDevice(&SimplePeripheral_gapRoleCBs);

  // Register callback with SimpleGATTprofile
  SimpleProfile_RegisterAppCBs(&SimplePeripheral_simpleProfileCBs);

  // Start Bond Manager and register callback
  VOID GAPBondMgr_Register(&simplePeripheral_BondMgrCBs);

  // Register with GAP for HCI/Host messages. This is needed to receive HCI
  // events. For more information, see the section in the User's Guide:
  // http://software-dl.ti.com/lprf/sdg-latest/html
  GAP_RegisterForMsgs(selfEntity);

  // Register for GATT local events and ATT Responses pending for transmission
  GATT_RegisterForMsgs(selfEntity);

  //Set default values for Data Length Extension
  {
    //Set initial values to maximum, RX is set to max. by default(251 octets, 2120us)
    #define APP_SUGGESTED_PDU_SIZE 251 //default is 27 octets(TX)
    #define APP_SUGGESTED_TX_TIME 2120 //default is 328us(TX)

    //This API is documented in hci.h
    //See the LE Data Length Extension section in the BLE-Stack User's Guide for information on using this command:
    //http://software-dl.ti.com/lprf/sdg-latest/html/cc2640/index.html
    //HCI_LE_WriteSuggestedDefaultDataLenCmd(APP_SUGGESTED_PDU_SIZE, APP_SUGGESTED_TX_TIME);
  }

#if !defined (USE_LL_CONN_PARAM_UPDATE)
  // Get the currently set local supported LE features
  // The HCI will generate an HCI event that will get received in the main
  // loop
  HCI_LE_ReadLocalSupportedFeaturesCmd();
#endif // !defined (USE_LL_CONN_PARAM_UPDATE)



  printk("BLE Peripheral init done.\n");
}

/*********************************************************************
 * @fn      SimplePeripheral_taskFxn
 *
 * @brief   Application task entry point for the Simple Peripheral.
 *
 * @param   a0, a1 - not used.
 *
 * @return  None.
 */
static void SimplePeripheral_taskFxn(UArg a0, UArg a1)
{
  // Initialize application
  SimplePeripheral_init();

  // Application main loop
  for (;;)
  {
    uint32_t events;

    // Waits for an event to be posted associated with the calling thread.
    // Note that an event associated with a thread is posted when a
    // message is queued to the message receive queue of the thread
    events = Event_pend(syncEvent, Event_Id_NONE, SBP_ALL_EVENTS,
                        ICALL_TIMEOUT_FOREVER);

    if (events)
    {
      ICall_EntityID dest;
      ICall_ServiceEnum src;
      ICall_HciExtEvt *pMsg = NULL;

      // Fetch any available messages that might have been sent from the stack
      if (ICall_fetchServiceMsg(&src, &dest,
                                (void **)&pMsg) == ICALL_ERRNO_SUCCESS)
      {
        uint8 safeToDealloc = TRUE;

        if ((src == ICALL_SERVICE_CLASS_BLE) && (dest == selfEntity))
        {
          ICall_Stack_Event *pEvt = (ICall_Stack_Event *)pMsg;

          if (pEvt->signature != 0xffff)
          {
            // Process inter-task message
            safeToDealloc = SimplePeripheral_processStackMsg((ICall_Hdr *)pMsg);
          }
        }

        if (pMsg && safeToDealloc)
        {
          ICall_freeMsg(pMsg);
        }
      }

      // If RTOS queue is not empty, process app message.
      if (events & SBP_QUEUE_EVT)
      {
        while (!Queue_empty(appMsgQueue))
        {
          sbpEvt_t *pMsg = (sbpEvt_t *)Util_dequeueMsg(appMsgQueue);
          if (pMsg)
          {
            // Process message.
            SimplePeripheral_processAppMsg(pMsg);

            // Free the space from the message.
            ICall_free(pMsg);
          }
        }
      }

      if (events & SBP_PERIODIC_EVT)
      {
        // Perform periodic application task
        SimplePeripheral_performPeriodicTask();
      }
    }
  }
}

/*********************************************************************
 * @fn      SimplePeripheral_processStackMsg
 *
 * @brief   Process an incoming stack message.
 *
 * @param   pMsg - message to process
 *
 * @return  TRUE if safe to deallocate incoming message, FALSE otherwise.
 */
static uint8_t SimplePeripheral_processStackMsg(ICall_Hdr *pMsg)
{
  uint8_t safeToDealloc = TRUE;

  switch (pMsg->event)
  {
    case GATT_MSG_EVENT:
      // Process GATT message
      safeToDealloc = SimplePeripheral_processGATTMsg((gattMsgEvent_t *)pMsg);
      break;

    case HCI_GAP_EVENT_EVENT:
      {

        // Process HCI message
        switch(pMsg->status)
        {
          case HCI_COMMAND_COMPLETE_EVENT_CODE:
            // Process HCI Command Complete Event
            {

#if !defined (USE_LL_CONN_PARAM_UPDATE)
              // This code will disable the use of the LL_CONNECTION_PARAM_REQ
              // control procedure (for connection parameter updates, the
              // L2CAP Connection Parameter Update procedure will be used
              // instead). To re-enable the LL_CONNECTION_PARAM_REQ control
              // procedures, define the symbol USE_LL_CONN_PARAM_UPDATE
              // The L2CAP Connection Parameter Update procedure is used to
              // support a delta between the minimum and maximum connection
              // intervals required by some iOS devices.

              // Parse Command Complete Event for opcode and status
              hciEvt_CmdComplete_t* command_complete = (hciEvt_CmdComplete_t*) pMsg;
              uint8_t   pktStatus = command_complete->pReturnParam[0];

              //find which command this command complete is for
              switch (command_complete->cmdOpcode)
              {
                case HCI_LE_READ_LOCAL_SUPPORTED_FEATURES:
                  {
                    if (pktStatus == SUCCESS)
                    {
                      uint8_t featSet[8];

                      // Get current feature set from received event (bits 1-9
                      // of the returned data
                      memcpy( featSet, &command_complete->pReturnParam[1], 8 );

                      // Clear bit 1 of byte 0 of feature set to disable LL
                      // Connection Parameter Updates
                      CLR_FEATURE_FLAG( featSet[0], LL_FEATURE_CONN_PARAMS_REQ );

                      // Update controller with modified features
                      HCI_EXT_SetLocalSupportedFeaturesCmd( featSet );
                    }
                  }
                  break;

                default:
                  //do nothing
                  break;
              }
#endif // !defined (USE_LL_CONN_PARAM_UPDATE)

            }
            break;

          case HCI_BLE_HARDWARE_ERROR_EVENT_CODE:
            AssertHandler(HAL_ASSERT_CAUSE_HARDWARE_ERROR,0);
            break;

          default:
            break;
        }
      }
      break;

      default:
        // do nothing
        break;

    }

  return (safeToDealloc);
}

/*********************************************************************
 * @fn      SimplePeripheral_processGATTMsg
 *
 * @brief   Process GATT messages and events.
 *
 * @return  TRUE if safe to deallocate incoming message, FALSE otherwise.
 */
static uint8_t SimplePeripheral_processGATTMsg(gattMsgEvent_t *pMsg)
{
  // See if GATT server was unable to transmit an ATT response
  if (attRsp_isAttRsp(pMsg))
  {
    // No HCI buffer was available. Let's try to retransmit the response
    // on the next connection event.
    if( SimplePeripheral_RegistertToAllConnectionEvent(FOR_ATT_RSP) == SUCCESS)
    {
      // Don't free the response message yet
      return (FALSE);
    }
  }
  else if (pMsg->method == ATT_FLOW_CTRL_VIOLATED_EVENT)
  {
    // ATT request-response or indication-confirmation flow control is
    // violated. All subsequent ATT requests or indications will be dropped.
    // The app is informed in case it wants to drop the connection.

    // Display the opcode of the message that caused the violation.
    printk("FC Violated: %d\n", pMsg->msg.flowCtrlEvt.opcode);
  }
  else if (pMsg->method == ATT_MTU_UPDATED_EVENT)
  {
    // MTU size updated
    printk("MTU Size: %d\n", pMsg->msg.mtuEvt.MTU);
  }

  // Free message payload. Needed only for ATT Protocol messages
  GATT_bm_free(&pMsg->msg, pMsg->method);

  // It's safe to free the incoming message
  return (TRUE);
}

/*********************************************************************
 * @fn      SimplePeripheral_processConnEvt
 *
 * @brief   Process connection event.
 *
 * @param pReport pointer to connection event report
 */
static void SimplePeripheral_processConnEvt(Gap_ConnEventRpt_t *pReport)
{

  if( CONNECTION_EVENT_REGISTRATION_CAUSE(FOR_ATT_RSP))
  {
    // The GATT server might have returned a blePending as it was trying
    // to process an ATT Response. Now that we finished with this
    // connection event, let's try sending any remaining ATT Responses
    // on the next connection event.
    // Try to retransmit pending ATT Response (if any)
    if (attRsp_sendAttRsp() == SUCCESS)
    {
        // Disable connection event end notice
        SimplePeripheral_UnRegistertToAllConnectionEvent (FOR_ATT_RSP);
    }
  }

}

/*********************************************************************
 * @fn      SimplePeripheral_processAppMsg
 *
 * @brief   Process an incoming callback from a profile.
 *
 * @param   pMsg - message to process
 *
 * @return  None.
 */
static void SimplePeripheral_processAppMsg(sbpEvt_t *pMsg)
{
  switch (pMsg->hdr.event)
  {
    case SBP_STATE_CHANGE_EVT:
      {
        SimplePeripheral_processStateChangeEvt((gaprole_States_t)pMsg->
                                                hdr.state);
      }
      break;

    case SBP_CHAR_CHANGE_EVT:
      {
        SimplePeripheral_processCharValueChangeEvt(pMsg->hdr.state);
      }
      break;

    // Pairing event
    case SBP_PAIRING_STATE_EVT:
      {
        SimplePeripheral_processPairState(pMsg->hdr.state, *pMsg->pData);

        ICall_free(pMsg->pData);
        break;
      }

    // Passcode event
    case SBP_PASSCODE_NEEDED_EVT:
      {
        SimplePeripheral_processPasscode(*pMsg->pData);

        ICall_free(pMsg->pData);
        break;
      }

	case SBP_CONN_EVT:
      {
        SimplePeripheral_processConnEvt((Gap_ConnEventRpt_t *)(pMsg->pData));

        ICall_free(pMsg->pData);
        break;
	  }

    default:
      // Do nothing.
      break;
  }
}

/*********************************************************************
 * @fn      SimplePeripheral_stateChangeCB
 *
 * @brief   Callback from GAP Role indicating a role state change.
 *
 * @param   newState - new state
 *
 * @return  None.
 */
static void SimplePeripheral_stateChangeCB(gaprole_States_t newState)
{
  SimplePeripheral_enqueueMsg(SBP_STATE_CHANGE_EVT, newState, NULL);
}

/*********************************************************************
 * @fn      SimplePeripheral_processStateChangeEvt
 *
 * @brief   Process a pending GAP Role state change event.
 *
 * @param   newState - new state
 *
 * @return  None.
 */
static void SimplePeripheral_processStateChangeEvt(gaprole_States_t newState)
{

  switch ( newState )
  {
    case GAPROLE_STARTED:
      {
        uint8_t ownAddress[B_ADDR_LEN];

        GAPRole_GetParameter(GAPROLE_BD_ADDR, ownAddress);
        // Display device address
        printk("Initialized: %s\n", Util_convertBdAddr2Str(ownAddress));

		adcval = read_adc(2);
		printk("VDDS: %d\n", adcval);

        SimplePeripheral_setDeviceName(ownAddress);

        clock_draw(UPDATE_FULL|DRAW_BT);

        Util_startClock(&periodicClock);

        // Device starts advertising upon initialization of GAP
        uint8_t initialAdvertEnable = TRUE;
        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &initialAdvertEnable);
		ble_state = 1;

		led_set(APP_LED_BLUE_PIN, 1);
      }
      break;

    case GAPROLE_ADVERTISING:
      printk("Advertising\n");
      break;

    case GAPROLE_CONNECTED:
      {
        linkDBInfo_t linkInfo;
        uint8_t numActive = 0;

        numActive = linkDB_NumActive();

        // Use numActive to determine the connection handle of the last connection
        if ( linkDB_GetInfo( numActive - 1, &linkInfo ) == SUCCESS )
        {
          printk("Num Conns: %d\n", (uint16_t)numActive);
          printk("Connected to %s\n", Util_convertBdAddr2Str(linkInfo.addr));
        }
        else
        {
          uint8_t peerAddress[B_ADDR_LEN];
          GAPRole_GetParameter(GAPROLE_CONN_BD_ADDR, peerAddress);
          printk("Connected to %s\n", Util_convertBdAddr2Str(peerAddress));
        }

		// 连接后要先禁止广播。否则连接断开后会自动启动广播。
        uint8_t initialAdvertEnable = FALSE;
        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &initialAdvertEnable);

		adcval = read_adc(2);
		clock_push();
		ble_state = 2;

      }
      break;

    case GAPROLE_CONNECTED_ADV:
      printk("Connected Advertising\n");
      break;

    case GAPROLE_WAITING:
      {
        attRsp_freeAttRsp(bleNotConnected);

		if(ble_state==2){
			printk("Disconnect.\n");
		}else{
	        printk("Advertising finished\n");
			clock_draw(UPDATE_FLY);
		}
		ble_state = 0;
      }
      break;

    case GAPROLE_WAITING_AFTER_TIMEOUT:
      attRsp_freeAttRsp(bleNotConnected);

      printk("Timed Out\n");

      break;

    case GAPROLE_ERROR:
      printk("Error\n");
      break;

    default:
      break;
  }

}

/*********************************************************************
 * @fn      SimplePeripheral_charValueChangeCB
 *
 * @brief   Callback from Simple Profile indicating a characteristic
 *          value change.
 *
 * @param   paramID - parameter ID of the value that was changed.
 *
 * @return  None.
 */
static void SimplePeripheral_charValueChangeCB(uint8_t paramID)
{
  SimplePeripheral_enqueueMsg(SBP_CHAR_CHANGE_EVT, paramID, 0);
}

/*********************************************************************
 * @fn      SimplePeripheral_processCharValueChangeEvt
 *
 * @brief   Process a pending Simple Profile characteristic value change
 *          event.
 *
 * @param   paramID - parameter ID of the value that was changed.
 *
 * @return  None.
 */
static void SimplePeripheral_processCharValueChangeEvt(uint8_t paramID)
{
  switch(paramID)
  {
    case SIMPLEPROFILE_CONTROL_POINT:
      printk("ControlPoint: %02x\n", simpleProfileControlPoint);
      break;

    case SIMPLEPROFILE_LONG_VALUE:
      printk("LongValue: %02x\n", simpleProfileLongValue[0]);
	  clock_cmd(simpleProfileLongValue);
      break;

    default:
      // should not reach here!
      break;
  }
}


void clock_fixup_set(int diff_sec, int minutes)
{
	// 计算新的修正值（基于4096精度的分数计算）
	int new_fixup_value = (int)(((int64_t)diff_sec*1000*4096)/minutes);
	printk("new_fixup_value: %04x\n", new_fixup_value);
	clock_fixup_value += new_fixup_value; // 累积修正值
	printk("clock_fixup_value: %04x\n", clock_fixup_value);
}


static int clock_fixup(void)
{
	int value;

	clock_fixup_count += clock_fixup_value; // 累积修正计数

	value = clock_fixup_count /4096; // 得到整数部分
	clock_fixup_count %= 4096;       // 保留余数部分

	return value; // 返回本次调整的毫秒数
}


void clock_timer_restart(void)
{
	int interval = clock_interval;
	int adj = clock_fixup();

	if(clock_interval_ones){
		interval = clock_interval_ones;
		clock_interval_ones = 0;
	}
	Util_restartClock(&periodicClock, interval*1000+adj);

	g_adj = adj;
}


int read_adc(int id)
{
    uint16_t rawValue;
    int microVolts = 0;

    ADC_Params params;
    ADC_Params_init(&params);

    ADC_Handle adc = ADC_open(id, &params);
    if (adc == NULL) {
        return 0;
    }

    int status = ADC_convert(adc, &rawValue);
    if (status == ADC_STATUS_SUCCESS) {
        microVolts = ADC_convertRawToMicroVolts(adc, rawValue);
		microVolts = microVolts/1000;
    }

    ADC_close(adc);

    return microVolts;
}

/*********************************************************************
 * @fn      SimplePeripheral_performPeriodicTask
 *
 * @brief   Perform a periodic application task. This function gets called
 *          every five seconds (SBP_PERIODIC_EVT_PERIOD). In this example,
 *          the value of the third characteristic in the SimpleGATTProfile
 *          service is retrieved from the profile, and then copied into the
 *          value of the the fourth characteristic.
 *
 * @param   None.
 *
 * @return  None.
 */
static void SimplePeripheral_performPeriodicTask(void)
{
	int flags = UPDATE_FLY;
	int stat = clock_update(clock_interval);

	clock_print();

	if(ble_state==2){
		clock_push();
	}

	if(stat>=3){
		flags = DRAW_BT | UPDATE_FULL; // 需要蓝牙图标+全量更新
		adcval = read_adc(2);
	}else if(stat>=2){
		flags = DRAW_BT | UPDATE_FAST; // 需要蓝牙图标+快速更新
	}

	// 如果需要显示蓝牙图标，启动广播
	if(flags&DRAW_BT){
       	uint8_t advertReEnable = TRUE;
        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &advertReEnable);
		ble_state = 1;
	}

	// 根据状态或标志更新屏幕显示
	if(stat>0 || flags&DRAW_BT){
		clock_draw(flags);
	}
}

/*********************************************************************
 * @fn      SimplePeripheral_pairStateCB
 *
 * @brief   Pairing state callback.
 *
 * @return  none
 */
static void SimplePeripheral_pairStateCB(uint16_t connHandle, uint8_t state,
                                            uint8_t status)
{
  uint8_t *pData;

  // Allocate space for the event data.
  if ((pData = ICall_malloc(sizeof(uint8_t))))
  {
    *pData = status;

    // Queue the event.
    SimplePeripheral_enqueueMsg(SBP_PAIRING_STATE_EVT, state, pData);
  }
}

/*********************************************************************
 * @fn      SimplePeripheral_processPairState
 *
 * @brief   Process the new paring state.
 *
 * @return  none
 */
static void SimplePeripheral_processPairState(uint8_t state, uint8_t status)
{
  if (state == GAPBOND_PAIRING_STATE_STARTED)
  {
    printk("Pairing started\n");
  }
  else if (state == GAPBOND_PAIRING_STATE_COMPLETE)
  {
    if (status == SUCCESS)
    {
      printk("Pairing success\n");
    }
    else
    {
      printk("Pairing fail: %d\n", status);
    }
  }
  else if (state == GAPBOND_PAIRING_STATE_BONDED)
  {
    if (status == SUCCESS)
    {
      printk("Bonding success\n");
    }
  }
  else if (state == GAPBOND_PAIRING_STATE_BOND_SAVED)
  {
    if (status == SUCCESS)
    {
      printk("Bond save success\n");
    }
    else
    {
      printk("Bond save failed: %d\n", status);
    }
  }
}

/*********************************************************************
 * @fn      SimplePeripheral_passcodeCB
 *
 * @brief   Passcode callback.
 *
 * @return  none
 */
static void SimplePeripheral_passcodeCB(uint8_t *deviceAddr,
                                        uint16_t connHandle,
                                        uint8_t uiInputs,
                                        uint8_t uiOutputs,
                                        uint32_t numComparison)
{
  uint8_t *pData;

  // Allocate space for the passcode event.
  if ((pData = ICall_malloc(sizeof(uint8_t))))
  {
    *pData = uiOutputs;

    // Enqueue the event.
    SimplePeripheral_enqueueMsg(SBP_PASSCODE_NEEDED_EVT, 0, pData);
  }
}

/*********************************************************************
 * @fn      SimplePeripheral_processPasscode
 *
 * @brief   Process the Passcode request.
 *
 * @return  none
 */
static void SimplePeripheral_processPasscode(uint8_t uiOutputs)
{
  // This app uses a default passcode. A real-life scenario would handle all
  // pairing scenarios and likely generate this randomly.
  uint32_t passcode = B_APP_DEFAULT_PASSCODE;

  // Display passcode to user
  if (uiOutputs != 0)
  {
    printk("Passcode: %d\n", passcode);
  }

  uint16_t connectionHandle;
  GAPRole_GetParameter(GAPROLE_CONNHANDLE, &connectionHandle);

  // Send passcode response
  GAPBondMgr_PasscodeRsp(connectionHandle, SUCCESS, passcode);
}

/*********************************************************************
 * @fn      SimplePeripheral_clockHandler
 *
 * @brief   Handler function for clock timeouts.
 *
 * @param   arg - event type
 *
 * @return  None.
 */

static uint32_t last_ticks = 0;

static void SimplePeripheral_clockHandler(UArg arg)
{
  clock_timer_restart();

  uint32_t tmp = Clock_getTicks();
  g_ticks = tmp-last_ticks;
  last_ticks = tmp;

  // Wake up the application.
  Event_post(syncEvent, arg);
}

/*********************************************************************
 * @fn      SimplePeripheral_connEvtCB
 *
 * @brief   Connection event callback.
 *
 * @param pReport pointer to connection event report
 */
static void SimplePeripheral_connEvtCB(Gap_ConnEventRpt_t *pReport)
{
  // Enqueue the event for processing in the app context.
  if( SimplePeripheral_enqueueMsg(SBP_CONN_EVT, 0 ,(uint8_t *) pReport) == FALSE)
  {
    ICall_free(pReport);
  }

}

/*********************************************************************
 *
 * @brief   Creates a message and puts the message in RTOS queue.
 *
 * @param   event - message event.
 * @param   state - message state.
 * @param   pData - message data pointer.
 *
 * @return  TRUE or FALSE
 */
static uint8_t SimplePeripheral_enqueueMsg(uint8_t event, uint8_t state,
                                           uint8_t *pData)
{
  sbpEvt_t *pMsg = ICall_malloc(sizeof(sbpEvt_t));

  // Create dynamic pointer to message.
  if (pMsg)
  {
    pMsg->hdr.event = event;
    pMsg->hdr.state = state;
    pMsg->pData = pData;

    // Enqueue the message.
    return Util_enqueueMsg(appMsgQueue, syncEvent, (uint8_t *)pMsg);
  }

  return FALSE;
}
/*********************************************************************
*********************************************************************/
