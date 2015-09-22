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
// Halon: turn on support for 10GbE type.  Not sure what else
//          STARSHIP includes, but avoid it for now.  This type
//          seems to be used only for constructing port handles.
//#ifdef STARSHIP
    PM_LPORT_10GIGE   = 0x7,
//#endif

// Halon: turn on support for 10Mbps and 2.5Mbps ports.
    PM_LPORT_10E      = 0x8,
    PM_LPORT_2_5GIGE  = 0x9,

// Halon: turn on support for 20GbE and 40GbE type.
    PM_LPORT_20GIGE   = 0xA,
    PM_LPORT_40GIGE   = 0xB,

    PM_LPORT_TYPE_MAX = 0xC
};
#define PM_LPORT_ANY   PM_LPORT_INVALID

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

#define LPORT_IS_PPP(handle) ((PM_HANDLE2LTYPE(handle)) == PM_LPORT_POS)
#define LPORT_TYPE_IS_PPP(type) ((type) == PM_LPORT_POS)

/*****************************************************************************/
/*  PORT HANDLE                                                              */
/*****************************************************************************/

typedef unsigned long long port_handle_t;

#define PM_PORT_HANDLE_INVALID ((port_handle_t)0)

#define PM_PORT_HANDLE_ntohl(h) NEMO_HTONL64(h)
#define PM_PORT_HANDLE_htonl(h) NEMO_NTOHL64(h)

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

/* create virtual port handle from phy port handle, lamda, chan, vpi, vci */
#define PM_VC2HANDLE(phy_handle, lamda, chan, vpi, vci) \
   (phy_handle | ((port_handle_t)lambda << 32) | ((port_handle_t)chan << 24) |\
    ((port_handle_t)vpi << 16) | ((port_handle_t)vci))

/* create svlan handle from phy port handle, vlan */
#define PM_SVLAN2HANDLE(phy_handle, vlan) \
   (phy_handle | ((port_handle_t)1 << 43) | ((port_handle_t)vlan))

/* Derive the physical port handle containing slot, module, port
   from the logical port handle */
#define PM_GET_PHYHANDLE(handle) (handle & 0xFFFFF00000000000LL)

#define PM_HANDLE2SLOT(handle)   ((int)((handle >> 58) & 0x1f)) /* SLOT */
#define PM_HANDLE2MODULE(handle) ((int)((handle >> 56) & 0x3))  /* MODULE */
#define PM_HANDLE2PORT(handle)   ((int)((handle >> 48) & 0xff)) /* PORT */
#define PM_HANDLE2LTYPE(handle)  ((int)((handle >> 44) & 0xf))  /* LPORT TYPE*/
#define PM_HANDLE2LAMBDA(handle) ((int)((handle >> 32) & 0x1f)) /* LAMBDA */
#define PM_HANDLE2CHAN(handle)   ((int)((handle >> 24) & 0xff)) /* CHANNEL */
#define PM_HANDLE2VPI(handle)    ((int)((handle >> 16) & 0xff)) /* VPI */
#define PM_HANDLE2VCI(handle)    ((int)((handle)       & 0xffff)) /* VCI */
#define PM_HANDLE2SVLAN(handle)  ((int)((handle)       & 0xfff)) /* VLAN */

#define PM_HANDLE2GLOBAL_PORT(handle) PM_GLOBAL_PORT_NUM_FROM_HANDLE( \
                                         PM_HANDLE2SLOT(handle),      \
                                         PM_HANDLE2MODULE(handle),    \
                                         PM_HANDLE2PORT(handle))

#define PM_IS_LPORT(handle)   ((handle >> 63) == 0)
#define PM_IS_SPORT(handle)   ((handle >> 63) == 1)
#define PM_IS_SVLAN(handle)   (PM_IS_LPORT(handle) & ((handle >> 43) & 0x1))

/* Generate PPP unit from a port handle */
#define PM_HANDLE2PPP(handle) \
    ((PM_HANDLE2SLOT(handle) << 27) | (PM_HANDLE2MODULE(handle) << 25) | \
     (PM_HANDLE2PORT(handle) << 17) | (PM_HANDLE2CHAN(handle) << 9))


#ifdef SWAP_GE_PORT_NUM
/*
 * WARNING: We must define a new GE LPORT type for new cards
 * that do not require a port number swap
 */
#define PM__LPORT_GIGE_LAST_PORT_NUM        9

extern int pm_port2hport_map[];
extern int pm_hport2port_map[];

#define PM_HANDLE2HPORT(_handle_)                                          \
    (                                                                      \
        (PM_HANDLE2LTYPE(_handle_) == PM_LPORT_GIGE)                       \
        ? pm_port2hport_map[PM_HANDLE2PORT(_handle_)]                      \
        : PM_HANDLE2PORT(_handle_)                                         \
    )

#define PM_PORT_2_HPORT(_port_)     pm_port2hport_map[_port_]

#define PM_HPORT_2_PORT(_hport_)    pm_hport2port_map[_hport_]

#else

#define PM_HANDLE2PORT_HW(_handle_) PM_HANDLE2PORT(_handle_)

#define PM_PORT_2_HPORT(_port_)     _port_

