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

//**************************************************************************
//    File               : mlacp_recv.c
//    Description        : LACP process dispatcher functions on the master
//                         CPU.
//**************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>

#include <avl.h>
#include <pm_cmn.h>
#include <lacp_cmn.h>
#include <mlacp_debug.h>

#include "lacp.h"
#include "mlacp_recv.h"
#include "mlacp_fproto.h"
#include "mvlan_lacp.h"

#include "lacp_support.h"
#include "lacp_ops_if.h"
#include "mvlan_sport.h"

VLOG_DEFINE_THIS_MODULE(mlacp_recv);

//***********************************************************************
// (local) Function prototypes
//***********************************************************************
// OpenSwitch
//void cli_set_debug_level(int level);
//void cli_unset_debug_level(int level);
//int  cli_get_debug_level(void);
//int  cli_set_debug_on_lport(port_handle_t lport_handle, int debug_level);
//int  cli_unset_debug_on_lport(port_handle_t lport_handle, int debug_level);
//void cli_show_debug_enabled_lports(struct MLt_lacp_api__lportsBeingDebugged *);
//void cli_set_actor_system_priority(int actor_system_priority);
//int  cli_get_actor_system_priority(void);

//*****************************************************************
// Function : mlacp_process_rx_pdu
//*****************************************************************
void
mlacp_process_rx_pdu(struct ML_event *pevent)
{
    struct MLt_drivers_mlacp__rxPdu *pRxPduMsg = pevent->msg;
    unsigned char *data = (unsigned char *)pRxPduMsg->data;

    LACP_process_input_pkt(pRxPduMsg->lport_handle, data, pRxPduMsg->pktLen);

} // mlacp_process_rx_pdu

//*****************************************************************
// Function : mlacp_process_timer
//*****************************************************************
void
mlacp_process_timer(void)
{
    RENTRY();

    LACP_periodic_tx();
    LACP_current_while_expiry();

    REXIT();

} // mlacp_process_timer

//*****************************************************************
// Function : mlacp_process_vlan_msg
// These messages are from VLAN mgr.
//*****************************************************************
void
mlacp_process_vlan_msg(ML_event *pevent)
{
    RENTRY();

    //***************************************************************
    // The card is known to be up & running when VLAN mgr gives us
    // any LACP related message.
    //***************************************************************

    switch (pevent->msgnum) {

        //***************************************************************
        // 'Set' commands -
        //***************************************************************
        case MLm_vpm_api__set_lacp_lport_params_event:
        {
            mlacpVapiLportEvent(pevent);
        }
        break;

        //***************************************************************
        // Admin related messages - link down/up etc.
        //***************************************************************
        case MLm_vpm_api__lport_state_up:
        {
            struct MLt_vpm_api__lport_state_change *pmsg = pevent->msg;
            mlacpVapiLinkUp(pmsg->lport_handle, pmsg->link_speed);
        }
        break;

        case MLm_vpm_api__lport_state_down:
        {
            struct MLt_vpm_api__lport_state_change *pmsg = pevent->msg;
            mlacpVapiLinkDown(pmsg->lport_handle);
        }
        break;

        case MLm_vpm_api__set_lacp_sport_params:
        case MLm_vpm_api__unset_lacp_sport_params:
        {
            // OpenSwitch
            // This is just a notification that something changed.
            // If partner SYSPRI or partner SYSID fields changed,
            // it will become UNSELECTED and trigger FSM state change.
            struct  MLt_vpm_api__lacp_sport_params *pmsg = pevent->msg;
            mlacpVapiSportParamsChange(pevent->msgnum, pmsg);
        }
        break;

        case MLm_vpm_api__set_lport_fallback_status:
        {
            struct MLt_vpm_api__lport_fallback_status *pMsg = pevent->msg;

            RDEBUG(DL_LACP_RCV,
                   "Lport fallback new status=%d\n",
                   pMsg->status);

            set_lport_fallback_status(pMsg->lport_handle, pMsg->status);
        }
        break;

        default:
        {
            VLOG_ERR("%s : Unknown req (%d)", __FUNCTION__, pevent->msgnum);
        }
        break;
    }

    REXIT();

} // mlacp_process_vlan_msg

