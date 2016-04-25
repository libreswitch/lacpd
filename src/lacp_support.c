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
#include <strings.h>
#include <assert.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <lacp_cmn.h>
#include <avl.h>
#include <nlib.h>

#include "lacp_stubs.h"
#include <pm_cmn.h>
#include <lacp_cmn.h>
#include <mlacp_debug.h>
#include <lacp_fsm.h>

#include "lacp.h"
#include "lacp_support.h"
#include "mlacp_fproto.h"
#include "mvlan_sport.h"
#include "lacp_ops_if.h"
#include <vswitch-idl.h>

VLOG_DEFINE_THIS_MODULE(lacpd_support);

/*****************************************************************************
 *                    Global Variables Definition
 ****************************************************************************/
unsigned char my_mac_addr[MAC_ADDR_LENGTH] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint actor_system_priority = DEFAULT_SYSTEM_PRIORITY;

/* Global per port variables table */
lacp_avl_tree_t lacp_per_port_vars_tree;

extern struct NList *mlacp_lag_tuple_list;

/*****************************************************************************
 *          Prototypes for static functions
 ****************************************************************************/
static void initialize_per_port_variables(
                              lacp_per_port_variables_t *plpinfo,
                              unsigned short port_id,
                              unsigned long flags,
                              short port_key,
                              short port_priority,
                              short lacp_activity,
                              short lacp_timeout,
                              short lacp_aggregation,
                              int link_state,
                              int link_speed,
                              int hw_collecting,
                              short sys_priority,
                              char *sys_id);


/*****************************************************************************
 *
 *         Port initialization function
 *
 *
 ****************************************************************************/

/*----------------------------------------------------------------------
 * Function: LACP_initialize_port(int lport)
 *
 * Synopsis: Called whenever a port is added to a smarttrunk.
 *           Does the following:
 *             1. Initializes the per port LACP variables to their
 *                init time default values.
 *             2. Registers the LACP multicast address to the L2 manager.
 *             3. Adds the port to the approp. key group within the
 *                smarttrunk.
 *             4. Generates approp. init time events in the 3 state machines.
 *
 * Input  :  port - physical port number
 * Returns:  void
 *----------------------------------------------------------------------*/
void
LACP_initialize_port(port_handle_t lport_handle,
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
                     char *sys_id)
{
    lacp_per_port_variables_t   *plpinfo;
    lacp_per_port_variables_t   *plpinfo_priority;
    super_port_t                *psport;
    lacp_int_sport_params_t     *placp_sport_params;
    int                         status = R_SUCCESS;
    int                         max_port_priority = MAX_PORT_PRIORITY;

    RENTRY();

    /* RDEBUG exists in the caller ... */

    plpinfo = LACP_AVL_FIND(lacp_per_port_vars_tree, &lport_handle);

    /* Should not happen, but if LACP is already running
     * on this port, just kill it and restart with the latest
     * config info. */
    if (plpinfo != NULL) {
        status = mvlan_get_sport(plpinfo->sport_handle , &psport,
                                 MLm_vpm_api__get_sport);
        if (R_SUCCESS == status) {
            plpinfo_priority = LACP_AVL_FIRST(lacp_per_port_vars_tree);
            while (plpinfo_priority) {
                if (plpinfo_priority->lport_handle != plpinfo->lport_handle &&
                    plpinfo_priority->sport_handle == psport->handle &&
                    max_port_priority > plpinfo_priority->actor_admin_port_priority) {
                    max_port_priority = plpinfo_priority->actor_admin_port_priority;
                }
                plpinfo_priority = LACP_AVL_NEXT(plpinfo_priority->avlnode);
            }
            placp_sport_params = psport->placp_params;
            placp_sport_params->lacp_params.actor_max_port_priority = max_port_priority;

        }
        VLOG_ERR("Calling LACP_initialize_port when already "
                 "initialized?  port_id=%d  lport=0x%llx",
                 port_id, lport_handle);
        LACP_disable_lacp(lport_handle);
        plpinfo = NULL;
    }

    /* First time adding the lport. */
    RDEBUG(DL_INFO, "alloc data structure for lport 0x%llx\n", lport_handle);

    plpinfo = calloc(1, sizeof(lacp_per_port_variables_t));
    if (plpinfo == NULL) {
        VLOG_FATAL("out of memory");
        exit(-1);
    }

    plpinfo->lport_handle = lport_handle;
    LACP_AVL_INIT_NODE(plpinfo->avlnode, plpinfo, &(plpinfo->lport_handle));

    if (LACP_AVL_INSERT(lacp_per_port_vars_tree, plpinfo->avlnode) == FALSE) {
        VLOG_FATAL("avl_insert failed for handle 0x%llx", lport_handle);
        exit(-1);
    }

    /* Start lacpd with "-l" option to set this dynamically */
    /* OPS_TODO: convert to use VLOG. */
    plpinfo->debug_level = DBG_ALL;
    /* plpinfo->debug_level = (DBG_FATAL | DBG_WARNING | DBG_ERROR); */

    /********************************************************************
     *  Set the state machines in the BEGIN state initially.
     ********************************************************************/
    plpinfo->recv_fsm_state = RECV_FSM_BEGIN_STATE;
    plpinfo->mux_fsm_state = MUX_FSM_BEGIN_STATE;
    plpinfo->periodic_tx_fsm_state = PERIODIC_TX_FSM_BEGIN_STATE;
    plpinfo->hw_attached_to_mux = FALSE;

    /********************************************************************
     * set this based on link speed (or port type, which could be passed
     * over by CLI) ?
     ********************************************************************/
    plpinfo->actor_admin_port_key = htons(LACP_PORT_KEY_DEFAULT);
    plpinfo->actor_admin_port_priority = htons(LACP_PORT_PRIORITY_DEFAULT);

    /* These are sent on the wire as (single) bit fields. */
    plpinfo->actor_admin_port_state.lacp_activity = LACP_PORT_ACTIVITY_DEFAULT;
    plpinfo->actor_admin_port_state.lacp_timeout = LACP_PORT_TIMEOUT_DEFAULT;
    plpinfo->actor_admin_port_state.aggregation = LACP_PORT_AGGREGATION_DEFAULT;

    /* END: first time adding the lport. */

    /***************************************************************************
     *   Initialize the per port variables:
     *     actor -> port number, port priority, port key,
     *     actor -> system mac address, system priority,
     *     actor -> port state variables.
     *   Copies all the above admin variables to corresponding
     *   oper variables.
     ***************************************************************************/
    initialize_per_port_variables(plpinfo,
                                  port_id,
                                  flags,
                                  port_key,
                                  port_priority,
                                  activity,
                                  timeout,
                                  aggregation,
                                  link_state,
                                  link_speed,
                                  hw_collecting,
                                  sys_priority,
                                  sys_id);

    /***************************************************************************
     *   Register the LACP Mcast address with the L2 Manager,
     *   so that the local consume bit is set in the L2 entry.
     ***************************************************************************/
    register_mcast_addr(lport_handle);

    /***************************************************************************
     *    Generate appropriate initial events for the state machines.
     ***************************************************************************/
    /* Put the recv fsm in the initialize state. */
    LACP_receive_fsm(E8,
                     plpinfo->recv_fsm_state,
                     NULL,
                     plpinfo);

    /* If port has moved, put the fsm in initialize again state. */
    if (plpinfo->lacp_control.port_moved == TRUE) {
        LACP_receive_fsm(E3,
                         plpinfo->recv_fsm_state,
                         NULL,
                         plpinfo);
    }

    /* According to the spec, should only transition into expired state
       if port is enabled!  Adding a check here. */
    /* Go to expired state. */
    if (plpinfo->lacp_control.port_enabled == TRUE) {
        LACP_receive_fsm(E6,
                         plpinfo->recv_fsm_state,
                         NULL,
                         plpinfo);
        /* the above action routine for expired state will
         * kick off a current while timer, whose handler routine
         * will call
         * LACP_receive_fsm(E1, lacp_per_port_variables[i].recv_fsm_state);
         * for moving the state to Defaulted.
         */
    }

    plpinfo->lacp_control.begin = TRUE;

    /* put the periodic_tx fsm to No Periodic state */
    LACP_periodic_tx_fsm(E1,
                         plpinfo->periodic_tx_fsm_state,
                         plpinfo);

    /* put the mux fsm to detached state */
    LACP_mux_fsm(E7,
                 plpinfo->mux_fsm_state,
                 plpinfo);

    plpinfo->selecting_lag = FALSE;
    plpinfo->lacp_up = TRUE;

    REXIT();

} /* LACP_initialize_port */

