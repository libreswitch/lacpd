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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <avl.h>
#include <pm_cmn.h>
#include <lacp_cmn.h>
#include <mlacp_debug.h>
#include <lacp_fsm.h>

#include "lacp.h"
#include "lacp_stubs.h"
#include "lacp_support.h"
#include "mlacp_fproto.h"
#include "mvlan_lacp.h"
#include "lacp_ops_if.h"

VLOG_DEFINE_THIS_MODULE(periodic_tx_fsm);

/****************************************************************************
 *   Static Variables
 ****************************************************************************/

/* preiodic tx machine fsm table */
static FSM_ENTRY periodic_tx_machine_fsm_table[PERIODIC_TX_FSM_NUM_INPUTS]
                                              [PERIODIC_TX_FSM_NUM_STATES] =
{
/*****************************************************************************
 *   Input Event E1 - Begin = True
 *****************************************************************************/
  {{PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC},  // Begin state
   {PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC},  // No Periodic
   {PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC},  // Fast Periodic
   {PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC},  // Slow Periodic
   {PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC}}, // Periodic Tx

/*****************************************************************************
 * Input Event E2 - UCT
 *****************************************************************************/
  {{PERIODIC_TX_FSM_RETAIN_STATE,           NO_ACTION},  // Begin state
   {PERIODIC_TX_FSM_FAST_PERIODIC_STATE,    ACTION_FAST_PERIODIC},
   {PERIODIC_TX_FSM_RETAIN_STATE,           NO_ACTION},  // Fast Periodic
   {PERIODIC_TX_FSM_RETAIN_STATE,           NO_ACTION},  // Slow Periodic
   {PERIODIC_TX_FSM_RETAIN_STATE,           NO_ACTION}}, // Periodic Tx

/*****************************************************************************
 * Input Event E3 - periodic timer expired
 *****************************************************************************/
  {{PERIODIC_TX_FSM_RETAIN_STATE,           NO_ACTION},  // Begin state
   {PERIODIC_TX_FSM_RETAIN_STATE,           NO_ACTION},  // No Periodic
   {PERIODIC_TX_FSM_PERIODIC_TX_STATE,      ACTION_PERIODIC_TX},
   {PERIODIC_TX_FSM_PERIODIC_TX_STATE,      ACTION_PERIODIC_TX},
   {PERIODIC_TX_FSM_RETAIN_STATE,           NO_ACTION}}, // Periodic Tx

/*****************************************************************************
 * Input Event E4 - Partner_Oper_Port_State.LACP_Timeout = Long Timeout
 *****************************************************************************/
  {{PERIODIC_TX_FSM_RETAIN_STATE,           NO_ACTION},  // Begin state
   {PERIODIC_TX_FSM_RETAIN_STATE,           NO_ACTION},  // No Periodic
   {PERIODIC_TX_FSM_SLOW_PERIODIC_STATE,    ACTION_SLOW_PERIODIC},
   {PERIODIC_TX_FSM_RETAIN_STATE,           NO_ACTION},  // Slow Periodic
   {PERIODIC_TX_FSM_SLOW_PERIODIC_STATE,    ACTION_SLOW_PERIODIC}},

/*****************************************************************************
 *   Input Event E5 - LACP_Enabled = FALSE
 *****************************************************************************/
  {{PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC},  // Begin state
   {PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC},  // No Periodic
   {PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC},  // Fast Periodic
   {PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC},  // Slow Periodic
   {PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC}}, // Periodic Tx

/*****************************************************************************
 * Input Event E6 - Partner_Oper_Port_State.LACP_Timeout = Short Timeout
 *****************************************************************************/
  {{PERIODIC_TX_FSM_RETAIN_STATE,           NO_ACTION},  // Begin state
   {PERIODIC_TX_FSM_RETAIN_STATE,           NO_ACTION},  // No Periodic
   {PERIODIC_TX_FSM_RETAIN_STATE,           NO_ACTION},  // Fast Periodic
   {PERIODIC_TX_FSM_PERIODIC_TX_STATE,      ACTION_PERIODIC_TX},
   {PERIODIC_TX_FSM_FAST_PERIODIC_STATE,    ACTION_FAST_PERIODIC}},

/*****************************************************************************
 * Input Event E7 - port_enabled = FALSE
 *****************************************************************************/
  {{PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC},  // Begin state
   {PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC},  // No Periodic
   {PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC},  // Fast Periodic
   {PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC},  // Slow Periodic
   {PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC}}, // Periodic Tx

/*****************************************************************************
 * Input Event E8 -  (Actor_Oper_Port_State.LACP_Activity = Passive AND
 *                    Actor_Oper_Port_state.LACP_Activity = Passive)
 *****************************************************************************/
  {{PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC},  // Begin state
   {PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC},  // No Periodic
   {PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC},  // Fast Periodic
   {PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC},  // Slow Periodic
   {PERIODIC_TX_FSM_NO_PERIODIC_STATE,   ACTION_NO_PERIODIC}}, // Periodic Tx
};

