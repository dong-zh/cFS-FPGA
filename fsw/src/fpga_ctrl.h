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
*******************************************************************************/

/**
 * @file
 *
 * Main header file for the SAMPLE application
 */

#ifndef FPGA_CTRL_H
#define FPGA_CTRL_H

/*
** Required header files.
*/
#include "cfe.h"
#include "cfe_error.h"
#include "cfe_evs.h"
#include "cfe_sb.h"
#include "cfe_es.h"

#include "fpga_ctrl_perfids.h"
#include "fpga_ctrl_msgids.h"
#include "fpga_ctrl_msg.h"

/***********************************************************************/
#define FPGA_CTRL_PIPE_DEPTH 32 /* Depth of the Command Pipe for Application */

#define FPGA_CTRL_NUMBER_OF_TABLES 1 /* Number of Table(s) */

/* Define filenames of default data images for tables */
#define FPGA_CTRL_TABLE_FILE "/cf/fpga_ctrl_tbl.tbl"

#define FPGA_CTRL_TABLE_OUT_OF_RANGE_ERR_CODE -1

#define FPGA_CTRL_TBL_ELEMENT_1_MAX 10
/************************************************************************
** Type Definitions
*************************************************************************/

/*
** Global Data
*/
typedef struct
{
    /*
    ** Command interface counters...
    */
    uint8 CmdCounter;
    uint8 ErrCounter;

    /*
    ** Housekeeping telemetry packet...
    */
    FPGA_CTRL_HkTlm_t HkTlm;

    /*
    ** Run Status variable used in the main processing loop
    */
    uint32 RunStatus;

    /*
    ** Operational data (not reported in housekeeping)...
    */
    CFE_SB_PipeId_t CommandPipe;

    /*
    ** Initialization data (not reported in housekeeping)...
    */
    char   PipeName[CFE_MISSION_MAX_API_LEN];
    uint16 PipeDepth;

    CFE_EVS_BinFilter_t EventFilters[FPGA_CTRL_EVENT_COUNTS];
    CFE_TBL_Handle_t    TblHandles[FPGA_CTRL_NUMBER_OF_TABLES];

} FPGA_CTRL_Data_t;

/****************************************************************************/
/*
** Local function prototypes.
**
** Note: Except for the entry point (FPGA_CTRL_Main), these
**       functions are not called from any other source module.
*/
void  FPGA_CTRL_Main(void);
int32 FPGA_CTRL_Init(void);
void  FPGA_CTRL_ProcessCommandPacket(CFE_SB_Buffer_t *SBBufPtr);
void  FPGA_CTRL_ProcessGroundCommand(CFE_SB_Buffer_t *SBBufPtr);
int32 FPGA_CTRL_ReportHousekeeping(const CFE_MSG_CommandHeader_t *Msg);
int32 FPGA_CTRL_ResetCounters(const FPGA_CTRL_ResetCountersCmd_t *Msg);
int32 FPGA_CTRL_Process(const FPGA_CTRL_ProcessCmd_t *Msg);
int32 FPGA_CTRL_Noop(const FPGA_CTRL_NoopCmd_t *Msg);
void  FPGA_CTRL_GetCrc(const char *TableName);

int32 FPGA_CTRL_TblValidationFunc(void *TblData);

bool FPGA_CTRL_VerifyCmdLength(CFE_MSG_Message_t *MsgPtr, size_t ExpectedLength);

#endif /* FPGA_CTRL_H */
