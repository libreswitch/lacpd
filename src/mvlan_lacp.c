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

/*****************************************************************************
   File               : mvlan_lacp.c
   Description        : This is  contains all the functions to deal with
                        LACP.
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include <mlacp_debug.h>
#include <lacp_cmn.h>
#include <pm_cmn.h>
#include <nlib.h>
#include "lacp_support.h"
#include "mlacp_fproto.h"
#include "lacp_ops_if.h"
#include "mvlan_sport.h"
#include "mvlan_lacp.h"
#include "lacp_fsm.h"

VLOG_DEFINE_THIS_MODULE(mvlan_lacp);

typedef enum match_type {
    EXACT_MATCH,
    PARTIAL_MATCH,
    PRIORITY_MATCH
} match_type_t;

static struct NList *placp_params_list;

/* OpenSwitch: matches port type, actor key, partner sys prio, partner sys id */
static int mvlan_match_aggregator(lacp_sport_params_t *psport_param,
                                  struct MLt_vpm_api__lacp_match_params *plag_param,
                                  match_type_t match);

/*-----------------------------------------------------------------------------
 * mvlan_api_modify_sport_params   --
 *
 *        placp_params - The params for this aggrator
 *        operation    -  Set/unset the lacp parameters.
 *
 * Description  -- This function  sets the parameters for the smart trunk to
 *                 run the LACP.  Once the  parameters are set  logical ports
 *                 cannot be added/deleted statically.
 *
 * Side effects --
 *
 * Return value --
 *            R_SUCCESS - on success
 *---------------------------------------------------------------------------*/
int
mvlan_api_modify_sport_params(struct MLt_vpm_api__lacp_sport_params *placp_params,
                              int operation)
{
    int status = R_SUCCESS;

    if (operation == MLm_vpm_api__set_lacp_sport_params) {

        // First validate if we can set the sport parameters.
        status = mvlan_api_validate_set_sport_params(placp_params);

        if (status != R_SUCCESS) {
            RDEBUG(DL_VPM, "validate_set_sport_params failed with %d\n", status);
            goto end;
        }

        // All validated so just set the parameters.
        status = mvlan_set_sport_params(placp_params);

        if (status != R_SUCCESS) {
            RDEBUG(DL_VPM, "set_sport_params failed with %d\n", status);
            goto end;
        }

    } else {
        // First validate if we can uset the sport parameters.
        status = mvlan_api_validate_unset_sport_params(placp_params);

        if (status != R_SUCCESS) {
            goto end;
        }

        // All validated so just set the parameters.
        status = mvlan_unset_sport_params(placp_params);

        if (status != R_SUCCESS) {
            goto end;
        }
    }

end:
    return status;

} // mvlan_api_modify_sport_params

/*-----------------------------------------------------------------------------
 * mvlan_api_validate_unset_sport_params   --
 *
 *        placp_params - The params for this aggrator
 *
 * Description  -- This function validates whether we can unset the lacp
 *                 parameters on the super port.
 *
 * Side effects --
 *
 * Return value --
 *
 *            R_SUCCESS - on success
 *---------------------------------------------------------------------------*/
int
mvlan_api_validate_unset_sport_params(struct MLt_vpm_api__lacp_sport_params *placp_params)
{
    int                     status = R_SUCCESS;
    super_port_t            *psport;
    lacp_int_sport_params_t *placp_sport_params;

    status = mvlan_get_sport(placp_params->sport_handle, &psport,
                             MLm_vpm_api__get_sport);

    if (status != R_SUCCESS) {
        VLOG_ERR("could not find sport handle 0x%llx",
                 placp_params->sport_handle);
        goto end;
    }

    // We must have the sport parameters set to unset it.
    if (psport->placp_params == NULL ) {
        RDEBUG(DL_VPM, "mvlan_api_validate_unset_sport_params: The specified"
               " super port has no lacp parameters set\n");

        status = MVLAN_LACP_SPORT_PARAMS_NOT_FOUND;
        goto end;
    }

    //******************************************************************
    // If the user has set partner sys id/pri or aggr_type (i.e. the 3
    // params other than the "required tuple") and not yet negated them,
    // we need to fail it, so that those cmds don't hang around in the
    // CLI config - this could be polished as required, later.
    //******************************************************************
    placp_sport_params = psport->placp_params;

    if (placp_sport_params->lacp_params.flags &
        (LACP_LAG_AGGRTYPE_FIELD_PRESENT |
         LACP_LAG_PARTNER_SYSPRI_FIELD_PRESENT |
         LACP_LAG_PARTNER_SYSID_FIELD_PRESENT)) {
        RDEBUG(DL_VPM, "negate the partner-sys-priority/id & "
               "aggr-type commands before attempting to delete the %s\n",
               psport->name);

        status = MVLAN_LACP_SPORT_PARAMS_SET;
        goto end;
    }

    // Eventhough  ports are added dynamically, user must
    // remove the ports from this sport before we allow
    // to negate the parameters
    if ((psport->num_lports > 0) && !(placp_params->flags &
                                      (LACP_LAG_PARTNER_SYSPRI_FIELD_PRESENT |
                                       LACP_LAG_PARTNER_SYSID_FIELD_PRESENT))) {
        RDEBUG(DL_VPM, "sport (0x%llx) has logical ports attached to it\n",
               psport->handle);

        status = MVLAN_SPORT_LPORT_ATTACHED;
        goto end;
    }

end:
    return status;

} // mvlan_api_validate_unset_sport_params

