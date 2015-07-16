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

/*----------------------------------------------------------------------
 *  MODULE:
 *
 *     lacp.h
 *
 *  SUB-SYSTEM:
 *
 *  ABSTRACT
 *     The file contains basic data stuctures for the LACPD.
 *
 *  AUTHOR:
 *
 *    Gowrishankar, Riverstone Networks
 *
 *  CREATION DATE:
 *
 *    March 5, 2000
 *
 *
 *----------------------------------------------------------------------*/

#ifndef _LACP_H_
#define _LACP_H_

#include <sys/types.h>
#include <nemo/avl.h>
#include <nemo/nemo_types.h>

/* HALON_TODO: Define h/w here for now.  This needs to be passed in
 * as build or environment variable based on CPU h/w architecture
 * (Big- vs. Little-Endian).
 */
#define __X86_HARDWARE__

/*****************************************************************************
 *                   MISC. MACROS
 *****************************************************************************/
#define MAC_ADDR_LENGTH 6
#define UNSELECTED 0
#define SELECTED 1
#define STANDBY 2

/*****************************************************************************
 *                   MACROS REQD FOR LACP
 *****************************************************************************/
#define LACP_SUBTYPE 0x01
#define LACP_VERSION 0x01
#define LACP_TLV_TERMINATOR_INFO 0x0
#define LACP_TLV_ACTOR_INFO 0x01
#define LACP_TLV_PARTNER_INFO 0x02
#define LACP_TLV_COLLECTOR_INFO 0x03
#define LACP_TLV_INFO_LENGTH 0x14
#define LACP_TLV_COLLECTOR_INFO_LENGTH 0x10
#define LACP_TLV_TERMINATOR_INFO_LENGTH 0x0
#define LACP_ETYPE 0x8809

/*****************************************************************************
 *                   MACROS REQD FOR MARKER PROTOCOL
 *****************************************************************************/
#define TERMINATOR_TLV_TYPE 0x0
#define TERMINATOR_LENGTH 0x0
#define MARKER_SUBTYPE 0x02
#define MARKER_VERSION 0x01
#define MARKER_TLV_TYPE 0x02
#define MARKER_TLV_INFO_LENGTH 0x10

/*****************************************************************************
 * LAG Id structure.
 *****************************************************************************/
typedef struct LAG_Id {

    int local_system_priority;
    macaddr_3_t local_system_mac_addr;
    int local_port_key;
    int local_port_priority;
    int local_port_number;

    int remote_system_priority;
    macaddr_3_t remote_system_mac_addr;
    int remote_port_key;
    int remote_port_priority;
    int remote_port_number;

} LAG_Id_t;

/*****************************************************************************
 * port list in the LAG structure : each nlist element in the LAG structure
 * is of this type.
 *****************************************************************************/
typedef struct lacp_lag_ppstruct {

    port_handle_t lport_handle;

} lacp_lag_ppstruct_t;

/*****************************************************************************
 *  Link Aggregation Group (LAG) structure.
 *****************************************************************************/
typedef struct LAG {

    enum PM_lport_type port_type;
    LAG_Id_t *LAG_Id;
    int ready;
    int loop_back;
    void *pplist; /* nlist of ports, of type lacp_lag_ppstruct */

    /* Halon: save the sport handle. */
    unsigned long long sp_handle;

} LAG_t;

/********************************************************************
 * Data structure containing the state paramter bit fields.
 ********************************************************************/
typedef struct state_parameters {
#ifdef __X86_HARDWARE__
    /* Halon: OCP h/w is Little-Endian. */
    u_char lacp_activity:1,
           lacp_timeout:1,
           aggregation:1,
           synchronization:1,
           collecting:1,
           distributing:1,
           defaulted:1,
           expired:1;
#else
    /* This structure is for Big-Endian. */
    u_char expired:1,
           defaulted:1,
           distributing:1,
           collecting:1,
           synchronization:1,
           aggregation:1,
           lacp_timeout:1,
           lacp_activity:1;
#endif
} state_parameters_t;

/********************************************************************
 * Data structure containing the system variables.
 ********************************************************************/
typedef struct system_variables {

    macaddr_3_t system_mac_addr;
    u_int system_priority;

} system_variables_t;


#pragma pack(push,1)

/********************************************************************
 * Data structure for the LACPDU payload, We use packed structures
 * to make sure no extra padding is added by the compiler.
 ********************************************************************/