/*----------------------------------------------------------------------
 * Function: LACP_update_port_params(lport_handle, flags, ...)
 *
 * Synopsis: Called whenever a port's dynamically changeable
 *           LACP parameter is modified.
 *----------------------------------------------------------------------*/
void
LACP_update_port_params(port_handle_t lport_handle,
                        unsigned long flags,
                        short timeout, int hw_collecting)
{
    lacp_per_port_variables_t *plpinfo;

    RENTRY();

    plpinfo = LACP_AVL_FIND(lacp_per_port_vars_tree, &lport_handle);

    if (plpinfo != NULL) {

        if (LACP_LPORT_TIMEOUT_FIELD_PRESENT & flags) {
            plpinfo->actor_admin_port_state.lacp_timeout = timeout;
            plpinfo->actor_oper_port_state.lacp_timeout =
                plpinfo->actor_admin_port_state.lacp_timeout;
        }

        if (LACP_LPORT_HW_COLL_STATUS_PRESENT & flags) {
            plpinfo->hw_collecting = hw_collecting;
            plpinfo->actor_oper_port_state.collecting = hw_collecting ? TRUE : FALSE;

            /**********************************************************************
             * Check if the forward exit condition prevail.
             * If both actor & partner are in sync, and partner is collecting,
             * transition the MUX FSM to COLL_DIST state.
             **********************************************************************/
            if ((plpinfo->lacp_control.selected == SELECTED) &&
                (plpinfo->partner_oper_port_state.synchronization == TRUE) &&
                (plpinfo->partner_oper_port_state.collecting == TRUE)) {

                if (plpinfo->mux_fsm_state == MUX_FSM_COLLECTING_STATE) {
                    LACP_mux_fsm(E8,
                                 plpinfo->mux_fsm_state,
                                 plpinfo);
                }
            }
        }

        // Inform the transmit state machine about the change.
        plpinfo->lacp_control.ntt = TRUE;

    } else {
        VLOG_ERR("Update LACP param: lport_handle 0x%llx not found",
                 lport_handle);
        return;
    }

    REXIT();

} /* LACP_update_port_params */