/*-----------------------------------------------------------------------------
 * mvlan_api_validate_set_sport_params   --
 *
 *        placp_params - The params for this aggrator
 *
 * Description  -- This function  validates whether we can set the lacp
 *                 parameters on the super port.
 *
 * Side effects --
 *
 * Return value --
 *
 *            R_SUCCESS - on success
 *            MVLAN_LACP_SPORT_PARAMS_SET
 *            MVLAN_SPORT_LPORT_ATTACHED
 *            MVLAN_LACP_DUPLICATE_SPORT_PARAMS
 *---------------------------------------------------------------------------*/
int
mvlan_api_validate_set_sport_params(struct MLt_vpm_api__lacp_sport_params *placp_params)
{
    int          status = R_SUCCESS;
    super_port_t *psport;
    char         *mac;

    // First validate that the specified aggregate ports exist.
    status = mvlan_get_sport(placp_params->sport_handle, &psport,
                             MLm_vpm_api__get_sport);

    if (status != R_SUCCESS) {
        VLOG_ERR("could not find sport handle 0x%llx",
                 placp_params->sport_handle);
        goto end;
    }

    if ((psport->num_lports > 0) && !(placp_params->flags &
                                      (LACP_LAG_PARTNER_SYSPRI_FIELD_PRESENT |
                                       LACP_LAG_PARTNER_SYSID_FIELD_PRESENT))) {
        RDEBUG(DL_VPM, "mvlan_api_validate_set_sport_params: The specified"
               " super port has logical ports attached to it\n");

        status = MVLAN_SPORT_LPORT_ATTACHED;
        goto end;
    }

    RDEBUG(DL_VPM, "flags 0x%x, port_type %d, actor_key %d, "
           " partner_key %d, aggr_type %d, partner_sys_pri %d, ",
           placp_params->flags,
           placp_params->port_type,
           placp_params->actor_key,
           placp_params->partner_key,
           placp_params->aggr_type,
           placp_params->partner_system_priority);

    mac = placp_params->partner_system_id;
    RDEBUG(DL_VPM, "partner_sys_id %x:%x:%x:%x:%x:%x\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if (psport->placp_params == NULL) {

        // When set the very first time, the tuple of
        // port_type/actor_key must be specified.
        if (!((placp_params->flags & LACP_LAG_PORT_TYPE_FIELD_PRESENT) &&
              (placp_params->flags & LACP_LAG_ACTOR_KEY_FIELD_PRESENT))) {
            RDEBUG(DL_VPM, "port_type, actor_key, partner_key must "
                   "be set before other params can be specified\n");
            status = MVLAN_LACP_SPORT_KEY_NOT_FOUND;
            goto end;
        }

        // OpenSwitch NOTE:
        // OPS_TODO: Update this comment to reflect lacpd implementation.
        // LACPd requires each LAG to have unique port type, actor key,
        // and partner key.  For OpenSwitch, we're allowing multiple LAGs to
        // have the same actor key in order to automate grouping of as
        // many ports as we can.  This is the recommended selection logic
        // in 802.3ad spec (section 43.4.14.2).
        //
        // As such, removed further validation of unique LAG attributes.
    }

end:
    return status;

} // mvlan_api_validate_set_sport_params

/*-----------------------------------------------------------------------------
 * mvlan_set_sport_params   --
 *
 *        placp_params - The params for this aggrator
 *
 * Description  -- This function sets the lacp parameters on the superport.
 *
 * Side effects --
 *
 *     NOTE that this function expects that all parameters are validated at
 *          this point.
 *
 * Return value --
 *
 *            R_SUCCESS - on success
 *---------------------------------------------------------------------------*/
