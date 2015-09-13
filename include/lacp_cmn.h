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

//*********************************************************************
// File : lacp_cmn.h
// This file has the structures and #defines accessed by various modules.
//*********************************************************************
#ifndef __LACP_CMN_H__
#define __LACP_CMN_H__

#define    LACP_MIN_KEY_VAL        (1)
#define    LACP_MAX_KEY_VAL        (65535)

// Halon - default actor key for all intra-closet LAGs
#define    ICL_ACTOR_PRIORITY      1
#define    ICL_ACTOR_KEY           1
#define    ICL_ACTOR_PRIORITY_STR  "1"
#define    ICL_ACTOR_KEY_STR       "1"

//*****************************************************************
// These are flags that indicate whether the user specified these
// or not. If the user did not specify one of these in a particular
// command, we should not overwrite the previously existing  values.
//*****************************************************************
#define     LACP_LPORT_ACTIVITY_FIELD_PRESENT      (0x01)
#define     LACP_LPORT_TIMEOUT_FIELD_PRESENT       (0x02)
#define     LACP_LPORT_AGGREGATION_FIELD_PRESENT   (0x04)

#define     LACP_LPORT_SYS_PRIORITY_FIELD_PRESENT  (0x10)
#define     LACP_LPORT_SYS_ID_FIELD_PRESENT        (0x20)

#define     LACP_LPORT_PORT_KEY_PRESENT            (0x100)
#define     LACP_LPORT_PORT_PRIORITY_PRESENT       (0x200)
#define     LACP_LPORT_LACP_ENABLE_FIELD_PRESENT   (0x400)
#define     LACP_LPORT_HW_COLL_STATUS_PRESENT      (0x800)

#define     LACP_LPORT_DYNAMIC_FIELDS_PRESENT      (0x1000)

//*****************************************************************
// These are the actual values for these parameters.
//*****************************************************************
// activity bit
#define LACP_PASSIVE_MODE 0
#define LACP_ACTIVE_MODE 1

// timeout bit
#define LONG_TIMEOUT  0
#define SHORT_TIMEOUT 1

// Aggregation state/bit
#define INDIVIDUAL 0
#define AGGREGATABLE 1
#define UNKNOWN 2

// Enable bit
#define LACP_STATE_DISABLED 0
#define LACP_STATE_ENABLED  1

#define DEFAULT_PORT_KEY_GIGE    (1) // from lacp_support.h
#define DEFAULT_PORT_PRIORITY    (1) // from lacp_support.h
#define DEFAULT_SYSTEM_PRIORITY  (1) // from lacp_support.h

//*****************************************************************
// Default States.
//*****************************************************************
#define LACP_PORT_STATE_DEFAULT       (LACP_PORT_STATE_DISABLED)
#define LACP_PORT_KEY_DEFAULT         (DEFAULT_PORT_KEY_GIGE)
#define LACP_PORT_PRIORITY_DEFAULT    (DEFAULT_PORT_PRIORITY)
#define LACP_PORT_ACTIVITY_DEFAULT    (LACP_ACTIVE_MODE)
#define LACP_PORT_TIMEOUT_DEFAULT     (SHORT_TIMEOUT)
#define LACP_PORT_AGGREGATION_DEFAULT (AGGREGATABLE)

//*****************************************************************
// These are the flags used while setting the sport_parameters,
// i.e. LAG/aggregator/"smarttrunk" parameters.
//*****************************************************************
#define LACP_LAG_PORT_TYPE_FIELD_PRESENT      (0x01)
#define LACP_LAG_ACTOR_KEY_FIELD_PRESENT      (0x02)
#define LACP_LAG_PARTNER_KEY_FIELD_PRESENT    (0x04)
#define LACP_LAG_PARTNER_SYSPRI_FIELD_PRESENT (0x08)
#define LACP_LAG_PARTNER_SYSID_FIELD_PRESENT  (0x10)
#define LACP_LAG_AGGRTYPE_FIELD_PRESENT       (0x20)

//*****************************************************************
// These are the values for these LAG parameters.
//*****************************************************************
#define LACP_LAG_AGGRTYPE_INDIVIDUAL   0
#define LACP_LAG_AGGRTYPE_AGGREGATABLE 1

#define LACP_LAG_PORTTYPE_FASTETHER   (1)
#define LACP_LAG_PORTTYPE_GIGAETHER   (2)
#define LACP_LAG_PORTTYPE_10GIGAETHER (3)

//*****************************************************************
// These are the defaults for these LAG parameters.
//*****************************************************************
#define LACP_LAG_DEFAULT_PORT_TYPE      (LACP_LAG_PORTTYPE_GIGAETHER)
#define LACP_LAG_DEFAULT_ACTOR_KEY      (1)
#define LACP_LAG_DEFAULT_PARTNER_KEY    (1)
#define LACP_LAG_DEFAULT_PARTNER_SYSPRI (1)
#define LACP_LAG_DEFAULT_AGGR_TYPE      (LACP_LAG_AGGRTYPE_AGGREGATABLE)
#define LACP_LAG_INVALID_ACTOR_KEY      (0)

#define LACP_PKT_SIZE (124) // Excluding CRC
#define LACP_HEADROOM_SIZE (14)  // 6 + 6 + 2

// The following enum must be same with one in mlacp_debug.h
enum {
    CLI_CAT_RESOURCE = 1,
    CLI_CAT_CRITICAL,
    CLI_CAT_SERIOUS,
    CLI_CAT_ABNORMAL,
    CLI_CAT_INFO,
    CLI_CAT_RX_FSM,
    CLI_CAT_TX_FSM,
    CLI_CAT_MUX_FSM,
    CLI_CAT_LAG_SELECT,
    CLI_CAT_TX_BPDU,
    CLI_CAT_RX_BPDU,
    CLI_CAT_ENTRY_EXIT_TIMERS,
};

#endif  // __LACP_CMN_H__
