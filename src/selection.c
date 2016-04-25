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
#include <sys/types.h>

#include "lacp_cmn.h"
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

// OpenSwitch
#include "mvlan_lacp.h"

VLOG_DEFINE_THIS_MODULE(selection);

//*************************************************************
// List of LAGs for ease & debug.
//*************************************************************
struct NList *mlacp_lag_tuple_list;

extern int lacp_lag_port_match(void *, void *);

/*****************************************************************************
 *          Prototypes for static functions
 ****************************************************************************/
static LAG_Id_t *form_lag_id(lacp_per_port_variables_t *);
static int compare_lag_id (LAG_Id_t *, LAG_Id_t *);
static int is_port_partner_port(port_handle_t, LAG_t *const);
static void LAG_select_aggregator(LAG_t *const, lacp_per_port_variables_t *);
static void print_lag_id(LAG_Id_t *lag_id);

/*
 * Synopsis: Compares 2 LAG IDs.
 * Input  :
 *           first_lag_id -  first LAG ID
 *           second_lag_id - second LAG ID
 *
 * Returns:  TRUE - if comparision succeeds
 *           FALSE - if comparision fails
 */
static int
compare_lag_id(LAG_Id_t *first_lag_id, LAG_Id_t *second_lag_id)
{
    if (!first_lag_id || !second_lag_id) {
        return FALSE;
    }

    // Compare local system ID (priority + mac addr), port
    // key and port ID (port priority + port number)
    if ((first_lag_id->local_system_priority !=
         second_lag_id->local_system_priority) ||

        memcmp(first_lag_id->local_system_mac_addr,
               second_lag_id->local_system_mac_addr,
               MAC_ADDR_LENGTH) ||

        (first_lag_id->local_port_key !=
         second_lag_id->local_port_key) ||

        (first_lag_id->local_port_priority !=
         second_lag_id->local_port_priority) ||

        (first_lag_id->local_port_number !=
         second_lag_id->local_port_number)) {

        return FALSE;
    }

    // Compare remote system ID (priority + mac addr), port
    // key and port ID (port priority + port number)
    if ((first_lag_id->remote_system_priority !=
         second_lag_id->remote_system_priority) ||

        memcmp(first_lag_id->remote_system_mac_addr,
               second_lag_id->remote_system_mac_addr,
               MAC_ADDR_LENGTH) ||

        (first_lag_id->remote_port_key !=
         second_lag_id->remote_port_key) ||

        (first_lag_id->remote_port_priority !=
         second_lag_id->remote_port_priority) ||

        (first_lag_id->remote_port_number !=
         second_lag_id->remote_port_number)) {

        return FALSE;
    }

    // Compare local fallback
    if (first_lag_id->fallback != second_lag_id->fallback) {
        return FALSE;
    }

    return TRUE;
} // compare_lag_id

int
compare_port_handle(void* lag_port_struct1, void* lag_port_struct2)
{
    if (((lacp_lag_ppstruct_t *)lag_port_struct1)->lport_handle >
        ((lacp_lag_ppstruct_t *)lag_port_struct2)->lport_handle) {
        return 1;
    } else {
        return -1;
    }
} // compare_port_handle