int
mvlan_set_sport_params(struct MLt_vpm_api__lacp_sport_params *pin_lacp_params)
{
    int                     status = R_SUCCESS;
    super_port_t            *psport;
    lacp_int_sport_params_t *placp_sport_params;
    int                     first_time = FALSE;
    int                     partner_param_changed = 0;

    status = mvlan_get_sport(pin_lacp_params->sport_handle, &psport,
                             MLm_vpm_api__get_sport);
    if(status != R_SUCCESS) {
        goto end;
    }

    if (psport->placp_params == NULL) {
        // Alloc only the first time. Note that the parameters could be
        // specified one at a time.
        placp_sport_params = (lacp_int_sport_params_t *)malloc(sizeof(lacp_int_sport_params_t));

        if (placp_sport_params == NULL ) {
            VLOG_ERR("mvlan_set_sport_params: No mem");
            status = MVLAN_SPORT_NO_MEM;
            goto end;

        } else {
            memset(placp_sport_params, 0, sizeof(lacp_int_sport_params_t));
        }

        // The very first time it's guaranteed to have (only) the tuple.
        // No need for default values etc.
        // As validation has been done, no need for assert etc.
        placp_sport_params->lacp_params.port_type = htons(pin_lacp_params->port_type);
        placp_sport_params->lacp_params.actor_key = htons(pin_lacp_params->actor_key);
        placp_sport_params->lacp_params.actor_max_port_priority = htons(pin_lacp_params->actor_max_port_priority);
        placp_sport_params->lacp_params.partner_max_port_priority = htons(pin_lacp_params->partner_max_port_priority);

        // Also, by default set the aggr_type as Aggregateable.
        placp_sport_params->lacp_params.aggr_type = LACP_LAG_DEFAULT_AGGR_TYPE;

        first_time = TRUE;

    } else {
        placp_sport_params = psport->placp_params;

        // Now we do allow the tuple key params also to be specified again
        // without having to negate (07/14/2002)

        if (pin_lacp_params->flags & LACP_LAG_PORT_TYPE_FIELD_PRESENT) {
            placp_sport_params->lacp_params.port_type = htons(pin_lacp_params->port_type);
        }

        if (pin_lacp_params->flags & LACP_LAG_ACTOR_KEY_FIELD_PRESENT) {
            placp_sport_params->lacp_params.actor_key = htons(pin_lacp_params->actor_key);
        }

        if (pin_lacp_params->flags & LACP_LAG_PARTNER_KEY_FIELD_PRESENT) {
            placp_sport_params->lacp_params.partner_key = htons(pin_lacp_params->partner_key);
        }

        if (pin_lacp_params->flags & LACP_LAG_AGGRTYPE_FIELD_PRESENT) {
            placp_sport_params->lacp_params.aggr_type = pin_lacp_params->aggr_type;
        }

        if (pin_lacp_params->flags & LACP_LAG_ACTOR_PORT_PRIORITY_FIELD_PRESENT) {
            placp_sport_params->lacp_params.actor_max_port_priority = pin_lacp_params->actor_max_port_priority;
        }

        if (pin_lacp_params->flags & LACP_LAG_PARTNER_PORT_PRIORITY_FIELD_PRESENT) {
            placp_sport_params->lacp_params.partner_max_port_priority = pin_lacp_params->partner_max_port_priority;
        }

        if (pin_lacp_params->flags & LACP_LAG_PARTNER_SYSPRI_FIELD_PRESENT) {
            if ((placp_sport_params->lacp_params.partner_system_priority !=
                 htons(pin_lacp_params->partner_system_priority))) {
                partner_param_changed++;
            }

            placp_sport_params->lacp_params.partner_system_priority  =
                htons(pin_lacp_params->partner_system_priority);
        }

        if (pin_lacp_params->flags & LACP_LAG_PARTNER_SYSID_FIELD_PRESENT) {
            if ((memcmp(placp_sport_params->lacp_params.partner_system_id,
                        pin_lacp_params->partner_system_id,
                        sizeof(macaddr_3_t)) != 0)) {
                partner_param_changed++;
            }

            memcpy(placp_sport_params->lacp_params.partner_system_id,
                   pin_lacp_params->partner_system_id,
                   sizeof(pin_lacp_params->partner_system_id));
        }

        first_time = FALSE;
    }

    if (pin_lacp_params->negation == 0) {
        placp_sport_params->lacp_params.flags |= pin_lacp_params->flags;

    } else {
        // This situation will arise only when the non-required params
        // partner sys is/pri and aggr_type are negated : the negation
        // of the "required tuple" will arrive as unset command by itself.
        placp_sport_params->lacp_params.flags &= ~(pin_lacp_params->flags);
    }

    if (first_time == TRUE) {
        placp_sport_params->psport = psport;
        placp_params_list = n_list_insert((struct NList *)
                                          placp_params_list,
                                          (void *)placp_sport_params,0);
        psport->placp_params = placp_sport_params;

        RDEBUG(DL_VPM, "created new set of aggr params (%s)\n", psport->name);

    } else {
        RDEBUG(DL_VPM, "updated aggr params (%s)\n", psport->name);
    }

    if (partner_param_changed) {
        // OpenSwitch: Inform LACP that aggregator's data has been changed.
        mlacpVapiSportParamsChange(MLm_vpm_api__set_lacp_sport_params,
                                   pin_lacp_params);
    }

end:
    return status;

} // mvlan_set_sport_params

