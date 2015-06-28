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

#include <nemo/nemo_types.h>
#include <nemo/avl.h>
#include <nemo/protocol/lacp/api.h>
#include <nemo/protocol/drivers/mlacp.h>
#include <nemo/pm/pm_cmn.h>
#include <nemo/lacp/lacp_cmn.h>
#include <nemo/lacp/mlacp_debug.h>

#include "lacp.h"
#include "mlacp_fproto.h"
#include "vpm/mvlan_lacp.h"

//Halon
#include "lacp_halon.h"
#include "lacp_support.h"
#include "lacp_halon_if.h"
#include "vpm/mvlan_sport.h"

//***********************************************************************
// Global & extern Variables
//***********************************************************************
// Halon: The following system related info are in lacp_support.c
extern unsigned char my_mac_addr[];
extern uint actor_system_priority;
extern void set_all_port_system_priority(void);

//***********************************************************************
// (local) Function prototypes
//***********************************************************************
// Halon
//void cli_set_debug_level(int level);
//void cli_unset_debug_level(int level);
//int  cli_get_debug_level(void);
//int  cli_set_debug_on_lport(port_handle_t lport_handle, int debug_level);
//int  cli_unset_debug_on_lport(port_handle_t lport_handle, int debug_level);
//void cli_show_debug_enabled_lports(struct MLt_lacp_api__lportsBeingDebugged *);
//void cli_set_actor_system_priority(int actor_system_priority);
//int  cli_get_actor_system_priority(void);

//*****************************************************************
// Function : mlacpBoltonRxPdu
//*****************************************************************
void
mlacpBoltonRxPdu(struct ML_event *pevent)
{
    struct MLt_drivers_mlacp__rxPdu *pRxPduMsg = pevent->msg;
    unsigned char *data = (unsigned char *)pRxPduMsg->data;
    void LACP_process_input_pkt(port_handle_t lport_handle, unsigned char *, int len);

    LACP_process_input_pkt(pRxPduMsg->lport_handle, data, pRxPduMsg->pktLen);

} // mlacpBoltonRxPdu

//*****************************************************************
// Function : mlacp_process_timer
//*****************************************************************
void
mlacp_process_timer(struct MLt_msglib__timer *tevent __attribute__ ((unused)))
{
    void LACP_periodic_tx(void);
    void LACP_current_while_expiry(void);

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
    // any LACP related message. XXX
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
            // Halon
            // This is just a notification that something changed.
            // If partner SYSPRI or partner SYSID fields changed,
            // it will become UNSELECTED and trigger FSM state change.
            struct  MLt_vpm_api__lacp_sport_params *pmsg = pevent->msg;
            mlacpVapiSportParamsChange(pevent->msgnum, pmsg);
        }
        break;

        case MLm_vpm_api__lacp_attach_reply:
        // not used now
        break;

        default:
        {
            RDEBUG(DL_ERROR, "%s : Unknown req (%d) \n",
                   __FUNCTION__, pevent->msgnum);
        }
        break;
    }

    REXIT();

} // mlacp_process_vlan_msg