//***************************************************************
// Function : initialize_per_port_variables
//***************************************************************
static void
initialize_per_port_variables(lacp_per_port_variables_t *plpinfo,
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
                              char *sys_id)
{
    RENTRY();

    /***************************************************************************
     * Set the actor's port number, priority and key value.
     ***************************************************************************/
    plpinfo->actor_admin_port_number = htons(port_id);

    if (flags & LACP_LPORT_PORT_KEY_PRESENT) {
        plpinfo->actor_admin_port_key = htons(port_key);
    } else {
        // nothing : leave the previous value (maybe default) as it is
    }

    plpinfo->port_type = htons(speed_to_lport_type(link_speed));

    if (flags & LACP_LPORT_PORT_PRIORITY_PRESENT) {
        plpinfo->actor_admin_port_priority = htons(port_priority);
    } else {
        // nothing : leave the previous value (maybe default) as it is
    }

    if (link_state == INTERFACE_LINK_STATE_UP) { // now it's operational_state (admin && link)
        plpinfo->lacp_control.port_enabled = TRUE;
    } else {
        plpinfo->lacp_control.port_enabled = FALSE;
    }

    /* There are override values that may be present, as noted below. */
    memcpy(plpinfo->actor_admin_system_variables.system_mac_addr,
           my_mac_addr,
           MAC_ADDR_LENGTH);

    plpinfo->actor_admin_system_variables.system_priority =
        htons(actor_system_priority);

    /***************************************************************************
     * Set the actor's port state variables.
     ***************************************************************************/
    if (flags & LACP_LPORT_ACTIVITY_FIELD_PRESENT) {
        plpinfo->actor_admin_port_state.lacp_activity = activity;
    }

    if (flags & LACP_LPORT_TIMEOUT_FIELD_PRESENT) {
        plpinfo->actor_admin_port_state.lacp_timeout = timeout;
    }

    if (flags & LACP_LPORT_AGGREGATION_FIELD_PRESENT) {
        plpinfo->actor_admin_port_state.aggregation = aggregation;
    }

    if (flags & LACP_LPORT_HW_COLL_STATUS_PRESENT) {
        plpinfo->hw_collecting = hw_collecting;
    }

    if (flags & LACP_LPORT_SYS_ID_FIELD_PRESENT) {
        plpinfo->actor_sys_id_override = TRUE;
        memcpy(plpinfo->actor_admin_system_variables.system_mac_addr, sys_id,
               MAC_ADDR_LENGTH);
    } else {
        plpinfo->actor_sys_id_override = FALSE;
    }

    if (flags & LACP_LPORT_SYS_PRIORITY_FIELD_PRESENT) {
        plpinfo->actor_prio_override = TRUE;
        plpinfo->actor_admin_system_variables.system_priority =
            htons(sys_priority);
    } else {
        plpinfo->actor_prio_override = FALSE;
    }

    RDEBUG(DL_INFO, "the updated settings are : "
           "port_key 0x%x port_priority 0x%x "
           "activity %d timeout %d aggregation %d "
           "hw_collecting %d",
           plpinfo->actor_admin_port_key,
           plpinfo->actor_admin_port_priority,
           plpinfo->actor_admin_port_state.lacp_activity,
           plpinfo->actor_admin_port_state.lacp_timeout,
           plpinfo->actor_admin_port_state.aggregation,
           plpinfo->hw_collecting);

    /***************************************************************************
     * Copy the operational state parameters
     * from the admin state paramter values.
     ***************************************************************************/
    set_actor_admin_parms_2_oper(plpinfo, ALL_PARAMS);

    /***************************************************************************
     * Set the partner's port number, priority and key value.
     ***************************************************************************/
    plpinfo->partner_admin_port_number   = htons(DEFAULT_PARTNER_PORT_NUMBER);
    plpinfo->partner_admin_port_priority = htons(DEFAULT_PARTNER_ADMIN_PORT_PRIORITY);
    plpinfo->partner_admin_key           = htons(DEFAULT_PARTNER_ADMIN_PORT_KEY);

    /***************************************************************************
     * Set the partner's system mac address and system priority.
     ***************************************************************************/
    memcpy(plpinfo->partner_admin_system_variables.system_mac_addr,
           default_partner_system_mac,
           MAC_ADDR_LENGTH);

    plpinfo->partner_admin_system_variables.system_priority =
        htons(DEFAULT_PARTNER_ADMIN_SYSTEM_PRIORITY);

    /***************************************************************************
     * Set the partner's port state variables.
     ***************************************************************************/
    plpinfo->partner_admin_port_state.lacp_activity = LACP_PASSIVE_MODE;
    plpinfo->partner_admin_port_state.lacp_timeout  = LONG_TIMEOUT;
    plpinfo->partner_admin_port_state.aggregation   = AGGREGATABLE; // as per RS 9.3

    /***************************************************************************
     * Copy the operational state parameters
     * from the admin state paramter values.
     ***************************************************************************/
    set_partner_admin_parms_2_oper(plpinfo, ALL_PARAMS);

    /***************************************************************************
     * Set the misc variables.
     ***************************************************************************/
    plpinfo->collector_max_delay = htons(DEFAULT_COLLECTOR_MAX_DELAY);

    /***************************************************************************
     * Enable LACP on the port, put port_moved to false.
     ***************************************************************************/
    plpinfo->lacp_control.port_moved = FALSE;

    REXIT();

} /* initialize_per_port_variables */

//***************************************************************
// Function : LACP_disable_lacp
//***************************************************************
void
LACP_disable_lacp(port_handle_t lport_handle)
{

    LAG_t *lag;
    struct NList *pdummy;
    lacp_lag_ppstruct_t *plag_port_struct = NULL;
    lacp_per_port_variables_t *plpinfo;

    RDEBUG(DL_INFO, "%s: lport_handle 0x%llx\n", __FUNCTION__, lport_handle);

    plpinfo = LACP_AVL_FIND(lacp_per_port_vars_tree, &lport_handle);
    if (plpinfo == NULL) {
        VLOG_ERR("Disable LACP: lport_handle 0x%llx not found",
                 lport_handle);
        return;
    }

    RDEBUG(DL_INFO, "lport_handle = 0x%llx, sport handle = 0x%llx\n",
           plpinfo->lport_handle,
           plpinfo->sport_handle);

    /*
     * Use locks here once we make it multithreaded
     */
    if(plpinfo->sport_handle != 0) {
        mlacp_blocking_send_disable_collect_dist(plpinfo);
        mlacp_blocking_send_detach_aggregator(plpinfo);
    }

    lag = plpinfo->lag;

    if (lag != NULL) {
        /*
         * Can be optimized by putting a pointer
         * to plag_port_struct in the perport variables
         */
        pdummy = n_list_find_data(lag->pplist,
                                  &lacp_lag_port_match,
                                  &plpinfo->lport_handle);
        if (pdummy != NULL) {
            plag_port_struct = pdummy->data;
            if(plag_port_struct != NULL) {
                lag->pplist =  n_list_remove_data(lag->pplist, plag_port_struct);
            }
        }

        if (lag->pplist == NULL) {
            /*
             * It was the last port in the LAG, remove the whole LAG.
             */

            // clear out sport params so it can be reused later.
            if (lag->sp_handle != 0) {
                mlacp_blocking_send_clear_aggregator(lag->sp_handle);
            }

            plpinfo->lag = NULL;
            free(lag->LAG_Id);

            //*********************************************************
            // Remove the LAG from the tuple_list before we free it.
            //*********************************************************
            mlacp_lag_tuple_list = n_list_remove_data(
                                               mlacp_lag_tuple_list,
                                               lag);
            free(lag);
        }
    }
    LACP_AVL_DELETE(lacp_per_port_vars_tree,plpinfo->avlnode);

    //****************************************************************
    // As LACP is per-port, go ahead & de-register for this port.
    //****************************************************************
    deregister_mcast_addr(plpinfo->lport_handle);

    free(plpinfo);

} /* LACP_disable_lacp */


