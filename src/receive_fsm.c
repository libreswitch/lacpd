/*
 * (c) Copyright 2015-2016 Hewlett Packard Enterprise Development LP
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
#include <sys/types.h>

#include <avl.h>
#include <pm_cmn.h>
#include <lacp_cmn.h>
#include <mlacp_debug.h>
#include <lacp_fsm.h>
#include <eventlog.h>

#include "lacp.h"
#include "lacp_stubs.h"
#include "lacp_support.h"
#include "mvlan_lacp.h"
#include "lacp_ops_if.h"
#include "mvlan_sport.h"
#include "mlacp_fproto.h"

VLOG_DEFINE_THIS_MODULE(receive_fsm);

/*****************************************************************************
 *   Static Variables
 ****************************************************************************/

/* Receive machine fsm table */
static FSM_ENTRY receive_machine_fsm_table[RECV_FSM_NUM_INPUTS]
                                          [RECV_FSM_NUM_STATES] =
{
/*****************************************************************************/
/* Input Event E1 - Received LACPDU                                          */
/*****************************************************************************/
  {{RECV_FSM_RETAIN_STATE,       NO_ACTION},         // Begin state
   {RECV_FSM_CURRENT_STATE,      ACTION_CURRENT},    // current
   {RECV_FSM_CURRENT_STATE,      ACTION_CURRENT},    // expired
   {RECV_FSM_CURRENT_STATE,      ACTION_CURRENT},    // defaulted
   {RECV_FSM_RETAIN_STATE,       NO_ACTION},         // LACP_disabled
   {RECV_FSM_RETAIN_STATE,       NO_ACTION},         // port_disabled
   {RECV_FSM_RETAIN_STATE,       NO_ACTION}},        // initialize

/*****************************************************************************/
/* Input Event E2 - current_while_timer_expired                              */
/*****************************************************************************/
  {{RECV_FSM_RETAIN_STATE,       NO_ACTION},         // Begin state
   {RECV_FSM_EXPIRED_STATE,      ACTION_EXPIRED},    // current
   {RECV_FSM_DEFAULTED_STATE,    ACTION_DEFAULTED},  // expired
   {RECV_FSM_RETAIN_STATE,       NO_ACTION},         // defaulted
   {RECV_FSM_RETAIN_STATE,       NO_ACTION},         // LACP_disabled
   {RECV_FSM_RETAIN_STATE,       NO_ACTION},         // port_disabled
   {RECV_FSM_RETAIN_STATE,       NO_ACTION}},        // initialize

/*****************************************************************************/
/* Input Event E3 - port_moved = TRUE                                        */
/*****************************************************************************/
  {{RECV_FSM_RETAIN_STATE,       NO_ACTION},         // Begin state
   {RECV_FSM_RETAIN_STATE,       NO_ACTION},         // current
   {RECV_FSM_RETAIN_STATE,       NO_ACTION},         // expired
   {RECV_FSM_RETAIN_STATE,       NO_ACTION},         // defaulted
   {RECV_FSM_RETAIN_STATE,       NO_ACTION},         // LACP_disabled
   {RECV_FSM_INITIALIZE_STATE,   ACTION_INITIALIZE}, // port_disabled
   {RECV_FSM_RETAIN_STATE,       NO_ACTION}},        // initialize

/*****************************************************************************/
/* Input Event E4 - port_moved = FALSE, port_enabled = FALSE, BEGIN = FALSE  */
/*****************************************************************************/
  {{RECV_FSM_RETAIN_STATE,         NO_ACTION},            // Begin state
   {RECV_FSM_PORT_DISABLED_STATE,  ACTION_PORT_DISABLED}, // current
   {RECV_FSM_PORT_DISABLED_STATE,  ACTION_PORT_DISABLED}, // expired
   {RECV_FSM_PORT_DISABLED_STATE,  ACTION_PORT_DISABLED}, // defaulte
   {RECV_FSM_PORT_DISABLED_STATE,  ACTION_PORT_DISABLED}, // LACP_disabled
   {RECV_FSM_PORT_DISABLED_STATE,  ACTION_PORT_DISABLED}, // port_disabled
   {RECV_FSM_PORT_DISABLED_STATE,  ACTION_PORT_DISABLED}},// initialize

/*****************************************************************************/
/* Input Event E5 - UCT                                                      */
/*****************************************************************************/
  {{RECV_FSM_RETAIN_STATE,        NO_ACTION},             // Begin state
   {RECV_FSM_RETAIN_STATE,        NO_ACTION},             // current
   {RECV_FSM_RETAIN_STATE,        NO_ACTION},             // expired
   {RECV_FSM_RETAIN_STATE,        NO_ACTION},             // defaulted
   {RECV_FSM_RETAIN_STATE,        NO_ACTION},             // LACP_disabled
   {RECV_FSM_RETAIN_STATE,        NO_ACTION},             // port_disabled
   {RECV_FSM_PORT_DISABLED_STATE, ACTION_PORT_DISABLED}}, // initialize

/*****************************************************************************/
/* Input Event E6 - port_enabled = TRUE, LACP_Enabled = True                 */
/*****************************************************************************/
  {{RECV_FSM_RETAIN_STATE,      NO_ACTION},         // Begin state
   {RECV_FSM_RETAIN_STATE,      NO_ACTION},         // current
   {RECV_FSM_RETAIN_STATE,      NO_ACTION},         // expired
   {RECV_FSM_RETAIN_STATE,      NO_ACTION},         // defaulted
   {RECV_FSM_RETAIN_STATE,      NO_ACTION},         // LACP_disabled
   {RECV_FSM_EXPIRED_STATE,     ACTION_EXPIRED},    // port_disabled
   {RECV_FSM_RETAIN_STATE,      NO_ACTION}},        // initialize

/*****************************************************************************/
/* Input Event E7 - port_enabled = TRUE, LACP_Enabled = FALSE                */
/*****************************************************************************/
  {{RECV_FSM_RETAIN_STATE,        NO_ACTION},            // Begin state
   {RECV_FSM_RETAIN_STATE,        NO_ACTION},            // current
   {RECV_FSM_RETAIN_STATE,        NO_ACTION},            // expired
   {RECV_FSM_RETAIN_STATE,        NO_ACTION},            // defaulted
   {RECV_FSM_RETAIN_STATE,        NO_ACTION},            // LACP_disabled
   {RECV_FSM_LACP_DISABLED_STATE, ACTION_LACP_DISABLED}, // port_disabled
   {RECV_FSM_RETAIN_STATE,        NO_ACTION}},           // initialize

/*****************************************************************************/
/* Input Event E8 - Begin = TRUE                                             */
/*****************************************************************************/
  {{RECV_FSM_INITIALIZE_STATE, ACTION_INITIALIZE},  // Begin state
   {RECV_FSM_INITIALIZE_STATE, ACTION_INITIALIZE},  // current
   {RECV_FSM_INITIALIZE_STATE, ACTION_INITIALIZE},  // expired
   {RECV_FSM_INITIALIZE_STATE, ACTION_INITIALIZE},  // defaulted
   {RECV_FSM_INITIALIZE_STATE, ACTION_INITIALIZE},  // LACP_disabled
   {RECV_FSM_INITIALIZE_STATE, ACTION_INITIALIZE},  // port_disabled
   {RECV_FSM_INITIALIZE_STATE, ACTION_INITIALIZE}}, // initialize

/*****************************************************************************/
/* Input Event E9 - Fallback changed                                         */
/*****************************************************************************/
  {{RECV_FSM_RETAIN_STATE,        NO_ACTION},            // Begin state
   {RECV_FSM_RETAIN_STATE,        NO_ACTION},            // current
   {RECV_FSM_RETAIN_STATE,        NO_ACTION},            // expired
   {RECV_FSM_DEFAULTED_STATE,     ACTION_DEFAULTED},     // defaulted
   {RECV_FSM_RETAIN_STATE,        NO_ACTION},            // LACP_disabled
   {RECV_FSM_RETAIN_STATE,        NO_ACTION},            // port_disabled
   {RECV_FSM_RETAIN_STATE,        NO_ACTION}}            // initialize

};


