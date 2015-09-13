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

/*****************************************************************************
   File               : mvlan_sport.c
   Description        : This file creates and maintains the super ports.

                        NOTE: This is a simplified version to support trunks
                              for Halon platform.
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <avl.h>
#include <mlacp_debug.h>
#include <nemo_types.h>
#include "mvlan_sport.h"

VLOG_DEFINE_THIS_MODULE(mvlan_sport);

/*****************************************************************************
 * Static  varibles for this file
 *****************************************************************************/
// Stores the super ports indexed by the super port handle
nemo_avl_tree_t sport_handle_tree = {0};

static bool sport_init_done = FALSE;

static int mvlan_validate_sport(struct MLt_vpm_api__create_sport *pcreate,
                                super_port_t **ppsport);

/*-----------------------------------------------------------------------------
 * mvlan_sport_init  --
 *
 *        first_time - If true we are up during the system boot. Else
 *                     we have somehow died and have been restarted.
 *
 * Description  -- This function initializes the super port manager
 *
 * Side effects --
 *
 * Return value --
 *            R_SUCCESS - on success
 *            MVLAN_SPORT_NOT_HANDLED
 *---------------------------------------------------------------------------*/
int
mvlan_sport_init(u_long  first_time)
{
    int status = R_SUCCESS;

    if (first_time != TRUE) {
        VLOG_ERR("Cannot handle revival from dead");
        return -1;
    }

    if (sport_init_done == TRUE) {
        VLOG_WARN("Already initialized");
        return -1;
    }

    // Initialize the various trees.
    NEMO_AVL_INIT_TREE(sport_handle_tree, nemo_compare_port_handle);

    sport_init_done = TRUE;

    return status;

} // mvlan_sport_init

/*-----------------------------------------------------------------------------
 * mvlan_validate_sport  --
 *
 *        psport_create - The pointer to the data structure to be validated
 *
 * Description  -- This checks whether a given super port to be created
 *                 is valid and can be safely created.
 *
 * Side effects --
 *
 * Return value --
 *            R_SUCCESS - on success
 *---------------------------------------------------------------------------*/
static int
mvlan_validate_sport(struct MLt_vpm_api__create_sport  *psport_create,
                     super_port_t **ppsport)
{
    int            status = R_SUCCESS;
    port_handle_t  handle;
    super_port_t  *psport = NULL;

    if (psport_create) {
        // These port types will have a name and we must be
        // sure that the name is unique.
        if (psport_create->handle == 0) {
            RDEBUG(DL_VPM, "sport has zero id\n");
            return -1;
        }

        handle = psport_create->handle;
    }

    psport = NEMO_AVL_FIND(sport_handle_tree, &handle);

    if (psport) {
        RDEBUG(DL_VPM, "sport handle 0x%llx already exists\n", handle);
        return MVLAN_SPORT_EXISTS;
    }

    if (ppsport) {
        *ppsport = psport;
    }

    return status;

} // mvlan_validate_sport

/*-----------------------------------------------------------------------------
 * mvlan_sport_create  --
 *
 *        psport_create - The pointer to the data structure to create
 *                        a super port.
 *
 *        ppsport    - will contain the pointer to the super port
 *                     if ppsuperport != NULL and the function succeeds
 *
 * Description  --
 *         This function creates the super port. Note that this
 *         function does not add the super port to any VLAN. It is the
 *         caller's responsibility to addthe super port to any vlan.
 *
 * Side effects --
 *
 * Return value --
 *            R_SUCCESS - on success
 *            MVLAN_SPORT_INSERT_FAILED
 *            MVLAN_SPORT_NO_MEM
 *---------------------------------------------------------------------------*/
int
mvlan_sport_create(struct MLt_vpm_api__create_sport  *psport_create,
                   super_port_t **ppsport)
{
    int              status  = R_SUCCESS;
    super_port_t     *psport = NULL;
    nemo_avl_node_t  *psport_node;
    port_handle_t    handle;
    int              memSize;
    int              lag_id;

    if (!psport_create) {
        VLOG_ERR("LACP - psport create without information!");
        goto end;
    }

    status = mvlan_validate_sport(psport_create, &psport);

    if(status != R_SUCCESS) {
        goto end;
    }

    handle = psport_create->handle;

    // Now alloc the block for this sport and initialize it.
    memSize = sizeof(super_port_t) + sizeof(nemo_avl_node_t);
    psport  = malloc(memSize);

    if (psport == NULL) {
        VLOG_ERR("sport no memory !!\n");
        status = -1;
        goto end;

    } else {
        memset(psport, 0, memSize);
    }

    psport_node = (nemo_avl_node_t *)(psport + 1);

    // Initialiize the various fields.
    psport->handle      = handle;
    psport->admin_state = SPORT_ADMIN_UP;
    psport->aggr_mode   = PORT_AGGR_MODE_DEFAULT;

    NEMO_AVL_INIT_NODE(*psport_node, psport, &(psport->handle));

    if (NEMO_AVL_INSERT(sport_handle_tree, *psport_node) == FALSE) {
        RDEBUG(DL_VPM, "sport insert to handle tree failed!!\n");
        status = -1;
        goto end;
    }

    // Halon: moved check for psport_create to beginning of
    //        this function.  (Found by Coverity).
    lag_id = PM_GET_SPORT_ID(psport_create->handle);
    sprintf(psport->name, "Lag%d", lag_id);
    psport->type =  psport_create->type;

end:

    if (ppsport != NULL) {
        *ppsport = psport;
    }

    if ((status != R_SUCCESS) && (status != MVLAN_SPORT_EXISTS)) {
        // Don't free it for MVLAN_SPORT_EXISTS : else configd/SNMP
        // duplicate commands will end up corrupting the tree.
        if (psport != NULL) {
            free(psport);
        }
    }

    return status;

} // mvlan_sport_create

