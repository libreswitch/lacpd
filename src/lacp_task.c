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
#include <sys/time.h>
#include <string.h>
#include <strings.h>

#include "lacp_cmn.h"
#include <avl.h>
#include <nlib.h>

#include <pm_cmn.h>

#include "lacp_stubs.h"
#include <lacp_cmn.h>
#include <mlacp_debug.h>
#include <lacp_fsm.h>

#include "lacp.h"
#include "mvlan_lacp.h"
#include "lacp_support.h"
#include "mlacp_fproto.h"

VLOG_DEFINE_THIS_MODULE(lacp_task);

/****************************************************************************
 *                    Global Variables Definition
 ****************************************************************************/
/* IEEE 802.3 Slow_Protocols_Multicast group address */
const unsigned char lacp_mcast_addr[MAC_ADDR_LENGTH] =
{0x01, 0x80, 0xc2, 0x00, 0x00, 0x02};

const unsigned char default_partner_system_mac[MAC_ADDR_LENGTH] =
{0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

/****************************************************************************
 *   Prototypes for static functions
 ****************************************************************************/
static void periodic_tx_timer_expiry(lacp_per_port_variables_t *);
static void current_while_timer_expiry(lacp_per_port_variables_t *);
static void mux_wait_while_timer_expiry(lacp_per_port_variables_t *);
static int LACP_marker_responder(lacp_per_port_variables_t *, void *);
static marker_pdu_payload_t *LACP_build_marker_response_payload(
                                           port_handle_t, marker_pdu_payload_t *);
static void LACP_transmit_marker_response(port_handle_t, void *);
static int is_pkt_from_same_system(lacp_per_port_variables_t *, lacpdu_payload_t *);
int lacp_lag_port_match(void *, void *);


/**************************************************************
 *       Periodic Tx Timer handler routines
 *************************************************************/

/*----------------------------------------------------------------------
 * Function: LACP_periodic_tx()
 * Synopsis: Goes thro' all the ports in all LACP link groups and does
 *           periodic Tx on each one of them depending upon their states.
 * Input  :
 * Returns:  void
 *----------------------------------------------------------------------*/
void
LACP_periodic_tx(void)
{
    lacp_per_port_variables_t *plpinfo;

    RENTRY();

    plpinfo = LACP_AVL_FIRST(lacp_per_port_vars_tree);

    while (plpinfo) {
        if (plpinfo->debug_level & DBG_TX_FSM) {
            print_lacp_fsm_state(plpinfo->lport_handle);
        }

        if (plpinfo->lacp_up == TRUE) { /* LACP port is initialized */
            periodic_tx_timer_expiry(plpinfo);
            mux_wait_while_timer_expiry(plpinfo);
        }
        plpinfo = LACP_AVL_NEXT(plpinfo->avlnode);
    }

    REXIT();

} /* LACP_periodic_tx */

/*----------------------------------------------------------------------
 * Function: periodic_tx_timer_expiry(int)
 * Synopsis: For the given port does periodic Tx if periodic Tx state
 *           of the port is in Fast Periodic or Slow Periodic states.
 * Input  :  int - port number
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
periodic_tx_timer_expiry(lacp_per_port_variables_t *plpinfo)
{
    RENTRY();

    /********************************************************************
     * If the state is no periodic do nothing.
     ********************************************************************/
    if (plpinfo->periodic_tx_fsm_state == PERIODIC_TX_FSM_NO_PERIODIC_STATE) {
        if (plpinfo->debug_level & DBG_TX_FSM) {
            RDBG("%s : do nothing (lport 0x%llx)\n",
                 __FUNCTION__, plpinfo->lport_handle);
        }
    } else {
        RDEBUG(DL_TIMERS, "decrement the expiry counter (lport 0x%llx)\n",
               plpinfo->lport_handle);

        /*********************************************************************
         * Decrement the expiry counter if greater than 0.
         *********************************************************************/
        if (plpinfo->periodic_tx_timer_expiry_counter > 0) {

            plpinfo->periodic_tx_timer_expiry_counter--;

            /*********************************************************************
             * Since the lowest expiry time is 1 second, we clear the async Tx
             * counter every 1 second.
             *********************************************************************/
            plpinfo->async_tx_count = 0;

            /*********************************************************************
             * If expiry counter is 0, generate the appropriate event.
             *********************************************************************/
            if (plpinfo->periodic_tx_timer_expiry_counter == 0) {

                /* Generate periodic Tx timer expired event (E3) */
                LACP_periodic_tx_fsm(E3,
                                     plpinfo->periodic_tx_fsm_state,
                                     plpinfo);

            } else if (TRUE == plpinfo->lacp_control.ntt) {
                // OpenSwitch FIX: if "async_tx_count" reached the max while
                // NTT was true, then LACPDUs would not have been
                // transmitted.  We need to transmit it now if NTT is
                // still true and periodic_tx_timer didn't expire in this
                // round (i.e. long timeout).
                LACP_async_transmit_lacpdu(plpinfo);
            }

        } // if (plpinfo->periodic_tx_timer_expiry_counter > 0)
    } // else - (FSM state != PERIODIC_TX_FSM_NO_PERIODIC_STATE)

    REXIT();

} /* periodic_tx_timer_expiry */

