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
//  File           : stp_peers.h
//  Description    : This file contains the definition of the protocol
//                   messages exchanged between the master STP task and
//                   the helper STP daemons on the line card CPUs.
//
//**************************************************************************

//%protocol stp/peers

/*
  struct ML_version MLv_stp_peers[] = 
  {
    { 1, 0 },
  };
*/

enum MLm_stp_peers {
    MLm_stp_peers__hello = 0,              //% MLt_stp_peers__hello
    MLm_stp_peers__byebye = 1,             //% MLt_stp_peers__byebye
    MLm_stp_peers__txPdu = 2,              //% MLt_stp_peers__txPdu
    MLm_stp_peers__rxPdu = 3,              //% MLt_stp_peers__rxPdu
    MLm_stp_peers__setVstpState = 4,       //% MLt_stp_peers__vStpState
};

struct MLt_stp_peers__hello {
    int cpuNum;
};

struct MLt_stp_peers__byebye {
    int cpuNum;
};

struct MLt_stp_peers__txPdu {
    int slotNum;
    int portNum;
    int pktLen; char *data;
};

struct MLt_stp_peers__rxPdu {
    int slotNum;
    int portNum;
    int pktLen; char *data;
};

struct MLt_stp_peers__vStpState {
     unsigned long long sportHandle;
     int vstpIndex;
     int state;
};
