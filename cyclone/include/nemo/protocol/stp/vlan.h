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
//  File           : stp_vlan.h
//  Description    : This file contains the definition of the protocol
//                   messages exchanged between the master STP task and
//                   the VLAN manager task on the mcpu.
//                   Note that due to the extensive validations etc to be
//                   performed when STP is enabled on a port, CLI talks to
//                   VLAN manager directly and STP process gets the config
//                   messages via the VLAN/SmartTrunk manager.
// XXX XXX XXX Thile file ought to be in vlan XXX XXX XXX
//**************************************************************************

#ifndef __h_NEMO_PROTOCOL_STP_VLAN_H__
#define __h_NEMO_PROTOCOL_STP_VLAN_H__

enum MLm_stp_vlan {
    MLm_stp_vlan__enable = 0,              //% MLt_stp_vlan__enable
    MLm_stp_vlan__makeRoot = 1,            //% MLt_stp_vlan__makeRoot
    MLm_stp_vlan__bridgeId = 2,            //% MLt_stp_vlan__bridgeId
    MLm_stp_vlan__portPri = 3,             //% MLt_stp_vlan__portPri
    MLm_stp_vlan__portCost = 4,            //% MLt_stp_vlan__portCost
};

struct MLt_stp_vlan__enable {
    int slotNum;
    int slotType;
    int numPorts;
    unsigned long long *portHandles;
};

struct MLt_stp_vlan__makeRoot {
    int xx;
};

struct MLt_stp_vlan__bridgeId {
    int id;
};

struct MLt_stp_vlan__portPri {
    int port;
    int pri;
};

struct MLt_stp_vlan__portCost {
    int port;
    int cost;
};

#endif  // __h_NEMO_PROTOCOL_STP_VLAN_H__
