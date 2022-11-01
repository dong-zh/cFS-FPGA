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
 * Define FPGA Ctrl Messages and info
 */

#ifndef FPGA_CTRL_MSG_H
#define FPGA_CTRL_MSG_H

/*
** SAMPLE App command codes
*/
#define FPGA_CTRL_NOOP_CC           0 // No-op (default)
#define FPGA_CTRL_RESET_COUNTERS_CC 1 // Reset counters (default)
#define FPGA_CTRL_PROCESS_CC        2 // Process data (default)
#define FPGA_CTRL_ENCRYPT_CC        3 // Perform encryption on attached data
#define FPGA_CTRL_INT_CTRL_CC       4 // Enable or disable interrupt task
#define FPGA_CTRL_REPROGRAM_CC      5 // Reprogram FPGA with new bitstream

/*************************************************************************/

/*
** Type definition (generic "no arguments" command)
*/
typedef struct
{
    CFE_MSG_CommandHeader_t CmdHeader; /**< \brief Command header */
} FPGA_CTRL_NoArgsCmd_t;

// 16 byte payload for encryption
typedef struct
{
    CFE_MSG_CommandHeader_t CmdHeader;
    char                    data[16];
} FPGA_CTRL_EncryptCmd_t;

// Boolean for starting or stopping interrupt task
typedef struct
{
    CFE_MSG_CommandHeader_t CmdHeader;
    uint8                   enable; // boolean
} FPGA_CTRL_IntCtrlCmd_t;

// Filename for bitstream
typedef struct
{
    CFE_MSG_CommandHeader_t CmdHeader;
    char                    path[128];
} FPGA_CTRL_ReprogramCmd_t;

// enum BitstreamCode
// {
//     AesEncryptCode = 0,
//     InterruptDemo  = 1,
// };

/*
** The following commands all share the "NoArgs" format
**
** They are each given their own type name matching the command name, which
** allows them to change independently in the future without changing the prototype
** of the handler function
*/
typedef FPGA_CTRL_NoArgsCmd_t FPGA_CTRL_NoopCmd_t;
typedef FPGA_CTRL_NoArgsCmd_t FPGA_CTRL_ResetCountersCmd_t;
typedef FPGA_CTRL_NoArgsCmd_t FPGA_CTRL_ProcessCmd_t;

/*************************************************************************/
/*
** Type definition (SAMPLE App housekeeping)
*/

typedef struct
{
    uint8 CommandCounter;
    uint8 CommandErrorCounter;
    char  cyphertextHexString[16 * 2 + 1];
    uint8 childTaskRunning; // boolean
    // uint8 padding[1];
} FPGA_CTRL_HkTlm_Payload_t;

typedef struct
{
    CFE_MSG_TelemetryHeader_t TlmHeader; /**< \brief Telemetry header */
    FPGA_CTRL_HkTlm_Payload_t Payload;   /**< \brief Telemetry payload */
} FPGA_CTRL_HkTlm_t;

// Telemetry packet for interrupt with switch positioning
typedef struct
{
    CFE_MSG_TelemetryHeader_t TlmHeader; /**< \brief Telemetry header */
    uint8                     switchPos; /**< \brief Switch position */
} FPGA_CTRL_IntTlm_t;

#endif /* FPGA_CTRL_MSG_H */