//******************************************************************
// Function : LAG_selection
//******************************************************************
void
LAG_selection(lacp_per_port_variables_t *lacp_port)
{
    int lock;
    LAG_Id_t *lagId;
    LAG_t *lag;
    lacp_per_port_variables_t *plp;
    lacp_lag_ppstruct_t *plag_port_struct = NULL;
    struct NList *pdummy;

    RENTRY();

    if (lacp_port->debug_level & DBG_SELECT) {
        RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, lacp_port->lport_handle);
    }

    if (lacp_port->lacp_up == FALSE || lacp_port->selecting_lag == TRUE) {
        if (lacp_port->debug_level & DBG_SELECT) {
            RDBG("%s : FALSE and so returning\n", __FUNCTION__);
        }
        return;
    }

    lock = lacp_lock();
    lacp_port->selecting_lag = TRUE;

    lagId = form_lag_id(lacp_port);

    if (lagId == NULL) {
        lacp_port->selecting_lag = FALSE;
        lacp_unlock(lock);
        return;
    }

    if (lacp_port->debug_level & DBG_SELECT) {
        print_lag_id(lagId);
    }

    // lagId contains the admin and op keys for both the actor and partner
    // and is unique if the port is set to be individual and not aggregatable.
    lag = lacp_port->lag;

    /*1*/
    if (lag == NULL) {      /* This port does not belong to any LAG. */

        if (lacp_port->debug_level & DBG_SELECT) {
            RDBG("%s : this port (0x%llx) does not belong to any LAG\n",
                 __FUNCTION__,
                 lacp_port->lport_handle);
        }

        for (plp = LACP_AVL_FIRST(lacp_per_port_vars_tree);
             plp;
             plp = LACP_AVL_NEXT(plp->avlnode))
        {
            if (!plp->lag) {
                continue;
            }

            if (plp->lag->port_type != lacp_port->port_type) {
                continue;
            }

            if (lacp_port->debug_level & DBG_SELECT) {
                print_lag_id(plp->lag->LAG_Id);
            }

            // OpenSwitch: if partner info has not been received, treat it
            //        as no match.  We need this since we're automating
            //        LACP management.  We'll always try to LAG up if
            //        possible, but if far end doesn't run LACP, we
            //        cannot allow the two to LAG up; otherwise, it
            //        results in a LAG being created on our end, but
            //        two separate ports on the far end, causing loss
            //        of traffic.
            if (0 == memcmp(plp->partner_oper_system_variables.system_mac_addr,
                            default_partner_system_mac,
                            MAC_ADDR_LENGTH)) {
                continue;
            }

            if (compare_lag_id(plp->lag->LAG_Id, lagId) == FALSE) {
                continue;
            }

            lag = plp->lag;
            break;
        }

        /*2*/
        if (lag == NULL) {
            // No LAG found with the same LAG id.  Could be the first
            // port to join a new LAG or the only (individual) port
            // to form an individual LAG.
            if ((lag = (LAG_t *)malloc(sizeof(LAG_t))) == NULL) {
                free(lagId);
                VLOG_FATAL("%s : out of memory", __FUNCTION__);
                lacp_port->selecting_lag = FALSE;
                lacp_unlock(lock);
                exit(-1);
                return;
            }
            memset(lag, 0, sizeof(LAG_t));

            if (lacp_port->debug_level & DBG_SELECT) {
                RDBG("%s : no LAG found; create new LAG (lport 0x%llx)\n",
                     __FUNCTION__, lacp_port->lport_handle);
            }

            lag->port_type = lacp_port->port_type;
            lag->LAG_Id = lagId;
            lag->loop_back = loop_back_check(lacp_port) ? TRUE : FALSE;

            plag_port_struct = calloc(1, sizeof(lacp_lag_ppstruct_t));
            if (plag_port_struct == NULL) {
                VLOG_FATAL("%s : out of memory", __FUNCTION__);
                exit(-1);
            }
            plag_port_struct->lport_handle = lacp_port->lport_handle;
            lag->pplist = n_list_insert_sorted(lag->pplist,
                                               (void *)plag_port_struct,
                                               compare_port_handle);
            lacp_port->lag = lag;

            //*************************************************************
            // Insert this LAG into the list.
            //*************************************************************
            mlacp_lag_tuple_list = n_list_append(mlacp_lag_tuple_list, lag);

            //*************************************************************
            // Done.
            //*************************************************************
            if (lacp_port->debug_level & DBG_SELECT) {
                char lag_id_str[LAG_ID_STRING_SIZE];
                LAG_id_string(lag_id_str, lagId);
                RDBG("%s : Port Added (%llx) to new LAG, ID string = %s",
                     __FUNCTION__, lacp_port->lport_handle, lag_id_str);
            }

            LAG_select_aggregator(lag, lacp_port);
            lacp_port->selecting_lag = FALSE;
            lacp_unlock(lock);
            return;
        } // (2) if (lag == NULL)

        // Found a LAG with the same port type and LAG id.
        if (lacp_port->debug_level & DBG_SELECT) {
            RDBG("%s : found LAG with same port type & LAG id "
                 "(lport 0x%llx)\n",
                 __FUNCTION__,
                 lacp_port->lport_handle);
        }

        if (n_list_find_data(lag->pplist,
                             &lacp_lag_port_match,
                             &lacp_port->lport_handle) == NULL) {

             // Add the port to the LAG, only if this port is not
             // a loop back and not a partner port to any of the
             // ports in the LAG and aggregatable on both the actor
             // and partner sides.
            if ((lag->loop_back = loop_back_check(lacp_port)) == FALSE &&
                is_port_partner_port(lacp_port->lport_handle, lag) == 0 &&
                lacp_port->actor_oper_port_state.aggregation == AGGREGATABLE &&
                lacp_port->partner_oper_port_state.aggregation == AGGREGATABLE) {

                plag_port_struct = calloc(1, sizeof(lacp_lag_ppstruct_t));
                if (plag_port_struct == NULL) {
                    VLOG_FATAL("%s : out of memory", __FUNCTION__);
                    exit(-1);
                }
                plag_port_struct->lport_handle = lacp_port->lport_handle;
                lag->pplist = n_list_insert_sorted(lag->pplist,
                                                   (void *)plag_port_struct,
                                                   compare_port_handle);
                lacp_port->lag = lag;
                if (lacp_port->debug_level & DBG_SELECT) {
                    RDBG("%s : Port (0x%llx) Added to Existing LAG\n",
                         __FUNCTION__, lacp_port->lport_handle);
                }

                LAG_select_aggregator(lag, lacp_port);
            }

            free(lagId);
            lacp_port->selecting_lag = FALSE;
            lacp_unlock(lock);
            return;
        }

        VLOG_FATAL("%s : FATAL : How come it got here ?!", __FUNCTION__);
        exit(-1);

    } // (1) if (lag == NULL)

    if (lacp_port->debug_level & DBG_SELECT) {
        RDBG("%s : this port (lport 0x%llx) already belongs to LAG.%d\n",
             __FUNCTION__, lacp_port->lport_handle, (int)PM_HANDLE2LAG(lag->sp_handle));
    }

    // Should the port remain in its present LAG?
    //
    // Remove it from the present LAG and recurse to find out what LAG it
    // should join, if LAG ID is changed.
    //
    // Note that LAG ID will change if the actor or partner port is changed
    // from aggregatable to individual or visa versa.  Or disconnected from
    // the current partner port and connected to another port on another system.
    //
    // OpenSwitch: LAG id also needs to change if speed (therefore, port_type) changes.
    // Lag id needs to change if fallback changed so the interfaces can be
    // removed and the super port cleaned allowing the interface to attach to a
    // default partner

    pdummy = n_list_find_data(lag->pplist,
                              &lacp_lag_port_match,
                              &lacp_port->lport_handle);
    if (pdummy != NULL) {
        plag_port_struct = pdummy->data;
    }

    if (plag_port_struct &&
        (((lag->loop_back = loop_back_check(lacp_port)) == TRUE) ||
         (compare_lag_id(lag->LAG_Id, lagId) == FALSE) ||
         (lag->port_type != lacp_port->port_type))) {

        // Make selected UNSELECTED, and cause approp. event in
        // the mux machine.
        lacp_port->lacp_control.selected = UNSELECTED;
        LACP_mux_fsm(E2,
                     lacp_port->mux_fsm_state,
                     lacp_port);

        lacp_port->lacp_control.ready_n = FALSE;
        lag->pplist =  n_list_remove_data(lag->pplist, plag_port_struct);

        if (lacp_port->debug_level & DBG_SELECT) {
            RDBG("%s : Port (0x%llx) Removed from current LAG\n",
                 __FUNCTION__, lacp_port->lport_handle);
        }

        if (lag->pplist == NULL) {
            // It was the last port in the LAG, remove the whole LAG.

            // OpenSwitch: clear out sport params so it can be reused later.
            if (lag->sp_handle != 0) {
                mlacp_blocking_send_clear_aggregator(lag->sp_handle);
            }

            lacp_port->lag = NULL;
            free(lag->LAG_Id);

            // Remove the LAG from the tuple_list before we free it.
            mlacp_lag_tuple_list = n_list_remove_data(mlacp_lag_tuple_list,
                                                      lag);
            free(lag);

        } else if (lacp_port->debug_level & DBG_SELECT) {
            // --- OpenSwitch: DEBUG ONLY ---
            lacp_lag_ppstruct_t *ptmp;
            RDBG("LAG.%d not empty:  ", (int)PM_HANDLE2LAG(lag->sp_handle));
            N_LIST_FOREACH(lag->pplist, ptmp) {
                RDBG("      0x%llx", ptmp->lport_handle);
            } N_LIST_FOREACH_END(lag->pplist, ptmp);
        }

        lacp_port->lag = NULL;
        free(lagId);
        lacp_port->selecting_lag = FALSE;
        lacp_unlock(lock);

        // Only if LACP is enabled try to find/create
        // a suitable lag & aggregator for this port.
        if (lacp_port->debug_level & DBG_SELECT) {
            RDBG("%s : recursive call to LAG_selection\n", __FUNCTION__);
        }

        LAG_selection(lacp_port);

        return;
    }

    // All is well and no change is required.
    free(lagId);

    // Port is already in a LAG.  Select an aggregator
    // if one exists with the same keys.
    if (lacp_port->lacp_control.selected == UNSELECTED) {
        LAG_select_aggregator(lag, lacp_port);
    }

    lacp_port->selecting_lag = FALSE;
    lacp_unlock(lock);

    REXIT();

    if (lacp_port->debug_level & DBG_SELECT) {
        RDBG("%s : exit\n", __FUNCTION__);
    }
} // LAG_selection

