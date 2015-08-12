/*
 * Copyright (C) 2015 Hewlett-Packard Development Company, L.P.
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

/*
 * lacp_halon.h
 *   HALON_TODO:
 *   This is a temporary include file to house all defines needed by
 *   Cyclone LACP code that we may or may not pull in from the rest
 *   of Cyclone source base.
 */
#ifndef __LACP_HALON_H__
#define __LACP_HALON_H__

#include <semaphore.h>

#include "nemo/pm/pm_cmn.h"
#include "nemo/halon_cmn.h"


/******************************************************************************************/
/**                             System stuff...                                          **/
/******************************************************************************************/
#define MLm_lacp_api__setActorSysMac  100
#define MLm_lacp_api__set_sport_ActorSysMac  101
#define MLm_lacp_api__clear_sport_ActorSysMac  102
#define MLm_lacp_api__set_sport_ActorSysPriority  103
#define MLm_lacp_api__clear_sport_ActorSysPriority  104

struct MLt_lacp_api__actorSysMac {
    unsigned char actor_sys_mac[6];
};

struct MLt_lacp_api__sport_actorSysMac {
    unsigned char actor_sys_mac[6];
    unsigned long long sport_handle;    /* extension - set sys mac by sport */
};

struct MLt_lacp_api__sport_actorSysPriority {
    int priority;
    unsigned long long sport_handle;    /* extension - set sys prio by sport */
};

/******************************************************************************************/
/**                              VPM API stuff..                                         **/
/******************************************************************************************/

// VPM message types
#define MLm_vpm_api__create_sport                      78
#define MLm_vpm_api__delete_sport                      79
#define MLm_vpm_api__get_sport                         80
#define MLm_vpm_api__get_next_sport                    81
#define MLm_vpm_api__lport_state_up                    93
#define MLm_vpm_api__lport_state_down                  94
#define MLm_vpm_api__set_lacp_sport_params             111
#define MLm_vpm_api__unset_lacp_sport_params           112
#define MLm_vpm_api__get_lacp_sport_params             113
#define MLm_vpm_api__getnext_lacp_sport_params         114
#define MLm_vpm_api__lacp_attach_reply                 132
#define MLm_vpm_api__set_lacp_lport_params_event       135
#define MLm_vpm_api__get_lacp_sport_connections        157
#define MLm_vpm_api__getnext_lacp_sport_connections    158

struct MLt_vpm_api__static_lag {
    unsigned long long sport_handle;  // The handle of smart trunk super port
    unsigned long long lport_handle;  // The handle of logical port to attach/detach
    int cookie;                       // Used by the caller to store
                                      // info and will be sent back
                                      // to the caller un-modified
    int error;                        // The error code of the operation
};

struct MLt_vpm_api__create_sport {
    short type;                       //  The type of super port
    unsigned long long handle;        // The handle of smart trunk super port
                                      // if the create succeeds will
                                      // be returned to the caller
    int cookie;                       // Used by the caller to store
                                      // info and will be sent back
                                      // to the caller un-modified
    int error;                        // The error code of the operation
};

struct MLt_vpm_api__delete_sport {
    unsigned long long handle;        // The handle of smart trunk super port
    int cookie;                       // Used by the caller to store
                                      // info and will be sent back
                                      // to the caller un-modified
    int error;                        // The error code of the operation
};

// This structure is used to set the lacp parameters of a superport.
struct MLt_vpm_api__lacp_sport_params {
    unsigned long long sport_handle;
    int  flags;                       // see lacp_cmn.h
    int  port_type;                   // The type of ports i.e. 10/100, gig etc
                                      // defined in pm_cmn.h
    int  actor_key;                   // A value between 1- 65535
    int  partner_key;                 // A value between 1- 65535
    int  partner_system_priority;     // A value between 1- 65535
    char partner_system_id[6];        // The mac addressS
    int  aggr_type;                   // individual or aggregateable
    int  negation;                    // whether it's negation : unset cmd is
                                      // used only while negating
    int  cookie;                      // Used by the caller to store
                                      // info and will be sent back
                                      // to the caller un-modified
    int  error;                       // The error code of the operation
};

struct MLt_vpm_api__lport_lacp_change {
    unsigned long long lport_handle;
    int port_id;
    int flags;
    int lacp_state;
    int port_key;
    int port_priority;
    int lacp_activity;
    int lacp_timeout;
    int lacp_aggregation;
    int link_state;
    int link_speed;
    int collecting_ready;
    int sys_priority;
    char sys_id[6];
};

struct MLt_vpm_api__lport_state_change {
    unsigned long long sport_handle;
    unsigned long long lport_handle;
    unsigned long      lport_flags;
    int                link_speed;
};

// The message give by the LACP module to match the
// given logical port to a corresponding aggregator.
struct MLt_vpm_api__lacp_match_params {
    unsigned long long lport_handle;   // The lport whose  parameters are to be
                                       // matched of smart trunk
    int  flags;                        // see lacp_cmn.h
    int  port_type;                    // The type of ports i.e. 10/100, gig etc
                                       // defined in pm_cmn.h
    int  actor_key;                    // A value between 1- 65535
    int  partner_key;                  // A value between 1- 65535
    int  partner_system_priority;      // A value between 1- 65535
    char partner_system_id[6];         // The mac addressS
    int  local_port_number;            // LAG's local_port_number : should
                                       // match aggr_type of aggregator
    int  actor_aggr_type;              // Individual or aggregatable
    int  partner_aggr_type;            // Individual or aggregatable
    unsigned long long sport_handle;   // will be returned if match is successful,
    int  cookie;                       // Used by the caller to store
                                       // info and will be sent back
                                       // to the caller un-modified
    int  error;                        // The error code of the operation
};

// The message given by the LACP module to attach
// a given logical port to an aggregator.
struct  MLt_vpm_api__lacp_attach  {
    unsigned long long lport_handle;   // The lport to be added
    unsigned long long sport_handle;   // The sport to which to attach/detach
    char partner_mac_addr[6];          // Partner mac address
    int  partner_priority;             // The partner priority
    int  cookie;                       // Used by the caller to store
                                       // info and will be sent back
                                       // to the caller un-modified
    int  error;                        // The error code of the operation
};

// This structure is used to set the LACP parameters of a logical port.
struct MLt_vpm_api__lacp_lport_params {
    unsigned long long lport_handle;
    int flags;                         // flags field to indicate subfields
    int lacp_state;                    // Enabled or Disabled
    int port_key;                      // The port key from 1-65535
    int port_priority;                 // The port priority from 1- 65535
    int lacp_activity;                 // activity : passive/active
    int lacp_timeout;                  // timeout : long/short
    int lacp_aggregation;              // aggregation : indiv/agg
    int cookie;                        // Used by the caller to store
                                       // info and will be sent back
                                       // to the caller un-modified
    int error;                         // The error code of the operation
};

struct MLt_vpm_api__lacp_sport_connections {
    unsigned long long local_sport;
    int num_lports;
    unsigned long long *lport_array;
    int num_lports2;
    int *lport_oper_state_array;
    int error;
};

/******************************************************************************************/
/**                               Halon ADAPTATION                                       **/
/******************************************************************************************/
extern sem_t lacpd_init_sem;
extern int lacpd_shutdown;

extern int ml_send_event(ML_event* event);
extern ML_event* ml_wait_for_next_event(void);
extern void ml_event_free(ML_event* event);

// LACPDU send function
extern int mlacp_send(unsigned char* data, int length, port_handle_t portHandle);

#endif /*__LACP_HALON_H__*/