//*****************************************************************
// Function : mlacp_process_api_msg
// These messages could be either from configd or from nemosh.
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

        // Halon: This should be for debugging only.  System
        //          MAC address should be read from h/w.
        case MLm_lacp_api__setActorSysMac:
        {
            struct MLt_lacp_api__actorSysMac *pMsg = pevent->msg;
            unsigned char *mac_addr = pMsg->actor_sys_mac;

            memcpy(my_mac_addr, mac_addr, 6);

            RDEBUG(DL_LACP_RCV, "Set sys mac addr: %02x:%02x:%02x:%02x:%02x:%02x\n",
                   my_mac_addr[0], my_mac_addr[1], my_mac_addr[2],
                   my_mac_addr[3], my_mac_addr[4], my_mac_addr[5]);
        }
        break;

        // Halon: static LAG support.
        case MLm_vpm_api__static_lag_create:
        {
            int lag_id;
            struct MLt_vpm_api__static_lag *pMsg = pevent->msg;

            lag_id = (int)PM_HANDLE2LAG(pMsg->sport_handle);
            RDEBUG(DL_LACP_RCV, "Create STATIC LAG %d.  handle=0x%llx\n",
                   lag_id, pMsg->sport_handle);

            // Static LAGs are not tracked in the LACP
            // state machine. Simply create trunk in h/w.
            halon_create_lag_in_hw(lag_id);
        }
        break;

        // Halon: static LAG support.
        case MLm_vpm_api__static_lag_delete:
        {
            int lag_id;
            struct MLt_vpm_api__static_lag *pMsg = pevent->msg;

            lag_id = (int)PM_HANDLE2LAG(pMsg->sport_handle);
            RDEBUG(DL_LACP_RCV, "Destroy STATIC LAG %d.  handle=0x%llx\n",
                   lag_id, pMsg->sport_handle);

            // Static LAGs are not tracked in the LACP
            // state machine. Simply destroy trunk in h/w.
            halon_destroy_lag_in_hw(lag_id);
        }
        break;

        // Halon: static LAG support.
        case MLm_vpm_api__static_lag_attach_lport:
        {
            int port;
            int lag_id;
            struct MLt_vpm_api__static_lag *pMsg = pevent->msg;

            lag_id = (int)PM_HANDLE2LAG(pMsg->sport_handle);
            port = (int)PM_HANDLE2PORT(pMsg->lport_handle);

            RDEBUG(DL_LACP_RCV, "Attach lport %d to STATIC LAG %d."
                   "sport_handle=0x%llx, lport_handle=0x%llx\n",
                   port, lag_id, pMsg->sport_handle, pMsg->lport_handle);

            // Static LAGs are not tracked in the LACP
            // state machine. Simply attach port in h/w.
            halon_attach_port_in_hw(lag_id, port);

            // For static LAGs, enable distributing right away.
            halon_trunk_port_egr_enable(lag_id, port);

            // Update DB with new info.
            db_add_lag_port(lag_id, port);
        }
        break;

        // Halon: static LAG support.
        case MLm_vpm_api__static_lag_detach_lport:
        {
            int port;
            int lag_id;
            struct MLt_vpm_api__static_lag *pMsg = pevent->msg;

            lag_id = (int)PM_HANDLE2LAG(pMsg->sport_handle);
            port = (int)PM_HANDLE2PORT(pMsg->lport_handle);

            RDEBUG(DL_LACP_RCV, "Detach lport %d from STATIC LAG %d."
                   "sport_handle=0x%llx, lport_handle=0x%llx\n",
                   port, lag_id, pMsg->sport_handle, pMsg->lport_handle);

            // Static LAGs are not tracked in the LACP
            // state machine. Simply detach port in h/w.
            halon_detach_port_in_hw(lag_id, port);

            // Update DB with new info.
            db_delete_lag_port(lag_id, port);
        }
        break;

        // Halon: static LAG support.
        case MLm_vpm_api__static_to_dynamic_lag:
        {
            int lag_id;
            int status;
            super_port_t *psport;
            struct MLt_vpm_api__create_sport create_msg;
            struct MLt_vpm_api__static_lag *pMsg = pevent->msg;

            lag_id = (int)PM_HANDLE2LAG(pMsg->sport_handle);

            RDEBUG(DL_LACP_RCV, "Changing from STATIC to dynamic LAG %d. "
                   " handle=0x%llx\n", lag_id, pMsg->sport_handle);

            // No need to delete trunk in h/w, but do need
            // to get this LAG into the state machine.
            memset(&create_msg, 0, sizeof(create_msg));
            create_msg.handle = pMsg->sport_handle;
            create_msg.type   = STYPE_802_3AD;
            status = mvlan_sport_create(&create_msg, &psport);
            if (R_SUCCESS != status) {
                RDEBUG(DL_ERROR, "Failed to create LAG sport, status=%d\n", status);
            }
        }
        break;

        // Halon: static LAG support.
        case MLm_vpm_api__dynamic_to_static_lag:
        {
            int status = R_SUCCESS;
            super_port_t *psport;
            struct MLt_vpm_api__static_lag *pMsg = pevent->msg;

            RDEBUG(DL_LACP_RCV, "Changing from dynamic to STATIC LAG %d. "
                   " handle=0x%llx\n",
                   (int)PM_HANDLE2LAG(pMsg->sport_handle), pMsg->sport_handle);

            status = mvlan_get_sport(pMsg->sport_handle, &psport,
                                     MLm_vpm_api__get_sport);
            if (R_SUCCESS == status) {
                status = mvlan_destroy_sport(psport);
                RDEBUG(DL_LACP_RCV, "Destroy sport.  handle=0x%llx\n", pMsg->sport_handle);

                if (R_SUCCESS != status) {
                    RDEBUG(DL_ERROR, "Failed to delete LAG sport, status=%d\n", status);
                }
            } else {
                RDEBUG(DL_ERROR, "Failed to find sport on change to static LAG, "
                       "handle=0x%llx.\n", pMsg->sport_handle);
            }
        }
        break;

        // Halon: Added this here instead of using VPM.
        case MLm_vpm_api__create_sport:
        {
            struct MLt_vpm_api__create_sport *pMsg = pevent->msg;
            super_port_t *psport;
            int status;

            status = mvlan_sport_create(pMsg, &psport);

            RDEBUG(DL_LACP_RCV, "Create LAG.  handle=0x%llx\n", pMsg->handle);

            if (R_SUCCESS == status) {
                // Halon: Add hook to create trunk entity in switch h/w.
                halon_create_lag_in_hw(PM_HANDLE2LAG(psport->handle));
            } else {
                RDEBUG(DL_ERROR, "Failed to create LAG sport, status=%d\n", status);
            }
        }
        break;

        // Halon: Added this here instead of using VPM.
        case MLm_vpm_api__delete_sport:
        {
            struct MLt_vpm_api__delete_sport *pMsg = pevent->msg;
            super_port_t *psport;
            int status = R_SUCCESS;

            status = mvlan_get_sport(pMsg->handle, &psport,
                                     MLm_vpm_api__get_sport);
            if (R_SUCCESS == status) {
                int lag_id;

                // Save LAG ID first.
                lag_id = PM_HANDLE2LAG(psport->handle);
                status = mvlan_destroy_sport(psport);

                RDEBUG(DL_LACP_RCV, "Delete LAG.  handle=0x%llx\n", pMsg->handle);

                if (R_SUCCESS == status) {
                    // Halon: Add hook to destroy trunk entity in switch h/w.
                    halon_destroy_lag_in_hw(lag_id);
                } else {
                    RDEBUG(DL_ERROR, "Failed to delete LAG sport, status=%d\n", status);
                }
            } else {
                RDEBUG(DL_ERROR, "Failed to find sport on delete, haldne=0x%llx.\n",
                       pMsg->handle);
            }
        }
        break;

        // Halon: Added this here instead of using VPM.
        case MLm_vpm_api__set_lacp_sport_params:
        {
            struct MLt_vpm_api__lacp_sport_params *pMsg = pevent->msg;
            int status;

            RDEBUG(DL_LACP_RCV, "Set LAG Sport parameters.  handle=0x%llx\n",
                   pMsg->sport_handle);

            status = mvlan_api_modify_sport_params(pMsg, pevent->msgnum);

            if (status != R_SUCCESS) {
                RDEBUG(DL_ERROR, "Failed to set LAG Sport parms, status=%d\n", status);
            }
        }
        break;

        default:
        {
            RDEBUG(DL_ERROR, "%s : Unknown req (%d) \n",
                   __FUNCTION__, pevent->msgnum);
        }
        break;
    }

    REXIT();

} // mlacp_process_api_msg

