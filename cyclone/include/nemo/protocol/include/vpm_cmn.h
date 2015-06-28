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

#ifndef __h_NEMO_PROTOCOL_INCLUDE_VPM_CMN_H__
#define __h_NEMO_PROTOCOL_INCLUDE_VPM_CMN_H__

struct MLt_include_vpm_cmn__vlan_id {
    short vid; // vlan 1Q id
    short cid; // customer id of this vlan belongs to
               // 0 means traditional vlans with no specific customer
};

enum MLet_include_vpm_cmn__mvst_operation {
    MLed_include_vpm_cmn__mvst_vlan_added = 0,
    MLed_include_vpm_cmn__mvst_vlan_deleted = 1,
    MLed_include_vpm_cmn__mvst_mvst_enabled = 2,
    MLed_include_vpm_cmn__mvst_mvst_disabled = 3,
    MLed_include_vpm_cmn__mvst_state_forwarding = 4,
    MLed_include_vpm_cmn__mvst_state_blocking = 5,
    MLed_include_vpm_cmn__mvst_state_learning = 6,
};

//
// MLed_include_vpm_cmn__mvst_vlan_added
// MLed_include_vpm_cmn__mvst_vlan_deleted
//    mvst_instance, valid
//    sport_handles, affected sports
//    mvst_stp_states, corresponding mvst stp states (should be ignored for
//    delete
//    vlan_handles, 1 handle that is getting added or deleted
// MLed_include_vpm_cmn__mvst_mvst_enabled
// MLed_include_vpm_cmn__mvst_mvst_disabled
//    mvst_instance, valid
//    sport_handles, 1 handle on which mvst is enabled or disabled
//    vlan_handles, list of handles that will be impacted by enable/disable of
//    mvst on the given sport handle
//    mvst_stp_states, 1 state and refers to stp state for the instance
// MLed_include_vpm_cmn__mvst_state_forwarding
// MLed_include_vpm_cmn__mvst_state_blocking
// MLed_include_vpm_cmn__mvst_state_learning
//    mvst_instance, valid
//    sport_handles, port for which state is getting updated
//    vlan_handles, list of vlan handles that are getting impacted on the port
//    mvst_stp_states, 1 state and refers to stp state for the instance
struct MLt_include_vpm_cmn__mvst_operation {
    int  mvst_instance;
    enum MLet_include_vpm_cmn__mvst_operation operation;
    int  sport_cnt;
    unsigned long long *sport_handles;
    int  vlan_cnt;
    unsigned long *vlan_handles;
    int  state_cnt;
    unsigned int *mvst_stp_states;
};

struct MLt_include_vpm_cmn__vlan_age {
   struct MLt_include_vpm_cmn__vlan_id vlanid;
   unsigned int agetimer;
   unsigned int flags;
   int error;
};

struct MLt_include_vpm_cmn__vlan_state_info {
   struct MLt_include_vpm_cmn__vlan_id vlan_id;
   int error;
   unsigned int link_state;
   unsigned int admin_state;
};

#endif  // __h_NEMO_PROTOCOL_INCLUDE_VPM_CMN_H__
