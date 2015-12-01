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

#ifndef _MLACP_FPROTO_H
#define _MLACP_FPROTO_H

#include "mvlan_lacp.h"

//***************************************************************
// Variables in lacpd.c
//***************************************************************
extern bool exiting;

//***************************************************************
// Functions in mlacp_main.c
//***************************************************************
extern void *mlacp_rx_pdu_thread(void *data  __attribute__ ((unused)));
extern void register_mcast_addr(port_handle_t lport_handle);
extern void deregister_mcast_addr(port_handle_t lport_handle);
extern int mlacp_tx_pdu(unsigned char* data, int length, port_handle_t lport_handle);
extern void *lacpd_protocol_thread(void *arg  __attribute__ ((unused)));
extern int mlacp_init(u_long);

//***************************************************************
// Functions in mlacp_send.c
//***************************************************************
extern  int mlacp_blocking_send_select_aggregator(LAG_t *const lag,
                                                  lacp_per_port_variables_t *lacp_port);
extern  int mlacp_blocking_send_attach_aggregator(lacp_per_port_variables_t *lacp_port);
extern  int mlacp_blocking_send_detach_aggregator(lacp_per_port_variables_t *lacp_port);
extern  int mlacp_blocking_send_enable_collecting(lacp_per_port_variables_t *lacp_port);
extern  int mlacp_blocking_send_enable_distributing(lacp_per_port_variables_t *lacp_port);
extern  int mlacp_blocking_send_disable_collect_dist(lacp_per_port_variables_t *lacp_port);

// New function to clear super port.
extern int mlacp_blocking_send_clear_aggregator(unsigned long long sport_handle);

//***************************************************************
// Functions in mlacp related portion of FSM files
//***************************************************************
extern void mlacp_task_init(void);
extern void mlacp_process_rxPdu(port_handle_t sport_handle, unsigned char *data);

//***************************************************************
// Functions in lacp_task.c
//***************************************************************
extern void LACP_periodic_tx(void);
extern void LACP_current_while_expiry(void);
extern int lacp_lag_port_match(void *v1, void *v2);
extern void LACP_process_input_pkt(port_handle_t lport_handle, unsigned char * data, int len);

//***************************************************************
// Functions in mlacp_recv.c
//***************************************************************
extern void mlacp_process_rx_pdu(struct ML_event *);
extern void mlacp_process_vlan_msg(struct ML_event *);
extern void mlacp_process_api_msg(struct ML_event *);
extern void mlacp_process_timer(void);
extern void mlacpVapiLportEvent(struct ML_event *pevent);

//***************************************************************
// Functions in lacp_support.c
//***************************************************************
extern void mlacpVapiLinkDown(port_handle_t lport_handle);
extern void mlacpVapiLinkUp(port_handle_t lport_handle, int speed);
extern void LACP_disable_lacp(port_handle_t lport_handle);
extern void mlacpVapiSportParamsChange(int msg,
                                       struct MLt_vpm_api__lacp_sport_params *pin_lacp_params);


#endif //_MLACP_FPROTO_H