/*-----------------------------------------------------------------------------
 * mvlan_unset_sport_params   --
 *
 *        placp_params - The params for this aggrator
 *
 * Description  -- This function removes the lacp parameters on the superport.
 *
 * Side effects --
 *
 *     NOTE that this function expects that all parameters are validated at
 *          this point.
 *
 * Return value --
 *
 *            R_SUCCESS - on success
 *---------------------------------------------------------------------------*/
int
mvlan_unset_sport_params(struct MLt_vpm_api__lacp_sport_params *in_lacp_params)
{
    int                     status = R_SUCCESS;
    super_port_t            *psport;
    lacp_int_sport_params_t *placp_sport_params = NULL;

    status = mvlan_get_sport(in_lacp_params->sport_handle, &psport,
                             MLm_vpm_api__get_sport);
    if (status != R_SUCCESS) {
        goto end;
    }

    placp_sport_params =  psport->placp_params;
    if (placp_sport_params == NULL) {
            RDEBUG(DL_VPM, "%s: placp_sport_params null!\n", __FUNCTION__);
    }

    placp_params_list = n_list_remove_data((struct NList *)(placp_params_list),
                                           (void *)placp_sport_params);
    psport->placp_params = NULL;

    free(placp_sport_params);

    // OpenSwitch: Inform LACP that aggregator's data has been changed.
    mlacpVapiSportParamsChange(MLm_vpm_api__unset_lacp_sport_params, in_lacp_params);

end:
    return status;

} // mvlan_unset_sport_params

/*-----------------------------------------------------------------------------
 * mvlan_select_aggregator   --
 *
 *        placp_params - The params for this aggrator
 *
 * Description  -- This function  selects a suitable aggregator for the given
 *                 parameters.  All attempts to select an aggregator must be
 *                 done with an match_type_t of EXACT_MATCH first.  If there's
 *                 no exact match, then this function will attempt to allocate
 *                 (use) an existing 'idle' LAG and update this newly allcoated
 *                 LAG with the latest parameters.
 *
 * Side effects --
 *
 * Return value --
 *
 *            R_SUCCESS - on success
 *            MVLAN_LACP_SPORT_PARAMS_NOT_FOUND
 *---------------------------------------------------------------------------*/