//******************************************************************
// Function : LAG_select_aggregator
//******************************************************************
static void
LAG_select_aggregator(LAG_t *const lag, lacp_per_port_variables_t *lacp_port)
{
    RENTRY();

    if (lacp_port->debug_level & DBG_SELECT) {
        RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, lacp_port->lport_handle);
    }

    if (!lag) {
        return;
    }

    if (mlacp_blocking_send_select_aggregator(lag,lacp_port) == R_SUCCESS) {
        lacp_port->lacp_control.selected = SELECTED;
        // OpenSwitch: Save handle for clearing later.
        lag->sp_handle = lacp_port->sport_handle;
        LACP_mux_fsm(E1,
                     lacp_port->mux_fsm_state,
                     lacp_port);
    }

    REXIT();
} // LAG_select_aggregator


//**************************************************************
// Function : is_port_partner_port
//**************************************************************
// Returns 1 if port is a partner port in LAG, 0 otherwise.
static int
is_port_partner_port(port_handle_t lport_handle, LAG_t *const lag)
{
    lacp_per_port_variables_t *plpinfo;

    RDEBUG(DL_SELECT, "%s : lport_handle 0x%llx\n", __FUNCTION__, lport_handle);

    if (!lag || lag->pplist == NULL) {
        return 0;
    }

    for (plpinfo = LACP_AVL_FIRST(lacp_per_port_vars_tree);
         plpinfo;
         plpinfo = LACP_AVL_NEXT(plpinfo->avlnode)) {
        if (n_list_find_data(lag->pplist,
                             &lacp_lag_port_match,
                             &lport_handle) == NULL)
        {
            continue;
        }

        if (plpinfo->partner_oper_port_number == plpinfo->actor_admin_port_number) {
             return 1;
        }
    }

    return 0;
} // is_port_partner_port