//*****************************************************************
// Function : mlacp_process_showmgr_msg
// Process messages from the Show Manager.
//*****************************************************************
void
mlacp_process_showmgr_msg(ML_event *pevent __attribute__ ((unused)))
{
    RDEBUG(DL_ERROR, " !!! Halon !!! %s not yet implemented\n", __FUNCTION__);

#if 0  // Halon
    struct MLt_mgmt_transport__show *preq;
    void *placp_req;
    int bad = 0;

    if (pevent->msgnum != MLm_mgmt_transport__show) {
        return;
    }

    RENTRY();

    preq = pevent->msg;
    placp_req = showmgr_demarshal_request("lacp/api",
                                          preq->object_id,
                                          preq->msg_len,
                                          preq->msg);
    switch (preq->object_id) {

        case MLm_lacp_api__getLportParams:
        {
            struct MLt_lacp_api__lport_params *pMsg =
                (struct MLt_lacp_api__lport_params *)placp_req;
            void cli_get_lport_params(struct MLt_lacp_api__lport_params *);

            cli_get_lport_params(pMsg);
        }
        break;

        case MLm_lacp_api__getnextLportParams:
        {
            struct MLt_lacp_api__lport_params *pMsg =
                (struct MLt_lacp_api__lport_params *)placp_req;
            void cli_getnext_lport_params(struct MLt_lacp_api__lport_params *);

            cli_getnext_lport_params(pMsg);
        }
        break;

        case MLm_lacp_api__getLportProtocol:
        {
            struct MLt_lacp_api__lport_protocol *pMsg =
                (struct MLt_lacp_api__lport_protocol *)placp_req;
            void cli_get_lport_protocol(struct MLt_lacp_api__lport_protocol *);

            cli_get_lport_protocol(pMsg);
        }
        break;

        case MLm_lacp_api__getnextLportProtocol:
        {
            struct MLt_lacp_api__lport_protocol *pMsg =
                (struct MLt_lacp_api__lport_protocol *)placp_req;
            void cli_getnext_lport_protocol(struct MLt_lacp_api__lport_protocol *);

            cli_getnext_lport_protocol(pMsg);
        }
        break;

        case MLm_lacp_api__getLportStats:
        {
            struct MLt_lacp_api__lport_stats *pMsg =
                (struct MLt_lacp_api__lport_stats *)placp_req;
            void cli_get_lport_stats(struct MLt_lacp_api__lport_stats *);

            cli_get_lport_stats(pMsg);
        }
        break;

        case MLm_lacp_api__getnextLportStats:
        {
            struct MLt_lacp_api__lport_stats *pMsg =
                (struct MLt_lacp_api__lport_stats *)placp_req;
            void cli_getnext_lport_stats(struct MLt_lacp_api__lport_stats *);

            cli_getnext_lport_stats(pMsg);
        }
        break;

        case MLm_lacp_api__getActorSysPriority:
        {
            struct MLt_lacp_api__actorSysPriority *pMsg =
                (struct MLt_lacp_api__actorSysPriority *)placp_req;

            pMsg->actor_system_priority = cli_get_actor_system_priority();
        }
        break;

        case MLm_lacp_api__getLportInfo:
        {
            struct MLt_lacp_api__lport_info *pMsg =
                (struct MLt_lacp_api__lport_info *)placp_req;
            void cli_fill_lacp_lport_info(struct MLt_lacp_api__lport_info *);

            cli_fill_lacp_lport_info(pMsg);
        }
        break;

        case MLm_lacp_api__getLagTuples:
        {
            struct MLt_lacp_api__lag_tuple_info *pMsg =
                (struct MLt_lacp_api__lag_tuple_info *)placp_req;
            void cli_fill_lag_tuple_info(struct MLt_lacp_api__lag_tuple_info *);

            cli_fill_lag_tuple_info(pMsg);
        }
        break;

        case MLm_lacp_api__getKeyGroups:
        {
            struct MLt_lacp_api__key_group_info *pMsg =
                (struct MLt_lacp_api__key_group_info *)placp_req;
            void cli_retrieve_key_group(struct MLt_lacp_api__key_group_info *);

            cli_retrieve_key_group(pMsg);
        }
        break;

        case MLm_lacp_api__clearLportStats:
        {
            struct MLt_lacp_api__lport_stats *pMsg =
                (struct MLt_lacp_api__lport_stats *)placp_req;
            void cli_clear_lport_stats(struct MLt_lacp_api__lport_stats *);

            cli_clear_lport_stats(pMsg);
        }
        break;

        case MLm_lacp_api__clearnextLportStats:
        {
            struct MLt_lacp_api__lport_stats *pMsg =
                (struct MLt_lacp_api__lport_stats *)placp_req;
            void cli_clearnext_lport_stats(struct MLt_lacp_api__lport_stats *);

            cli_clearnext_lport_stats(pMsg);
        }
        break;

        default:
        {
            RDEBUG(DL_ERROR, "%s : Unknown req (%d) \n",
                   __FUNCTION__, preq->object_id);
            bad = 1;
        }
        break;
    }

    if (!bad) {
        showmgr_marshal_reply(pevent,
                              "lacp/api",
                              preq->object_id,
                              placp_req,
                              preq);
    }
    ml_marshal_free("lacp/api", preq->object_id, placp_req);

#endif // Halon
} // mlacp_process_showmgr_msg

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
                                 (short) placp_msg->port_id,
                                 placp_msg->flags,
                                 (short) placp_msg->port_key,
                                 (short) placp_msg->port_priority,
                                 (short) placp_msg->lacp_activity,
                                 (short) placp_msg->lacp_timeout,
                                 (short) placp_msg->lacp_aggregation,
                                 placp_msg->link_state,
                                 placp_msg->link_speed,
                                 placp_msg->collecting_ready);
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