int
mvlan_select_aggregator(struct MLt_vpm_api__lacp_match_params *placp_match_params,
                        match_type_t match)
{
    int                     status = R_SUCCESS;
    super_port_t            *psport;
    lacp_int_sport_params_t *ptemp_lacp_sport_params = NULL;
    struct NList            *plist;
    struct NList            *plist_start = NULL;
    int                     found_match = FALSE;
    struct  MLt_vpm_api__lacp_sport_params pmsg;

    plist_start = plist = placp_params_list;

    while (plist != NULL) {
        ptemp_lacp_sport_params = (lacp_int_sport_params_t *)(N_LIST_ELEMENT(plist));

        psport = ptemp_lacp_sport_params->psport;
        RDEBUG(DL_VPM, "matching attributes of sport 0x%llx (%s) with "
               "incoming params\n", psport->handle, psport->name);

        RDEBUG(DL_VPM, "Existing sport params are :\n");
        RDEBUG(DL_VPM, "port_type 0x%x, actor_key 0x%x, partner_key 0x%x\n",
               ptemp_lacp_sport_params->lacp_params.port_type,
               ptemp_lacp_sport_params->lacp_params.actor_key,
               ptemp_lacp_sport_params->lacp_params.partner_key);
        RDEBUG(DL_VPM, "partner_sys_pri 0x%x, "
               "partner_sys_id %02x:%02x:%02x:%02x:%02x:%02x, "
               "aggr_type %d\n",
               ptemp_lacp_sport_params->lacp_params.partner_system_priority,
               ptemp_lacp_sport_params->lacp_params.partner_system_id[0],
               ptemp_lacp_sport_params->lacp_params.partner_system_id[1],
               ptemp_lacp_sport_params->lacp_params.partner_system_id[2],
               ptemp_lacp_sport_params->lacp_params.partner_system_id[3],
               ptemp_lacp_sport_params->lacp_params.partner_system_id[4],
               ptemp_lacp_sport_params->lacp_params.partner_system_id[5],
               ptemp_lacp_sport_params->lacp_params.aggr_type);

        RDEBUG(DL_VPM, "Incoming params are :\n");
        RDEBUG(DL_VPM, "port_type 0x%x, actor_key 0x%x, partner_key 0x%x\n",
               placp_match_params->port_type,
               placp_match_params->actor_key,
               placp_match_params->partner_key);
        RDEBUG(DL_VPM, "partner_sys_pri 0x%x, "
               "partner_sys_id %02x:%02x:%02x:%02x:%02x:%02x, "
               "local_port_number %d, flags=0x%x\n\n",
               placp_match_params->partner_system_priority,
               placp_match_params->partner_system_id[0],
               placp_match_params->partner_system_id[1],
               placp_match_params->partner_system_id[2],
               placp_match_params->partner_system_id[3],
               placp_match_params->partner_system_id[4],
               placp_match_params->partner_system_id[5],
               placp_match_params->local_port_number,
               placp_match_params->flags);

        if (mvlan_match_aggregator(&(ptemp_lacp_sport_params->lacp_params),
                                   placp_match_params, match)) {
            found_match = TRUE;
            RDEBUG(DL_VPM, "matched!  psport->handle=0x%llx, match_type=%d.\n",
                   psport->handle, match);

            // If we found something that wasn't exact_match, go ahead and update
            // the partner information so the next call to select can find it.
            //
            // This is needed because there may be a small time delay between the
            // time select is made and attach to LAG is called due to protocol
            // timers.
            //
            // By updating the information here as soon as a selection is made,
            // we allow back-to-back selections of the same parameters to end up
            // with the same LAG before any attach is performed.
            //
            // When a priority match happens we still need to update the values in
            // the super port in order to replace the old information with the new
            // one coming from a higher priority port
            if (PARTIAL_MATCH == match || PRIORITY_MATCH == match) {
                memcpy(ptemp_lacp_sport_params->lacp_params.partner_system_id,
                       placp_match_params->partner_system_id,
                       sizeof(ptemp_lacp_sport_params->lacp_params.partner_system_id));

                ptemp_lacp_sport_params->lacp_params.partner_system_priority =
                    placp_match_params->partner_system_priority;
                ptemp_lacp_sport_params->lacp_params.partner_key =
                    placp_match_params->partner_key;

                // Set the port with the max priority, we set this even when the match is
                // PARTIAL_MATCH because that match should happen when the first lport is
                // attached to a sport
                ptemp_lacp_sport_params->lacp_params.actor_max_port_priority =
                                    placp_match_params->actor_oper_port_priority;

                // Check if the partner priority is higher than the current max partner priority
                // In partial match we always update partner max port priority
                if (PARTIAL_MATCH == match ||
                    ptemp_lacp_sport_params->lacp_params.partner_max_port_priority >
                    placp_match_params->partner_oper_port_priority) {
                    ptemp_lacp_sport_params->lacp_params.partner_max_port_priority =
                                    placp_match_params->partner_oper_port_priority;
                }
                ptemp_lacp_sport_params->lacp_params.flags |=
                                    (LACP_LAG_PARTNER_SYSPRI_FIELD_PRESENT
                                     | LACP_LAG_PARTNER_SYSID_FIELD_PRESENT
                                     | LACP_LAG_PARTNER_KEY_FIELD_PRESENT
                                     | LACP_LAG_ACTOR_PORT_PRIORITY_FIELD_PRESENT
                                     | LACP_LAG_PARTNER_PORT_PRIORITY_FIELD_PRESENT
                                    );

                // Also update port_type and actor_key now that
                // OpenSwitch's managing these parameters.
                ptemp_lacp_sport_params->lacp_params.port_type =
                    placp_match_params->port_type;
                ptemp_lacp_sport_params->lacp_params.actor_key =
                    placp_match_params->actor_key;

                RDEBUG(DL_VPM, "Updating DB with new LAG info: LAG.%d, port_type=%d",
                       (int)PM_HANDLE2LAG(psport->handle), placp_match_params->port_type);

                // OpenSwitch: update database with new LAG information when first selected.
                db_update_lag_partner_info((int)PM_HANDLE2LAG(psport->handle));
            }
            // (EXACT_MATCH == match)
            else{
                // In exact_match we need to update the max_actor_port_priority and
                // max_partner_port_priority of the sport only if the matched port
                // has port_priority field present and has higher priority
                if ((ptemp_lacp_sport_params->lacp_params.flags & LACP_LAG_ACTOR_PORT_PRIORITY_FIELD_PRESENT) &&
                    ptemp_lacp_sport_params->lacp_params.actor_max_port_priority >
                    placp_match_params->actor_oper_port_priority){

                    ptemp_lacp_sport_params->lacp_params.actor_max_port_priority =
                                                         placp_match_params->actor_oper_port_priority;
                }
                if ((ptemp_lacp_sport_params->lacp_params.flags & LACP_LAG_PARTNER_PORT_PRIORITY_FIELD_PRESENT) &&
                    ptemp_lacp_sport_params->lacp_params.partner_max_port_priority >
                    placp_match_params->partner_oper_port_priority){

                    ptemp_lacp_sport_params->lacp_params.partner_max_port_priority =
                                                         placp_match_params->partner_oper_port_priority;
                }
            }

            if (PRIORITY_MATCH == match) {
                // Only necessary to set the flags and the sport handle
                pmsg.flags = ptemp_lacp_sport_params->lacp_params.flags;
                pmsg.sport_handle = psport->handle;
                mlacpVapiSportParamsChange(MLm_vpm_api__set_lacp_sport_params, &pmsg);
            }

            break;
        }

        plist = N_LIST_NEXT(plist);

        if (plist == (void *) plist_start) {
            break;
        }

    } // while (plist != NULL)

    if (found_match == FALSE) {
        RDEBUG(DL_VPM, "mvlan_api_select_aggregator: The specified parameters do not exist\n");
        status  =  MVLAN_LACP_SPORT_PARAMS_NOT_FOUND;
        goto end;
    }

    ptemp_lacp_sport_params  = plist->data;
    psport = ptemp_lacp_sport_params->psport;
    placp_match_params->sport_handle = psport->handle;

end:
    return status;

} // mvlan_select_aggregator