/*----------------------------------------------------------------------
 * Function: set_actor_admin_parms_2_oper(int port_number)
 *
 * Synopsis: copies the actor's admin params to its oper params.
 *
 * Input  :
 * Returns:  void.
 *----------------------------------------------------------------------*/
void
set_actor_admin_parms_2_oper(lacp_per_port_variables_t *plpinfo,
                             int params_to_be_set)
{
    /***************************************************************************
     * Set the port number, priority and key value.
     ***************************************************************************/
    if (params_to_be_set & PORT_NUMBER_BIT) {
        plpinfo->actor_oper_port_number = plpinfo->actor_admin_port_number;
    }

    if (params_to_be_set & PORT_PRIORITY_BIT) {
        plpinfo->actor_oper_port_priority = plpinfo->actor_admin_port_priority;
    }

    if (params_to_be_set & PORT_KEY_BIT) {
        plpinfo->actor_oper_port_key = plpinfo->actor_admin_port_key;
    }

    /***************************************************************************
     * Set the system mac address and system priority.
     ***************************************************************************/
    if (params_to_be_set & PORT_SYSTEM_MAC_ADDR_BIT) {
        memcpy((char *)plpinfo->actor_oper_system_variables.system_mac_addr,
               (char *)plpinfo->actor_admin_system_variables.system_mac_addr,
               MAC_ADDR_LENGTH);
    }

    if (params_to_be_set & PORT_SYSTEM_PRIORITY_BIT) {
        plpinfo->actor_oper_system_variables.system_priority =
            plpinfo->actor_admin_system_variables.system_priority;
    }

    /***************************************************************************
     * Set the actor's port state variables.
     ***************************************************************************/
    if (params_to_be_set & PORT_STATE_LACP_ACTIVITY_BIT) {
        plpinfo->actor_oper_port_state.lacp_activity =
            plpinfo->actor_admin_port_state.lacp_activity;
    }

    /***************************************************************************
     *   If both the actor and the partner have their LACP modes as PASSIVE
     *   then put the periodic tx fsm in the NO_PERIODIC state.
     ***************************************************************************/
    if ((plpinfo->actor_oper_port_state.lacp_activity == LACP_PASSIVE_MODE) &&
        (plpinfo->partner_oper_port_state.lacp_activity == LACP_PASSIVE_MODE)) {
        LACP_periodic_tx_fsm(E8,
                             plpinfo->periodic_tx_fsm_state,
                             plpinfo);
    } else {
        if ((plpinfo->lacp_control.begin == TRUE) ||
            (plpinfo->lacp_control.port_enabled == FALSE)) {
            /* Do nothing, let periodic Tx machine stay in NO PERIODIC state */
        } else {
            LACP_periodic_tx_fsm(E2,
                                 plpinfo->periodic_tx_fsm_state,
                                 plpinfo);
        }
    }

    if (params_to_be_set & PORT_STATE_LACP_TIMEOUT_BIT) {
        plpinfo->actor_oper_port_state.lacp_timeout =
            plpinfo->actor_admin_port_state.lacp_timeout;
    }

    if (params_to_be_set & PORT_STATE_AGGREGATION_BIT) {
        plpinfo->actor_oper_port_state.aggregation =
            plpinfo->actor_admin_port_state.aggregation;
    }

} /* set_actor_admin_parms_2_oper */

/*----------------------------------------------------------------------
 * Function: set_partner_admin_parms_2_oper(int port_number)
 *
 * Synopsis: copies the partner's admin params to its oper params.
 *
 * Input  :
 * Returns:  void.
 *----------------------------------------------------------------------*/
