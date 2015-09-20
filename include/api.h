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

struct MLt_lacp_api__actorSysPriority {
    int actor_system_priority;
};

#endif  // __h_NEMO_PROTOCOL_LACP_API_H__