/****************************************************************************
 *             Prototypes for static functions
 ****************************************************************************/
static void LACP_no_periodic_state_action(lacp_per_port_variables_t *);
static void LACP_fast_periodic_state_action(lacp_per_port_variables_t *);
static void LACP_slow_periodic_state_action(lacp_per_port_variables_t *);
static void LACP_periodic_tx_state_action(lacp_per_port_variables_t *);
static lacpdu_payload_t *LACP_build_lacpdu_payload(lacp_per_port_variables_t *);

/*----------------------------------------------------------------------
 * Function: LACP_periodic_tx_fsm(event, current_state, port_number)
 * Synopsis: Entry routine for periodic tx state machine.
 * Input  :
 *           event = the event that occured
 *           current_state = the current state of the fsm
 *           port_number = port number on which to act upon.
 * Returns:  void
 *----------------------------------------------------------------------*/
void
LACP_periodic_tx_fsm(int event,
                     int current_state,
                     lacp_per_port_variables_t *plpinfo)
{
    int action;
    char previous_state_string[STATE_STRING_SIZE];
    char current_state_string[STATE_STRING_SIZE];

    RENTRY();

    // Get the action routine and the next state to transition to.
    GET_FSM_TABLE_CELL_CONTENTS(periodic_tx_machine_fsm_table,
                                event,
                                current_state,
                                action);

    // Update the state only if required so.
    if (current_state != PERIODIC_TX_FSM_RETAIN_STATE) {

        if (plpinfo->debug_level & DBG_TX_FSM) {
            //***********************************************************
            // receive_fsm  debug is DBG_RX_FSM
            // periodic_fsm debug is DBG_TX_FSM
            // mux_fsm      debug is DBG_MUX_FSM
            // selection    debug is DBG_SELECTION
            //***********************************************************
            periodic_tx_state_string(plpinfo->periodic_tx_fsm_state,
                                     previous_state_string);
            periodic_tx_state_string(current_state, current_state_string);
            RDBG("%s : transitioning from %s to %s, action %d "
                 "(lport 0x%llx)\n",
                 __FUNCTION__,
                 previous_state_string,
                 current_state_string,
                 action,
                 plpinfo->lport_handle);
        }

        plpinfo->periodic_tx_fsm_state = current_state;

    } else {
        if (plpinfo->debug_level & DBG_TX_FSM) {
            RDBG("%s : retain old state (%d)\n",
                 __FUNCTION__, plpinfo->periodic_tx_fsm_state);
        }
    }

    // Call the appropriate action routine.
    switch (action) {
        case ACTION_NO_PERIODIC:
        {
            LACP_no_periodic_state_action(plpinfo);
        }
        break;

        case ACTION_FAST_PERIODIC:
        {
            LACP_fast_periodic_state_action(plpinfo);
        }
        break;

        case ACTION_SLOW_PERIODIC:
        {
            LACP_slow_periodic_state_action(plpinfo);
        }
        break;

        case ACTION_PERIODIC_TX:
        {
            LACP_periodic_tx_state_action(plpinfo);
        }
        break;

        default:
        break;
    }

    db_update_interface(plpinfo);

    REXIT();
} // LACP_periodic_tx_fsm