/*****************************************************************************
 *                Prototypes for static functions
 *****************************************************************************/
static void current_state_action(lacp_per_port_variables_t *, lacpdu_payload_t *);
static void expired_state_action(lacp_per_port_variables_t *);
static void defaulted_state_action(lacp_per_port_variables_t *);
static void lacp_disabled_state_action(lacp_per_port_variables_t *);
static void port_disabled_state_action(lacp_per_port_variables_t *);
static void initialize_state_action(lacp_per_port_variables_t *);
static void update_Selected (lacpdu_payload_t *, lacp_per_port_variables_t *);
static void update_NTT(lacpdu_payload_t *, lacp_per_port_variables_t *);
static void recordPDU (lacpdu_payload_t *, lacp_per_port_variables_t *);
static void choose_Matched(lacpdu_payload_t *, lacp_per_port_variables_t *);
static void recordDefault(lacp_per_port_variables_t *);
static void update_Default_Selected(lacp_per_port_variables_t *);
static void start_current_while_timer(lacp_per_port_variables_t *, int);
static void generate_mux_event_from_recordPdu(lacp_per_port_variables_t *);
static void format_state(state_parameters_t, char *);
static void update_max_port_priority(lacp_per_port_variables_t *);

/*----------------------------------------------------------------------
 * Function: LACP_receive_fsm(event, current_state, recvd_lacpdu, plpinfo)
 * Synopsis: Entry routine for LACP Receive state machine.
 * Input  :
 *           event = the event that occured
 *           current_state = the current state of the fsm
 *           recvd_lacpdu = received LACPDU
 *           plpinfo = pointer to lport data
 * Returns:  void
 *----------------------------------------------------------------------*/
void
LACP_receive_fsm(int event, int current_state,
                 lacpdu_payload_t *recvd_lacpdu,
                 lacp_per_port_variables_t *plpinfo)
{
    u_int action;
    char previous_state_string[STATE_STRING_SIZE];
    char current_state_string[STATE_STRING_SIZE];
    char actor_state_str[STATE_FLAGS_SIZE];
    char partner_state_str[STATE_FLAGS_SIZE];
    int port;
    struct iface_data *idp = NULL;

    RENTRY();
    RDEBUG(DL_RX_FSM, "RxFSM: event %d current_state %d\n", event, current_state);

    // Get the action routine and the next state to transition to.
    GET_FSM_TABLE_CELL_CONTENTS(receive_machine_fsm_table,
                                event,
                                current_state,
                                action);

    // Update the state only if required so.
    if (current_state != RECV_FSM_RETAIN_STATE) {

        if (plpinfo->debug_level & DBG_RX_FSM) {
            //***********************************************************
            // receive_fsm  debug is DBG_RX_FSM
            // periodic_fsm debug is DBG_TX_FSM
            // mux_fsm      debug is DBG_MUX_FSM
            //***********************************************************
            rx_state_string(plpinfo->recv_fsm_state, previous_state_string);
            rx_state_string(current_state, current_state_string);

            RDBG("%s : transitioning from %s to %s, action %d "
                 "(lport 0x%llx)\n",
                 __FUNCTION__,
                 previous_state_string, current_state_string,
                 action, plpinfo->lport_handle);
        }

        plpinfo->recv_fsm_state = current_state;

    } else {
        if (plpinfo->debug_level & DBG_RX_FSM) {
            RDBG("%s : retain old state (%d)\n",
                 __FUNCTION__, plpinfo->recv_fsm_state);
        }
    }

    port = PM_HANDLE2PORT(plpinfo->lport_handle);
    idp = find_iface_data_by_index(port);

    // Call the appropriate action routine.
    switch (action) {

    case ACTION_CURRENT:
        current_state_action(plpinfo, recvd_lacpdu);
        break;

    case ACTION_EXPIRED:
        expired_state_action(plpinfo);
        break;

    case ACTION_DEFAULTED:
        defaulted_state_action(plpinfo);
        if (log_event("LACP_PARTNER_TIMEOUT",
                      EV_KV("intf_id", "%s",
                            idp->name),
                      EV_KV("lag_id", "sport: %d",
                            idp->cfg_lag_id),
                      EV_KV("fsm_state", "%s -> %s",
                            previous_state_string,
                            current_state_string)) < 0) {
            VLOG_ERR("Could not log event LACP_PARTNER_TIMEOUT");
        }
        break;

    case ACTION_LACP_DISABLED:
        lacp_disabled_state_action(plpinfo);
        break;

    case ACTION_PORT_DISABLED:
        port_disabled_state_action(plpinfo);
        format_state(plpinfo->actor_oper_port_state,
                     actor_state_str);
        format_state(plpinfo->partner_oper_port_state,
                     partner_state_str);
        if (log_event("LACP_PARTNER_OUT_OF_SYNC",
                      EV_KV("intf_id", "%s", idp->name),
                      EV_KV("lag_id", "sport: %d", idp->cfg_lag_id),
                      EV_KV("actor_state", "%s",
                            actor_state_str),
                      EV_KV("partner_state", "%s",
                            partner_state_str)
                      ) < 0) {
            VLOG_ERR("Could not log event LACP_PARTNER_OUT_OF_SYNC");
        }
        break;

    case ACTION_INITIALIZE:
        initialize_state_action(plpinfo);
        break;

    case NO_ACTION:
    default:
        break;
    }

    db_update_interface(plpinfo);

    REXIT();

} // LACP_receive_fsm

