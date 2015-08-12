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

//**************************************************************************
//  File           : api.h
//  Description    : This file contains the definition of the protocol
//                   messages exchanged between the master LACP task and
//                   the CLI/Show manager task on the mcpu.
//**************************************************************************

#ifndef __h_NEMO_PROTOCOL_LACP_API_H__
#define __h_NEMO_PROTOCOL_LACP_API_H__

#include <nemo/protocol/include/lacp.h>

enum MLm_lacp_api {
    MLm_lacp_api__getLportParams = 0,         //% MLt_lacp_api__lport_params
    MLm_lacp_api__getnextLportParams = 1,     //% MLt_lacp_api__lport_params
    MLm_lacp_api__getLportProtocol = 2,       //% MLt_lacp_api__lport_protocol
    MLm_lacp_api__getnextLportProtocol = 3,   //% MLt_lacp_api__lport_protocol
    MLm_lacp_api__getLportStats = 4,          //% MLt_lacp_api__lport_stats
    MLm_lacp_api__getnextLportStats = 5,      //% MLt_lacp_api__lport_stats
    MLm_lacp_api__setDebugLevel = 6,          //% MLt_lacp_api__debugLevel
    MLm_lacp_api__unsetDebugLevel = 7,        //% MLt_lacp_api__debugLevel
    MLm_lacp_api__getDebugLevel = 8,          //% MLt_lacp_api__debugLevel
    MLm_lacp_api__setDebugOnLport = 9,        //% MLt_lacp_api__debugOnLport
    MLm_lacp_api__unsetDebugOnLport = 10,      //% MLt_lacp_api__debugOnLport
    MLm_lacp_api__showLportsBeingDebugged = 11, //% MLt_lacp_api__lportsBeingDebugged
    MLm_lacp_api__setActorSysPriority = 12,    //% MLt_lacp_api__actorSysPriority
    MLm_lacp_api__getActorSysPriority = 13,    //% MLt_lacp_api__actorSysPriority
    MLm_lacp_api__getLportInfo = 14,           //% MLt_lacp_api__lport_info
    MLm_lacp_api__getLagTuples = 15,           //% MLt_lacp_api__lag_tuple_info
    MLm_lacp_api__getKeyGroups = 16,           //% MLt_lacp_api__key_group_info
    MLm_lacp_api__restarted_send_config = 17,  //% MLt_lacp_api__restarted_send_config
    MLm_lacp_api__clearLportStats = 18,        //% MLt_lacp_api__lport_stats
    MLm_lacp_api__clearnextLportStats = 19,    //% MLt_lacp_api__lport_stats
    MLm_lacp_api__test_attach_detach = 20,     //% MLt_lacp_api__test_attach_detach
};

struct MLt_lacp_api__lport_params {
    unsigned long long lport_handle;
    int error;

    int  actor_system_priority;
    char actor_system_mac[6];
    int  actor_admin_port_key;
    int  actor_oper_port_key;
    int  actor_admin_port_number;
    int  actor_oper_port_number;
    int  actor_admin_port_priority;
    int  actor_oper_port_priority;
    int  actor_activity;
    int  actor_timeout;
    int  actor_aggregation;
    int  actor_synchronization;
    int  actor_collecting;
    int  actor_distributing;
    int  actor_defaulted;
    int  actor_expired;

    int  partner_system_priority;
    char partner_system_mac[6];
    int  partner_admin_key;
    int  partner_oper_key;
    int  partner_oper_port_number;
    int  partner_oper_port_priority;
    int  partner_activity;
    int  partner_timeout;
    int  partner_aggregation;
    int  partner_synchronization;
    int  partner_collecting;
    int  partner_distributing;
    int  partner_defaulted;
    int  partner_expired;

    int  sys_priority;
    char sys_id[6];
};

struct MLt_lacp_api__lport_protocol {
    unsigned long long lport_handle;
    unsigned long long sport_handle;// relevant if attached to Aggregator
    int  error;
    int  recv_fsm_state;
    int  mux_fsm_state;
    int  periodic_tx_fsm_state;
    int  lacp_up;
    int  control_begin;
    int  control_ready_n;
    int  control_selected;
    int  control_port_moved;
    int  control_ntt;
    int  control_port_enabled;
    int  partner_sync;
    int  partner_collecting;
    int  periodic_tx_timer_expiry_counter;
    int  current_while_timer_expiry_counter;
    int  wait_while_timer_expiry_counter;
};

struct MLt_lacp_api__lport_stats {
    unsigned long long lport_handle;
    int error;

    int lacp_pdus_sent;
    int marker_response_pdus_sent;
    int lacp_pdus_received;
    int marker_pdus_received;
};

struct MLt_lacp_api__debugLevel {
    int error;
    int debug_level;
};

struct MLt_lacp_api__debugOnLport {
    int error;
    unsigned long long lport_handle;
    int debug_level;
};

struct MLt_lacp_api__lportsBeingDebugged {
    int error;
    int numLports;
    struct MLt_include_lacp__debug *lportsArray;
};

struct MLt_lacp_api__actorSysPriority {
    int actor_system_priority;
};

struct MLt_lacp_api__lport_info {
   unsigned long long local_lport;
   int  remote_lport;
   char remote_mac[6];
   int  link_state;
   int  local_port_key;
   int  remote_port_key;
   int  error;
};

struct MLt_lacp_api__lag_tuple_info {
   int error;
   int numLags;
    struct MLt_include_lacp__lag_tuple *lagTuple;
};

struct MLt_lacp_api__key_group_info {
   int error;
   int numKeys;
    struct MLt_include_lacp__key_group *keyGroup;
};

// This message is used for task restart.
// When the 'LACP' task dies and restarts = ,
// it sends this message to 'configd'.
// When 'configd' gets this message, it
// re-executes all commands associated with
// 'LACP' task.
struct MLt_lacp_api__restarted_send_config {
    int moduleNameLen;
    char *moduleName;
};

struct MLt_lacp_api__test_attach_detach {
    unsigned long long lport_handle;
    unsigned long long sport_handle;
    int num_events;
    int event_mask;
    int delay;
    int error;
};

#endif  // __h_NEMO_PROTOCOL_LACP_API_H__