/*----------------------------------------------------------------------
 * Function: mux_wait_while_timer_expiry(int)
 * Synopsis: Decrements the counter, if it reaches 0, causes
 *           an approp. event in the Mux machine.
 * Input  :  physical port number
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
mux_wait_while_timer_expiry(lacp_per_port_variables_t *lacp_port)
{
    LAG_t *lag;
    lacp_per_port_variables_t *plp;

    RENTRY();

    RDEBUG(DL_TIMERS, "%s: lport 0x%llx\n", __FUNCTION__, lacp_port->lport_handle);

    lag = lacp_port->lag;
    if (!lag) {
        return;
    }

    if (lag->pplist == NULL) {
        return;
    }

    if (n_list_find_data(lag->pplist,
                         &lacp_lag_port_match,
                         &lacp_port->lport_handle) == NULL) {
        VLOG_ERR("lport (ox%llx) not set ??", lacp_port->lport_handle);
        return;
    }

    if (lacp_port->wait_while_timer_expiry_counter > 0) {

        RDEBUG(DL_TIMERS, "decrement wait_while_timer (lport 0x%llx)\n",
               lacp_port->lport_handle);

        lacp_port->wait_while_timer_expiry_counter--;

        /*
         * If expiry counter is 0, check for ready and selected variables.
         * If selected is SELECTED for the port and ready is TRUE for the
         * link group, then generate event E3 for the port's mux fsm.
         */
        if (lacp_port->wait_while_timer_expiry_counter <= 0) {

            lacp_port->lacp_control.ready_n = TRUE;
            lag->ready = TRUE;      /* assume */

            for (plp = LACP_AVL_FIRST(lacp_per_port_vars_tree);
                 plp;
                 plp = LACP_AVL_NEXT(plp->avlnode)) {

                if (n_list_find_data(lag->pplist,
                                     &lacp_lag_port_match,
                                     &plp->lport_handle) == NULL) {
                    continue;
                }

                if (plp->lacp_control.ready_n == FALSE) {
                    lag->ready = FALSE;
                    break;
                }
            }

            if (lag->ready == TRUE &&
                lacp_port->lacp_control.selected ==  SELECTED) {
                LACP_mux_fsm(E3,
                             lacp_port->mux_fsm_state,
                             lacp_port);
            } else {
                start_wait_while_timer(lacp_port);
            }

            lag->ready = FALSE;
        }
    }

    REXIT();

} /* mux_wait_while_timer_expiry */


/*********************************************************************
 *     Receive Timer (current while timer) handler routines
 *********************************************************************/

/*----------------------------------------------------------------------
 * Function: LACP_current_while_expiry()
 * Synopsis: Goes thro' all the ports in all LACP link groups and does
 *           current while expiry action on each one of them depending
 *           upon their states.
 * Input  :
 * Returns:  void
 *----------------------------------------------------------------------*/
void
LACP_current_while_expiry(void)
{
    lacp_per_port_variables_t *lacp_port;

    RENTRY();

    lacp_port = LACP_AVL_FIRST(lacp_per_port_vars_tree);
    while (lacp_port) {
        if (lacp_port->lacp_up == TRUE) { /* LACP port is initialized */

            RDEBUG(DL_TIMERS, "invoke current_while_timer_expiry.  lport=0x%llx\n",
                   lacp_port->lport_handle);

            current_while_timer_expiry(lacp_port);
        }

        lacp_port = LACP_AVL_NEXT(lacp_port->avlnode);
    }

    REXIT();

} /* LACP_current_while_expiry */