/*----------------------------------------------------------------------
 * Function: current_state_action(plpinfo, recvd_lacpdu)
 * Synopsis: Function implementing current state action
 * Input  :
 *           plpinfo = pointer to lport data
 *           recvd_lacpdu = received LACPDU payload.
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
current_state_action(lacp_per_port_variables_t *plpinfo,
                     lacpdu_payload_t *recvd_lacpdu)
{
    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
    }

    update_Selected(recvd_lacpdu, plpinfo);

    // OpenSwitch: This used to be after recordPDU() call below.
    //        Moved this up here to allow partner checks done
    //        earlier so our responses can be accurate ASAP.
    //        (ANVL LACP Conformance Test 7.3)
    choose_Matched(recvd_lacpdu, plpinfo);

    update_NTT(recvd_lacpdu, plpinfo);

    recordPDU(recvd_lacpdu, plpinfo);

    LAG_selection(plpinfo);

    start_current_while_timer(plpinfo,
                              plpinfo->actor_oper_port_state.lacp_timeout);

    plpinfo->actor_oper_port_state.expired = FALSE;

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // current_state_action

/*----------------------------------------------------------------------
 * Function: expired_state_action(plpinfo)
 * Synopsis: Function implementing expired state action
 * Input  :
 *           plpinfo = pointer to lport data
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
expired_state_action(lacp_per_port_variables_t *plpinfo)
{
    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
    }

    plpinfo->partner_oper_port_state.synchronization = FALSE;
    plpinfo->partner_oper_port_state.lacp_timeout = SHORT_TIMEOUT;

    // OpenSwitch - we just forcefully changed the partner's oper timeout to
    // SHORT_TIMEOUT.  If it was previously LONG, then we need to let
    // the periodic TX machine know of the change.  If it was already
    // SHORT, then no changes to the TX state machine.
    LACP_periodic_tx_fsm(E6,
                         plpinfo->periodic_tx_fsm_state,
                         plpinfo);

    start_current_while_timer(plpinfo, SHORT_TIMEOUT);
    plpinfo->actor_oper_port_state.expired = TRUE;
    plpinfo->actor_oper_port_state.defaulted = FALSE;

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // expired_state_action

/*----------------------------------------------------------------------
 * Function: defaulted_state_action(plpinfo)
 * Synopsis: Function implementing defaulted state action
 * Input  :
 *           plpinfo = pointer to lport data
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
defaulted_state_action(lacp_per_port_variables_t *plpinfo)
{
    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
    }

    update_max_port_priority(plpinfo);
    update_Default_Selected(plpinfo);
    recordDefault(plpinfo);

    if (plpinfo->fallback_enabled) {

        // OpenSwitch - We enter defaulted state when we time out waiting for
        // LACPDU on this port.  This probably means far end does not support
        // LACP. We need to default partner to be in-sync &
        // collecting/distributing so that we'll form a single LAG & pass
        // traffic. This could also be done by changing the
        // "partner_admin_port_state" default values, but ANVL doesn't like
        // that. So we make the change to oper_state here.
        plpinfo->partner_oper_port_state.synchronization = TRUE;
        plpinfo->partner_oper_port_state.collecting      = TRUE;
        plpinfo->partner_oper_port_state.distributing    = TRUE;
        plpinfo->partner_oper_port_state.defaulted       = FALSE;
        plpinfo->partner_oper_port_state.expired         = FALSE;

        plpinfo->actor_oper_port_state.expired = FALSE;

    }
    else {

        // If fallback is not enabled we should not create a default partner
        // in sync or collecting/distributing
        plpinfo->partner_oper_port_state.synchronization = FALSE;
        plpinfo->partner_oper_port_state.collecting      = FALSE;
        plpinfo->partner_oper_port_state.distributing    = FALSE;
        plpinfo->partner_oper_port_state.defaulted       = TRUE;
        plpinfo->partner_oper_port_state.expired         = TRUE;
    }

    LAG_selection(plpinfo); // OPS_TODO: Check if this is ok

    // OpenSwitch - If selected is SELECTED && partner.sync = TRUE
    // generate E5.  This will trigger a transition to coll/dist
    // state if we're still in ATTACHED state and there is no
    // change in selection status.  This is needed for cases
    // where ports went down & came back up.
    if ((SELECTED == plpinfo->lacp_control.selected) &&
        (TRUE == plpinfo->partner_oper_port_state.synchronization)) {

        LACP_mux_fsm(E5,
                     plpinfo->mux_fsm_state,
                     plpinfo);
    }
    // If the interface is not in-sync and selected (attached) we have to
    // move it to detached
    else {

        plpinfo->lacp_control.selected = UNSELECTED;
        plpinfo->lacp_control.ready_n = FALSE;
    }
    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // defaulted_state_action

/*----------------------------------------------------------------------
 * Function: lacp_disabled_state_action(plpinfo)
 * Synopsis: Function implementing lacp disabled state action
 * Input  :
 *           plpinfo = pointer to lport data
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
lacp_disabled_state_action(lacp_per_port_variables_t *plpinfo)
{
    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
    }

    plpinfo->lacp_control.selected = UNSELECTED;
    LACP_mux_fsm(E2,
                 plpinfo->mux_fsm_state,
                 plpinfo);

    recordDefault(plpinfo);

    plpinfo->partner_oper_port_state.aggregation = FALSE;

    plpinfo->partner_oper_port_state.expired = FALSE;

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // lacp_disabled_state_action

/*----------------------------------------------------------------------
 * Function: port_disabled_state_action(plpinfo)
 * Synopsis: Function implementing port disabled state action
 * Input  :
 *           plpinfo = pointer to lport data
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
port_disabled_state_action(lacp_per_port_variables_t *plpinfo)
{
    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
    }

    plpinfo->partner_oper_port_state.synchronization = FALSE;

    // Generate an event in the mux state machine.
    LACP_mux_fsm(E6,
                 plpinfo->mux_fsm_state,
                 plpinfo);

    LAG_selection(plpinfo); // OPS_TODO: is it okay?

    // Transition to the next state if approp. conditions prevail.
    if (plpinfo->lacp_control.port_moved == TRUE) {
        LACP_receive_fsm(E3,
                         plpinfo->recv_fsm_state,
                         NULL,
                         plpinfo);
    }

    if (plpinfo->lacp_control.port_enabled == TRUE) {
        LACP_receive_fsm(E6,
                         plpinfo->recv_fsm_state,
                         NULL,
                         plpinfo);
    }

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // port_disabled_state_action

/*----------------------------------------------------------------------
 * Function: initialize_state_action(plpinfo)
 * Synopsis: Function implementing initialize state action
 * Input  :
 *           plpinfo = pointer to lport data
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
initialize_state_action(lacp_per_port_variables_t *plpinfo)
{
    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
    }

    plpinfo->lacp_control.begin = FALSE;

    plpinfo->lacp_control.selected = UNSELECTED;
    LACP_mux_fsm(E2,
                 plpinfo->mux_fsm_state,
                 plpinfo);

    recordDefault(plpinfo);

    plpinfo->partner_oper_port_state.expired = FALSE;
    plpinfo->lacp_control.port_moved = FALSE;
    LACP_receive_fsm(E5,
                     plpinfo->recv_fsm_state,
                     NULL,
                     plpinfo);

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // initialize_state_action

/*----------------------------------------------------------------------
 * Function: update_Selected (recvd_lacpdu, plpinfo)
 * Synopsis: Function implementing update selected state action
 * Input  :
 *           recvd_lacpdu = received LACPDU
 *           plpinfo = pointer to lport data
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
update_Selected(lacpdu_payload_t *recvd_lacpdu,
                lacp_per_port_variables_t *plpinfo)
{
    lacp_per_port_variables_t   *plpinfo_priority;
    super_port_t                *psport;
    lacp_int_sport_params_t     *placp_sport_params;
    int                         status = R_SUCCESS;
    int                         max_port_priority = MAX_PORT_PRIORITY;

    RENTRY();

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
    }

    // Compare actor_port in LACPDU and partner_oper_port_number
    // variable in the local system.
    if (recvd_lacpdu->actor_port != plpinfo->partner_oper_port_number) {

        if (plpinfo->debug_level & DBG_RX_FSM) {
            RDBG("%s : recvd_lacpdu->actor_port 0x%x "
                 "plpinfo->partner_oper_port_number 0x%x\n",
                 __FUNCTION__,
                 recvd_lacpdu->actor_port,
                 plpinfo->partner_oper_port_number);
        }

        plpinfo->lacp_control.selected = UNSELECTED;
        LACP_mux_fsm(E2,
                     plpinfo->mux_fsm_state,
                     plpinfo);
        goto exit;
    }

    // Compare actor_oper_port_priority in LACPDU and
    // partner_oper_port_priority variable in the local system.
    if (recvd_lacpdu->actor_port_priority !=
        plpinfo->partner_oper_port_priority) {

        if (plpinfo->debug_level & DBG_RX_FSM) {
            RDBG("%s : recvd_lacpdu->actor_port_priority 0x%x "
                 "plpinfo->partner_oper_port_priority 0x%x\n",
                 __FUNCTION__,
                 recvd_lacpdu->actor_port_priority,
                 plpinfo->partner_oper_port_priority);
        }

        status = mvlan_get_sport(plpinfo->sport_handle , &psport,
                                         MLm_vpm_api__get_sport);
        if (R_SUCCESS == status) {
            plpinfo_priority = LACP_AVL_FIRST(lacp_per_port_vars_tree);

            while (plpinfo_priority) {
                if (plpinfo_priority->lport_handle != plpinfo->lport_handle &&
                    plpinfo_priority->sport_handle == psport->handle &&
                    plpinfo_priority->partner_oper_port_priority != 0 &&
                    max_port_priority > plpinfo_priority->partner_oper_port_priority) {
                    max_port_priority = plpinfo_priority->partner_oper_port_priority;
                }
                plpinfo_priority = LACP_AVL_NEXT(plpinfo_priority->avlnode);
            }

            placp_sport_params = psport->placp_params;
            placp_sport_params->lacp_params.partner_max_port_priority = max_port_priority;
        }

        plpinfo->lacp_control.selected = UNSELECTED;
        LACP_mux_fsm(E2,
                     plpinfo->mux_fsm_state,
                     plpinfo);
        goto exit;
    }

    // Compare actor_system in LACPDU and partner_oper_system_variables
    // system_mac_addr variable in the local system.
    if (memcmp((char *)recvd_lacpdu->actor_system,
               (char *)plpinfo->partner_oper_system_variables.system_mac_addr,
               MAC_ADDR_LENGTH)) {

        if (plpinfo->debug_level & DBG_RX_FSM) {
            RDBG("%s : rcvd_pdu mac %x:%x:%x:%x:%x:%x "
                 "and the mac we had %x:%x:%x:%x:%x:%x:\n",
                 __FUNCTION__,
                 (((char *)(recvd_lacpdu->actor_system))[0]),
                 (((char *)(recvd_lacpdu->actor_system))[1]),
                 (((char *)(recvd_lacpdu->actor_system))[2]),
                 (((char *)(recvd_lacpdu->actor_system))[3]),
                 (((char *)(recvd_lacpdu->actor_system))[4]),
                 (((char *)(recvd_lacpdu->actor_system))[5]),
                 (((char *)(plpinfo->partner_oper_system_variables.system_mac_addr))[0]),
                 (((char *)(plpinfo->partner_oper_system_variables.system_mac_addr))[1]),
                 (((char *)(plpinfo->partner_oper_system_variables.system_mac_addr))[2]),
                 (((char *)(plpinfo->partner_oper_system_variables.system_mac_addr))[3]),
                 (((char *)(plpinfo->partner_oper_system_variables.system_mac_addr))[4]),
                 (((char *)(plpinfo->partner_oper_system_variables.system_mac_addr))[5]));
        }

        plpinfo->lacp_control.selected = UNSELECTED;
        LACP_mux_fsm(E2,
                     plpinfo->mux_fsm_state,
                     plpinfo);
        goto exit;
    }

    // Compare actor_system_priority in LACPDU and
    // partner_oper_system_variables.system_mac_addr_priority variable
    // in the local system.
    if (recvd_lacpdu->actor_system_priority !=
        plpinfo->partner_oper_system_variables.system_priority) {

        if (plpinfo->debug_level & DBG_RX_FSM) {
            RDBG("%s : recvd_lacpdu->actor_system_priority 0x%x "
                 "plpinfo->partner_oper_system_variables.system_priority 0x%x\n",
                 __FUNCTION__,
                 recvd_lacpdu->actor_system_priority,
                 plpinfo->partner_oper_system_variables.system_priority);
        }

        plpinfo->lacp_control.selected = UNSELECTED;
        LACP_mux_fsm(E2,
                     plpinfo->mux_fsm_state,
                     plpinfo);
        goto exit;
    }

    // Compare actor_key in LACPDU and partner_oper_key variable
    // in the local system.
    if (recvd_lacpdu->actor_key != plpinfo->partner_oper_key) {

        if (plpinfo->debug_level & DBG_RX_FSM) {
            RDBG("%s : recvd_lacpdu->actor_key 0x%x "
                 "plpinfo->partner_oper_key.system_priority 0x%x\n",
                 __FUNCTION__,
                 recvd_lacpdu->actor_key,
                 plpinfo->partner_oper_key);
        }

        plpinfo->lacp_control.selected = UNSELECTED;
        LACP_mux_fsm(E2,
                     plpinfo->mux_fsm_state,
                     plpinfo);
        goto exit;
    }

    // Compare actor_state.aggregation in LACPDU and partner_oper_port_state.
    // aggregation  variable in the local system.
    if (recvd_lacpdu->actor_state.aggregation !=
        plpinfo->partner_oper_port_state.aggregation) {

        if (plpinfo->debug_level & DBG_RX_FSM) {
            RDBG("%s : recvd_lacpdu->actor_state.aggregation 0x%x "
                 "plpinfo->partner_oper_port_state.aggregation 0x%x\n",
                 __FUNCTION__,
                 recvd_lacpdu->actor_state.aggregation,
                 plpinfo->partner_oper_port_state.aggregation);
        }

        plpinfo->lacp_control.selected = UNSELECTED;
        LACP_mux_fsm(E2,
                     plpinfo->mux_fsm_state,
                     plpinfo);
    }

    REXIT();

 exit:

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // update_Selected

/*----------------------------------------------------------------------
 * Function: update_NTT(recvd_lacpdu, plpinfo)
 * Synopsis: Function implementing update NTT
 * Input  :
 *           recvd_lacpdu = received LACPDU
 *           plpinfo = pointer to lport data
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
update_NTT(lacpdu_payload_t *recvd_lacpdu, lacp_per_port_variables_t *plpinfo)
{
    RENTRY();

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
    }

    // Compare partner_port in LACPDU and actor_oper_port_number variable
    // in the local system.
    if (recvd_lacpdu->partner_port != plpinfo->actor_oper_port_number) {

        if (plpinfo->debug_level & DBG_RX_FSM) {
            RDBG("%s : recvd_lacpdu->partner_port 0x%x "
                 "plpinfo->actor_oper_port_number 0x%x\n",
                 __FUNCTION__,
                 recvd_lacpdu->partner_port,
                 plpinfo->actor_oper_port_number);
        }

        plpinfo->lacp_control.ntt = TRUE;

        // Transmit a LACPDU.
        LACP_async_transmit_lacpdu(plpinfo);
        goto exit;
    }

    // Compare partner_port_priority in LACPDU and actor_oper_port_priority
    // variable in the local system.
    if (recvd_lacpdu->partner_port_priority !=
        plpinfo->actor_oper_port_priority) {

        if (plpinfo->debug_level & DBG_RX_FSM) {
            RDBG("%s : recvd_lacpdu->partner_port_priority 0x%x "
                 "plpinfo->actor_oper_port_priority 0x%x\n",
                 __FUNCTION__,
                 recvd_lacpdu->partner_port_priority,
                 plpinfo->actor_oper_port_priority);
        }

        plpinfo->lacp_control.ntt = TRUE;

        // Transmit a LACPDU.
        LACP_async_transmit_lacpdu(plpinfo);
        goto exit;
    }

    // Compare partner_system in LACPDU and actor_system variable
    // in the local system.
    if (memcmp((char *)recvd_lacpdu->partner_system,
               (char *)plpinfo->actor_oper_system_variables.system_mac_addr,
               MAC_ADDR_LENGTH)) {

        if (plpinfo->debug_level & DBG_RX_FSM) {
            RDBG("%s : rcvd_pdu mac %x:%x:%x:%x:%x:%x "
                 "and the mac we had %x:%x:%x:%x:%x:%x:\n",
                 __FUNCTION__,
                 (((char *)(recvd_lacpdu->actor_system))[0]),
                 (((char *)(recvd_lacpdu->actor_system))[1]),
                 (((char *)(recvd_lacpdu->actor_system))[2]),
                 (((char *)(recvd_lacpdu->actor_system))[3]),
                 (((char *)(recvd_lacpdu->actor_system))[4]),
                 (((char *)(recvd_lacpdu->actor_system))[5]),
                 (((char *)(plpinfo->partner_oper_system_variables.system_mac_addr))[0]),
                 (((char *)(plpinfo->partner_oper_system_variables.system_mac_addr))[1]),
                 (((char *)(plpinfo->partner_oper_system_variables.system_mac_addr))[2]),
                 (((char *)(plpinfo->partner_oper_system_variables.system_mac_addr))[3]),
                 (((char *)(plpinfo->partner_oper_system_variables.system_mac_addr))[4]),
                 (((char *)(plpinfo->partner_oper_system_variables.system_mac_addr))[5]));
        }
        plpinfo->lacp_control.ntt = TRUE;

        // Transmit a LACPDU.
        LACP_async_transmit_lacpdu(plpinfo);
        goto exit;
    }

    // Compare partner_system_priority in LACPDU and actor_system_priority
    // variable in the local system.
    if (recvd_lacpdu->partner_system_priority !=
        plpinfo->actor_oper_system_variables.system_priority) {

        if (plpinfo->debug_level & DBG_RX_FSM) {
            RDBG("%s : recvd_lacpdu->partner_system_priority 0x%x "
                 "plpinfo->actor_oper_system_variables.system_priority 0x%x\n",
                 __FUNCTION__,
                 recvd_lacpdu->partner_system_priority,
                 plpinfo->actor_oper_system_variables.system_priority);
        }

        plpinfo->lacp_control.ntt = TRUE;

        // Transmit a LACPDU.
        LACP_async_transmit_lacpdu(plpinfo);
        goto exit;
    }

    // Compare partner_key in LACPDU and actor_oper_port_key variable
    // in the local system.
    if (recvd_lacpdu->partner_key != plpinfo->actor_oper_port_key) {

        if (plpinfo->debug_level & DBG_RX_FSM) {
            RDBG("%s : recvd_lacpdu->partner_key 0x%x "
                 "plpinfo->actor_oper_port_key 0x%x\n",
                 __FUNCTION__,
                 recvd_lacpdu->partner_key,
                 plpinfo->actor_oper_port_key);
        }

        plpinfo->lacp_control.ntt = TRUE;

        // Transmit a LACPDU.
        LACP_async_transmit_lacpdu(plpinfo);
        goto exit;
    }

    // Compare partner_state.LACP_Activity in LACPDU and actor_oper_port_state.
    // LACP_Activity variable in the local system.
    if (recvd_lacpdu->partner_state.lacp_activity !=
        plpinfo->actor_oper_port_state.lacp_activity) {

        if (plpinfo->debug_level & DBG_RX_FSM) {
            RDBG("%s : recvd_lacpdu->partner_state.lacp_activity 0x%x "
                 "plpinfo->actor_oper_port_state.lacp_activity 0x%x\n",
                 __FUNCTION__,
                 recvd_lacpdu->partner_state.lacp_activity,
                 plpinfo->actor_oper_port_state.lacp_activity);
        }

        plpinfo->lacp_control.ntt = TRUE;

        // Transmit a LACPDU.
        LACP_async_transmit_lacpdu(plpinfo);
        goto exit;
    }

    // Compare partner_state.lacp_timeout in LACPDU and actor_oper_port_state.
    // lacp_timeout variable in the local system.
    if (recvd_lacpdu->partner_state.lacp_timeout !=
        plpinfo->actor_oper_port_state.lacp_timeout) {

        if (plpinfo->debug_level & DBG_RX_FSM) {
            RDBG("%s : recvd_lacpdu->partner_state.lacp_timeout 0x%x "
                 "plpinfo->actor_oper_port_state.lacp_timeout 0x%x\n",
                 __FUNCTION__,
                 recvd_lacpdu->partner_state.lacp_timeout,
                 plpinfo->actor_oper_port_state.lacp_timeout);
        }

        plpinfo->lacp_control.ntt = TRUE;

        // Transmit a LACPDU.
        LACP_async_transmit_lacpdu(plpinfo);
        goto exit;
    }

    // Compare partner_state.synchronization in LACPDU and
    // actor_oper_port_state.synchronization variable
    // in the local system.
    if (recvd_lacpdu->partner_state.synchronization !=
        plpinfo->actor_oper_port_state.synchronization) {

        if (plpinfo->debug_level & DBG_RX_FSM) {
            RDBG("%s : recvd_lacpdu->partner_state.synchronization "
                 "0x%x plpinfo->actor_oper_port_state.lacp_timeout 0x%x\n",
                 __FUNCTION__,
                 recvd_lacpdu->partner_state.synchronization,
                 plpinfo->actor_oper_port_state.synchronization);
        }

        plpinfo->lacp_control.ntt = TRUE;

        // Transmit a LACPDU.
        LACP_async_transmit_lacpdu(plpinfo);
        goto exit;
    }

    // Compare partner_state.aggregation in LACPDU and
    // actor_oper_port_state.aggregation variable
    // in the local system.
    if (recvd_lacpdu->partner_state.aggregation !=
        plpinfo->actor_oper_port_state.aggregation) {

        if (plpinfo->debug_level & DBG_RX_FSM) {
            RDBG("%s : recvd_lacpdu->partner_state.aggregation 0x%x "
                 "plpinfo->actor_oper_port_state.aggregation 0x%x\n",
                 __FUNCTION__,
                 recvd_lacpdu->partner_state.aggregation,
                 plpinfo->actor_oper_port_state.aggregation);
        }

        plpinfo->lacp_control.ntt = TRUE;

        // Transmit a LACPDU.
        LACP_async_transmit_lacpdu(plpinfo);
    }

    REXIT();

exit:

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // update_NTT

/*----------------------------------------------------------------------
 * Function: recordPDU (recvd_lacpdu, plpinfo)
 * Synopsis:
 * Input  :
 *           recvd_lacpdu = received LACPDU
 *           plpinfo = pointer to lport data
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
recordPDU(lacpdu_payload_t *recvd_lacpdu, lacp_per_port_variables_t *plpinfo)
{
    RENTRY();

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
    }

    // Record the actor_port in the LACPDU as partner_oper_port_number
    // in the local system
    plpinfo->partner_oper_port_number = recvd_lacpdu->actor_port;
    plpinfo->partner_oper_port_priority = recvd_lacpdu->actor_port_priority;

    memcpy((char *)plpinfo->partner_oper_system_variables.system_mac_addr,
           (char *)recvd_lacpdu->actor_system,
           MAC_ADDR_LENGTH);

    plpinfo->partner_oper_system_variables.system_priority =
        recvd_lacpdu->actor_system_priority;
    plpinfo->partner_oper_key =
        recvd_lacpdu->actor_key;
    plpinfo->partner_oper_port_state.lacp_activity =
        recvd_lacpdu->actor_state.lacp_activity;
    plpinfo->partner_oper_port_state.lacp_timeout =
        recvd_lacpdu->actor_state.lacp_timeout;
    plpinfo->partner_oper_port_state.aggregation =
        recvd_lacpdu->actor_state.aggregation;
    plpinfo->partner_oper_port_state.collecting =
        recvd_lacpdu->actor_state.collecting;
    plpinfo->partner_oper_port_state.distributing =
        recvd_lacpdu->actor_state.distributing;
    plpinfo->partner_oper_port_state.defaulted =
        recvd_lacpdu->actor_state.defaulted;
    plpinfo->partner_oper_port_state.expired =
        recvd_lacpdu->actor_state.expired;

    // Set actor_oper_port_state.defaulted to FALSE.
    plpinfo->actor_oper_port_state.defaulted = FALSE;

    // Received LACPDU, generate UCT if either parties in active mode.
    if ((plpinfo->actor_oper_port_state.lacp_activity == LACP_ACTIVE_MODE) ||
        (plpinfo->partner_oper_port_state.lacp_activity == LACP_ACTIVE_MODE)) {
        LACP_periodic_tx_fsm(E2,
                             plpinfo->periodic_tx_fsm_state,
                             plpinfo);
    }

    if (plpinfo->partner_oper_port_state.lacp_timeout == LONG_TIMEOUT) {
        if (plpinfo->debug_level & DBG_RX_FSM) {
            RDBG("%s : trigger periodic_tx_fsm - long timeout "
                 "lport 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
        }

        LACP_periodic_tx_fsm(E4,
                             plpinfo->periodic_tx_fsm_state,
                             plpinfo);

    } else if (plpinfo->partner_oper_port_state.lacp_timeout == SHORT_TIMEOUT ) {
        if (plpinfo->debug_level & DBG_RX_FSM) {
            RDBG("%s : trigger periodic_tx_fsm - short timeout "
                 "lport 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
        }

        LACP_periodic_tx_fsm(E6,
                             plpinfo->periodic_tx_fsm_state,
                             plpinfo);
    }

    generate_mux_event_from_recordPdu(plpinfo);

    REXIT();

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // recordPDU

//******************************************************************
// Function : generate_mux_event_from_recordPdu
//******************************************************************
static void
generate_mux_event_from_recordPdu(lacp_per_port_variables_t *plpinfo)
{
    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
    }

    // If selected is SELECTED && partner.sync = TRUE generate E5.
    // NOTE: Can't avoid using the mux's current state in the decision,
    //       because the 2 systems can reach ATTACHED at different times.
    if ((plpinfo->lacp_control.selected == SELECTED) &&
        (plpinfo->partner_oper_port_state.synchronization == TRUE)) {

        if (plpinfo->mux_fsm_state == MUX_FSM_ATTACHED_STATE) {
            LACP_mux_fsm(E5,
                         plpinfo->mux_fsm_state,
                         plpinfo);
        }
    }

    // If selected is SELECTED && partner.sync = TRUE
    // partner.collecting == TRUE generate E8.
    if ((plpinfo->lacp_control.selected == SELECTED) &&
        (plpinfo->partner_oper_port_state.synchronization == TRUE) &&
        (plpinfo->actor_oper_port_state.collecting == TRUE) &&
        (plpinfo->partner_oper_port_state.collecting == TRUE)) {

        if (plpinfo->mux_fsm_state == MUX_FSM_COLLECTING_STATE) {
            LACP_mux_fsm(E8,
                         plpinfo->mux_fsm_state,
                         plpinfo);
        }
    }

    // If partner.sync is TRUE && partner.collecting is FALSE, generate E9.
    if ((plpinfo->partner_oper_port_state.synchronization == TRUE) &&
        (plpinfo->partner_oper_port_state.collecting == FALSE)) {

        if (plpinfo->mux_fsm_state == MUX_FSM_COLLECTING_DISTRIBUTING_STATE) {
            LACP_mux_fsm(E9,
                         plpinfo->mux_fsm_state,
                         plpinfo);
        }
    }

    // If partner.sync is FALSE, generate E6.
    if (plpinfo->partner_oper_port_state.synchronization == FALSE) {
        LACP_mux_fsm(E6,
                     plpinfo->mux_fsm_state,
                     plpinfo);
    }

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // generate_mux_event_from_recordPdu

/*----------------------------------------------------------------------
 * Function: choose_Matched(recvd_lacpdu, plpinfo)
 * Synopsis: If the same unique LAG is correctly identified by the info in
 *           recvd LACPDU, the Matched variable for the port is set to TRUE,
 *           FALSE otherwise.
 *
 * Change : As per 802.3ad/3.1, there is no matched variable at all...
 * Input  :
 *           recvd_lacpdu = received LACPDU
 *           plpinfo = pointer to lport data
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
choose_Matched(lacpdu_payload_t *recvd_lacpdu,
               lacp_per_port_variables_t *plpinfo)
{
    int qualifier = TRUE;

    RENTRY();

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
    }

    // Compare partner_port in LACPDU and actor_oper_port_number variable
    // in the local system.
    if (recvd_lacpdu->partner_port != plpinfo->actor_oper_port_number) {
        qualifier = FALSE;
        goto exit;
    }

    // Compare partner_port_priority in LACPDU and actor_oper_port_priority
    // variable in the local system.
    if (recvd_lacpdu->partner_port_priority != plpinfo->actor_oper_port_priority) {
        qualifier = FALSE;
        goto exit;
    }

    // Compare partner_system in LACPDU and actor_system variable
    // in the local system.
    if (memcmp((char *)recvd_lacpdu->partner_system,
               (char *)plpinfo->actor_oper_system_variables.system_mac_addr,
               MAC_ADDR_LENGTH)) {
        qualifier = FALSE;
        goto exit;
    }

    // Compare partner_system_priority in LACPDU and actor_system_priority
    // variable in the local system.
    if (recvd_lacpdu->partner_system_priority !=
        plpinfo->actor_oper_system_variables.system_priority) {
        qualifier = FALSE;
        goto exit;
    }

    // Compare partner_key in LACPDU and actor_oper_port_key variable
    // in the local system.
    if (recvd_lacpdu->partner_key != plpinfo->actor_oper_port_key) {
        qualifier = FALSE;
        goto exit;
    }

    // Compare partner_state.aggregation in LACPDU and
    // actor_oper_port_state.aggregation variable
    // in the local system.
    if (recvd_lacpdu->partner_state.aggregation !=
        plpinfo->actor_oper_port_state.aggregation) {
        qualifier = FALSE;
        goto exit;
    }

exit:

    if (recvd_lacpdu->actor_state.aggregation == FALSE) {
        qualifier = TRUE;
    }

    if ((qualifier == TRUE) &&
        (recvd_lacpdu->actor_state.synchronization == TRUE)) {
        plpinfo->partner_oper_port_state.synchronization = TRUE;
    } else {
        plpinfo->partner_oper_port_state.synchronization = FALSE;
    }

} // choose_Matched

/*----------------------------------------------------------------------
 * Function: recordDefault(plpinfo)
 * Synopsis:
 * Input  :
 *           plpinfo = pointer to lport data
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
recordDefault(lacp_per_port_variables_t *plpinfo)
{
    RENTRY();

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
    }

    plpinfo->partner_oper_port_number =
        plpinfo->partner_admin_port_number;
    plpinfo->partner_oper_port_priority =
        plpinfo->partner_admin_port_priority;

    memcpy((char *)plpinfo->partner_oper_system_variables.system_mac_addr,
           (char *)plpinfo->partner_admin_system_variables.system_mac_addr,
           MAC_ADDR_LENGTH);

    plpinfo->partner_oper_system_variables.system_priority =
        plpinfo->partner_admin_system_variables.system_priority;
    plpinfo->partner_oper_key =
        plpinfo->partner_admin_key;

    plpinfo->partner_oper_port_state.lacp_activity =
        plpinfo->partner_admin_port_state.lacp_activity;
    plpinfo->partner_oper_port_state.lacp_timeout =
        plpinfo->partner_admin_port_state.lacp_timeout;
    plpinfo->partner_oper_port_state.aggregation =
        plpinfo->partner_admin_port_state.aggregation;
    plpinfo->partner_oper_port_state.synchronization =
        plpinfo->partner_admin_port_state.synchronization;
    plpinfo->partner_oper_port_state.collecting =
        plpinfo->partner_admin_port_state.collecting;
    plpinfo->partner_oper_port_state.distributing =
        plpinfo->partner_admin_port_state.distributing;
    plpinfo->partner_oper_port_state.defaulted =
        plpinfo->partner_admin_port_state.defaulted;
    plpinfo->partner_oper_port_state.expired =
        plpinfo->partner_admin_port_state.expired;

    // recordPdu would make it FALSE
    plpinfo->actor_oper_port_state.defaulted = TRUE;

    // If both the actor and the partner have their LACP modes as PASSIVE
    // then put the periodic tx fsm in the NO_PERIODIC state.
    if ((plpinfo->actor_oper_port_state.lacp_activity == LACP_PASSIVE_MODE) &&
        (plpinfo->partner_oper_port_state.lacp_activity == LACP_PASSIVE_MODE))
    {
        LACP_periodic_tx_fsm(E1,
                             plpinfo->periodic_tx_fsm_state,
                             plpinfo);
    }

    REXIT();

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // recordDefault

/*----------------------------------------------------------------------
 * Function: update_Default_Selected(plpinfo)
 * Synopsis:
 * Input  :
 *           plpinfo = pointer to lport data
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
update_Default_Selected(lacp_per_port_variables_t *plpinfo)
{
    RENTRY();

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
    }

    if (plpinfo->partner_oper_port_number !=
        plpinfo->partner_admin_port_number) {

        plpinfo->lacp_control.selected = UNSELECTED;

        // Generate an event in the Mux state machine.
        LACP_mux_fsm(E2,
                     plpinfo->mux_fsm_state,
                     plpinfo);
        goto exit;
    }

    if (plpinfo->partner_oper_port_priority !=
        plpinfo->partner_admin_port_priority) {

        plpinfo->lacp_control.selected = UNSELECTED;

        // Generate an event in the Mux state machine.
        LACP_mux_fsm(E2,
                     plpinfo->mux_fsm_state,
                     plpinfo);
        goto exit;
    }

    if (memcmp((char *)plpinfo->partner_oper_system_variables.system_mac_addr,
               (char *)plpinfo->partner_admin_system_variables.system_mac_addr,
               MAC_ADDR_LENGTH)) {

        plpinfo->lacp_control.selected = UNSELECTED;

        // Generate an event in the Mux state machine.
        LACP_mux_fsm(E2,
                     plpinfo->mux_fsm_state,
                     plpinfo);
        goto exit;
    }

    if (plpinfo->partner_oper_system_variables.system_priority !=
        plpinfo->partner_admin_system_variables.system_priority) {

        plpinfo->lacp_control.selected = UNSELECTED;

        // Generate an event in the Mux state machine
        LACP_mux_fsm(E2,
                     plpinfo->mux_fsm_state,
                     plpinfo);
        goto exit;
    }

    if (plpinfo->partner_oper_key != plpinfo->partner_admin_key) {

        plpinfo->lacp_control.selected = UNSELECTED;

        // Generate an event in the Mux state machine.
        LACP_mux_fsm(E2,
                     plpinfo->mux_fsm_state,
                     plpinfo);
        goto exit;
    }

    if (plpinfo->partner_oper_port_state.aggregation !=
        plpinfo->partner_admin_port_state.aggregation) {

        plpinfo->lacp_control.selected = UNSELECTED;

        // Generate an event in the Mux state machine.
        LACP_mux_fsm(E2,
                     plpinfo->mux_fsm_state,
                     plpinfo);
    }

exit:

    REXIT();

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // update_Default_Selected

/*----------------------------------------------------------------------
 * Function: LACP_process_lacpdu(plpinfo, data)
 * Synopsis:
 * Input  :  plpinfo = pointer to lport data
 *           data = received LACPDU data
 * Returns:  void
 *----------------------------------------------------------------------*/