/*----------------------------------------------------------------------
 * Function: LACP_no_periodic_state_action(int port_number)
 * Synopsis: Function implementing no periodic state
 * Input  :
 *           port_number = port number on which to act upon.
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
LACP_no_periodic_state_action(lacp_per_port_variables_t *plpinfo)
{
    if (plpinfo->debug_level & DBG_TX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n",
             __FUNCTION__, plpinfo->lport_handle);
    }

    plpinfo->lacp_control.begin = FALSE;

    // Put the port in NO PERIODIC state.
    plpinfo->periodic_tx_fsm_state = PERIODIC_TX_FSM_NO_PERIODIC_STATE;

    // Reset the expiry counter.
    plpinfo->periodic_tx_timer_expiry_counter = 0;

    if ((plpinfo->lacp_control.port_enabled == FALSE) ||
        ((plpinfo->actor_oper_port_state.lacp_activity == LACP_PASSIVE_MODE) &&
         (plpinfo->partner_oper_port_state.lacp_activity == LACP_PASSIVE_MODE))) {
        // Do nothing and stay in the NO PERIODIC state.
        return;
    }

    // UCT to FAST_PERIODIC state.
    LACP_periodic_tx_fsm(E2, plpinfo->periodic_tx_fsm_state, plpinfo);

    if (plpinfo->debug_level & DBG_TX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // LACP_no_periodic_state_action

/*----------------------------------------------------------------------
 * Function: LACP_fast_periodic_state_action(int port_number)
 * Synopsis: Function implementing fast periodic state
 * Input  :
 *           port_number = port number on which to act upon.
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
LACP_fast_periodic_state_action(lacp_per_port_variables_t *plpinfo)
{
    if (plpinfo->debug_level & DBG_TX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n",
             __FUNCTION__, plpinfo->lport_handle);
    }

    // Put the port in FAST_PERIODIC state.
    plpinfo->periodic_tx_fsm_state = PERIODIC_TX_FSM_FAST_PERIODIC_STATE;

    // Reinitialize the expiry counter.
    plpinfo->periodic_tx_timer_expiry_counter = FAST_PERIODIC_COUNT;

    // Go to SLOW_PERIODIC state if approp. conditions prevail.
    if (plpinfo->partner_oper_port_state.lacp_timeout == LONG_TIMEOUT) {
        LACP_periodic_tx_fsm(E4, plpinfo->periodic_tx_fsm_state, plpinfo);
    }

    if (plpinfo->debug_level & DBG_TX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // LACP_fast_periodic_state_action

/*----------------------------------------------------------------------
 * Function: LACP_slow_periodic_state_action(int port_number)
 * Synopsis: Function implementing slow periodic state
 * Input  :
 *           port_number = port number on which to act upon.
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
LACP_slow_periodic_state_action(lacp_per_port_variables_t *plpinfo)
{
    if (plpinfo->debug_level & DBG_TX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n",
             __FUNCTION__, plpinfo->lport_handle);
    }

    // Put the port in SLOW_PERIODIC state.
    plpinfo->periodic_tx_fsm_state = PERIODIC_TX_FSM_SLOW_PERIODIC_STATE;

    // Reinitialize the expiry counter.
    plpinfo->periodic_tx_timer_expiry_counter = SLOW_PERIODIC_COUNT;

    // Go to PERIODIC_TX state if approp. conditions prevail.
    if (plpinfo->partner_oper_port_state.lacp_timeout == SHORT_TIMEOUT) {
        LACP_periodic_tx_fsm(E6, plpinfo->periodic_tx_fsm_state, plpinfo);
    }

    if (plpinfo->debug_level & DBG_TX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // LACP_slow_periodic_state_action

/*----------------------------------------------------------------------
 * Function: LACP_periodic_tx_state_action(int port_number)
 * Synopsis: Function implementing periodic Tx state
 * Input  :
 *           port_number = port number on which to act upon.
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
LACP_periodic_tx_state_action(lacp_per_port_variables_t *plpinfo)
{
    if (plpinfo->debug_level & DBG_TX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n",
             __FUNCTION__, plpinfo->lport_handle);
    }

    // Put the port in PERIODIC_TX state.
    plpinfo->periodic_tx_fsm_state = PERIODIC_TX_FSM_PERIODIC_TX_STATE;

    // Set the NTT(need to transmit) flag.
    plpinfo->lacp_control.ntt = TRUE;

    // Call the routine to transmit a LACPdu on the port.
    LACP_sync_transmit_lacpdu(plpinfo);

    // If both the actor and the partner have their LACP modes as PASSIVE
    // then put the periodic tx fsm in the NO_PERIODIC state.
    if ((plpinfo->actor_oper_port_state.lacp_activity == LACP_PASSIVE_MODE) &&
        (plpinfo->partner_oper_port_state.lacp_activity == LACP_PASSIVE_MODE)) {
        LACP_periodic_tx_fsm(E8, plpinfo->periodic_tx_fsm_state, plpinfo);
    } else {
        // Generate E6 or E4 events depending upon the partner's lacp timeout.
        if (plpinfo->partner_oper_port_state.lacp_timeout == SHORT_TIMEOUT) {
            LACP_periodic_tx_fsm(E6, plpinfo->periodic_tx_fsm_state, plpinfo);

        } else if (plpinfo->partner_oper_port_state.lacp_timeout==LONG_TIMEOUT) {
            LACP_periodic_tx_fsm(E4, plpinfo->periodic_tx_fsm_state, plpinfo);
        }
    }

    if (plpinfo->debug_level & DBG_TX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // LACP_periodic_tx_state_action

/*----------------------------------------------------------------------
 * Function: LACP_transmit_lacpdu(int pnum)
 * Synopsis: Function to transmit a lacpdu
 * Input  :
 *           port_number = port number on which to act upon.
 * Returns:  void
 *----------------------------------------------------------------------*/
