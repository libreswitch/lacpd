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

#ifndef _h_NEMO_PROTOCOL_INCLUDE_LACP_H
#define _h_NEMO_PROTOCOL_INCLUDE_LACP_H 1

struct MLt_include_lacp__debug {
    unsigned long long lport_handle;
    int debug_level;
};

struct MLt_include_lacp__lag_tuple {
  unsigned long long sport_handle; 
  int local_system_priority;
  char local_system_mac_addr[6];
  int local_port_key;
  int local_port_priority;
  int local_port_number;
 
  int remote_system_priority;
  char remote_system_mac_addr[6];
  int remote_port_key;
  int remote_port_priority; 
  int remote_port_number;

  int numPorts; unsigned long long *lport_handles;
};

struct MLt_include_lacp__key_group {
  int key_val;
  int numPorts; unsigned long long *lport_handles;
};

#endif  // _h_NEMO_PROTOCOL_INCLUDE_LACP_H
