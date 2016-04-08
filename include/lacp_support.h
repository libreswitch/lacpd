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

#ifndef _LACP_SUPPORT_H_
#define _LACP_SUPPORT_H_

#include "lacp.h"

/*****************************************************************************
 *   MISC. MACROS (to be placed in appropriate h files later)
 *****************************************************************************/

#define FAST_PERIODIC_COUNT             1                          /* 1 second */
#define SLOW_PERIODIC_COUNT             30                         /* 30 seconds */
#define SHORT_TIMEOUT_COUNT             (3 * FAST_PERIODIC_COUNT)  /* 3 seconds */
#define LONG_TIMEOUT_COUNT              (3 * SLOW_PERIODIC_COUNT)  /* 90 seconds */
#define AGGREGATE_WAIT_COUNT            2                          /* 2 seconds */

#define STATE_STRING_SIZE               32

#define STATE_FLAGS_SIZE                9

#define MAX_ASYNC_TX                    3

/*****************************************************************************
 *      DEFAULT VALUES
 *****************************************************************************/
#define DEFAULT_PARTNER_PORT_NUMBER             0x0
#define DEFAULT_PARTNER_ADMIN_PORT_KEY          0x0
#define DEFAULT_PARTNER_ADMIN_PORT_PRIORITY     0x0
#define DEFAULT_PARTNER_ADMIN_SYSTEM_PRIORITY   0x0

#define DEFAULT_COLLECTOR_MAX_DELAY             1

/*****************************************************************************
 *      BIT FIELDS FOR PER PORT VARIABLES
 *****************************************************************************/
#define PORT_NUMBER_BIT                         0x1
#define PORT_PRIORITY_BIT                       0x2
#define PORT_KEY_BIT                            0x4
#define PORT_STATE_LACP_ACTIVITY_BIT            0x8
#define PORT_STATE_LACP_TIMEOUT_BIT             0x10
#define PORT_STATE_AGGREGATION_BIT              0x20
#define PORT_SYSTEM_MAC_ADDR_BIT                0x800
#define PORT_SYSTEM_PRIORITY_BIT                0x1000
#define ALL_PARAMS                              0xFFFF

#define NO_SYSTEM_ID "0,00:00:00:00:00:00"

/*****************************************************************************/
/*                   Prototypes for Global routines                          */
/*****************************************************************************/
extern void LACP_mux_fsm(int, int, lacp_per_port_variables_t *);
extern void LACP_periodic_tx_fsm(int, int, lacp_per_port_variables_t *);
extern void LACP_receive_fsm(int, int, lacpdu_payload_t *,
                             lacp_per_port_variables_t *);
extern void LACP_transmit_lacpdu(lacp_per_port_variables_t *);
extern void LACP_process_lacpdu(struct lacp_per_port_variables *,
                                void *);
extern void LACP_initialize_port(port_handle_t lport_handle,
                                 unsigned short port_id,
                                 unsigned long flags,
                                 short port_key,
                                 short port_priority,
                                 short activity,
                                 short timeout,
                                 short aggregation,
                                 int link_state,
                                 int link_speed,
                                 int hw_collecting,
                                 short sys_priority,
                                 char *sys_id);
extern void LACP_update_port_params(port_handle_t lport_handle,
                                    unsigned long flags,
                                    short data, int hw_collecting);
extern int port_number_2_link_group_index(int);
extern void LAG_selection(lacp_per_port_variables_t *);
extern void LAG_id_string(char *const, LAG_Id_t *const);
extern int loop_back_check(lacp_per_port_variables_t *);
extern void print_lacp_fsm_state(port_handle_t);
extern void set_actor_admin_parms_2_oper(lacp_per_port_variables_t *, int);
extern void set_partner_admin_parms_2_oper(lacp_per_port_variables_t *, int);
extern void start_wait_while_timer(lacp_per_port_variables_t *);
extern void periodic_tx_state_string(int, char *);
extern void mux_state_string(int, char *);
extern void rx_state_string(int, char *);
extern int lacp_lock(void);
extern void lacp_unlock(int);
extern void LACP_async_transmit_lacpdu(lacp_per_port_variables_t *);
extern void LACP_sync_transmit_lacpdu(lacp_per_port_variables_t *);
extern void display_lacpdu(lacpdu_payload_t *,char *, char *, int);
extern void LAG_detach_aggregator(LAG_t *const);
extern void LACP_poll_link_state(lacp_per_port_variables_t *);
extern void LACPPutPortUp(ulong);
extern void set_all_port_system_priority(void);
extern void set_lport_fallback_status(port_handle_t, int);
extern void set_all_port_system_mac_addr(void);
extern void set_lport_overrides(port_handle_t, int, unsigned char *);

extern void lacp_support_diag_dump(int port);

/*****************************************************************************
 *       Extern declarations for global variables
 *****************************************************************************/
extern unsigned char my_mac_addr[];
extern uint actor_system_priority;
extern lacp_avl_tree_t lacp_per_port_vars_tree;
extern const unsigned char lacp_mcast_addr[];
extern const unsigned char default_partner_system_mac[];
extern int lacp_tables_last_changed_time;
extern LAG_t *lacp_lags[];
extern unsigned int lacp_cli_opt_mask[];

#endif  /* _LACP_SUPPORT_H_ */