/*-----------------------------------------------------------------------------
 * mvlan_api_select_aggregator   --
 *
 *        placp_params - The params for this aggrator
 *
 * Description  -- This function  selects a suitable aggregator for the given
 *                 parameters.
 *
 * Side effects --
 *
 * Return value --
 *
 *            R_SUCCESS - on success
 *            MVLAN_LACP_SPORT_PARAMS_NOT_FOUND
 *---------------------------------------------------------------------------*/
int
mvlan_api_select_aggregator(struct MLt_vpm_api__lacp_match_params *placp_match_params)
{
    int status = R_SUCCESS;

    // First, search for an exact match.  If none, find one that's available.

    if (mvlan_select_aggregator(placp_match_params, EXACT_MATCH) != R_SUCCESS   &&
        mvlan_select_aggregator(placp_match_params, PARTIAL_MATCH) != R_SUCCESS &&
        mvlan_select_aggregator(placp_match_params, PRIORITY_MATCH) != R_SUCCESS) {
        status = MVLAN_LACP_SPORT_PARAMS_NOT_FOUND;
    }

    return status;

} // mvlan_api_select_aggregator

/*-----------------------------------------------------------------------------
 * mvlan_api_attach_lport_to_aggregator   --
 *
 *        placp_attach_params - The lport and sport handles for this operation
 *
 * Description  -- This function attaches a logical port to a super port.
 *                 This is called only from LACP.
 *
 * Side effects --
 *
 * Return value --
 *
 *            R_SUCCESS - on success
 *            MVLAN_LACP_SPORT_PARAMS_NOT_FOUND
 *---------------------------------------------------------------------------*/
int
mvlan_api_attach_lport_to_aggregator(struct MLt_vpm_api__lacp_attach *placp_attach_params)
{
    int                     status = R_SUCCESS;
    super_port_t            *psport;
    lacp_int_sport_params_t *sport_lacp_params = NULL;

    RDEBUG(DL_VPM, "%s: Entry\n", __FUNCTION__);

    status = mvlan_get_sport(placp_attach_params->sport_handle,
                             &psport, MLm_vpm_api__get_sport);
    if (status != R_SUCCESS) {
        goto end;
    }

    sport_lacp_params = psport->placp_params;

    // Even though we gave this aggregator to LACP as part of its selection,
    // by the time LACP sends us the attach we may not have this aggregator
    // due to negate_all etc : and so return error instead of assert.
    if (sport_lacp_params == NULL) {
        status = MVLAN_LACP_SPORT_PARAMS_NOT_FOUND;
        RDEBUG(DL_VPM, "aggregator params vanished in between select "
               "and attach from LACP - possibly negated ? (sport 0x%llx)\n",
               psport->handle);
        goto end;
    }

    // Increment number of logical ports attached to this super port.
    psport->num_lports++;

    RDEBUG(DL_VPM, "LAG.%d, num_lports=%d \n",
           (int)PM_HANDLE2LAG(psport->handle),
           psport->num_lports);
end:
    return status;

} // mvlan_api_attach_lport_to_aggregator

/*-----------------------------------------------------------------------------
 * mvlan_api_detach_lport_from_aggregator   --
 *
 *        params - The lport and sport handles for this operation
 *
 * Description  -- This function detaches a logical port to a super port This
 *                 is called  only from LACP.
 *
 * Side effects --
 *
 * Return value --
 *
 *            R_SUCCESS - on success
 *            MVLAN_LACP_SPORT_PARAMS_NOT_FOUND
 *---------------------------------------------------------------------------*/
int
mvlan_api_detach_lport_from_aggregator(struct MLt_vpm_api__lacp_attach *placp_detach_params)

{
    int                     status = R_SUCCESS;
    super_port_t            *psport;
    lacp_int_sport_params_t *sport_lacp_params = NULL;

    RDEBUG(DL_VPM, "%s: Entry\n", __FUNCTION__);

    status = mvlan_get_sport(placp_detach_params->sport_handle,
                             &psport,MLm_vpm_api__get_sport);
    if (status != R_SUCCESS) {
        RDEBUG(DL_VPM, "Could not get sport 0x%llx (already cleaned up ?)\n",
               placp_detach_params->sport_handle);
        goto end;
    }

    sport_lacp_params  = psport->placp_params;

    if (sport_lacp_params == NULL) {
        status = MVLAN_LACP_SPORT_PARAMS_NOT_FOUND;
        RDEBUG(DL_VPM, "aggregator params vanished before detach (handle = 0x%llx)!\n",
               psport->handle);
        goto end;
    }

    // Decrement number of logical ports attached to this super port.
    psport->num_lports--;

    RDEBUG(DL_VPM, "LAG.%d, num_lports=%d \n",
           (int)PM_HANDLE2LAG(psport->handle),
           psport->num_lports);
end:
    return status;

} // mvlan_api_detach_lport_from_aggregator