void
LACP_process_lacpdu(lacp_per_port_variables_t *plpinfo, void *data)
{
    RENTRY();

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
    }

    // If the portocol is down, then do nothing.
    if (plpinfo->lacp_up == FALSE) {
        return;
    }

    // Increment the stats counter.
    plpinfo->lacp_pdus_received++;

    LACP_receive_fsm(E1,
                     plpinfo->recv_fsm_state,
                     data,
                     plpinfo);
    REXIT();

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // LACP_process_lacpdu

/*----------------------------------------------------------------------
 * Function: start_current_while_timer(plpinfo, lacp_timeout)
 * Synopsis:
 * Input  :
 *           plpinfo = pointer to lport data
 *           lacp_timeout = lacp timeout value.
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
start_current_while_timer(lacp_per_port_variables_t *plpinfo,
                          int lacp_timeout)
{
    int timeout=0;

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
    }

    if (lacp_timeout == SHORT_TIMEOUT) {
        timeout = SHORT_TIMEOUT_COUNT;

    } else if (lacp_timeout == LONG_TIMEOUT) {
        timeout = LONG_TIMEOUT_COUNT;
    }

    // Initialize the counter with the timeout value.
    plpinfo->current_while_timer_expiry_counter = timeout;

    if (plpinfo->debug_level & DBG_RX_FSM) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // start_current_while_timer

/*----------------------------------------------------------------------
 * Function: format_state(state)
 * Synopsis:
 * Input  :
 *           state = struct with lacp state flags
 * Returns:  char *
 *----------------------------------------------------------------------*/

