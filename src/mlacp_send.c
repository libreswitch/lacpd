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

//**************************************************************************
//    File               : mlacp_send.c
//    Description        : mcpu LACP send functions
//**************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <strings.h>

#include <avl.h>
#include <pm_cmn.h>
#include <lacp_cmn.h>
#include <mlacp_debug.h>

#include "lacp.h"
#include "lacp_support.h"
#include "mlacp_fproto.h"
#include "mvlan_lacp.h"
#include "lacp_ops_if.h"

// OpenSwitch: in lieu of sending events to VPM
#include "mvlan_lacp.h"

VLOG_DEFINE_THIS_MODULE(mlacp_send);

//***********************************************************************
// Local function prototypes
//***********************************************************************

int
mlacp_blocking_send_select_aggregator(LAG_t *const lag,
                                      lacp_per_port_variables_t *lacp_port)
{
    struct MLt_vpm_api__lacp_match_params match_params = {0};
    int status = R_SUCCESS;

    match_params.lport_handle   = lacp_port->lport_handle;

    //********************************************************************
    // All flags are present, because of the default values.
    //********************************************************************
    match_params.flags = (LACP_LAG_PORT_TYPE_FIELD_PRESENT              |
                          LACP_LAG_ACTOR_KEY_FIELD_PRESENT              |
                          LACP_LAG_PARTNER_KEY_FIELD_PRESENT            |
                          LACP_LAG_PARTNER_SYSPRI_FIELD_PRESENT         |
                          LACP_LAG_PARTNER_SYSID_FIELD_PRESENT          |
                          LACP_LAG_AGGRTYPE_FIELD_PRESENT               |
                          LACP_LAG_ACTOR_PORT_PRIORITY_FIELD_PRESENT    |
                          LACP_LAG_PARTNER_PORT_PRIORITY_FIELD_PRESENT);

    //********************************************************************
    // Send all the params for matching.
    //********************************************************************
    match_params.port_type         = lag->port_type;
    match_params.actor_key         = lag->LAG_Id->local_port_key;
    match_params.partner_key       = lag->LAG_Id->remote_port_key;
    match_params.local_port_number = lag->LAG_Id->local_port_number;
    match_params.actor_aggr_type   = lacp_port->actor_oper_port_state.aggregation;
    match_params.partner_aggr_type = lacp_port->partner_oper_port_state.aggregation;

    match_params.actor_oper_port_priority = lacp_port->actor_admin_port_priority;
    match_params.partner_oper_port_priority = lacp_port->partner_oper_port_priority;

    match_params.partner_system_priority =
        lacp_port->partner_oper_system_variables.system_priority;

    memcpy(match_params.partner_system_id,
           lacp_port->partner_oper_system_variables.system_mac_addr,
           sizeof(macaddr_3_t));

    RDEBUG(DL_LACP_SEND, "sending the following params to VLAN/LAG mgr :\n");
    RDEBUG(DL_LACP_SEND, "port_type %d, actor_key 0x%x, partner_key 0x%x "
           "partner_sys_pri %d, partner_sys_id %02x:%02x:%02x:%02x:%02x:%02x "
           "local_port_number 0x%x\n",
           match_params.port_type,
           match_params.actor_key,
           match_params.partner_key,
           match_params.partner_system_priority,
           match_params.partner_system_id[0],
           match_params.partner_system_id[1],
           match_params.partner_system_id[2],
           match_params.partner_system_id[3],
           match_params.partner_system_id[4],
           match_params.partner_system_id[5],
           match_params.local_port_number);

    // OpenSwitch: Change to direct function call.  Also sport_handle
    //        is written directly into match_params struct.
    status = mvlan_api_select_aggregator(&match_params);

    if (R_SUCCESS == status) {
        lacp_port->sport_handle = match_params.sport_handle;

        if (lacp_port->debug_level & DBG_LACP_SEND) {
            RDBG("%s : Got matching aggr from MVPM "
                 "(lport 0x%llx, sport 0x%llx) !\n",
                 __FUNCTION__, lacp_port->lport_handle,
                 lacp_port->sport_handle);
        }
    } else {
        if (lacp_port->debug_level & DBG_LACP_SEND) {
            RDBG("%s : Failed to get matching aggr from MVPM "
                 "(lport 0x%llx) : status %d\n",
                 __FUNCTION__, lacp_port->lport_handle, status);
        }
    }

    return status;
} // mlacp_blocking_send_select_aggregator

int
mlacp_blocking_send_attach_aggregator(lacp_per_port_variables_t *lacp_port)
{
    struct MLt_vpm_api__lacp_attach attach = {0};
    int status = R_SUCCESS;

    attach.lport_handle      = lacp_port->lport_handle;
    attach.sport_handle      = lacp_port->sport_handle;
    attach.partner_priority  = lacp_port->partner_oper_system_variables.system_priority;

    memcpy(attach.partner_mac_addr,
           lacp_port->partner_oper_system_variables.system_mac_addr,
           sizeof(macaddr_3_t));

    // OpenSwitch: Change to direct function call.
    status = mvlan_api_attach_lport_to_aggregator(&attach);

    if (R_SUCCESS == status) {
        if (lacp_port->debug_level & DBG_LACP_SEND) {
            RDBG("%s : Attached port %d to LAG.%d! (lport 0x%llx sport 0x%llx)\n",
                 __FUNCTION__, (int)PM_HANDLE2PORT(lacp_port->lport_handle),
                 (int)PM_HANDLE2LAG(lacp_port->sport_handle),
                 lacp_port->lport_handle, lacp_port->sport_handle);
        }
    } else {
        if (lacp_port->debug_level & DBG_LACP_SEND) {
            RDBG("%s : Failed to attach : did the sport vanish ?? "
                 "(lport 0x%llx sport 0x%llx)\n",
                 __FUNCTION__, lacp_port->lport_handle, lacp_port->sport_handle);
        }
    }

   return status;
} // mlacp_blocking_send_attach_aggregator