void
set_partner_admin_parms_2_oper(lacp_per_port_variables_t *plpinfo,
                               int params_to_be_set)
{
    /***************************************************************************
     * Set the port number, priority and key value.
     ***************************************************************************/
    if (params_to_be_set & PORT_NUMBER_BIT) {
        plpinfo->partner_oper_port_number = plpinfo->partner_admin_port_number;
    }

    if (params_to_be_set & PORT_PRIORITY_BIT) {
        plpinfo->partner_oper_port_priority =
            plpinfo->partner_admin_port_priority;
    }

    if (params_to_be_set & PORT_KEY_BIT) {
        plpinfo->partner_oper_key = plpinfo->partner_admin_key;
    }

    /***************************************************************************
     * Set the system mac address and system priority.
     ***************************************************************************/
    if (params_to_be_set & PORT_SYSTEM_MAC_ADDR_BIT) {
        memcpy((char *)plpinfo->partner_oper_system_variables.system_mac_addr,
               (char *)plpinfo->partner_admin_system_variables.system_mac_addr,
               MAC_ADDR_LENGTH);
    }

    if (params_to_be_set & PORT_SYSTEM_PRIORITY_BIT) {
        plpinfo->partner_oper_system_variables.system_priority =
            plpinfo->partner_admin_system_variables.system_priority;
    }

    /***************************************************************************
     * Set the partner's port state variables.
     ***************************************************************************/
    if (params_to_be_set & PORT_STATE_LACP_ACTIVITY_BIT) {
        plpinfo->partner_oper_port_state.lacp_activity =
            plpinfo->partner_admin_port_state.lacp_activity;
    }

    /***************************************************************************
     * If both the actor and the partner have their LACP modes as PASSIVE
     * then put the periodic tx fsm in the NO_PERIODIC state.
     ***************************************************************************/
    if ((plpinfo->actor_oper_port_state.lacp_activity == LACP_PASSIVE_MODE) &&
        (plpinfo->partner_oper_port_state.lacp_activity == LACP_PASSIVE_MODE)) {
        LACP_periodic_tx_fsm(E8,
                             plpinfo->periodic_tx_fsm_state,
                             plpinfo);
    } else {
        /***************************************************************************
         *  If either one is not passive, then bring the periodic Tx machine out of
         *  NO PERIODIC state if it is that state.
         ***************************************************************************/
        if ((plpinfo->lacp_control.begin == TRUE) ||
            (plpinfo->lacp_control.port_enabled == FALSE)) {
            /* Do nothing, let periodic Tx machine stay in NO PERIODIC state */
        } else {
            LACP_periodic_tx_fsm(E2,
                                 plpinfo->periodic_tx_fsm_state,
                                 plpinfo);
        }
    }

    if (params_to_be_set & PORT_STATE_LACP_TIMEOUT_BIT) {
        plpinfo->partner_oper_port_state.lacp_timeout =
            plpinfo->partner_admin_port_state.lacp_timeout;
    }

    if (params_to_be_set & PORT_STATE_AGGREGATION_BIT) {
        plpinfo->partner_oper_port_state.aggregation =
            plpinfo->partner_admin_port_state.aggregation;
    }

} /* set_partner_admin_parms_2_oper */

//**************************************************************
// Function : print_lacp_fsm_state
//**************************************************************
void
print_lacp_fsm_state(port_handle_t lport_handle)
{
    int lock;
    char state_string[STATE_STRING_SIZE];
    lacp_per_port_variables_t *lacp_port;

    lacp_port = LACP_AVL_FIND(lacp_per_port_vars_tree, &lport_handle);
    if (lacp_port == NULL) {
        VLOG_ERR(" fsm print - can't find lport 0x%llx", lport_handle);
        return;
    }

    lock = lacp_lock();

    RDBG("logical port 0x%llx LACP Protocol State:\n", lport_handle);
    RDBG("   LACP State Machines:\n");

    rx_state_string(lacp_port->recv_fsm_state, state_string);
    RDBG("      Receive FSM:      %s\n", state_string);

    mux_state_string(lacp_port->mux_fsm_state, state_string);
    RDBG("      Mux FSM:      %s ", state_string);

    if ((lacp_port->mux_fsm_state == MUX_FSM_ATTACHED_STATE) ||
        (lacp_port->mux_fsm_state == MUX_FSM_COLLECTING_DISTRIBUTING_STATE)) {
        ulong lag_id;

        //*************************************************************
        // Display the name of the LAG (super_port) to which we're attached !
        //*************************************************************
        lag_id = PM_GET_SPORT_ID(lacp_port->sport_handle);

        RDBG("(Attached to LAG 0x%llx [LAG.%ld])\n",
             lacp_port->sport_handle, lag_id);
    } else {
        RDBG("\n");
    }

    periodic_tx_state_string(lacp_port->periodic_tx_fsm_state, state_string);
    RDBG("      Periodic Tx FSM:   %s\n", state_string);
    RDBG("   Control Variables\n");
    RDBG("      BEGIN:         %s\n", lacp_port->lacp_control.begin ? "TRUE" : "FALSE");
    RDBG("      Lacp Up:      %s\n", lacp_port->lacp_up ? "TRUE" : "FALSE");
    RDBG("      Ready_N:      %s\n", lacp_port->lacp_control.ready_n ? "TRUE" : "FALSE");
    RDBG("      Selected:      %s\n", lacp_port->lacp_control.selected ? "SELECTED" : "UNSELECTED");
    RDBG("      Port_moved:      %s\n", lacp_port->lacp_control.port_moved ? "TRUE" : "FALSE");
    RDBG("      NTT:         %s\n", lacp_port->lacp_control.ntt ? "TRUE" : "FALSE");
    RDBG("      port_enabled:      %s\n", lacp_port->lacp_control.port_enabled ? "TRUE" : "FALSE");
    RDBG("      PartnerSync:      %s\n", lacp_port->partner_oper_port_state.synchronization ?
                                         "TRUE" : "FALSE");
    RDBG("      PartnerCollect:      %s\n", lacp_port->partner_oper_port_state.collecting ?
                                         "TRUE" : "FALSE");
    RDBG("   Timer counters\n");
    RDBG("      periodic tx timer:   %d\n", lacp_port->periodic_tx_timer_expiry_counter);
    RDBG("      current while timer:   %d\n", lacp_port->current_while_timer_expiry_counter);
    RDBG("      wait while timer:   %d\n", lacp_port->wait_while_timer_expiry_counter);

    lacp_unlock(lock);

} /* print_lacp_fsm_state */

/*----------------------------------------------------------------------
 * Function: rx_state_string(int state_number, char *string)
 * Synopsis: Finds out the Rx state and stores the approp. string in the
 *           buffer.
 *
 * Input  :  int - state number, char * - buffer to store the string
 * Returns:  void
 *----------------------------------------------------------------------*/