/*-----------------------------------------------------------------------------
 * mvlan_match_aggregator   --
 *
 *        psport_param     - Our aggregator parameters set by the user
 *
 *        plag_param  - The lag parameter to be matched sent by mlacp
 *
 * Description  -- This function searches for a match between user set
 *                 sport parameter with the one supplied by the mplacp
 *                 after negotion with its peer entity.
 *
 *                 NOTE: if a LAG does not yet have any partner information
 *                       present, it cannot be considered a match for anyone
 *                       else.  In other words, if the far end doesn't support
 *                       LACP, we will put this port in a LAG of its own.
 *
 * Side effect --
 *
 * Return value --
 *
 *            TRUE/FALSE
 *---------------------------------------------------------------------------*/
static int
mvlan_match_aggregator(lacp_sport_params_t *psport_param,
                       struct MLt_vpm_api__lacp_match_params *plag_param,
                       match_type_t match)
{
    int status = FALSE;
    int is_priority_match = FALSE;

    // First check the port type if present.
    if ((psport_param->port_type != PM_LPORT_INVALID) || (EXACT_MATCH == match)) {
        if (psport_param->port_type != plag_param->port_type) {
               RDEBUG(DL_VPM, "   match_aggregator: port types don't match.\n");
               goto end;
        }
    } else {
        RDEBUG(DL_VPM, "   match_aggregator: Port type field NOT yet set. Skip check.\n");
    }

    // Next compare the actor key if present.
    if ((psport_param->actor_key != LACP_LAG_INVALID_ACTOR_KEY) || (EXACT_MATCH == match)) {
        if (psport_param->actor_key != plag_param->actor_key) {
            RDEBUG(DL_VPM, "   match_aggregator: actor keys don't match.\n");
            goto end;
        }
    } else {
        RDEBUG(DL_VPM, "   match_aggregator: Actor Key field NOT yet set. Skip check.\n");
    }

    // Check partner key if present.
    // If PRIORITY_MATCH and the partner key doesn't match, check if this port has more priority
    if ((psport_param->flags & LACP_LAG_PARTNER_KEY_FIELD_PRESENT) || (EXACT_MATCH==match)) {
        if (psport_param->partner_key != plag_param->partner_key) {
            RDEBUG(DL_VPM,"match_aggregator: Partner key field does not match.");
            if (PRIORITY_MATCH == match &&
                (psport_param->flags & LACP_LAG_ACTOR_PORT_PRIORITY_FIELD_PRESENT)) {

                if (psport_param->actor_max_port_priority > plag_param->actor_oper_port_priority) {
                    RDEBUG(DL_VPM,"match_aggregator: Current actor priority is higher");
                    RDEBUG(DL_VPM,"match_aggregator: Priority match allows different partner key");
                }
                else if ((psport_param->flags & LACP_LAG_PARTNER_PORT_PRIORITY_FIELD_PRESENT) &&
                         psport_param->actor_max_port_priority == plag_param->actor_oper_port_priority &&
                         psport_param->partner_max_port_priority > plag_param->partner_oper_port_priority) {
                    RDEBUG(DL_VPM,"match_aggregator: Current partner priority is higher");
                    RDEBUG(DL_VPM,"match_aggregator: Priority match allows different partner key");
                }
                else {
                    goto end;
                }
            } else {
                goto end;
            }
        }
    } else {
        RDEBUG(DL_VPM, "   match_aggregator: Partner key field NOT yet set. Skip check.\n");
    }

    // If the partner priority is set then we must compare with the partner priority.
    if ((psport_param->flags & LACP_LAG_PARTNER_SYSPRI_FIELD_PRESENT) || (EXACT_MATCH==match)) {
        if (psport_param->partner_system_priority != plag_param->partner_system_priority) {
            RDEBUG(DL_VPM, "   match_aggregator: Partner system pri field does not match.\n");
            if (PRIORITY_MATCH == match &&
                plag_param->partner_system_priority != 0 &&
                psport_param->partner_system_priority > plag_param->partner_system_priority) {
                RDEBUG(DL_VPM, "match_aggregator: Priority match allows higher partner system priority.");
                is_priority_match = TRUE;
            }
            else {
                goto end;
            }
        }
    }

    // If the partner system id is set in the sport, we need to compare with this.
    // if the partner system id is different, we need to make sure we are choosing the one
    // with higher priority
    // NOTE: if partner has not responded (e.g. no LACP running on far end), then the
    //       partner system id will be the default value, which should never be used by
    //       any valid system running LACP.  If that's the case, we don't match, which
    //       forces each port to form its own LAG.
    if ((psport_param->flags & LACP_LAG_PARTNER_SYSID_FIELD_PRESENT) || (EXACT_MATCH==match)) {
        if ((memcmp(psport_param->partner_system_id, plag_param->partner_system_id,
                    sizeof(macaddr_3_t)) != 0) ||
            (memcmp(psport_param->partner_system_id, default_partner_system_mac,
                    sizeof(macaddr_3_t)) == 0)) {
            RDEBUG(DL_VPM, "PARTNER_SYSID does not match\n");
            if (!is_priority_match) {
                goto end;
            }
            else {
                RDEBUG(DL_VPM, "but sys priority match");
            }
        }
    } else {
        RDEBUG(DL_VPM, "   match_aggregator PARTNER_SYSID Not yet set and so skip the check\n");
    }

    status = TRUE;

end:
    return status;

} // mvlan_match_aggregator