static void
format_state(state_parameters_t state, char *result)
{
    sprintf(result, "%c%c%c%c",
            state.lacp_activity ? 'A' : 'P',
            state.lacp_timeout ? 'L' : 'S',
            state.aggregation ? 'F' : 'I',
            state.synchronization ? 'N' : 'O');
    if (state.collecting) {
        strcat(result, "C");
    }
    if (state.distributing) {
        strcat(result, "D");
    }
    if (state.defaulted) {
        strcat(result, "E");
    }
    if (state.expired) {
        strcat(result,"X");
    }
} /* format_state */

/*----------------------------------------------------------------------
 * Function: update_max_port_priority(plpinfo)
 * Synopsis: Updates the max port priority in the super port
 * Input  :
 *           plpinfo = pointer to lport data
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
update_max_port_priority(lacp_per_port_variables_t *plpinfo)
{
    super_port_t *psport;
    lacp_int_sport_params_t *placp_sport_params;
    int status = R_SUCCESS;
    int max_port_priority = MAX_PORT_PRIORITY;
    lacp_per_port_variables_t *plpinfo_priority;
    struct MLt_vpm_api__lacp_sport_params pmsg;
    int current_port_priority;

    status = mvlan_get_sport(plpinfo->sport_handle, &psport,
                             MLm_vpm_api__get_sport);

    if (R_SUCCESS == status) {
        placp_sport_params = psport->placp_params;
        plpinfo_priority = LACP_AVL_FIRST(lacp_per_port_vars_tree);
        current_port_priority = placp_sport_params->lacp_params.actor_max_port_priority;

        while (plpinfo_priority) {
            if (plpinfo_priority->sport_handle == psport->handle &&
                plpinfo_priority->recv_fsm_state != RECV_FSM_DEFAULTED_STATE &&
                max_port_priority > plpinfo_priority->actor_oper_port_priority) {

                max_port_priority = plpinfo_priority->actor_oper_port_priority;
            }
            plpinfo_priority = LACP_AVL_NEXT(plpinfo_priority->avlnode);
        }
        // If max_port_priority changed, we need to reselect all interfaces
        if (current_port_priority != max_port_priority) {
            placp_sport_params->lacp_params.actor_max_port_priority = max_port_priority;
            pmsg.flags = placp_sport_params->lacp_params.flags;
            pmsg.sport_handle = psport->handle;
            mlacpVapiSportParamsChange(MLm_vpm_api__set_lacp_sport_params, &pmsg);
        }
    }
} /* update_max_port_priority */