void
rx_state_string(int state_number, char *string)
{
    switch (state_number) {
    case RECV_FSM_BEGIN_STATE :
        sprintf(string, "Begin State");
        break;
    case RECV_FSM_CURRENT_STATE :
        sprintf(string, "Current State");
        break;
    case RECV_FSM_EXPIRED_STATE :
        sprintf(string, "Expired State");
        break;
    case RECV_FSM_DEFAULTED_STATE :
        sprintf(string, "Defaulted State");
        break;
    case RECV_FSM_LACP_DISABLED_STATE :
        sprintf(string, "LACP Disabled State");
        break;
    case RECV_FSM_PORT_DISABLED_STATE :
        sprintf(string, "Port Disabled State");
        break;
    case RECV_FSM_INITIALIZE_STATE :
        sprintf(string, "Initialize State");
        break;
    default:
        sprintf(string, "Unknown State");
        break;
    }
} /* rx_state_string */

/*----------------------------------------------------------------------
 * Function: mux_state_string(int state_number, char *string)
 * Synopsis: Finds out the mux state and stores the approp. string in the
 *           buffer.
 *
 * Input  :  int - state number, char * - buffer to store the string
 * Returns:  void
 *----------------------------------------------------------------------*/
void
mux_state_string(int state_number, char *string)
{
    switch (state_number) {
    case MUX_FSM_BEGIN_STATE :
        sprintf(string, "Begin State");
        break;
    case MUX_FSM_DETACHED_STATE :
        sprintf(string, "Detached State");
        break;
    case MUX_FSM_WAITING_STATE :
        sprintf(string, "Waiting State");
        break;
    case MUX_FSM_ATTACHED_STATE :
        sprintf(string, "Attached State");
        break;
    case MUX_FSM_COLLECTING_STATE :
        sprintf(string, "Collecting State");
        break;
    case MUX_FSM_COLLECTING_DISTRIBUTING_STATE :
        sprintf(string, "Collecting_Distributing State");
        break;
    default:
        sprintf(string, "Unknown State");
        break;
    }
} /* mux_state_string */

/*----------------------------------------------------------------------
 * Function: periodic_tx_state_string
 * Synopsis: Finds out the periodic Tx state and stores the approp.
 *           string in the buffer.
 *
 * Input  :  int - state number, char * - buffer pointer to store the string
 * Returns:  void
 *----------------------------------------------------------------------*/
void
periodic_tx_state_string(int state_number, char *string)
{
    switch (state_number) {
    case PERIODIC_TX_FSM_BEGIN_STATE :
        sprintf(string, "Begin State");
        break;
    case PERIODIC_TX_FSM_NO_PERIODIC_STATE :
        sprintf(string, "No Periodic State");
        break;
    case PERIODIC_TX_FSM_FAST_PERIODIC_STATE :
        sprintf(string, "Fast Periodic State");
        break;
    case PERIODIC_TX_FSM_SLOW_PERIODIC_STATE :
        sprintf(string, "Slow Periodic State");
        break;
    case PERIODIC_TX_FSM_PERIODIC_TX_STATE :
        sprintf(string, "Periodic Tx State");
        break;
    default:
        sprintf(string, "Unknown State");
        break;
    }
} /* periodic_tx_state_string */



//************************************************************
// Function : LAG_id_string
//************************************************************
/*
 * On return 'str' will have the string format of the LAG id.
 */
void
LAG_id_string(char *const str, LAG_Id_t *const lag_id)
{
    char local_system_mac_addr_str[MAC_STRING_ADDR_SIZE];
    char remote_system_mac_addr_str[MAC_STRING_ADDR_SIZE];

    if (!lag_id) {
        *str = '\0';
        return;
    }

    L2_hexmac_to_strmac((u8_t*)lag_id->local_system_mac_addr,
                        local_system_mac_addr_str,
                        sizeof(local_system_mac_addr_str),
                        L2_MAC_TWOxSIX);
    L2_hexmac_to_strmac((u8_t*)lag_id->remote_system_mac_addr,
                        remote_system_mac_addr_str,
                        sizeof(remote_system_mac_addr_str),
                        L2_MAC_TWOxSIX);
    snprintf(str, LAG_ID_STRING_SIZE, "[(%d, %s, %d, %d, %d), (%d, %s, %d, %d, %d)]",
            ntohs(lag_id->local_system_priority),
            local_system_mac_addr_str,
            ntohs(lag_id->local_port_key),
            ntohs(lag_id->local_port_priority),
            ntohs(lag_id->local_port_number),
            ntohs(lag_id->remote_system_priority),
            remote_system_mac_addr_str,
            ntohs(lag_id->remote_port_key),
            ntohs(lag_id->remote_port_priority),
            ntohs(lag_id->remote_port_number));

} /* LAG_id_string */


/********************************************************************
 *  Semaphore routines
 ********************************************************************/
inline int
lacp_lock(void)
{
    return(0);
}

inline void
lacp_unlock(int lock __attribute__ ((unused)))
{
}

