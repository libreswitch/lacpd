/*
 * Copyright (C) 2005-2015 Hewlett-Packard Development Company, L.P.
 * All Rights Reserved.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License"); you may
 *   not use this file except in compliance with the License. You may obtain
 *   a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *   WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *   License for the specific language governing permissions and limitations
 *   under the License.
 */

#ifndef __VLAN_CMN_H__
#define __VLAN_CMN_H__

/*****************************************************************************
   File               : vlan_cmn.h
   Description        : The file  contains the common defines that will be
                        referenced outside of the vlan module.
*****************************************************************************/

#define IMPLCIT_VLAN_IMPLEMENTED 1

#include <nemo/protocol/include/vpm_cmn.h>

#define  VLAN_MAX_NAME_SIZE   (20)
#define  SPORT_MAX_NAME_SIZE  (20)

#define VLAN_MIN_ID                        (0)
#define VLAN_MAX_ID                        (4094)
#define VLAN_CONTROL                       (00)
#define VLAN_DEFAULT                       (01)
#define VLAN_BLACKHOLE                     (4095)
#define VLAN_TRANSPARENT                   (0x7000)
#define VLAN_GET_AVAIL_ID                  (-1)
#define VLAN_IMPLICIT_HW_ID                (4095)

#ifdef IMPLCIT_VLAN_IMPLEMENTED
#define IMPLICIT_VLAN_MIN_ID               (4096)
#define IMPLICIT_VLAN_MAX_ID               (8191)
#define IS_IMPLICIT_VLAN(vid)  (vid >= IMPLICIT_VLAN_MIN_ID && \
                                vid <= IMPLICIT_VLAN_MAX_ID)
#define VLAN_ID_SW2HW(vid) ((IS_IMPLICIT_VLAN(vid))?(VLAN_IMPLICIT_HW_ID): \
                                                    (vid))
#ifdef STARSHIP
#define IS_HW_VLAN_IMPLICIT  IS_IMPLICIT_VLAN
#else
#define IS_HW_VLAN_IMPLICIT(vid)  (vid == VLAN_IMPLICIT_HW_ID)
#endif
#else
#define IMPLICIT_VLAN_MIN_ID               (0)
#define IMPLICIT_VLAN_MAX_ID               (4095)
#define VLAN_ID_SW2HW(vid) (vid)
#endif

/************************************************************/
/* VLAN Status                                              */
/************************************************************/
#define VLAN_STATUS_ACTIVE 0
#define VLAN_STATUS_SUSPENDED 1

/* Global Customer ID for all non-customer specific vlans */
#define VLAN_GLOBAL_CID (00)


#define STYPE_REGULAR         (0x1)     /*  Super port has exactly one
                                            logical port */
#define STYPE_MLPPP           (0x2)     /*  Is a type of WAN port and
                                            has multiple logical ports */
#define STYPE_802_3AD         (0x4)     /*  Is of type IEEE 803.3ad and has
                                            multiple logical ports */
#define STYPE_TRUNK           (0x8)     /*  Is set to be a smart trunk */

#define     MVLAN_STP_VERSION_OSTP            (0x01)
#define     MVLAN_STP_VERSION_RSTP            (0x02)

#define     MVLAN_STP_TYPE_1VST               (0x03) // just so that no overlap
#define     MVLAN_STP_TYPE_MVST               (0x04)

#define     MVLAN_STP_DEFAULT_BRIDGE_PRIORITY   (32768)
#define     MVLAN_STP_DEFAULT_MAX_AGE           (20)
#define     MVLAN_STP_DEFAULT_FORWARD_DELAY     (15)
#define     MVLAN_STP_DEFAULT_HELLO_TIME        (2)

#define     MVLAN_NUM_VSTP_ENTRIES_PER_FLIPPER (4096)

#define VLAN_CUSTOMER_ID_2_HANDLE(vid, cid) ((1<<31) | (vid << 16) | cid)
#define VLAN_ID_2_HANDLE(vlan_id) (VLAN_CUSTOMER_ID_2_HANDLE(vlan_id.vid, vlan_id.cid))
#define VLAN_HANDLE_2_VID(handle) ((handle & 0x7fff0000) >> 16)
#define VLAN_HANDLE_2_CID(handle) (handle & 0x0000ffff)

