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
//                   messages exchanged between the master STP task and
//                   the CLI/Show manager task on the mcpu.
//**************************************************************************

#ifndef __h_NEMO_PROTOCOL_STP_API_H__
#define __h_NEMO_PROTOCOL_STP_API_H__

#include <nemo/protocol/include/stp.h>

enum MLm_stp_api {
    MLm_stp_api__setProtocolVersion = 0,    //% MLt_stp_api__protocolVersion
    MLm_stp_api__getProtocolVersion = 1,    //% MLt_stp_api__protocolVersion

    MLm_stp_api__setBridgePriority = 2,     //% MLt_stp_api__bridgeInfo
    MLm_stp_api__setBridgeHelloTime = 3,    //% MLt_stp_api__bridgeInfo
    MLm_stp_api__setBridgeMaxAge = 4,       //% MLt_stp_api__bridgeInfo
    MLm_stp_api__setBridgeFwdDelay = 5,     //% MLt_stp_api__bridgeInfo

    MLm_stp_api__getBridgeInfo = 6,         //% MLt_stp_api__bridgeInfo

    MLm_stp_api__setPortCost = 7,           //% MLt_stp_api__portInfo
    MLm_stp_api__setPortPriority = 8,       //% MLt_stp_api__portInfo

    MLm_stp_api__getPortInfo = 9,           //% MLt_stp_api__portInfo
    MLm_stp_api__getNextPortInfo = 10,       //% MLt_stp_api__portInfo

    MLm_stp_api__setDebugLevel = 13,        //% MLt_stp_api__debugLevel
    MLm_stp_api__unsetDebugLevel = 14,        //% MLt_stp_api__debugLevel
    MLm_stp_api__getDebugLevel = 15,        //% MLt_stp_api__debugLevel
    MLm_stp_api__setDebugOnSport = 16,      //% MLt_stp_api__debugOnSport
    MLm_stp_api__unsetDebugOnSport = 17,      //% MLt_stp_api__debugOnSport
    MLm_stp_api__showSportsBeingDebugged = 18, //% MLt_stp_api__sportsBeingDebugged
    MLm_stp_api__restarted_send_config = 19,   //% MLt_stp_api__restarted_send_config
    MLm_stp_api__flush_arp = 20,               //% MLt_stp_api__flush_arp
    MLm_stp_api__get_port_stp_info = 21,    //% MLt_stp_api__sport_stp_info
};

struct MLt_stp_api__protocolVersion {
    int error;
    int stp_version;
};

struct MLt_stp_api__bridgeInfo {
    int mvst_instance;
    char my_bridge_id[8];
    char root_bridge_id[8];
    int priority;  // figures again, for the sake of Set CLI ...
    int am_i_root;
    int root_port_id;
    unsigned long long root_port_handle;
    int port_cost;
    int total_ports;
    int max_age;
    int hello_time;
    int fwd_delay;
    int bridge_max_age;    // for SNMP
    int bridge_hello_time; // for SNMP
    int bridge_fwd_delay;  // for SNMP
    int hold_time;         // for SNMP
    int num_topo_changes;
    int last_topo_change_time;
    int error;
};

struct MLt_stp_api__portInfo {
    char desig_bridge[8];
    int mvst_instance;
    int is_stp_enabled;
    int state;
    int path_cost; // port_cost XXX ??
    unsigned long long port_handle;
    int port_id;
    int priority;
    int port_role;
    int desig_port_id;
    int desig_port_pri;
    char desig_root[8]; // for SNMP
    int desig_cost;     // for SNMP
    int cnt_fwd_transitions;     // for SNMP
    int tx_cnt;
    int rx_cnt;
    int error; // for GetNext
};

struct MLt_stp_api__ProtocolVersion {
    char version[10];
};

struct MLt_stp_api__vlanAging {
    int slotNum;
    int portNum;
    int vlanId;
    int timeout;
};

struct MLt_stp_api__debugLevel {
    int error;
    unsigned int debug_level;
};

struct MLt_stp_api__debugOnSport {
    int error;
    int mvst_inst;
    unsigned long long sport_handle;
    unsigned int debug_level;
};

struct MLt_stp_api__sportsBeingDebugged {
    int error;
    int mvst_inst;
    int numSports;
    struct MLt_include_stp__debug *sportsArray;
};

// This message is used for task restart.
// When the 'PM' task dies and restarts,
// it sends this message to 'configd'.
// When 'configd' gets this message, it
// re-executes all commands associated with
// 'PM' task.
struct MLt_stp_api__restarted_send_config {
    int moduleNameLen;
    char *moduleName;
};

struct MLt_stp_api__flush_arp {
    int num_sports;
    unsigned long long *sport_handles;
    int mvst_instance;
};

struct MLt_stp_api__port_fsm_log {
    unsigned short seq_no;
    int  state;
    int  event;
    char fname[50];
};

struct MLt_stp_api__sport_stp_info {
    // following two are input flags
    int                     mvst_instance;
    unsigned long long      handle;
    int                     status;

    char                    port_name[20];
    unsigned short          port_id;
    int                     state;
    unsigned long           path_cost;
    unsigned char           designated_root[8];
    int                     designated_cost;
    unsigned char           designated_bridge[8];
    unsigned short          designated_port;
    unsigned char           topology_change_ack;
    unsigned char           stp_enable_flag;
    int                     state_set_by_mgmt;
    int                     link_state;
    int                     phy_state;
    unsigned long           normal_age_time;
    unsigned char           status_flag;
    unsigned long           no_of_forward_transitions;
    unsigned long           no_of_bpdus_sent;
    unsigned long           no_of_bpdus_rcvd;

    // FAST STP RELATED INFO
    unsigned short          prt_state;
    unsigned short          operEdgePort;
    unsigned short          role;
    unsigned short          learn;
    unsigned short          forward;
    unsigned short          info;
    unsigned short          rcvdInfo;
    unsigned short          rcvdTcn;
    unsigned short          rcvdTc;
    unsigned short          rcvdTcAck;
    unsigned short          rcvdDi;
    unsigned short          rcvdDc;
    unsigned short          txmtDi;
    unsigned short          txmtDc;
    unsigned short          txmtInfo;
    unsigned short          txmtTcn;
    unsigned short          txmtTc;
    unsigned short          txmtTcAck;
    unsigned short          rcvdNew;
    unsigned short          rcvdOld;
    unsigned short          sendNew;

    unsigned short          msg_age_timer;
    unsigned short          fwd_delay_timer;
    unsigned short          hold_timer;
    int                     stp_age_timer;

    // FAST STP TIMERS
    unsigned short          helloWhen;
    unsigned short          tcWhile;
    unsigned short          fdWhile;
    unsigned short          infoAge;
    unsigned short          rrWhile;
    unsigned short          rbWhile;
    unsigned short          msyncWhile;

    int num_logs;
    struct MLt_stp_api__port_fsm_log *log;
};

#endif  // __h_NEMO_PROTOCOL_STP_API_H__