//**************************************************************
// Function : loop_back_check
//**************************************************************
int
loop_back_check(lacp_per_port_variables_t *plpinfo)
{
    int status = FALSE;

    RDEBUG(DL_SELECT, "%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);

    // If the local system identifier is the same as the remote system
    // identifier then we have a loop back link. If loop back, store
    // the remote port number in the cgPort_t structure.
    if ((!(memcmp(plpinfo->actor_oper_system_variables.system_mac_addr,
                  plpinfo->partner_oper_system_variables.system_mac_addr,
                  MAC_ADDR_LENGTH))) &&
        (plpinfo->actor_oper_system_variables.system_priority ==
         plpinfo->partner_oper_system_variables.system_priority)) {

        /* Signal detected loop back */
        status = TRUE;
    }

    return status;
} // loop_back_check

//**************************************************************
// Function : form_lag_id
//**************************************************************
static LAG_Id_t *
form_lag_id(lacp_per_port_variables_t *lacp_port)
{
    LAG_Id_t *lagId;

    RENTRY();

    // Allocate memory for LAG ID.
    if (!(lagId = (LAG_Id_t *)malloc(sizeof(LAG_Id_t)))) {
        VLOG_FATAL("out of memory");
        return NULL;
    }

    // Zero out the LAG ID.
    memset(lagId, 0, sizeof(LAG_Id_t));

    // Local paramters.
    lagId->local_system_priority =
        lacp_port->actor_oper_system_variables.system_priority;

    memcpy(lagId->local_system_mac_addr,
           lacp_port->actor_oper_system_variables.system_mac_addr,
           sizeof(macaddr_3_t));

    lagId->local_port_key = lacp_port->actor_oper_port_key;

    // Partner paramters.
    lagId->remote_system_priority =
        lacp_port->partner_oper_system_variables.system_priority;

    memcpy(lagId->remote_system_mac_addr,
           lacp_port->partner_oper_system_variables.system_mac_addr,
           sizeof(macaddr_3_t));

    lagId->remote_port_key = lacp_port->partner_oper_key;

    // Check if the remote system's and the local system's Aggregation bit,
    //  1. Aggregatable - don't include port identifier in the LAG ID
    //  2. Individual   - include port identifier in the LAG ID
    if ((lacp_port->actor_oper_port_state.aggregation == INDIVIDUAL) ||
        (lacp_port->partner_oper_port_state.aggregation == INDIVIDUAL)) {

        lagId->local_port_priority = lacp_port->actor_oper_port_priority;
        lagId->local_port_number = lacp_port->actor_oper_port_number;
        lagId->remote_port_priority = lacp_port->partner_oper_port_priority;
        lagId->remote_port_number = lacp_port->partner_oper_port_number;
    }

    lagId->fallback = lacp_port->fallback_enabled;

    REXIT();

    return (lagId);
} // form_lag_id

//************************************************************
// Function : print_lag_id
//************************************************************
static void
print_lag_id(LAG_Id_t *lag_id)
{
    char lag_id_str[LAG_ID_STRING_SIZE];

    LAG_id_string(lag_id_str, lag_id);
    RDEBUG(DL_SELECT, "%s\n", lag_id_str);
} // print_lag_id