/*----------------------------------------------------------------------
 * Function: current_while_timer_expiry(int port_number)
 * Synopsis: Decrements the expiry counter, if greater than 0.
 *           If counter reaches 0, generates a current_while timer
 *           expired event (E2).
 *
 * Input  :
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
current_while_timer_expiry(lacp_per_port_variables_t *plpinfo)
{
    RENTRY();

    RDEBUG(DL_TIMERS, "%s: lport 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);

    if (plpinfo->current_while_timer_expiry_counter > 0) {

        RDEBUG(DL_TIMERS, "current_while_timer %d lport 0x%llx\n",
               plpinfo->current_while_timer_expiry_counter,
               plpinfo->lport_handle);

        /********************************************************************
         * Decrement the expiry counter and if it has reached 0, generate
         * Event E2.
         ********************************************************************/
        plpinfo->current_while_timer_expiry_counter--;

        if (plpinfo->current_while_timer_expiry_counter == 0) {
            /*********************************************************************
             *  Generate current while timer expired event (E2).
             *********************************************************************/
            if (plpinfo->debug_level & DBG_RX_FSM) {
                RDBG("%s : Generate E2 (lport 0x%llx)\n", __FUNCTION__, plpinfo->lport_handle);
            }

            LACP_receive_fsm(E2,
                             plpinfo->recv_fsm_state,
                             NULL,
                             plpinfo);
        }
    }

    REXIT();

} /* current_while_timer_expiry */

/********************************************************************
 * Function which is called when a LACPDU is received
 ********************************************************************/
void
LACP_process_input_pkt(port_handle_t lport_handle, unsigned char *data, int len)
{
    lacpdu_payload_t *lacpdu_payload;
    lacp_per_port_variables_t *plpinfo;

    RENTRY();

    plpinfo = LACP_AVL_FIND(lacp_per_port_vars_tree, &lport_handle);
    if (plpinfo == NULL || plpinfo->lacp_up == FALSE) {
        VLOG_WARN("Got LACPDU, but LACP not enabled (port 0x%llx)",
                  lport_handle);
        return;
    }

    if (plpinfo->debug_level & DBG_LACPDU) {
        int ii;
        /* Reserving necessary space to write the data, + \n every 16 bytes +
         * null terminator + final \n. */
        char buf[len + len/16 + 2];

        buf[0] = '\n';
        buf[1] = '\0';
        RDBG("%s : Received %d bytes on lport 0x%llx\n",
             __FUNCTION__, len, plpinfo->lport_handle);
        RDBG("\n######################################\n");
        for (ii = 0; ii < len; ii++) {
            snprintf(&buf[strlen(buf)], sizeof(char), "%02x ", data[ii]);
            if ( ((ii + 1) % 16) == 0) {
                sprintf(&buf[strlen(buf)], "\n");
            }
        }
        RDBG("%s", buf);
        RDBG("\n######################################\n");
    }

    /*********************************************************************
     * If LACP is initialized and up, and if the packet is a Marker PDU,
     * then send a Marker response.
     *
     * The function will return TRUE, if the frame was indeed a Marker
     * PDU, else it will return FALSE.
     *********************************************************************/
    if (LACP_marker_responder(plpinfo, data) == TRUE) {
        if (plpinfo->debug_level & DBG_LACPDU) {
            RDBG("%s : marker_responder action done (lport 0x%llx)\n",
                 __FUNCTION__, lport_handle);
        }
        return;
    }

    /********************************************************************
     * Check if the PDU type is LACP.
     ********************************************************************/
    lacpdu_payload = (lacpdu_payload_t *)data;
    if (lacpdu_payload->subtype != LACP_SUBTYPE) {
        return;
    }

    /*
     * Discard if a loop back packet.
     */
    if (is_pkt_from_same_system(plpinfo, lacpdu_payload)) {
        if (plpinfo->rx_lacpdu_display == TRUE) {
            RDEBUG(DL_LACPDU, "Rx LACPDU on port 0x%llx discarded - "
                   "ls it's in loop back.\n", lport_handle);
        }
        return;
    }

    /*
     * OpenSwitch: discard LACPDU if it contains following invalid data:
     *    actor port = 0
     *    actor key  = 0
     * (ANVL LACP Conformance Test numbers 4.5 and 4.11)
     */
    /*
     * OpenSwitch NOTE: allowing actor key of 0 again.  This is to allow
     *    our box to work with Procurve 3400 box.
     */
    if (lacpdu_payload->actor_port == 0) {
        RDEBUG(DL_LACPDU, "Rx LACPDU on port 0x%llx discarded - "
               "port (%d) is 0.\n",
               lport_handle, lacpdu_payload->actor_port);
        return;
    }

    /*********************************************************************
     * If the Rx LACPDU display is on, then display the received packet.
     *********************************************************************/
    if (plpinfo->rx_lacpdu_display == TRUE) {

        printf("Rx LACPDU on port %llx:\n", lport_handle);
        printf("=====================\n\n");
        display_lacpdu(lacpdu_payload, (char *)&data[MAC_ADDR_LENGTH],
                       (char *)&data[0], LACP_ETYPE);
        printf("\n\n");
    }

    /*********************************************************************
     * Process the LACPDU.
     *********************************************************************/
    LACP_process_lacpdu(plpinfo, data);

    REXIT();

} /* LACP_process_input_pkt */