//*****************************************************************
// Function : mlacp_process_diagmgr_msg
//*****************************************************************
void
mlacp_process_diagmgr_msg(ML_event *pevent __attribute__ ((unused)))
{
    RDEBUG(DL_ERROR, " !!! Halon !!! %s not yet implemented\n", __FUNCTION__);

#if 0  // Halon
    struct MLt_mgmt_transport__show *preq;
    void *placp_req;
    int bad = 0;

    if (pevent->msgnum != MLm_mgmt_transport__show) {
        return;
    }

    RENTRY();

    preq = pevent->msg;
    placp_req = diagmgr_demarshal_request("lacp/api",
                                          preq->object_id,
                                          preq->msg_len,
                                          preq->msg);
    switch (preq->object_id) {

        case MLm_lacp_api__setDebugLevel:
        {
            struct MLt_lacp_api__debugLevel *pInMsg =
                (struct MLt_lacp_api__debugLevel *)placp_req;

            cli_set_debug_level(pInMsg->debug_level);
        }
        break;

        case MLm_lacp_api__unsetDebugLevel:
        {
            struct MLt_lacp_api__debugLevel *pInMsg =
                (struct MLt_lacp_api__debugLevel *)placp_req;

            cli_unset_debug_level(pInMsg->debug_level);
        }
        break;

        case MLm_lacp_api__setDebugOnLport:
        {
            struct MLt_lacp_api__debugOnLport *pInMsg =
                (struct MLt_lacp_api__debugOnLport *)placp_req;

            cli_set_debug_on_lport(
                pInMsg->lport_handle, pInMsg->debug_level);
        }
        break;

        case MLm_lacp_api__unsetDebugOnLport:
        {
            struct MLt_lacp_api__debugOnLport *pInMsg =
                (struct MLt_lacp_api__debugOnLport *)placp_req;

            cli_unset_debug_on_lport(
                pInMsg->lport_handle, pInMsg->debug_level);
        }
        break;

        case MLm_lacp_api__getDebugLevel:
        {
            struct MLt_lacp_api__debugLevel *pInMsg =
                (struct MLt_lacp_api__debugLevel *)placp_req;

            pInMsg->debug_level = cli_get_debug_level();
        }
        break;

        case MLm_lacp_api__showLportsBeingDebugged:
        {
            struct MLt_lacp_api__lportsBeingDebugged *pInMsg =
                (struct MLt_lacp_api__lportsBeingDebugged *)placp_req;

            cli_show_debug_enabled_lports(pInMsg);
        }
        break;

        case MLm_lacp_api__test_attach_detach:
        {
            struct MLt_lacp_api__test_attach_detach *pInMsg =
                (struct MLt_lacp_api__test_attach_detach *)placp_req;
            void cli_test_attach_detach(
                struct MLt_lacp_api__test_attach_detach *pInMsg);

            cli_test_attach_detach(pInMsg);
        }
        break;

        default:
        {
            RDEBUG(DL_ERROR, "%s : Unknown req (%d) \n",
                   __FUNCTION__, preq->object_id);
            bad = 1;
        }
        break;
    }

    if (!bad) {
        diagmgr_marshal_reply(pevent,
                              "lacp/api",
                              preq->object_id,
                              placp_req,
                              preq);
    }
    ml_marshal_free("lacp/api", preq->object_id, placp_req);

#endif // Halon
} // mlacp_process_diagmgr_msg