#define PM_HPORT_2_PORT(_hport_)    _hport_

#endif

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
#define SPORT_TYPE_MASK         0xF
#define SPORT_MSB_OFFSET        63

#define PM_HANDLE2LAG(handle)    ((handle >> SPORT_ID_OFFSET) & SPORT_ID_MASK)
#define PM_HANDLE2STYPE(handle)  ((handle >> SPORT_TYPE_OFFSET) & SPORT_TYPE_MASK)


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

#define PM_GET_SPORT_TYPE(handle)  ((handle >> SPORT_TYPE_OFFSET) & SPORT_TYPE_MASK)
#define PM_GET_SPORT_ID(handle)    ((handle >> SPORT_ID_OFFSET) & SPORT_ID_MASK)

/* Convert LAG id to port handle */
#define PM_LAG2HANDLE(lagid) PM_SPORT2HANDLE(PM_SPORT_LAG, lagid)
/* Convert MLPPP id to port handle */
#define PM_MLPPP2HANDLE(mlpppid) PM_SPORT2HANDLE(PM_SPORT_MLPPP, mlpppid)
/* Convert MPLS id to port handle */
#define PM_MPLS2HANDLE(mplsid) PM_SPORT2HANDLE(PM_SPORT_MPLS, mplsid)
/* Convert Martini id to port handle */
#define PM_MARTINI2HANDLE(martiniid) PM_SPORT2HANDLE(PM_SPORT_MARTINI, martiniid)

#define PM_IS_MARTINI_PORT(handle) \
    (PM_IS_SPORT(handle) && (PM_GET_SPORT_TYPE(handle) == PM_SPORT_MARTINI))

#define PM_IS_MPLS_PORT(handle) \
    (PM_IS_SPORT(handle) && (PM_GET_SPORT_TYPE(handle) == PM_SPORT_MPLS))

#define PM_IS_MPLS_PORT(handle) (PM_IS_SPORT(handle) && \
                                 (PM_GET_SPORT_TYPE(handle) == PM_SPORT_MPLS))

#define PM_IS_LAG(handle) (PM_IS_SPORT(handle) && \
                                 (PM_GET_SPORT_TYPE(handle) == PM_SPORT_LAG))

#define   PM_INVALID_VCID         (0)
#define   PM_MPLS_VCID_MIN        (4096)
#define   PM_IS_VCID_MPLS(vcid)    (vcid >= PM_MPLS_VCID_MIN)
#define   PM_IS_VCID_SVLAN(vcid)   ((vcid >0) && (vcid < PM_MPLS_VCID_MIN))
#define   PM_MPLS_2_HWVCID(handle) (PM_GET_SPORT_ID(handle) + PM_MPLS_VCID_MIN)
#define   PM_HWVCID_2_MPLS(vcid)   (PM_MPLS2HANDLE(((vcid)- PM_MPLS_VCID_MIN)))

/* If upper 32 bits of handle is non-zero then it should be a port handle */
#define PM_IS_PORT_HANDLE(handle) ((port_handle_t)handle >> 32)

/*
 * Port strings defined below
 */
#define PM_PHY_PORT_STR_GIG    "gigabitethernet"
#define PM_PHY_PORT_STR_FAE    "ethernet"
#define PM_PHY_PORT_STR_POS    "pos"
#define PM_PHY_PORT_STR_SER    "serial"

/*
 * Media module strings defined below
 * if the following enum is modified,
 *  please update struct nmm mod_info[]
 *  in libs/cli/port_mgt.c
 */
#define PM_MM_STR_12_PORT_GIG  "12-port-gig"
#define PM_MM_STR_4_PORT_POS   "4port-pos"
#ifdef STARSHIP
#define PM_MM_STR_10_PORT_GIG  "10-port-gig"
#define PM_MM_STR_1_PORT_10GIG "1-port-10gig"
#endif

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

#define PM_IS_COPPER_CARD(type) ((type == MODULE_GBE_DELC_E) || \
                                 (type == MODULE_GBE_DELC_S))

#define PM_IS_10G_CARD(type) \
    (((type) == MODULE_10G_XELC_E) ||\
     ((type) == MODULE_10G_XELC_S))

#define PM_IS_PHYSICAL(handle) (PM_IS_LPORT(handle) && !(PM_IS_SVLAN(handle)))

#define PM_IS_IP_NEMO_CPU(ip)  (((ip>>16) & 0xffff) == 0x7f01)
#define PM_INVALID_HW_NUM       (0xffff)

// Starts with 1, minus 1 before program to hw
#define PORT_AGGR_MODE_PORT_BASED   1
#define PORT_AGGR_MODE_MAC_BASED    2
#define PORT_AGGR_MODE_L3_BASED     3
#define PORT_AGGR_MODE_L4_BASED     4
#define PORT_AGGR_MODE_DEFAULT      PORT_AGGR_MODE_L3_BASED

#define PM_MAX_NUM_JUMBO_MTU_PORT   4
#define PM_MAX_NUM_PORT_PER_LAG     DRFT_FTE_MAX_MULTIPATHS
#define PM_MAX_NUM_MAC_PER_PORT     2

#endif  // __PM_CMN_H__
