/*
 * (c) Copyright 2015-2016 Hewlett Packard Enterprise Development LP
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

#ifndef _MVLAN_LACP_H_
#define _MVLAN_LACP_H_

#include <pm_cmn.h>

/******************************************************************************************/
/**                             System stuff...                                          **/
/******************************************************************************************/
#define MLm_lacp_api__setActorSysPriority       1
#define MLm_lacp_api__setActorSysMac            2
#define MLm_lacp_api__set_lport_overrides       3


struct MLt_lacp_api__actorSysPriority {
    int actor_system_priority;
};

struct MLt_lacp_api__actorSysMac {
    unsigned char actor_sys_mac[MAC_BYTEADDR_SIZE];
};

struct MLt_lacp_api__set_lport_overrides {
    int priority;
    unsigned char actor_sys_mac[MAC_BYTEADDR_SIZE];
    unsigned long long lport_handle;    /* set port overrides on interface */
};

/******************************************************************************************/
/**                              VPM API stuff..                                         **/
/******************************************************************************************/

// VPM message types
#define MLm_vpm_api__create_sport                     11
#define MLm_vpm_api__delete_sport                     12
#define MLm_vpm_api__get_sport                        13
#define MLm_vpm_api__lport_state_up                   14
#define MLm_vpm_api__lport_state_down                 15
#define MLm_vpm_api__set_lacp_sport_params            16
#define MLm_vpm_api__unset_lacp_sport_params          17
#define MLm_vpm_api__set_lacp_lport_params_event      18
#define MLm_vpm_api__set_lport_fallback_status        19

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
    char partner_system_id[MAC_BYTEADDR_SIZE];        // The mac addressS
    int  aggr_type;                   // individual or aggregateable
    int  actor_max_port_priority;     // Max actor port priority in this sport
    int  partner_max_port_priority;   // Max partner port priority in this sport
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
    char sys_id[MAC_BYTEADDR_SIZE];
};

struct MLt_vpm_api__lport_state_change {
    unsigned long long sport_handle;
    unsigned long long lport_handle;
    unsigned long      lport_flags;
    int                link_speed;
};

struct MLt_vpm_api__lport_fallback_status {
    unsigned long long lport_handle;   // The lport to be updated
    int status;                        // Fallback new status
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
    char partner_system_id[MAC_BYTEADDR_SIZE];         // The mac addressS
    int  local_port_number;            // LAG's local_port_number : should
                                       // match aggr_type of aggregator
    u_short actor_oper_port_priority;  // Actor port priority
    u_short partner_oper_port_priority;// Partner port priority
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
    char partner_mac_addr[MAC_BYTEADDR_SIZE];          // Partner mac address
    int  partner_priority;             // The partner priority
    int  cookie;                       // Used by the caller to store
                                       // info and will be sent back
                                       // to the caller un-modified
    int  error;                        // The error code of the operation
};

/*********************************************************************
 * Structure defining the sport LACP information.
 *********************************************************************/
typedef struct lacp_sport_params_s {
    int      flags;                     /* see lacp_cmn.h */
    int      port_type;
    int      actor_key;                 /* A value between 1- 65535 */
    int      partner_key;               /* A value between 1- 65535 */
    int      partner_system_priority;   /* A value between 1- 65535 */
    char     partner_system_id[MAC_BYTEADDR_SIZE];      /* The mac address */
    int      aggr_type;                 /* whether aggregateable or indiv */
    int      actor_max_port_priority;   /* Priority of the actor port with higher priority */
    int      partner_max_port_priority; /* Priority of the partner port with higher priority */
} lacp_sport_params_t;

/*********************************************************************
 *  Internal representation of the LACP info.
 *********************************************************************/
typedef struct lacp_int_sport_params_s {
    lacp_sport_params_t  lacp_params;   /* Should be the first field in this struct */
    void                *psport;        /* Pointer to the super port */
} lacp_int_sport_params_t;


extern int mvlan_api_modify_sport_params(struct  MLt_vpm_api__lacp_sport_params *placp_params,
                                         int operations);
extern int mvlan_api_validate_unset_sport_params(struct MLt_vpm_api__lacp_sport_params *placp_params);
extern int mvlan_api_validate_set_sport_params(struct MLt_vpm_api__lacp_sport_params *placp_params);
extern int mvlan_set_sport_params(struct MLt_vpm_api__lacp_sport_params *pin_lacp_params);
extern int mvlan_unset_sport_params(struct MLt_vpm_api__lacp_sport_params *pin_lacp_params);

// Called to clear out LAG information when all ports are really detached.
// This essentially clears the LAG and put it back to the "free" pool
extern int mvlan_api_clear_sport_params(unsigned long long sport_handle);

extern int mvlan_api_select_aggregator(struct MLt_vpm_api__lacp_match_params *placp_match_params);
extern int mvlan_api_attach_lport_to_aggregator(struct MLt_vpm_api__lacp_attach *placp_attach_params);
extern int mvlan_api_detach_lport_from_aggregator(struct MLt_vpm_api__lacp_attach *placp_detach_params);

extern int ml_send_event(ML_event* event);
extern ML_event* ml_wait_for_next_event(void);
extern void ml_event_free(ML_event* event);

// LACPDU send function
extern int mlacp_send(unsigned char* data, int length, port_handle_t portHandle);

#endif /* _MVLAN_LACP_H_ */