typedef struct lacpdu_payload {

    u_char headroom[LACP_HEADROOM_SIZE];
    u_char subtype;
    u_char version_number;
    u_char tlv_type_actor;
    u_char actor_info_length;
    u_short actor_system_priority;
    macaddr_3_t actor_system;
    u_short actor_key;
    u_short actor_port_priority;
    u_short actor_port;
    state_parameters_t actor_state;
    u_char reserved1[3];
    u_char tlv_type_partner;
    u_char partner_info_length;
    u_short partner_system_priority;
    macaddr_3_t partner_system;
    u_short partner_key;
    u_short partner_port_priority;
    u_short partner_port;
    state_parameters_t partner_state;
    u_char reserved2[3];
    u_char tlv_type_collector;
    u_char collector_info_length;
    u_short collector_max_delay;
    u_char reserved3[12];
    u_char tlv_type_terminator;
    u_char terminator_length;
    u_char reserved4[50];

} lacpdu_payload_t;

/********************************************************************
 * Data structure for the Marker PDU payload. We make all the fields
 * packed to make sure the compiler doesn't add any extra padding for
 * alignment.
 ********************************************************************/
typedef struct marker_pdu_payload {

    u_char headroom[LACP_HEADROOM_SIZE];
    u_char subtype;
    u_char version_number;
    u_char tlv_type_marker;
    u_char marker_info_length;
    u_short requester_port;
    macaddr_3_t requester_system;
    u_int requester_transaction_id;
    u_short pad;
    u_char tlv_type_terminator;
    u_char terminator_length;
    u_char reserved[90];

} marker_pdu_payload_t;

#pragma pack(pop)


/********************************************************************
 * Data structure for the state machine control variables.
 ********************************************************************/
typedef struct lacp_control_variables {

    int begin;
    int actor_churn;
    int partner_churn;
    int ready_n;
    int selected;
    int port_moved;
    int ntt;
    int port_enabled;

} lacp_control_variables_t;

/********************************************************************
 * Data structure containing the per port variables.
 ********************************************************************/
typedef struct lacp_per_port_variables {

    /********************************************************************
     *              Actor variables
     ********************************************************************/
    u_short actor_admin_port_number;
    u_short actor_oper_port_number;
    u_short actor_admin_port_priority;
    u_short actor_oper_port_priority;
    u_short actor_admin_port_key;
    u_short actor_oper_port_key;
    state_parameters_t actor_admin_port_state;
    state_parameters_t actor_oper_port_state;
    system_variables_t actor_admin_system_variables;
    system_variables_t actor_oper_system_variables;

    /********************************************************************
     *  Partner's variables
     ********************************************************************/
    u_short partner_admin_port_number;
    u_short partner_oper_port_number;
    u_short partner_admin_port_priority;
    u_short partner_oper_port_priority;
    u_short partner_admin_key;
    u_short partner_oper_key;
    state_parameters_t partner_admin_port_state;
    state_parameters_t partner_oper_port_state;
    system_variables_t partner_admin_system_variables;
    system_variables_t partner_oper_system_variables;

    /********************************************************************
     *  LACP fsm control variables
     ********************************************************************/
    lacp_control_variables_t lacp_control;

    /********************************************************************
     *  LACP fsm state variables
     ********************************************************************/
    u_int recv_fsm_state;
    u_int mux_fsm_state;
    u_int periodic_tx_fsm_state;

    /* Added to avoid sending lport_attach on the way back from
     * Collecting_Distributing.
     */
    u_int prev_mux_fsm_state;

    /* Halon - Indicates if the port is part of the LAG bitmap in DB.
     * LACPD set this flag to true after attaching the port to LAG.
     */
    int hw_attached_to_mux;

    /* HALON_TODO: update comment on how hw_collecting indicator works. */
    int hw_collecting;

    /********************************************************************
     *  Timer counters
     ********************************************************************/
    int periodic_tx_timer_expiry_counter;
    int current_while_timer_expiry_counter;
    int wait_while_timer_expiry_counter;
    int async_tx_count;

    /********************************************************************
     *  LACP statistics
     ********************************************************************/
    u_int lacp_pdus_sent;
    u_int marker_response_pdus_sent;
    u_int lacp_pdus_received;
    u_int marker_pdus_received;

    /********************************************************************
     *  Debug variables
     ********************************************************************/
    int rx_machine_debug;
    int periodic_tx_machine_debug;
    int mux_machine_debug;
    int tx_lacpdu_display;
    int rx_lacpdu_display;

    /********************************************************************
     *  Misc. variables
     ********************************************************************/
    u_short collector_max_delay;
    u_int aggregation_state;
    int selecting_lag;  /* LAG_selection() in progress */
    int lacp_up;

    /********************************************************************
     *  AVL tree related variables
     ********************************************************************/
    enum PM_lport_type port_type;
    port_handle_t lport_handle;
    nemo_avl_node_t avlnode;
    LAG_t *lag;
    port_handle_t sport_handle; /* The aggregator handle */
    int debug_level;

} lacp_per_port_variables_t;

extern  u_int actor_system_priority;

#endif /* _LACP_H_ */
