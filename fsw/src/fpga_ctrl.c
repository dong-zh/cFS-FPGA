/*******************************************************************************
**
**      GSC-18128-1, "Core Flight Executive Version 6.7"
**
**      Copyright (c) 2006-2019 United States Government as represented by
**      the Administrator of the National Aeronautics and Space Administration.
**      All Rights Reserved.
**
**      Licensed under the Apache License, Version 2.0 (the "License");
**      you may not use this file except in compliance with the License.
**      You may obtain a copy of the License at
**
**        http://www.apache.org/licenses/LICENSE-2.0
**
**      Unless required by applicable law or agreed to in writing, software
**      distributed under the License is distributed on an "AS IS" BASIS,
**      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**      See the License for the specific language governing permissions and
**      limitations under the License.
**
** File: fpga_ctrl.c
**
** Purpose:
**   This file contains the source code for the FPGA Ctrl app.
**
*******************************************************************************/

/*
** Include Files:
*/
#include <string.h>

// Platform specific includes
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include "cfe.h"

#include "fpga_ctrl.h"
#include "fpga_ctrl_events.h"
#include "fpga_ctrl_table.h"
#include "fpga_ctrl_version.h"

#include "fpga_ctrl_interrupts.h"
#include "fpga_ctrl_aes.h"
#include "fpga_ctrl_load_bitstream.h"
#include "mmio_lib.h"

/* The sample_lib module provides the SAMPLE_LIB_Function() prototype */
// #include "sample_lib.h"

static int32 FPGA_CTRL_Init(void);
static void  FPGA_CTRL_ProcessCommandPacket(CFE_SB_Buffer_t *SBBufPtr);
static void  FPGA_CTRL_ProcessGroundCommand(CFE_SB_Buffer_t *SBBufPtr);
static int32 FPGA_CTRL_ReportHousekeeping(const CFE_MSG_CommandHeader_t *Msg);
static int32 FPGA_CTRL_ResetCounters(const FPGA_CTRL_ResetCountersCmd_t *Msg);
static int32 FPGA_CTRL_Process(const FPGA_CTRL_ProcessCmd_t *Msg);
static int32 FPGA_CTRL_Noop(const FPGA_CTRL_NoopCmd_t *Msg);
static void  FPGA_CTRL_GetCrc(const char *TableName);
static int32 FPGA_CTRL_TblValidationFunc(void *TblData);
static bool  FPGA_CTRL_VerifyCmdLength(CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength);