//********************************************************************
// Function : display_lacpdu
//********************************************************************
void
display_lacpdu(lacpdu_payload_t *lacpdu_payload,
           char *src_mac,
           char *dst_mac,
           int type)
{
  char mac_addr_str[MAC_STRING_ADDR_SIZE];

  L2_hexmac_to_strmac((u8_t*)dst_mac, mac_addr_str, sizeof(mac_addr_str), L2_MAC_TWOxSIX);
  printf("Dst MAC: %s\n", mac_addr_str);
  L2_hexmac_to_strmac((u8_t*)src_mac, mac_addr_str, sizeof(mac_addr_str), L2_MAC_TWOxSIX);
  printf("Src MAC: %s\n", mac_addr_str);

  printf("Type: 0x%x \n", type);
  printf("SubType: 0x%x\n", lacpdu_payload->subtype);
  printf("Version Number: %d\n", lacpdu_payload->version_number);
  printf("TLV type Actor: %d\n", lacpdu_payload->tlv_type_actor);
  printf("Actor Info. length: %d\n", lacpdu_payload->actor_info_length);
  printf("Actor System Priority: %d\n", ntohs(lacpdu_payload->actor_system_priority));

  L2_hexmac_to_strmac((u8_t*)lacpdu_payload->actor_system,
		              mac_addr_str, sizeof(mac_addr_str), L2_MAC_TWOxSIX);
  printf("Actor system MAC : %s\n", mac_addr_str);
  printf("Actor Key: %d\n", ntohs(lacpdu_payload->actor_key));
  printf("Actor Port Priority: %d\n", ntohs(lacpdu_payload->actor_port_priority));
  printf("Actor Port : %d\n", ntohs(lacpdu_payload->actor_port));

  printf("TLV type Partner: %d\n", lacpdu_payload->tlv_type_partner);
  printf("Partner Info. length: %d\n", lacpdu_payload->partner_info_length);
  printf("Partner System Priority: %d\n", ntohs(lacpdu_payload->partner_system_priority));

  L2_hexmac_to_strmac((u8_t*)lacpdu_payload->partner_system,
		              mac_addr_str, sizeof(mac_addr_str), L2_MAC_TWOxSIX);
  printf("Partner system MAC: %s\n", mac_addr_str);
  printf("Partner Key: %d\n", ntohs(lacpdu_payload->partner_key));
  printf("Partner Port Priority: %d\n", ntohs(lacpdu_payload->partner_port_priority));
  printf("Partner Port : %d\n", ntohs(lacpdu_payload->partner_port));

  printf("TLV type Collector Information: %d\n", lacpdu_payload->tlv_type_collector);
  printf("Collector Info. length: %d\n", lacpdu_payload->collector_info_length);
  printf("CollectorMaxDelay: %d\n", ntohs(lacpdu_payload->collector_max_delay));
  printf("TLV type Terminator: %d\n", lacpdu_payload->tlv_type_terminator);
  printf("Terminator Info. length: %d\n", lacpdu_payload->terminator_length);

} /* display_lacpdu */



//*****************************************************************
// Function : mlacpVapiLinkUp
// Note : the functionality in "port_link_state()" will be here.
//*****************************************************************
void
mlacpVapiLinkUp(port_handle_t lport_handle, int speed)
{
    int lock;
    lacp_per_port_variables_t *plpinfo;
    enum PM_lport_type new_lport_type;

    RDEBUG(DL_INFO, "%s: lport_handle 0x%llx\n", __FUNCTION__, lport_handle);

    plpinfo = LACP_AVL_FIND(lacp_per_port_vars_tree, &lport_handle);
    if (plpinfo == NULL) {
        VLOG_ERR("link up but can't find lport 0x%llx", lport_handle);
        return;
    }

    assert(plpinfo->lacp_up == TRUE);

    lock = lacp_lock();

    new_lport_type = htons(speed_to_lport_type(speed));

    if (new_lport_type != plpinfo->port_type) {
        // We support dynamic changes to link speed.
        // If a port links up with a different speed, we need
        // to force a change into unselected state, which
        // will result in a reselect.
        plpinfo->lacp_control.selected = UNSELECTED;
        LACP_mux_fsm(E2,
                     plpinfo->mux_fsm_state,
                     plpinfo);
    }

    // Save the new lport_type (based on operating link speed).
    plpinfo->port_type = new_lport_type;
    plpinfo->lacp_control.port_enabled = TRUE;

    LACP_receive_fsm(E6,
                     plpinfo->recv_fsm_state,
                     NULL,
                     plpinfo);

    /*
     * Generate an Event in the periodic Tx machine if appropriate
     * conditions prevail.
     */

    //********************************************************************
    // Assuming that lacp_enabled field is TRUE always and so removed
    // the check below. Also, port_enabled was set right here (above) and
    // so removed that too from below.
    //********************************************************************

    if ((plpinfo->lacp_control.begin == FALSE) &&
        ((plpinfo->actor_oper_port_state.lacp_activity != LACP_PASSIVE_MODE) ||
         (plpinfo->partner_oper_port_state.lacp_activity != LACP_PASSIVE_MODE))) {
        /* UCT to FAST_PERIODIC state. */
        LACP_periodic_tx_fsm(E2,
                             plpinfo->periodic_tx_fsm_state,
                             plpinfo);
    }

    lacp_unlock(lock);

} /* mlacpVapiLinkUp */

//*****************************************************************
// Function : mlacpVapiLinkDown
//*****************************************************************
void
mlacpVapiLinkDown(port_handle_t lport_handle)
{
    lacp_per_port_variables_t *plpinfo;

    RDEBUG(DL_INFO, "%s: lport_handle 0x%llx\n", __FUNCTION__, lport_handle);

    plpinfo = LACP_AVL_FIND(lacp_per_port_vars_tree, &lport_handle);
    if (plpinfo == NULL) {
        VLOG_ERR("link down, but can't find lport 0x%llx", lport_handle);
        return;
    }

    plpinfo->lacp_control.port_enabled = FALSE;

    assert(plpinfo->lacp_up == TRUE);

    /* put the periodic_tx fsm to No Periodic state */
    LACP_periodic_tx_fsm(E1,
                         plpinfo->periodic_tx_fsm_state,
                         plpinfo);

    /*
     * Generate an Event in the Rx machine if appropriate conditions prevail
     */
    if ((plpinfo->lacp_control.begin == FALSE) &&
        (plpinfo->lacp_control.port_enabled == FALSE) &&
        (plpinfo->lacp_control.port_moved == FALSE)) {

        LACP_receive_fsm(E4,
                         plpinfo->recv_fsm_state,
                         NULL,
                         plpinfo);
    }

} /* mlacpVapiLinkDown */

//*****************************************************************
// Function : set_all_port_system_mac_addr
//*****************************************************************
void
set_all_port_system_mac_addr(void)
{
    lacp_per_port_variables_t *plpinfo;

    plpinfo = LACP_AVL_FIRST(lacp_per_port_vars_tree);

    while (plpinfo) {
        if (plpinfo->actor_sys_id_override == FALSE) {
            memcpy(plpinfo->actor_admin_system_variables.system_mac_addr, my_mac_addr,
                   MAC_ADDR_LENGTH);
            memcpy(plpinfo->actor_oper_system_variables.system_mac_addr, my_mac_addr,
                   MAC_ADDR_LENGTH);
        }
        plpinfo = LACP_AVL_NEXT(plpinfo->avlnode);
    }

} /* set_all_port_system_mac_addr */