/*-----------------------------------------------------------------------------
 * mvlan_sport_delete_validate_generic  --
 *
 *        psport - The pointer to the the super port
 *
 * Description  -- This function  validates whether a superport can be deleted.
 *
 * Side effects --
 *      Note this function does not check for ref counts or attached vlans.
 *
 * Return value --
 *            R_SUCCESS - on success
 *            MVLAN_SPORT_LPORT_ATTACHED
 *            MVLAN_SPORT_PORT_HAS_STP
 *            MVLAN_SPORT_IS_TRUNK
 *            MVLAN_LACP_SPORT_PARAMS_SET
 *---------------------------------------------------------------------------*/
int
mvlan_sport_delete_validate_generic(super_port_t *psport)
{
    int status = R_SUCCESS;

    // if any logical port is attached to this
    // super port then we cannot delete this super port.
    if (psport->num_lports > 0) {
        RDEBUG(DL_VPM, "sport 0x%llx has valid lports", psport->handle);
        status = MVLAN_SPORT_LPORT_ATTACHED;
        goto end;
    }

    // If trunking is enabled on the sport we cannot delete it.
    if (psport->type & STYPE_TRUNK) {
        RDEBUG(DL_VPM, "The specified super port has trunking enabled");
        status = MVLAN_SPORT_IS_TRUNK;
        goto end;
    }

    // If the super port has lacp params set then we cannot delete the
    // super port The parameters must be negated.
    if (psport->placp_params != NULL ) {
        RDEBUG(DL_VPM, "The specified super port has lacp parameters set");
        status = MVLAN_LACP_SPORT_PARAMS_SET;
        goto end;
    }

end:
    return status;

} // mvlan_sport_delete_validate_generic

/*-----------------------------------------------------------------------------
 * mvlan_destroy_sport  --
 *
 *        psport - The pointer to the the super port
 *
 * Description  -- This function frees up all memory associated with sport
 *
 * Side effects --
 *
 * Return value --
 *
 *            R_SUCCESS - on success
 *---------------------------------------------------------------------------*/
int
mvlan_destroy_sport(super_port_t *psport)
{
    int status = R_SUCCESS;
    nemo_avl_node_t *psport_node = NULL;

    psport_node = (nemo_avl_node_t *)( psport + 1);

    NEMO_AVL_DELETE(sport_handle_tree, *psport_node);

    free(psport);

    return status;

} // mvlan_destroy_sport

/*-----------------------------------------------------------------------------
 * mvlan_get_sport  --
 *
 *        handle   - Unique handle of the sport
 *
 *        ppsport - The pointer to the super port if found
 *
 *        type    - get / getnext
 *
 * Description  -- Given a handle this function returns the super port
 *         This function is used for use between the various vlan modules.
 *         The interface to the config is provided elsewhere.
 *
 * Side effects --
 *
 * Return value --
 *            R_SUCCESS - on success
 *            MVLAN_SPORT_NOT_FOUND
 *---------------------------------------------------------------------------*/
int
mvlan_get_sport(port_handle_t handle, super_port_t **ppsport, int type)
{
    int status = R_SUCCESS;

    if (type == MLm_vpm_api__get_sport) {
        *ppsport = NEMO_AVL_FIND(sport_handle_tree,&handle);

        if (*ppsport == NULL) {
            RDEBUG(DL_VPM, "%s: sport handle 0x%llx not found",
                   __FUNCTION__, handle);
            status = MVLAN_SPORT_NOT_FOUND;
        }

    } else {
        *ppsport = NEMO_AVL_FIND_NEXT(sport_handle_tree,&handle);

        if (*ppsport == NULL) {
            RDEBUG(DL_VPM, "Next sport 0x%llx handle does not exist ", handle);
            status = MVLAN_SPORT_EOT;
        }
    }

    return status;

} // mvlan_get_sport