int
mlacp_blocking_send_detach_aggregator(lacp_per_port_variables_t *lacp_port)
{
    struct MLt_vpm_api__lacp_attach detach = {0};
    int status = R_SUCCESS;

    detach.lport_handle   = lacp_port->lport_handle;
    detach.sport_handle   = lacp_port->sport_handle;

    // OpenSwitch: Change to direct function call.
    status = mvlan_api_detach_lport_from_aggregator(&detach);

    if (R_SUCCESS == status) {
        if (lacp_port->debug_level & DBG_LACP_SEND) {
            RDBG("%s : Detached port %d from LAG.%d! (lport 0x%llx sport 0x%llx)\n",
                 __FUNCTION__, (int)PM_HANDLE2PORT(lacp_port->lport_handle),
                 (int)PM_HANDLE2LAG(lacp_port->sport_handle),
                 lacp_port->lport_handle, lacp_port->sport_handle);
        }
    } else {
        if (lacp_port->debug_level & DBG_LACP_SEND) {
            RDBG("%s : Failed to detach ?? (lport 0x%llx sport 0x%llx)\n",
                 __FUNCTION__, lacp_port->lport_handle, lacp_port->sport_handle);
        }
    }

    return status;
} // mlacp_blocking_send_detach_aggregator

int
mlacp_blocking_send_enable_collecting(lacp_per_port_variables_t *lacp_port)
{
    int port;
    int lag_id;
    int status = R_SUCCESS;

    // OpenSwitch: Add the port to trunk in hardware only
    //        if it wasn't already done.
    if (FALSE == lacp_port->hw_attached_to_mux) {

        lag_id = (int)PM_HANDLE2LAG(lacp_port->sport_handle);
        port = (int)PM_HANDLE2PORT(lacp_port->lport_handle);

        //--- OPS_TODO: enable attach for now since STP is not running... ---
        //---------------------------------------------------------------
        // NOTE: lacpd is no longer responsible for attaching/detaching
        // ports to LAGs in h/w.  stpd is now doing that in order to
        // maintain STG state consistency and not worry about any race
        // conditions or missed transient state transitions.
        //---------------------------------------------------------------

        // Add the port to a trunk in hardware
        ops_attach_port_in_hw(lag_id, port);

        // Update DB with new info.
        db_add_lag_port(lag_id, port, lacp_port);

        // Set indicator.
        lacp_port->hw_attached_to_mux = TRUE;
    }

    return status;
} // mlacp_blocking_send_enable_collecting

int
mlacp_blocking_send_enable_distributing(lacp_per_port_variables_t *lacp_port)
{
    int port;
    int lag_id;
    int status = R_SUCCESS;

    if (TRUE == lacp_port->hw_attached_to_mux) {

        // OpenSwitch: Take out egress disable flag from trunk member port flags.
        lag_id = (int)PM_HANDLE2LAG(lacp_port->sport_handle);
        port = (int)PM_HANDLE2PORT(lacp_port->lport_handle);

        ops_trunk_port_egr_enable(lag_id, port);
    }

    return status;
} // mlacp_blocking_send_enable_distributing

int
mlacp_blocking_send_disable_collect_dist(lacp_per_port_variables_t *lacp_port)
{
    int port;
    int lag_id;
    int status = R_SUCCESS;

    // OpenSwitch: Remove the port from trunk in hardware
    //          only if it wasn't already done.
    if (TRUE == lacp_port->hw_attached_to_mux) {

        lag_id = (int)PM_HANDLE2LAG(lacp_port->sport_handle);
        port = (int)PM_HANDLE2PORT(lacp_port->lport_handle);

        //--- OPS_TODO: enable attach for now since STP is not running... ---
        //---------------------------------------------------------------
        // NOTE: lacpd is no longer responsible for attaching/detaching
        // ports to LAGs in h/w.  stpd is now doing that in order to
        // maintain STG state consistency and not worry about any race
        // conditions or missed transient state transitions.
        //---------------------------------------------------------------

        // Detach the port from LAG in h/w
        ops_detach_port_in_hw(lag_id, port);

        // Update DB with new info.
        db_delete_lag_port(lag_id, port, lacp_port);

        // Clear out indicator.
        lacp_port->hw_collecting = FALSE;
        lacp_port->hw_attached_to_mux = FALSE;
    }

    return status;
} // mlacp_blocking_send_disable_collect_dist

// OpenSwitch: New function to clear super port.
int
mlacp_blocking_send_clear_aggregator(unsigned long long sport_handle)
{
    int status = R_SUCCESS;

    status = mvlan_api_clear_sport_params(sport_handle);

    if (status != R_SUCCESS) {
        VLOG_ERR("Failed to clear sport params for 0x%llx",
                 sport_handle);
    }

    return status;
} // mlacp_blocking_send_clear_aggregator