void
LACP_transmit_lacpdu(lacp_per_port_variables_t *plpinfo)
{
    lacpdu_payload_t *lacpdu_payload;
    int datasize = sizeof(lacpdu_payload_t);

    RENTRY();

    if (plpinfo->debug_level & DBG_TX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n",
             __FUNCTION__, plpinfo->lport_handle);
    }

    // If the port is in NO PERIODIC state, then don't transmit any LACPDUs.
    // Set the NTT to false.
    if (plpinfo->periodic_tx_fsm_state == PERIODIC_TX_FSM_NO_PERIODIC_STATE) {
        plpinfo->lacp_control.ntt = FALSE;
        goto exit;
    }

    // Form the LACPDU.
    if (!(lacpdu_payload = LACP_build_lacpdu_payload(plpinfo))) {

        // Report error and exit
        VLOG_ERR("Failed to build LACPDU payload");
        goto exit;
    }

    // OpenSwitch
    mlacp_tx_pdu((unsigned char *)lacpdu_payload,
                 datasize, plpinfo->lport_handle);

    plpinfo->lacp_pdus_sent++;

    // Free LACPDU memory.
    free(lacpdu_payload);

 exit:

    if (plpinfo->debug_level & DBG_TX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // LACP_transmit_lacpdu

/*----------------------------------------------------------------------
 * Function: LACP_build_lacpdu(int port_number)
 * Synopsis: Function to construct the lacpdu.
 * Input  :
 *           port_number = port number on which to act upon.
 * Returns:  pointer to the constructed lacpdu payload or NULL in case
 *           of error.
 *----------------------------------------------------------------------*/
static lacpdu_payload_t *
LACP_build_lacpdu_payload(lacp_per_port_variables_t *plpinfo)
{
    lacpdu_payload_t *lacpdu_payload;

    RENTRY();

    if (plpinfo->debug_level & DBG_TX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n",
             __FUNCTION__, plpinfo->lport_handle);
    }

    // Allocate memory for the lacpdu
    lacpdu_payload = (lacpdu_payload_t *)malloc(sizeof(lacpdu_payload_t));
    if (lacpdu_payload == NULL) {
        VLOG_FATAL("out of memory");
        goto exit;
    }

    // Zero out the memory.
    memset(lacpdu_payload, 0, sizeof(lacpdu_payload_t));

    // Fill in the general parameters in the lacpdu_payload.
    lacpdu_payload->subtype = LACP_SUBTYPE;
    lacpdu_payload->version_number = LACP_VERSION;

    // Fill in the actor's (local port) parameters in the lacpdu_payload.
    lacpdu_payload->tlv_type_actor = LACP_TLV_ACTOR_INFO;
    lacpdu_payload->actor_info_length = LACP_TLV_INFO_LENGTH;
    lacpdu_payload->actor_system_priority =
        plpinfo->actor_oper_system_variables.system_priority;

    memcpy((char *)lacpdu_payload->actor_system,
           (char *)plpinfo->actor_oper_system_variables.system_mac_addr,
           MAC_ADDR_LENGTH);

    lacpdu_payload->actor_key = plpinfo->actor_oper_port_key;
    lacpdu_payload->actor_port_priority = plpinfo->actor_oper_port_priority;
    lacpdu_payload->actor_port = plpinfo->actor_oper_port_number;
    lacpdu_payload->actor_state.lacp_activity =
        plpinfo->actor_oper_port_state.lacp_activity;
    lacpdu_payload->actor_state.lacp_timeout =
        plpinfo->actor_oper_port_state.lacp_timeout;
    lacpdu_payload->actor_state.aggregation =
        plpinfo->actor_oper_port_state.aggregation;
    lacpdu_payload->actor_state.synchronization =
        plpinfo->actor_oper_port_state.synchronization;
    lacpdu_payload->actor_state.collecting =
        plpinfo->actor_oper_port_state.collecting;
    lacpdu_payload->actor_state.distributing =
        plpinfo->actor_oper_port_state.distributing;
    lacpdu_payload->actor_state.defaulted =
        plpinfo->actor_oper_port_state.defaulted;
    lacpdu_payload->actor_state.expired =
        plpinfo->actor_oper_port_state.expired;

    // Fill in the partner's (local port) parameters in the lacpdu_payload.
    lacpdu_payload->tlv_type_partner = LACP_TLV_PARTNER_INFO;
    lacpdu_payload->partner_info_length = LACP_TLV_INFO_LENGTH;

    lacpdu_payload->partner_system_priority =
        plpinfo->partner_oper_system_variables.system_priority;
    memcpy((char *)lacpdu_payload->partner_system,
           (char *)plpinfo->partner_oper_system_variables.system_mac_addr,
           MAC_ADDR_LENGTH);

    lacpdu_payload->partner_key = plpinfo->partner_oper_key;
    lacpdu_payload->partner_port_priority =
        plpinfo->partner_oper_port_priority;
    lacpdu_payload->partner_port =
        plpinfo->partner_oper_port_number;

    // State parameters.
    lacpdu_payload->partner_state.lacp_activity =
        plpinfo->partner_oper_port_state.lacp_activity;
    lacpdu_payload->partner_state.lacp_timeout =
        plpinfo->partner_oper_port_state.lacp_timeout;
    lacpdu_payload->partner_state.aggregation =
        plpinfo->partner_oper_port_state.aggregation;
    lacpdu_payload->partner_state.synchronization =
        plpinfo->partner_oper_port_state.synchronization;
    lacpdu_payload->partner_state.collecting =
        plpinfo->partner_oper_port_state.collecting;
    lacpdu_payload->partner_state.distributing =
        plpinfo->partner_oper_port_state.distributing;
    lacpdu_payload->partner_state.defaulted =
        plpinfo->partner_oper_port_state.defaulted;
    lacpdu_payload->partner_state.expired =
        plpinfo->partner_oper_port_state.expired;

    lacpdu_payload->tlv_type_collector = LACP_TLV_COLLECTOR_INFO;
    lacpdu_payload->collector_info_length = LACP_TLV_COLLECTOR_INFO_LENGTH;
    lacpdu_payload->collector_max_delay = plpinfo->collector_max_delay;
    lacpdu_payload->tlv_type_terminator = LACP_TLV_TERMINATOR_INFO;
    lacpdu_payload->terminator_length = LACP_TLV_TERMINATOR_INFO_LENGTH;

exit:

  REXIT();
  return (lacpdu_payload);

} // LACP_build_lacpdu_payload

