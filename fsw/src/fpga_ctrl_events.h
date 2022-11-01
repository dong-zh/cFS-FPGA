/************************************************************************
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
*************************************************************************/

/**
 * @file
 *
 * Define FPGA Ctrl Events IDs
 */

#ifndef FPGA_CTRL_EVENTS_H
#define FPGA_CTRL_EVENTS_H

enum EventIds {
    FPGA_CTRL_RESERVED_EID = 0,
    FPGA_CTRL_STARTUP_INF_EID,
    FPGA_CTRL_COMMAND_ERR_EID,
    FPGA_CTRL_COMMANDNOP_INF_EID,
    FPGA_CTRL_COMMANDRST_INF_EID,
    FPGA_CTRL_INVALID_MSGID_ERR_EID,
    FPGA_CTRL_LEN_ERR_EID,
    FPGA_CTRL_PIPE_ERR_EID,
    FPGA_CTRL_DEBUG_INF_EID,
};

// #define FPGA_CTRL_RESERVED_EID          0
// #define FPGA_CTRL_STARTUP_INF_EID       1
// #define FPGA_CTRL_COMMAND_ERR_EID       2
// #define FPGA_CTRL_COMMANDNOP_INF_EID    3
// #define FPGA_CTRL_COMMANDRST_INF_EID    4
// #define FPGA_CTRL_INVALID_MSGID_ERR_EID 5
// #define FPGA_CTRL_LEN_ERR_EID           6
// #define FPGA_CTRL_PIPE_ERR_EID          7
// #define FPGA_CTRL_DEBUG_INF_EID         8

#define FPGA_CTRL_EVENT_COUNTS 8

#endif /* FPGA_CTRL_EVENTS_H */