/*
** global data
*/
FPGA_CTRL_Data_t globalState;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
/* FPGA_CTRL_Main() -- Application entry point and main process loop         */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * *  * * * * **/
void FPGA_CTRL_Main(void)
{
    int32            status;
    CFE_SB_Buffer_t *SBBufPtr;

    /*
    ** Create the first Performance Log entry
    */
    CFE_ES_PerfLogEntry(FPGA_CTRL_PERF_ID);

    /*
    ** Perform application specific initialization
    ** If the Initialization fails, set the RunStatus to
    ** CFE_ES_RunStatus_APP_ERROR and the App will not enter the RunLoop
    */
    status = FPGA_CTRL_Init();
    if (status != CFE_SUCCESS)
    {
        globalState.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    /*
    ** FPGA_CTRL Runloop
    */
    while (CFE_ES_RunLoop(&globalState.RunStatus) == true)
    {
        /*
        ** Performance Log Exit Stamp
        */
        CFE_ES_PerfLogExit(FPGA_CTRL_PERF_ID);

        /* Pend on receipt of command packet */
        status = CFE_SB_ReceiveBuffer(&SBBufPtr, globalState.CommandPipe, CFE_SB_PEND_FOREVER);

        /*
        ** Performance Log Entry Stamp
        */
        CFE_ES_PerfLogEntry(FPGA_CTRL_PERF_ID);

        if (status == CFE_SUCCESS)
        {
            FPGA_CTRL_ProcessCommandPacket(SBBufPtr);
        }
        else
        {
            CFE_EVS_SendEvent(FPGA_CTRL_PIPE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "FPGA CTRL: SB Pipe Read Error, App Will Exit");

            globalState.RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
    }

    /*
    ** Performance Log Exit Stamp
    */
    CFE_ES_PerfLogExit(FPGA_CTRL_PERF_ID);

    CFE_ES_ExitApp(globalState.RunStatus);

} /* End of FPGA_CTRL_Main() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  */
/*                                                                            */
/* FPGA_CTRL_Init() --  initialization                                       */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
static int32 FPGA_CTRL_Init(void)
{
    int32 status;

    globalState.RunStatus = CFE_ES_RunStatus_APP_RUN;

    /*
    ** Initialize app command execution counters
    */
    globalState.CmdCounter          = 0;
    globalState.ErrCounter          = 0;
    globalState.childTaskRunning    = false;
    globalState.childTaskShouldExit = true;
    globalState.childTaskId         = CFE_ES_TASKID_UNDEFINED;
    snprintf(globalState.cyphertextHexString, 16 * 2 + 1, "Nothing_encrypted");

    /*
    ** Initialize app configuration data
    */
    globalState.PipeDepth = FPGA_CTRL_PIPE_DEPTH;

    strncpy(globalState.PipeName, "FPGA_CTRL_CMD_PIPE", sizeof(globalState.PipeName));
    globalState.PipeName[sizeof(globalState.PipeName) - 1] = 0;

    /*
    ** Initialize event filter table...
    */
    globalState.EventFilters[0].EventID = FPGA_CTRL_STARTUP_INF_EID;
    globalState.EventFilters[0].Mask    = 0x0000;
    globalState.EventFilters[1].EventID = FPGA_CTRL_COMMAND_ERR_EID;
    globalState.EventFilters[1].Mask    = 0x0000;
    globalState.EventFilters[2].EventID = FPGA_CTRL_COMMANDNOP_INF_EID;
    globalState.EventFilters[2].Mask    = 0x0000;
    globalState.EventFilters[3].EventID = FPGA_CTRL_COMMANDRST_INF_EID;
    globalState.EventFilters[3].Mask    = 0x0000;
    globalState.EventFilters[4].EventID = FPGA_CTRL_INVALID_MSGID_ERR_EID;
    globalState.EventFilters[4].Mask    = 0x0000;
    globalState.EventFilters[5].EventID = FPGA_CTRL_LEN_ERR_EID;
    globalState.EventFilters[5].Mask    = 0x0000;
    globalState.EventFilters[6].EventID = FPGA_CTRL_PIPE_ERR_EID;
    globalState.EventFilters[6].Mask    = 0x0000;

    /*
    ** Register the events
    */
    status = CFE_EVS_Register(globalState.EventFilters, FPGA_CTRL_EVENT_COUNTS, CFE_EVS_EventFilter_BINARY);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("FPGA Ctrl: Error Registering Events, RC = 0x%08lX\n", (unsigned long)status);
        return (status);
    }

    /*
    ** Initialize housekeeping packet (clear user data area).
    */
    CFE_MSG_Init(&globalState.HkTlm.TlmHeader.Msg, CFE_SB_ValueToMsgId(FPGA_CTRL_HK_TLM_MID),
                 sizeof(globalState.HkTlm));

    /*
    ** Create Software Bus message pipe.
    */
    status = CFE_SB_CreatePipe(&globalState.CommandPipe, globalState.PipeDepth, globalState.PipeName);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("FPGA Ctrl: Error creating pipe, RC = 0x%08lX\n", (unsigned long)status);
        return (status);
    }

    /*
    ** Subscribe to Housekeeping request commands
    */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(FPGA_CTRL_SEND_HK_MID), globalState.CommandPipe);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("FPGA Ctrl: Error Subscribing to HK request, RC = 0x%08lX\n", (unsigned long)status);
        return (status);
    }

    /*
    ** Subscribe to ground command packets
    */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(FPGA_CTRL_CMD_MID), globalState.CommandPipe);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("FPGA Ctrl: Error Subscribing to Command, RC = 0x%08lX\n", (unsigned long)status);

        return (status);
    }

    /*
    ** Register Table(s)
    */
    status = CFE_TBL_Register(&globalState.TblHandles[0], "FpgaCtrlTable", sizeof(FPGA_CTRL_Table_t),
                              CFE_TBL_OPT_DEFAULT, FPGA_CTRL_TblValidationFunc);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("FPGA Ctrl: Error Registering Table, RC = 0x%08lX\n", (unsigned long)status);

        return (status);
    }
    else
    {
        status = CFE_TBL_Load(globalState.TblHandles[0], CFE_TBL_SRC_FILE, FPGA_CTRL_TABLE_FILE);
    }

    CFE_EVS_SendEvent(FPGA_CTRL_STARTUP_INF_EID, CFE_EVS_EventType_INFORMATION, "FPGA Ctrl Initialized.%s",
                      FPGA_CTRL_VERSION_STRING);

    return (CFE_SUCCESS);

} /* End of FPGA_CTRL_Init() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*  Name:  FPGA_CTRL_ProcessCommandPacket                                    */
/*                                                                            */
/*  Purpose:                                                                  */
/*     This routine will process any packet that is received on the SAMPLE    */
/*     command pipe.                                                          */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
static void FPGA_CTRL_ProcessCommandPacket(CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;

    CFE_MSG_GetMsgId(&SBBufPtr->Msg, &MsgId);

    switch (CFE_SB_MsgIdToValue(MsgId))
    {
        case FPGA_CTRL_CMD_MID:
            FPGA_CTRL_ProcessGroundCommand(SBBufPtr);
            break;

        case FPGA_CTRL_SEND_HK_MID:
            FPGA_CTRL_ReportHousekeeping((CFE_MSG_CommandHeader_t *)SBBufPtr);
            break;

        default:
            CFE_EVS_SendEvent(FPGA_CTRL_INVALID_MSGID_ERR_EID, CFE_EVS_EventType_ERROR,
                              "FPGA_CTRL: invalid command packet,MID = 0x%x", (unsigned int)CFE_SB_MsgIdToValue(MsgId));
            break;
    }

    return;

} /* End FPGA_CTRL_ProcessCommandPacket */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* FPGA_CTRL_ProcessGroundCommand() -- SAMPLE ground commands                */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
static void FPGA_CTRL_ProcessGroundCommand(CFE_SB_Buffer_t *SBBufPtr)
{
    CFE_MSG_FcnCode_t CommandCode = 0;

    CFE_MSG_GetFcnCode(&SBBufPtr->Msg, &CommandCode);

    /*
    ** Process "known" SAMPLE app ground commands
    */
    switch (CommandCode)
    {
        case FPGA_CTRL_NOOP_CC:
            if (FPGA_CTRL_VerifyCmdLength(&SBBufPtr->Msg, sizeof(FPGA_CTRL_NoopCmd_t)))
            {
                ++globalState.CmdCounter;
                FPGA_CTRL_Noop((FPGA_CTRL_NoopCmd_t *)SBBufPtr);
            }

            break;

        case FPGA_CTRL_RESET_COUNTERS_CC:
            if (FPGA_CTRL_VerifyCmdLength(&SBBufPtr->Msg, sizeof(FPGA_CTRL_ResetCountersCmd_t)))
            {
                ++globalState.CmdCounter;
                FPGA_CTRL_ResetCounters((FPGA_CTRL_ResetCountersCmd_t *)SBBufPtr);
            }

            break;

        case FPGA_CTRL_PROCESS_CC:
            if (FPGA_CTRL_VerifyCmdLength(&SBBufPtr->Msg, sizeof(FPGA_CTRL_ProcessCmd_t)))
            {
                ++globalState.CmdCounter;
                FPGA_CTRL_Process((FPGA_CTRL_ProcessCmd_t *)SBBufPtr);
            }

            break;

        case FPGA_CTRL_ENCRYPT_CC:
            if (FPGA_CTRL_VerifyCmdLength(&SBBufPtr->Msg, sizeof(FPGA_CTRL_EncryptCmd_t)))
            {
                ++globalState.CmdCounter;
                FPGA_CTRL_Encrypt((FPGA_CTRL_EncryptCmd_t *)SBBufPtr);
            }

            break;

        case FPGA_CTRL_INT_CTRL_CC:
            if (FPGA_CTRL_VerifyCmdLength(&SBBufPtr->Msg, sizeof(FPGA_CTRL_IntCtrlCmd_t)))
            {
                ++globalState.CmdCounter;
                FPGA_CTRL_IntCtrl((FPGA_CTRL_IntCtrlCmd_t *)SBBufPtr);
            }

            break;

        case FPGA_CTRL_REPROGRAM_CC:
            if (FPGA_CTRL_VerifyCmdLength(&SBBufPtr->Msg, sizeof(FPGA_CTRL_ReprogramCmd_t)))
            {
                ++globalState.CmdCounter;
                FPGA_CTRL_LoadBitstream((FPGA_CTRL_ReprogramCmd_t *)SBBufPtr);
            }

            break;

        /* default case already found during FC vs length test */
        default:
            ++globalState.ErrCounter;
            CFE_EVS_SendEvent(FPGA_CTRL_COMMAND_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Invalid ground command code: CC = %d", CommandCode);

            break;
    }

    return;

} /* End of FPGA_CTRL_ProcessGroundCommand() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*  Name:  FPGA_CTRL_ReportHousekeeping                                          */
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function is triggered in response to a task telemetry request */
/*         from the housekeeping task. This function will gather the Apps     */
/*         telemetry, packetize it and send it to the housekeeping task via   */
/*         the software bus                                                   */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
static int32 FPGA_CTRL_ReportHousekeeping(const CFE_MSG_CommandHeader_t *Msg)
{
    /*
    ** Get command execution counters...
    */
    FPGA_CTRL_HkTlm_Payload_t *const payload = &globalState.HkTlm.Payload;
    payload->CommandErrorCounter             = globalState.ErrCounter;
    payload->CommandCounter                  = globalState.CmdCounter;
    payload->childTaskRunning                = globalState.childTaskRunning;
    memcpy(payload->cyphertextHexString, globalState.cyphertextHexString, sizeof(globalState.cyphertextHexString));

    /*
    ** Send housekeeping telemetry packet...
    */
    CFE_SB_TimeStampMsg(&globalState.HkTlm.TlmHeader.Msg);
    CFE_SB_TransmitMsg(&globalState.HkTlm.TlmHeader.Msg, true);

    /*
    ** Manage any pending table loads, validations, etc.
    */
    for (int i = 0; i < FPGA_CTRL_NUMBER_OF_TABLES; i++)
    {
        CFE_TBL_Manage(globalState.TblHandles[i]);
    }

    // CFE_EVS_SendEvent(FPGA_CTRL_DEBUG_INF_EID, CFE_EVS_EventType_INFORMATION, "Hello");

    // testFunction();
    return CFE_SUCCESS;

} /* End of FPGA_CTRL_ReportHousekeeping() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* FPGA_CTRL_Noop -- SAMPLE NOOP commands                                        */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
static int32 FPGA_CTRL_Noop(const FPGA_CTRL_NoopCmd_t *Msg)
{

    // globalState.CmdCounter++;

    CFE_EVS_SendEvent(FPGA_CTRL_COMMANDNOP_INF_EID, CFE_EVS_EventType_INFORMATION, "FPGA_CTRL: NOOP command %s",
                      FPGA_CTRL_VERSION);

    return CFE_SUCCESS;

} /* End of FPGA_CTRL_Noop */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*  Name:  FPGA_CTRL_ResetCounters                                               */
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function resets all the global counter variables that are     */
/*         part of the task telemetry.                                        */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
static int32 FPGA_CTRL_ResetCounters(const FPGA_CTRL_ResetCountersCmd_t *Msg)
{

    globalState.CmdCounter = 0;
    globalState.ErrCounter = 0;

    CFE_EVS_SendEvent(FPGA_CTRL_COMMANDRST_INF_EID, CFE_EVS_EventType_INFORMATION, "FPGA_CTRL: RESET command");

    return CFE_SUCCESS;

} /* End of FPGA_CTRL_ResetCounters() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*  Name:  FPGA_CTRL_Process                                                     */
/*                                                                            */
/*  Purpose:                                                                  */
/*         This function Process Ground Station Command                       */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * *  * *  * * * * */
static int32 FPGA_CTRL_Process(const FPGA_CTRL_ProcessCmd_t *Msg)
{
    int32              status;
    FPGA_CTRL_Table_t *TblPtr;
    const char        *TableName = "FPGA_CTRL.FpgaCtrlTable";

    /* Sample Use of Table */

    status = CFE_TBL_GetAddress((void *)&TblPtr, globalState.TblHandles[0]);

    if (status < CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("FPGA Ctrl: Fail to get table address: 0x%08lx", (unsigned long)status);
        return status;
    }

    CFE_ES_WriteToSysLog("FPGA Ctrl: Table Value 1: %d  Value 2: %d", TblPtr->Int1, TblPtr->Int2);

    FPGA_CTRL_GetCrc(TableName);

    status = CFE_TBL_ReleaseAddress(globalState.TblHandles[0]);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("FPGA Ctrl: Fail to release table address: 0x%08lx", (unsigned long)status);
        return status;
    }

    /* Invoke a function provided by FPGA_CTRL_LIB */
    // SAMPLE_LIB_Function();

    return CFE_SUCCESS;

} /* End of FPGA_CTRL_ProcessCC */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
/*                                                                            */
/* FPGA_CTRL_VerifyCmdLength() -- Verify command packet length                   */
/*                                                                            */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * **/
static bool FPGA_CTRL_VerifyCmdLength(CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength)
{
    bool              result       = true;
    size_t            ActualLength = 0;
    CFE_SB_MsgId_t    MsgId        = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_FcnCode_t FcnCode      = 0;

    CFE_MSG_GetSize(MsgPtr, &ActualLength);

    /*
    ** Verify the command packet length.
    */
    if (ExpectedLength != ActualLength)
    {
        CFE_MSG_GetMsgId(MsgPtr, &MsgId);
        CFE_MSG_GetFcnCode(MsgPtr, &FcnCode);

        CFE_EVS_SendEvent(FPGA_CTRL_LEN_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid Msg length: ID = 0x%X,  CC = %u, Len = %u, Expected = %u",
                          (unsigned int)CFE_SB_MsgIdToValue(MsgId), (unsigned int)FcnCode, (unsigned int)ActualLength,
                          (unsigned int)ExpectedLength);

        result = false;

        globalState.ErrCounter++;
    }

    return (result);

} /* End of FPGA_CTRL_VerifyCmdLength() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FPGA_CTRL_TblValidationFunc -- Verify contents of First Table      */
/* buffer contents                                                 */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static int32 FPGA_CTRL_TblValidationFunc(void *TblData)
{
    int32              ReturnCode = CFE_SUCCESS;
    FPGA_CTRL_Table_t *TblDataPtr = (FPGA_CTRL_Table_t *)TblData;

    /*
    ** Sample Table Validation
    */
    if (TblDataPtr->Int1 > FPGA_CTRL_TBL_ELEMENT_1_MAX)
    {
        /* First element is out of range, return an appropriate error code */
        ReturnCode = FPGA_CTRL_TABLE_OUT_OF_RANGE_ERR_CODE;
    }

    return ReturnCode;

} /* End of FPGA_CTRL_TBLValidationFunc() */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FPGA_CTRL_GetCrc -- Output CRC                                     */
/*                                                                 */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static void FPGA_CTRL_GetCrc(const char *TableName)
{
    int32          status;
    uint32         Crc;
    CFE_TBL_Info_t TblInfoPtr;

    status = CFE_TBL_GetInfo(&TblInfoPtr, TableName);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("FPGA Ctrl: Error Getting Table Info");
    }
    else
    {
        Crc = TblInfoPtr.Crc;
        CFE_ES_WriteToSysLog("FPGA Ctrl: CRC: 0x%08lX\n\n", (unsigned long)Crc);
    }

    return;

} /* End of FPGA_CTRL_GetCrc */