/****************************************************************************
 *       Transmit machine
 ****************************************************************************/
/*----------------------------------------------------------------------
 * Function: LACP_sync_transmit_lacpdu(int port_number)
 * Synopsis: Function to transmit the periodic LACP pdu
 * Input  :
 *           port_number = port number on which to act upon.
 * Returns:
 *----------------------------------------------------------------------*/
void
LACP_sync_transmit_lacpdu(lacp_per_port_variables_t *plpinfo)
{
    if (plpinfo->debug_level & DBG_TX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n",
             __FUNCTION__, plpinfo->lport_handle);
    }

    if (plpinfo->periodic_tx_fsm_state == PERIODIC_TX_FSM_NO_PERIODIC_STATE) {
        plpinfo->lacp_control.ntt = FALSE;
        goto exit;
    }

    if (plpinfo->lacp_control.ntt == TRUE) {
        LACP_transmit_lacpdu(plpinfo);
        plpinfo->lacp_control.ntt = FALSE;
    }

exit:

    if (plpinfo->debug_level & DBG_TX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // LACP_sync_transmit_lacpdu

/*----------------------------------------------------------------------
 * Function: LACP_async_transmit_lacpdu(int port_number)
 * Synopsis: Function to transmit an async LACP pdu
 * Input  :
 *           port_number = port number on which to act upon.
 * Returns:
 *----------------------------------------------------------------------*/
void
LACP_async_transmit_lacpdu(lacp_per_port_variables_t *plpinfo)
{
    if (plpinfo->debug_level & DBG_TX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n",
             __FUNCTION__, plpinfo->lport_handle);
    }

    if (plpinfo->async_tx_count < MAX_ASYNC_TX) {
        plpinfo->async_tx_count++;
        LACP_sync_transmit_lacpdu(plpinfo);
    }

    if (plpinfo->debug_level & DBG_TX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // LACP_async_transmit_lacpdu