/*-----------------------------------------------------------------------------
 * mvlan_api_clear_sport_params   --
 *
 *        params - The sport handle for this operation
 *
 * Description  -- This function clear out a super port.  This should only
 *                 be called after all lports have been detached.
 *
 * Side effects --
 *
 * Return value --
 *
 *            R_SUCCESS - on success
 *            MVLAN_LACP_SPORT_PARAMS_NOT_FOUND
 *---------------------------------------------------------------------------*/
int
mvlan_api_clear_sport_params(unsigned long long sport_handle)
{
    int                       status = R_SUCCESS;
    super_port_t              *psport;
    lacp_int_sport_params_t   *sport_lacp_params = NULL;
    lacp_per_port_variables_t *plpinfo;

    RDEBUG(DL_VPM, "%s: Entry\n", __FUNCTION__);

    status = mvlan_get_sport(sport_handle, &psport, MLm_vpm_api__get_sport);

    if (status != R_SUCCESS) {
        VLOG_ERR("Could not get sport 0x%llx (already cleaned up ?)",
                 sport_handle);
        goto end;
    }

    sport_lacp_params  = psport->placp_params;

    if (sport_lacp_params == NULL) {
        status = MVLAN_LACP_SPORT_PARAMS_NOT_FOUND;
        VLOG_ERR("aggregator params vanished before clear (handle = 0x%llx)!",
                 psport->handle);
        goto end;
    }

    // Make sure we detach every port associated with this sport

    RDEBUG(DL_VPM, "Detaching all lports");

    plpinfo = LACP_AVL_FIRST(lacp_per_port_vars_tree);

    while (plpinfo) {
        if (plpinfo->sport_handle == sport_handle) {
            /*
             * Make selected UNSELECTED, and cause approp. event in
             * the mux machine.
             */
            plpinfo->lacp_control.selected = UNSELECTED;
            LACP_mux_fsm(E2, plpinfo->mux_fsm_state, plpinfo);
            plpinfo->lacp_control.ready_n = FALSE;
        }
        plpinfo = LACP_AVL_NEXT(plpinfo->avlnode);
    }
    // All logical ports have been detached from this aggregator (sport).
    // Clean up partner information so that we can reuse this sport
    // for subsequent aggregation.
    RDEBUG(DL_VPM, "Clearing LAG.%d info, port_type was %d",
           (int)PM_HANDLE2LAG(psport->handle),
           sport_lacp_params->lacp_params.port_type);

    memcpy(sport_lacp_params->lacp_params.partner_system_id,
           default_partner_system_mac,
           MAC_ADDR_LENGTH);

    sport_lacp_params->lacp_params.partner_system_priority = 0;
    sport_lacp_params->lacp_params.partner_key = 0;
    sport_lacp_params->lacp_params.actor_max_port_priority= 0;
    sport_lacp_params->lacp_params.partner_max_port_priority= 0;
    sport_lacp_params->lacp_params.flags &= ~(LACP_LAG_PARTNER_SYSPRI_FIELD_PRESENT         |
                                              LACP_LAG_PARTNER_SYSID_FIELD_PRESENT          |
                                              LACP_LAG_PARTNER_KEY_FIELD_PRESENT            |
                                              LACP_LAG_ACTOR_PORT_PRIORITY_FIELD_PRESENT    |
                                              LACP_LAG_PARTNER_PORT_PRIORITY_FIELD_PRESENT);

    // OpenSwitch: do not clear actor key & port type. For OpenSwitch,
    // LAGs & actor keys are specified and bound together until deleted.
    //  sport_lacp_params->lacp_params.port_type = PM_LPORT_INVALID;
    //
    //  sport_lacp_params->lacp_params.actor_key = LACP_LAG_INVALID_ACTOR_KEY;

    // OpenSwitch: clear database with new information now that no port is attached.
    db_clear_lag_partner_info((int)PM_HANDLE2LAG(psport->handle));

end:
    return status;

} // mvlan_api_clear_sport_params
