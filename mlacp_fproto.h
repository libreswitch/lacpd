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
//    File               : mlacp_fproto.h
//    Description        : mcpu LACP function prototypes
//**************************************************************************

#ifndef _MLACP_FPROTO_H
#define _MLACP_FPROTO_H

#include "lacp_halon.h"

//***************************************************************
// Functions in mlacp_main.c
//***************************************************************
void mlacpBoltonRxPdu(struct ML_event *);
void register_mcast_addr(port_handle_t lport_handle);
void deregister_mcast_addr(port_handle_t lport_handle);

//***************************************************************
// Functions in mlacp.c
//***************************************************************
int  mlacp_init(u_long);
void mlacp_process_timer(struct MLt_msglib__timer *tevent);
void timerHandler(void);

//***************************************************************
// Functions in mlacp_send.c
//***************************************************************
int mlacp_blocking_send_select_aggregator(LAG_t *const lag, lacp_per_port_variables_t *lacp_port);
int mlacp_blocking_send_attach_aggregator(lacp_per_port_variables_t *lacp_port);
int mlacp_blocking_send_detach_aggregator(lacp_per_port_variables_t *lacp_port);
int mlacp_blocking_send_enable_collecting(lacp_per_port_variables_t *lacp_port);
int mlacp_blocking_send_enable_distributing(lacp_per_port_variables_t *lacp_port);
int mlacp_blocking_send_disable_collect_dist(lacp_per_port_variables_t *lacp_port);

// Halon: New function to clear super port.
int mlacp_blocking_send_clear_aggregator(unsigned long long sport_handle);

//***************************************************************
// Functions in mlacp related portion of FSM files
//***************************************************************
void mlacp_task_init(void);
void mlacp_process_rxPdu(port_handle_t sport_handle, unsigned char *data);

//***************************************************************
// Functions in mlacp_task.c
//***************************************************************

int lacp_lag_port_match(void *v1, void *v2);
//***************************************************************
// Functions in mlacp_recv.c
//***************************************************************
void mlacp_process_vlan_msg(struct ML_event *);
void mlacp_process_api_msg(struct ML_event *);
void mlacp_process_showmgr_msg(struct ML_event *);
void mlacp_process_diagmgr_msg(struct ML_event *);

void mlacpVapiLportEvent(struct ML_event *pevent);


//***************************************************************
// Functions in mlacp_klib.c
//***************************************************************
int mLacpLibInit(void);

//***************************************************************
// Functions in lacp_support.c
//***************************************************************
void mlacpVapiLinkDown(port_handle_t lport_handle);
void mlacpVapiLinkUp(port_handle_t lport_handle, int speed);
void LACP_disable_lacp(port_handle_t lport_handle);
void mlacpVapiSportParamsChange(int msg,
                    struct MLt_vpm_api__lacp_sport_params *pin_lacp_params);
//***************************************************************
// Functions in selection.c 
//***************************************************************
void LAG_attached_to_aggregator(port_handle_t lport_handle,int result);


//***************************************************************
// Functions in mux_fsm.c 
//***************************************************************
void stop_wait_while_timer(lacp_per_port_variables_t *plpinfo);

#endif //_MLACP_FPROTO_H