/*----------------------------------------------------------------------
 * Function: LACP_marker_responder(int port_number, void *data)
 * Synopsis: Checks if the recvd PDU is a marker PDU, if so
 *           transmits a marker response PDU.
 *
 * Input  :  int - port number, void * - pointer to recvd PDU Data.
 * Returns:  TRUE if the recvd PDU was indeed a marker PDU.
 *           FALSE if the recvd PDU was not a marker PDU.
 *----------------------------------------------------------------------*/
static int
LACP_marker_responder(lacp_per_port_variables_t *plpinfo, void *data)
{
    int status = FALSE;
    marker_pdu_payload_t *marker_payload;
    marker_pdu_payload_t *marker_response_payload;

    RENTRY();

    if (plpinfo->debug_level & DBG_LACPDU) {
        RDBG("%s : lport 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
    }

    marker_payload = (marker_pdu_payload_t *)data;

    if (marker_payload->subtype != MARKER_SUBTYPE) {
        goto exit;
    }

    plpinfo->marker_pdus_received++;
    status = TRUE;

    if (!(marker_response_payload =
          LACP_build_marker_response_payload(plpinfo->lport_handle, data))) {
        /* Report error and exit with status as TRUE */
        VLOG_ERR("CANT TX MARKER RESPONSE");
        goto exit;
    }

    LACP_transmit_marker_response(plpinfo->lport_handle,
                                  (void *)marker_response_payload);
    free((void *)marker_response_payload);

exit:
    REXIT();

    return (status);

} /* LACP_marker_responder */

/*----------------------------------------------------------------------
 * Function: LACP_build_lacpdu(int port_number)
 * Synopsis: Function to construct the lacpdu.
 * Input  :
 *           port_number = port number on which to act upon.
 * Returns:  pointer to the constructed lacpdu payload or NULL in case
 *           of error.
 *----------------------------------------------------------------------*/
static marker_pdu_payload_t *
LACP_build_marker_response_payload(port_handle_t lport_handle,
                                   marker_pdu_payload_t *marker_pdu)
{
    marker_pdu_payload_t *marker_response_payload;

    RENTRY();

    // DL4 (not per-port debug) as this is not common.
    RDEBUG(DL_LACPDU, "%s: lport 0x%llx\n", __FUNCTION__, lport_handle);

    /***************************************************************************
     * Allocate memory for the lacpdu.
     ***************************************************************************/
    marker_response_payload = (marker_pdu_payload_t *)malloc(sizeof(marker_pdu_payload_t));
    if (marker_response_payload == NULL) {
        /* Report error */
        VLOG_ERR("%s : LACP_E_NOMEMMARKERRESPONSE", __FUNCTION__);
        goto exit;
    }

    /***************************************************************************
     * Zero out the memory.
     ***************************************************************************/
    memset(marker_response_payload, 0, sizeof(marker_pdu_payload_t));

    /***************************************************************************
     * Fill in the general parameters in the marker_response_payload.
     ***************************************************************************/
    marker_response_payload->subtype = MARKER_SUBTYPE;
    marker_response_payload->version_number = MARKER_VERSION;

    /***************************************************************************
     * Fill in the other parameters in the marker_response_payload.
     * No ntoh changes here, as we're using the incoming data itself to
     * form this PDU and turn it around.
     ***************************************************************************/
    marker_response_payload->tlv_type_marker = MARKER_TLV_TYPE;
    marker_response_payload->marker_info_length = MARKER_TLV_INFO_LENGTH;
    marker_response_payload->requester_port = marker_pdu->requester_port;
    memcpy((char *)marker_response_payload->requester_system,
           (char *)marker_pdu->requester_system, MAC_ADDR_LENGTH);
    marker_response_payload->requester_transaction_id =
                                    marker_pdu->requester_transaction_id;
    marker_response_payload->tlv_type_terminator = TERMINATOR_TLV_TYPE;
    marker_response_payload->terminator_length =   TERMINATOR_LENGTH;

exit:
    REXIT();

    return (marker_response_payload);

} /* LACP_build_marker_response_payload */

/*----------------------------------------------------------------------
 * Function: LACP_transmit_marker_response(int pnum, void *data)
 * Synopsis: Function to transmit a  marker pdu
 * Input  :
 *           port_number = port number on which to act upon.
 * Returns:  void
 *----------------------------------------------------------------------*/
static void
LACP_transmit_marker_response(port_handle_t lport_handle, void *data)
{
    unsigned int ii;

    RENTRY();

    // DL4 (not per-port debug) as this is not common.
    RDEBUG(DL_LACPDU, "%s: lport 0x%llx\n", __FUNCTION__, lport_handle);

    //********************************************************************
    // No ntoh conversions here, as we turn around the incoming packet itself.
    //********************************************************************/
    if (VLOG_IS_DBG_ENABLED()) {
        for (ii = 0; ii < sizeof(marker_pdu_payload_t); ii++) {
            RDBG("%2x ", ((unsigned char *) data)[ii]);
            if ( ((ii + 1) % 16) == 0) RDBG("\n");
        }
        RDBG("\n");
    }

    // OpenSwitch
    mlacp_tx_pdu((unsigned char *)data,
                 sizeof(marker_pdu_payload_t),
                 lport_handle);

} /* LACP_transmit_marker_response */

/*----------------------------------------------------------------------
 * Function: is_same_system(int port_number, lacpdu_payload_t *recvd_lacpdu)
 *
 * Synopsis: Checks if the recvd pkt was sent from the same system(loop back)
 * Input  :
 *           port_number - port number
 *           recvd_lacpdu - recieved pdu
 *
 * Returns:  TRUE - if a loop back is detected
 *           FALSE - if there is no loop back detected.
 *----------------------------------------------------------------------*/
static int
is_pkt_from_same_system(lacp_per_port_variables_t *plpinfo,
                        lacpdu_payload_t *recvd_lacpdu)
{
    int status = FALSE;

    /***************************************************************************
     * If the local system identifier is the same as the remote system identifier
     * then we have a loop back link. If loop back, store the remote port number
     * in the cgPort_t structure.
     ***************************************************************************/
    if (!memcmp(plpinfo->actor_oper_system_variables.system_mac_addr,
                recvd_lacpdu->actor_system,
                MAC_ADDR_LENGTH)) {
        /* Signal detected loop back */
        status = TRUE;

        if (plpinfo->debug_level & DBG_RX_FSM) {
            RDBG("%s : is_pkt_from_same_system TRUE (lport 0x%llx)\n",
                 __FUNCTION__, plpinfo->lport_handle);
        }
    }
    return status;

} /* is_pkt_from_same_system */

int
lacp_lag_port_match(void *v1, void *v2)
{
    lacp_lag_ppstruct_t *ppstruct = v1;
    port_handle_t lport_handle = *(port_handle_t *)v2;

    if (ppstruct->lport_handle == lport_handle) {
        return 1;
    } else {
        return 0;
    }

} /* lacp_lag_port_match */