/* Sport operational state definitions. */
#define SPORT_ALLOW_BRIDGED     (1 << 0)
#define SPORT_ALLOW_ROUTED_IP   (1 << 1)
#define SPORT_ALLOW_ROUTED_IPV6 (1 << 2)
#define SPORT_ALLOW_MPLS        (1 << 3)
#define SPORT_ALLOW_OSI         (1 << 4)

#define SPORT_ALLOW_ALL (SPORT_ALLOW_BRIDGED | \
                         SPORT_ALLOW_ROUTED_IP | \
                         SPORT_ALLOW_ROUTED_IPV6 | \
                         SPORT_ALLOW_MPLS | \
                         SPORT_ALLOW_OSI)

/* VLAN operational state definitions. These bits *must* be an exact copy
 * of the SPORT_ bits defined above.
 */
#define VLAN_ALLOW_BRIDGED      (1 << 0)
#define VLAN_ALLOW_ROUTED_IP    (1 << 1)
#define VLAN_ALLOW_ROUTED_IPV6  (1 << 2)
#define VLAN_ALLOW_MPLS         (1 << 3)
#define VLAN_ALLOW_OSI          (1 << 4)

#define VLAN_ALLOW_ALL (VLAN_ALLOW_BRIDGED | \
                        VLAN_ALLOW_ROUTED_IP | \
                        VLAN_ALLOW_ROUTED_IPV6 | \
                        VLAN_ALLOW_MPLS | \
                        VLAN_ALLOW_OSI)

typedef struct MLt_include_vpm_cmn__vlan_id vlan_id_t;

extern unsigned char vstpBitMap[];

/* mpls martini related defines: ref by cli and mvpm */
#define MPLS_MARTINI_INITIAL_REQUEST 0x1
#define MPLS_MARTINI_NOMORE_PORTS    0x2
#define MPLS_MARTINI_NUM_PORTS_TO_GET 1

#define MPLS_MARTINI_PORT_DST_FLAG           0x01
#define MPLS_MARTINI_PORT_VLAN_FLAG          0x02
#define MPLS_MARTINI_PORT_ETHER_FLAG         0x04
#define MPLS_MARTINI_PORT_VCID_FLAG          0x08
#define MPLS_MARTINI_TRANSPORT_TYPE_FLAG     0x10
#define MPLS_MARTINI_REGISTRATION_DONE_FLAG  0x20
#define MPLS_MARTINI_PORT_TYPE_FLAG          0x100
#define MPLS_MARTINI_ILM_PROG_SENT_FLAG      0x200
/*
 * Set when all labels and tunnel information is available
 */
#define MPLS_MARTINI_USEABLE                 0x400
/*
 * Set when mpls requests a change of epti info
 */
#define MPLS_MARTINI_UPDATE_REQUIRED         0x800
#define MPLS_MARTINI_DEST_ADDR_FLAG          0x1000

#define MPLS_MARTINI_VCTYPE_ETHERNET_VLAN  4
#define MPLS_MARTINI_VCTYPE_ETHERNET       5

enum TRANSPORT_tnnl_type {
    TRANSPORT_RSVPTE = 1,
    TRANSPORT_LDP,
    TRANSPORT_ANY,
    TRANSPORT_NONE,
};

#define SPORT_STP_ENABLED         (0x01)
#define SPORT_STP_BLOCKING        (0x02)
#define SPORT_STP_FORWARDING      (0x04)
#define SPORT_STP_LEARNING        (0x08)
#define MVST_STP_FORWARDING       (0x10)
#define MVST_STP_BLOCKING         (0x20)
#define MVST_STP_ENABLED          (0x40)
#define MVST_STP_LEARNING         (0x80)

#define DEFAULT_DOT1Q_ETHERTYPE       0x8100
#define DEFAULT_SVLAN_DOT1Q_ETHERTYPE 0x9100
#define DOT1Q_ETHERTYPE_PRESENT       0x1
#define SVLAN_DOT1Q_ETHERTYPE_PRESENT 0x2

#endif /* __VLAN_CMN_H__ */