//*****************************************************************
// Function : mlacp_process_api_msg
// These messages could be either from configd or from lacpsh.
//*****************************************************************
void
mlacp_process_api_msg(ML_event *pevent)
{
    RENTRY();

    switch (pevent->msgnum) {

        case MLm_lacp_api__setActorSysPriority:
        {
            struct MLt_lacp_api__actorSysPriority *pMsg = pevent->msg;

            RDEBUG(DL_LACP_RCV, "Actor sys priority=%d\n", pMsg->actor_system_priority);

            actor_system_priority = pMsg->actor_system_priority;
            set_all_port_system_priority();
        }
        break;

        case MLm_lacp_api__setActorSysMac:
        {
            struct MLt_lacp_api__actorSysMac *pMsg = pevent->msg;
            unsigned char *mac_addr = pMsg->actor_sys_mac;

            memcpy(my_mac_addr, mac_addr, MAC_ADDR_LENGTH);
            set_all_port_system_mac_addr();

            RDEBUG(DL_LACP_RCV, "Set sys mac addr: %02x:%02x:%02x:%02x:%02x:%02x\n",
                   my_mac_addr[0], my_mac_addr[1], my_mac_addr[2],
                   my_mac_addr[3], my_mac_addr[4], my_mac_addr[5]);
        }
        break;

        case MLm_lacp_api__set_lport_overrides:
        {
            struct MLt_lacp_api__set_lport_overrides *pMsg = pevent->msg;

            set_lport_overrides(pMsg->lport_handle, pMsg->priority, pMsg->actor_sys_mac);

            RDEBUG(SL_LACP_RCV, "Set interface %lld port overrides: %d, %02x:%02x:%02x:%02x:%02x:%02x\n",
                    pMsg->lport_handle,
                    pMsg->priority,
                    pMsg->actor_sys_mac[0],
                    pMsg->actor_sys_mac[1],
                    pMsg->actor_sys_mac[2],
                    pMsg->actor_sys_mac[3],
                    pMsg->actor_sys_mac[4],
                    pMsg->actor_sys_mac[5]);
        }
        break;

        case MLm_vpm_api__create_sport:
        {
            struct MLt_vpm_api__create_sport *pMsg = pevent->msg;
            super_port_t *psport;
            int status;

            status = mvlan_sport_create(pMsg, &psport);

            RDEBUG(DL_LACP_RCV, "Create LAG.  handle=0x%llx\n", pMsg->handle);

            if (R_SUCCESS != status) {
                VLOG_ERR("Failed to create LAG sport, status=%d", status);
            }
        }
        break;

        case MLm_vpm_api__delete_sport:
        {
            struct MLt_vpm_api__delete_sport *pMsg = pevent->msg;
            super_port_t *psport;
            int status = R_SUCCESS;

            status = mvlan_get_sport(pMsg->handle, &psport,
                                     MLm_vpm_api__get_sport);
            if (R_SUCCESS == status) {

                status = mvlan_destroy_sport(psport);

                RDEBUG(DL_LACP_RCV, "Delete LAG.  handle=0x%llx\n", pMsg->handle);

                if (R_SUCCESS != status) {
                    VLOG_ERR("Failed to delete LAG sport, status=%d", status);
                }
            } else {
                VLOG_ERR("Failed to find sport on delete, handle=0x%llx.",
                         pMsg->handle);
            }
        }
        break;

        case MLm_vpm_api__set_lacp_sport_params:
        case MLm_vpm_api__unset_lacp_sport_params:
        {
            struct MLt_vpm_api__lacp_sport_params *pMsg = pevent->msg;
            int status;

            RDEBUG(DL_LACP_RCV, "%s LAG Sport parameters.  handle=0x%llx\n",
                   pevent->msgnum == MLm_vpm_api__set_lacp_sport_params ?
                   "Set" : "Unset",
                   pMsg->sport_handle);

            status = mvlan_api_modify_sport_params(pMsg, pevent->msgnum);

            if (status != R_SUCCESS) {
                VLOG_ERR("Failed to set/unset LAG Sport parms, status=%d", status);
            }
        }
        break;

        default:
        {
            VLOG_ERR("%s : Unknown req (%d)", __FUNCTION__, pevent->msgnum);
        }
        break;
    }

    REXIT();

} // mlacp_process_api_msg

//*****************************************************************
// Function : mlacpVapiLportEvent
//*****************************************************************
void
mlacpVapiLportEvent(struct ML_event *pevent)
{
    struct MLt_vpm_api__lport_lacp_change *placp_msg = pevent->msg;

    //***************************************************************
    // Assuming that every time we get called with the whole set of
    // parameters and all flag bits will be set as required.
    //***************************************************************
    if (placp_msg->lacp_state == LACP_STATE_ENABLED) {

        RDEBUG(DL_LACP_RCV, "LACP message on lport_handle 0x%llx"
               " port_id 0x%x, flags 0x%x, state %d, port_key 0x%x, pri 0x%x,"
               " activity %d, timeout %d, aggregation %d,"
               " link_state 0x%x link_speed 0x%x collecting_ready=%d\n",
               placp_msg->lport_handle,
               placp_msg->port_id,
               placp_msg->flags,
               placp_msg->lacp_state,
               placp_msg->port_key,
               placp_msg->port_priority,
               placp_msg->lacp_activity,
               placp_msg->lacp_timeout,
               placp_msg->lacp_aggregation,
               placp_msg->link_state,
               placp_msg->link_speed,
               placp_msg->collecting_ready);

        // Check if an individual parameter that can be updated without having
        // to reinitialize the state machine is changed. If so, call the update
        // funtion to avoid LAG dissolution.
        if (LACP_LPORT_DYNAMIC_FIELDS_PRESENT & placp_msg->flags) {

            LACP_update_port_params(placp_msg->lport_handle,
                                    placp_msg->flags,
                                    (short) placp_msg->lacp_timeout,
                                    (short) placp_msg->collecting_ready);
        } else {
            LACP_initialize_port(placp_msg->lport_handle,
                                 (unsigned short) placp_msg->port_id,
                                 placp_msg->flags,
                                 (short) placp_msg->port_key,
                                 (short) placp_msg->port_priority,
                                 (short) placp_msg->lacp_activity,
                                 (short) placp_msg->lacp_timeout,
                                 (short) placp_msg->lacp_aggregation,
                                 placp_msg->link_state,
                                 placp_msg->link_speed,
                                 placp_msg->collecting_ready,
                                 (short)placp_msg->sys_priority,
                                 placp_msg->sys_id);
        }
    } else {
        RDEBUG(DL_LACP_RCV, "disable LACP on lport_handle 0x%llx"
               " port_id 0x%x, flags 0x%x, port_key 0x%x, pri 0x%x, "
               " link_state 0x%x\n",
               placp_msg->lport_handle,
               placp_msg->port_id,
               placp_msg->flags,
               placp_msg->port_key,
               placp_msg->port_priority,
               placp_msg->link_state);

        LACP_disable_lacp(placp_msg->lport_handle);
    }

} // mlacpVapiLportEvent
