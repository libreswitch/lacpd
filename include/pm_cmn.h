/*
 * (c) Copyright 2015 Hewlett Packard Enterprise Development LP
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

//*********************************************************************
// File : pm_cmn.h
// This file has the structures and #defines accessed by various modules.
//*********************************************************************
#ifndef  __PM_CMN_H__
#define  __PM_CMN_H__

/***********************************************************/
/* PORT Macros                                             */
/***********************************************************/

// if this enum is modified, please update libs/cli/port_mgt.c
// struct npc port_info[]
enum PM_lport_type {
    PM_LPORT_INVALID  = 0x0,
    PM_LPORT_FAE      = 0x1,
    PM_LPORT_GIGE     = 0x2,
    PM_LPORT_POS      = 0x3,
    PM_LPORT_ATM      = 0x4,
    PM_LPORT_CMTS     = 0x5,
    PM_LPORT_SERIAL   = 0x6,
    PM_LPORT_10GIGE   = 0x7,
    PM_LPORT_10E      = 0x8,
    PM_LPORT_2_5GIGE  = 0x9,
    PM_LPORT_20GIGE   = 0xA,
    PM_LPORT_40GIGE   = 0xB,
    PM_LPORT_TYPE_MAX = 0xC
};

// if this enum is modified, please update libs/cli/port_mgt.c
// struct npc port_info[]
enum PM_sport_type {
    PM_SPORT_REGULAR  = 0x0,
    PM_SPORT_LAG      = 0x1,
    PM_SPORT_MLPPP    = 0x2,
    PM_SPORT_MPLS     = 0x3,
    PM_SPORT_MARTINI  = 0x4,
    PM_SPORT_TYPE_MAX = 0x5
};

/*****************************************************************************/
/*  PORT HANDLE                                                              */
/*****************************************************************************/

typedef unsigned long long port_handle_t;


/* P O R T - H A N D L E S                                                   */
/*    bracket () indicates the value                                         */
/* +-----------------------------------------------------------------------+ */
/* |TYPE | LPORT/SPORT INFO                                                | */
/* | 1   |      63                                                         | */
/* +-----------------------------------------------------------------------+ */
/* TYPE : LPORT (0), SPORT (1)                                               */
/*                                                                           */
/* Physical Port Handle (LPORT)                                              */
/* +-----------------------------------------------------------------------+ */
/* |TYPE | slot |module| port | lport |svlan|Reserv| lamda| cha_ | vpi| vci| */
/* |     |      |      |      | type  |     |ed    |      | nnel |    |    | */
/* |1 (0)|  5   |  2   |  8   |  4    |1 (0)|  6   |   5  |  8   |  8 | 16 | */
/* +-----------------------------------------------------------------------+ */
/* Stacked Vlan Port Handle                                                  */
/* +-----------------------------------------------------------------------+ */
/* |TYPE | slot |module| port | lport |svlan| Reserved    | vlan           | */
/* |     |      |      |      | type  |     |             |                | */
/* |1 (0)|  5   |  2   |  8   |  4    |1 (1)|    31       |  12            | */
/* +-----------------------------------------------------------------------+ */

/* Following API's should be used by all modules to generate port handles */

/* Convert Slot, Module, Port, lport type to physical port handle */
#define PM_SMPT2HANDLE(slot, module, port, lport_type) \
    (((port_handle_t)slot << 58) | ((port_handle_t)module << 56)  | \
     ((port_handle_t)port << 48) | ((port_handle_t)lport_type << 44))

#define PM_HANDLE2PORT(handle)   ((int)((handle >> 48) & 0xff)) /* PORT */


/* SPORT Handle (LAG, MLPPP, MPLS)                                           */
/* +-----------------------------------------------------------------------+ */
/* |TYPE   | sport | sport | Unused                                        | */
/* |(SPORT)| type  |  id   |                                               | */
/* | 1 (1) |  4    |  16   | 11+32                                         | */
/* +-----------------------------------------------------------------------+ */

/* Convert LAG id to port handle */
#define SPORT_ID_OFFSET         43
#define SPORT_ID_MASK           0xFFFF
#define SPORT_TYPE_OFFSET       59
#define SPORT_MSB_OFFSET        63

#define PM_HANDLE2LAG(handle)    ((handle >> SPORT_ID_OFFSET) & SPORT_ID_MASK)


/* MLPPP                                                                     */
/* +-----------------------------------------------------------------------+ */
/* |MSB is | sport | mlppp | Unused                                        | */
/* |one    |  type |  id   |                                               | */
/* | 1 (1) |4 (MP) |  16   | 11+32                                         | */
/* +-----------------------------------------------------------------------+ */
#define PM_SPORT2HANDLE(sport_type, sid) \
     ( (port_handle_t)1 << SPORT_MSB_OFFSET | \
       ((port_handle_t)sport_type << SPORT_TYPE_OFFSET) | \
       ((port_handle_t)sid << SPORT_ID_OFFSET))

#define PM_GET_SPORT_ID(handle)    ((handle >> SPORT_ID_OFFSET) & SPORT_ID_MASK)

/* Convert LAG id to port handle */
#define PM_LAG2HANDLE(lagid) PM_SPORT2HANDLE(PM_SPORT_LAG, lagid)

/*
 * if the following enum is modified,
 *  please update struct nmm mod_info[]
 *  in libs/cli/port_mgt.c
 */
// Format of Enum
// PM_MM_SizexType

enum PM_MEDIA_MODULE_TYPES {
    PM_MM_UNKNOWN = 0,
    PM_MM_12xGIGE,
    PM_MM_OC48,
#ifdef STARSHIP
    PM_MM_4xGIGE,
    PM_MM_10xGIGE,                  /* Generic 10port GIGE */
    PM_MM_24xGIGE,                  /* Generic 24port GIGE */
    PM_MM_1x10GIGE,                 /* Generic 1port 10GIGE */
    PM_MM_OC192
#endif
};

// Starts with 1, minus 1 before program to hw
#define PORT_AGGR_MODE_PORT_BASED   1
#define PORT_AGGR_MODE_MAC_BASED    2
#define PORT_AGGR_MODE_L3_BASED     3
#define PORT_AGGR_MODE_L4_BASED     4
#define PORT_AGGR_MODE_DEFAULT      PORT_AGGR_MODE_L3_BASED

#endif  // __PM_CMN_H__