//*****************************************************************
// Function : set_all_port_system_priority
//*****************************************************************
void
set_all_port_system_priority(void)
{
    lacp_per_port_variables_t *plpinfo;

    plpinfo = LACP_AVL_FIRST(lacp_per_port_vars_tree);

    while (plpinfo) {
        if (plpinfo->actor_prio_override == FALSE) {
            plpinfo->actor_admin_system_variables.system_priority =
                htons(actor_system_priority);
            plpinfo->actor_oper_system_variables.system_priority =
                plpinfo->actor_admin_system_variables.system_priority;
            /* Update interface status when a system setting changes */
            db_update_interface(plpinfo);
        }

        plpinfo = LACP_AVL_NEXT(plpinfo->avlnode);
    }

} /* set_all_port_system_priority */

/******************************************************************************
 * Function : set_lport_fallback_status
 *
 * We need to set new fallback status to all ports that belongs to an specific
 * LAG. Then we need to send the event "Fallback changed" to the Receive SM
 *****************************************************************************/
void
set_lport_fallback_status(port_handle_t lport_handle, int status)
{
    lacp_per_port_variables_t *plpinfo = NULL;

    plpinfo = LACP_AVL_FIND(lacp_per_port_vars_tree, &lport_handle);

    if (plpinfo != NULL) {
        plpinfo->fallback_enabled = status;

        // Send the event "fallback changed", this will call the defaulted
        // action if the current state is defaulted, this has to be done if
        // fallback change while the interfaces are defaulted
        LACP_receive_fsm(E9,
                         plpinfo->recv_fsm_state,
                         NULL,
                         plpinfo);

    } else {
        VLOG_ERR("Set lport fallback status: lport_handle 0x%llx not found",
                 lport_handle);
    }

} /* set_lport_fallback_status */

//*****************************************************************
// Function : set_lport_overrides
//*****************************************************************
void
set_lport_overrides(port_handle_t lport_handle, int prio, unsigned char *mac)
{
    lacp_per_port_variables_t *plpinfo;

    plpinfo = LACP_AVL_FIND(lacp_per_port_vars_tree, &lport_handle);

    if (plpinfo != NULL) {
        /* process priority */
        if (prio == 0 && plpinfo->actor_prio_override == TRUE) {
            plpinfo->actor_prio_override = FALSE;
            plpinfo->actor_admin_system_variables.system_priority =
                htons(actor_system_priority);
            plpinfo->actor_oper_system_variables.system_priority =
                plpinfo->actor_admin_system_variables.system_priority;
        } else if (prio != 0) {
            plpinfo->actor_prio_override = TRUE;
            plpinfo->actor_admin_system_variables.system_priority = htons(prio);
            plpinfo->actor_oper_system_variables.system_priority =
                plpinfo->actor_admin_system_variables.system_priority;
        }

        /* process system_id (mac) */
        if (mac[0] == 0 && mac[1] == 0 && mac[2] == 0 &&
            mac[3] == 0 && mac[4] == 0 && mac[5] == 0 &&
            plpinfo->actor_sys_id_override == TRUE) {
            plpinfo->actor_sys_id_override = FALSE;
            memcpy(plpinfo->actor_admin_system_variables.system_mac_addr,
                   my_mac_addr,
                   MAC_ADDR_LENGTH);
            memcpy(plpinfo->actor_oper_system_variables.system_mac_addr,
                   my_mac_addr,
                   MAC_ADDR_LENGTH);
        } else if (mac[0] != 0 || mac[1] != 0 || mac[2] != 0 ||
                   mac[3] != 0 || mac[4] != 0 || mac[5] != 0) {
            plpinfo->actor_sys_id_override = TRUE;
            memcpy(plpinfo->actor_admin_system_variables.system_mac_addr,
                   mac,
                   MAC_ADDR_LENGTH);
            memcpy(plpinfo->actor_oper_system_variables.system_mac_addr,
                   mac,
                   MAC_ADDR_LENGTH);
        }
    } else {
        VLOG_ERR("Set port overrides: lport_handle 0x%llx not found",
                 lport_handle);
    }
} /* set_lport_overrides */

//*****************************************************************
// Function : mlacpVapiSportParamsChange
// Aggregator parameters changed, detach all the lports
//*****************************************************************
void
mlacpVapiSportParamsChange(int msg __attribute__ ((unused)),
                           struct MLt_vpm_api__lacp_sport_params *pin_lacp_params)
{
    lacp_per_port_variables_t *plpinfo, *lacp_port;

    RDEBUG(DL_INFO, "%s: sport_handle 0x%llx\n", __FUNCTION__,
           pin_lacp_params->sport_handle);

    plpinfo = LACP_AVL_FIRST(lacp_per_port_vars_tree);

    while (plpinfo) {
        if (plpinfo->sport_handle == pin_lacp_params->sport_handle) {
            lacp_port = plpinfo;

            if (pin_lacp_params->flags &
                (LACP_LAG_PARTNER_SYSPRI_FIELD_PRESENT |
                 LACP_LAG_PARTNER_SYSID_FIELD_PRESENT)) {
                /*
                 * Make selected UNSELECTED, and cause approp. event in
                 * the mux machine.
                 */
                lacp_port->lacp_control.selected = UNSELECTED;
                LACP_mux_fsm(E2, lacp_port->mux_fsm_state, lacp_port);
                lacp_port->lacp_control.ready_n = FALSE;
            }
        }
        plpinfo = LACP_AVL_NEXT(plpinfo->avlnode);
    }

} /* mlacpVapiSportParamsChange */
