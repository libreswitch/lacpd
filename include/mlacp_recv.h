/*
 * (c) Copyright 2015 Hewlett Packard Enterprise Development LP
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

#ifndef __MLACP_RECV_H__
#define __MLACP_RECV_H__


enum MLm_drivers_mlacp {
    MLm_drivers_mlacp__rxPdu = 0,   //% MLt_drivers_mlacp__rxPdu
};

struct MLt_drivers_mlacp__rxPdu {
    unsigned long long lport_handle;
    int  pktLen;
    char data[LACP_PKT_SIZE];
};

#endif  /* __MLACP_RECV_H__ */
