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
//    File               : mlacp_main.c
//    Description        : Master (mcpu) LACP Manager's main entry point
//**************************************************************************

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/filter.h>

#include <util.h>
#include <openvswitch/vlog.h>

#include <nemo/mqueue.h>
#include <nemo/protocol/drivers/mlacp.h>
#include <nemo/pm/pm_cmn.h>
#include <nemo/lacp/lacp_cmn.h>
#include <nemo/lacp/mlacp_debug.h>
#include <vpm/mvlan_sport.h>
#include "lacp.h"
#include "mlacp_fproto.h"
#include "lacp_support.h"
#include "lacp_halon_if.h"

VLOG_DEFINE_THIS_MODULE(mlacp_main);

//***********************************************************************
// Global & extern Variables
//***********************************************************************

// Halon Change: Each of the indices below that's needed is moved into
//               include/lacp_halon.h.
int mlacp_helper_index = MSGLIB_INVALID_INDEX;
int diagMgr_index = MSGLIB_INVALID_INDEX;
int l2Mgr_index   = MSGLIB_INVALID_INDEX;
int mlacp_tindex;

static int lacp_init_done = FALSE;
int lacpd_shutdown = 0;

// System MAC address, lives in lacp_support.c
extern unsigned char my_mac_addr[];
extern nemo_avl_tree_t lacp_per_port_vars_tree;

#if 0 // HALON_TODO
// For LACP mcast address registration.
PBM_t mcast_registered_pbm = (PBM_t)0;
enum PM_lport_type cycl_port_type[CHAR_BIT * sizeof(PBM_t)];
#endif // 0

// Message Queue for LACPD main protocol thread
mqueue_t lacpd_main_rcvq;

//////////////////// Internal global defines ////////////////////

/* LACP filter
 *
 * BPF filter to receive LACPDU from interfaces.
 *
 * LACP: "ether proto 0x8809 and ether dst 01:80:c2:00:00:02"
 *       "ether subtype = 0x1" - LACP
 *       "ether subtype = 0x2" - Marker protocol
 *
 * Since LACP protocol 0x8809 is already specified in the socket bind,
 * we just need to filter on the destination MAC address.  For now
 * we're not filtering on the subtype.  Capture all slow-protocol frames.
 *
 *    tcpdump -dd "(ether dst 01:80:c2:00:00:02)"
 */
#define LACPD_FILTER_F \
    { 0x20, 0, 0, 0x00000002 }, \
    { 0x15, 0, 3, 0xc2000002 }, \
    { 0x28, 0, 0, 0x00000000 }, \
    { 0x15, 0, 1, 0x00000180 }, \
    { 0x6, 0, 0, 0x0000ffff }, \
    { 0x6, 0, 0, 0x00000000 }

static struct sock_filter lacpd_filter_f[] = { LACPD_FILTER_F };
static struct sock_fprog lacpd_fprog = {
    .filter = lacpd_filter_f,
    .len = sizeof(lacpd_filter_f) / sizeof(struct sock_filter)
};

//***********************************************************************
// Event Receiver Functions
//***********************************************************************
int
ml_init_event_rcvr(void)
{
    int rc;

    rc = mqueue_init(&lacpd_main_rcvq);
    if (rc) {
        VLOG_ERR("Failed LACP main receive queue init: %s",
                 strerror(rc));
    }

    return rc;
} /* ml_init_event_rcvr */

int
ml_send_event(ML_event *event)
{
    int rc;

    rc = mqueue_send(&lacpd_main_rcvq, event);
    if (rc) {
        VLOG_ERR("Failed to send to LACP main receive queue: %s",
                 strerror(rc));
    }

    return rc;
} /* ml_send_event */

ML_event *
ml_wait_for_next_event(void)
{
    int rc;
    ML_event *event = NULL;

    rc = mqueue_wait(&lacpd_main_rcvq, (void **)(void *)&event);
    if (!rc) {
        /* Set up event->msg pointer to just after the event
         * structure itself. This must be done here since the
         * sender's event->msg pointer points sender's memory
         * space, and will result in fatal errors if we try to
         * access it in LACP process space.
         */
        event->msg = (void *)(event+1);
    } else {
        VLOG_ERR("LACP main receive queue wait error, rc=%s",
                 strerror(rc));
    }

    return event;
} /* ml_wait_for_next_event */

void
ml_event_free(ML_event *event)
{
    if (event != NULL) {
        free(event);
    }
} /* ml_event_free */

//***********************************************************************
// LACPDU Send and Receive Functions
//***********************************************************************

void *
mlacp_rx_pdu_thread(void *data)
{
    int rc;
    int port;
    int sockfd;
    int if_idx = 0;
    port_handle_t lport_handle = (port_handle_t)data;
    const char *if_name;
    struct ifreq ifr = { .ifr_name = {} };
    struct sockaddr_ll addr;

    /* Detach thread to avoid memory leak upon exit. */
    pthread_detach(pthread_self());

    /* Find the interface data first. */
    port = PM_HANDLE2PORT(lport_handle);
    if_name = find_ifname_by_index(port);

    if_idx = if_nametoindex(if_name);
    if (0 == if_idx) {
        VLOG_ERR("Error getting ifindex for port %d (if_name=%s)!",
                 port, if_name);
        return NULL;
    }

    VLOG_DBG("Eth %s, ifindex=%d\n", if_name, if_idx);

    snprintf(ifr.ifr_name, IFNAMSIZ, if_name);
    if ((sockfd = socket(PF_PACKET, SOCK_RAW, 0)) < 0) {
        rc = errno;
        VLOG_ERR("Failed to open datagram socket for %s, rc=%s",
                 if_name, strerror(rc));
        return NULL;
    }

    rc = setsockopt(sockfd, SOL_SOCKET, SO_ATTACH_FILTER,
                    &lacpd_fprog, sizeof(lacpd_fprog));
    if (rc < 0) {
        VLOG_ERR("Failed to attach socket filter for %s, rc=%s",
                 if_name, strerror(rc));
        close(sockfd);
        return NULL;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_ifindex = if_idx;
    addr.sll_protocol = htons(ETH_P_SLOW); /* IEEE802.3 slow protocol (LACP) */

    rc = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0) {
        VLOG_ERR("Failed to bind socket to addr for %s, rc=%s",
                 if_name, strerror(rc));
        close(sockfd);
        return NULL;
    }

    while (1) {
        int count;
        int clientlen;
        struct sockaddr_ll clientaddr;
        ML_event *event;
        int total_msg_size;
        struct MLt_drivers_mlacp__rxPdu *pkt_event;

        // Cyclone LACPDU size hard-coded to 124 max.
        // See MLt_drivers_mlacp__rxPdu in include/nemo/protocol/drivers/mlacp.h
        total_msg_size = sizeof(ML_event) + sizeof(struct MLt_drivers_mlacp__rxPdu);

        event = xzalloc(total_msg_size);
        event->sender.peer = ml_bolton_index;
        pkt_event = (struct MLt_drivers_mlacp__rxPdu *)(event+1);

        clientlen = sizeof(clientaddr);
        count = recvfrom(sockfd, (void *)pkt_event->data,
                         MAX_LACPDU_PKT_SIZE, 0,
                         (struct sockaddr *)&clientaddr,
                         (unsigned int *)&clientlen);
        if (count < 0) {
            /* General socket error... */
            VLOG_ERR("Read failed for %s, fd=%d: errno=%d",
                     if_name, sockfd, errno);
            free(event);
            continue;

        } else if (!count) {
            /* Socket is closed.  Get out. */
            VLOG_ERR("%s, socket=%d closed", if_name, sockfd);
            free(event);
            break;

        } else if (count <= MAX_LACPDU_PKT_SIZE) {
            pkt_event->lport_handle = lport_handle;
            pkt_event->pktLen = count;

            ml_send_event(event);
        }
    } /* while (1) */

    return NULL;

} /* mlacp_rx_pdu_thread */

void
register_mcast_addr(port_handle_t lport_handle)
{
    int rc;
    pthread_t rx_thread;

    VLOG_DBG("%s: lport 0x%llx, port=%d",
             __FUNCTION__, lport_handle, PM_HANDLE2PORT(lport_handle));

    /* Spawn off thread to listen for LACPDU on the interface. */
    rc = pthread_create(&rx_thread,
                        (pthread_attr_t *)NULL,
                        mlacp_rx_pdu_thread,
                        (void *)lport_handle);
    if (rc) {
        VLOG_ERR("pthread_create for LACPDU Rx thread failed!");
    }

    /* HALON_TODO: save thread info to kill it when not needed. */

} /* register_mcast_addr */

void
deregister_mcast_addr(port_handle_t lport_handle)
{
    VLOG_DBG("%s: HALON_TODO: lport 0x%llx, port=%d",
             __FUNCTION__, lport_handle, PM_HANDLE2PORT(lport_handle));
} /* deregister_mcast_addr */

int
mlacp_tx_pdu(unsigned char* data, int length, port_handle_t lport_handle)
{
    VLOG_DBG("%s: lport 0x%llx, port=%d, data=%p, len=%d",
             __FUNCTION__, lport_handle, PM_HANDLE2PORT(lport_handle),
             data, length);

    /* Set up LACPDU header dest/src MAC addresses. */
    memcpy(data, lacp_mcast_addr, MAC_ADDR_LENGTH);
    memcpy(&data[6], my_mac_addr, MAC_ADDR_LENGTH);

    /* IEEE802.3 slow protocol (LACP) */
    data[12] = 0x88;
    data[13] = 0x09;

    /* HALON_TODO: temporary, just create & close socket for each transmit. */
    {
        int rc;
        int port;
        const char *if_name;
        int send_sockfd;
        int send_ifidx = 0;
        struct ifreq send_ifr = { .ifr_name = {} };
        struct sockaddr_ll send_addr;

        port = PM_HANDLE2PORT(lport_handle);
        if_name = find_ifname_by_index(port);
        send_ifidx = if_nametoindex(if_name);
        if (0 == send_ifidx) {
            VLOG_ERR("Error getting ifindex for port %d (if_name=%s)!",
                     port, if_name);
            return 1;
        }

        VLOG_DBG("%s: Eth %s, ifindex=%d\n", __FUNCTION__, if_name, send_ifidx);

        snprintf(send_ifr.ifr_name, IFNAMSIZ, if_name);
        if ((send_sockfd = socket(PF_PACKET, SOCK_RAW, 0)) < 0) {
            rc = errno;
            VLOG_ERR("Failed to open datagram socket for send, %s", strerror(rc));
            return 1;
        }

        memset(&send_addr, 0, sizeof(send_addr));
        send_addr.sll_family = AF_PACKET;
        send_addr.sll_ifindex = send_ifidx;

        rc = bind(send_sockfd, (struct sockaddr *)&send_addr, sizeof(send_addr));
        if (rc < 0) {
            VLOG_ERR("Failed to bind socket to send_addr, %s", strerror(rc));
            close(send_sockfd);
            return 1;
        }

        rc = sendto(send_sockfd, data, length, 0, NULL, 0);
        if (rc == -1) {
            VLOG_ERR("Failed to send LACPDU, rc=%d", errno);
            return 1;
        }

        close(send_sockfd);
    }

    return 0;
} /* mlacp_tx_pdu */

//***********************************************************************
// LACP Protocol Thread
//***********************************************************************
void *
lacpd_protocol_thread(void *arg)
{
    ML_event *pevent;

    // Detach thread to avoid memory leak upon exit.
    pthread_detach(pthread_self());

    VLOG_DBG("%s : waiting for events in the main loop", __FUNCTION__);

    //*******************************************************************
    // The main receive loop starts
    //*******************************************************************
    while (1) {

        pevent = ml_wait_for_next_event();

        if (lacpd_shutdown) {
            break;
        }

        if (!pevent) {
            VLOG_ERR("LACPD protocol: Received NULL event!");
            continue;
        }

        if (pevent->sender.peer == ml_vlan_index) {
            //***********************************************************
            // msg from VLAN task on the Management-Module
            //***********************************************************
            mlacp_process_vlan_msg(pevent);

        } else if (pevent->sender.peer == ml_showMgr_index) {
            //***********************************************************
            // msg from Show Manager
            //***********************************************************
            //mlacp_process_showmgr_msg(pevent);
            lacp_support_diag_dump(pevent->msgnum);

        } else if (pevent->sender.peer == diagMgr_index) {
            //***********************************************************
            // msg from Diag Manager
            //***********************************************************
            mlacp_process_diagmgr_msg(pevent);

        } else if (pevent->sender.peer == ml_cfgMgr_index) {
            //***********************************************************
            // msg from Cfg Manager
            //***********************************************************
            mlacp_process_api_msg(pevent);

        } else if (pevent->sender.peer == ml_timer_index) {
            struct MLt_msglib__timer *tevent = pevent->msg;
            //***********************************************************
            // msg from LACP timers
            //***********************************************************
            mlacp_process_timer(tevent);

        } else if (pevent->sender.peer == ml_bolton_index) {
            //***********************************************************
            // Packet has arrived thru socket.
            //***********************************************************
            VLOG_DBG("%s : Packet (%d) arrived from bolton",
                   __FUNCTION__, pevent->msgnum);

            // HALON_TODO: rename function.  This is really coming
            // from Eth interface.
            mlacpBoltonRxPdu(pevent);

        } else {
            //***********************************************
            // unknown/unregistered sender
            //***********************************************
            VLOG_ERR("%s : message %d from unknown sender %d",
                     __FUNCTION__, pevent->msgnum, pevent->sender.peer);
        }

        ml_event_free(pevent);

    } // while loop

    return NULL;
} // lacpd_protocol_thread

//***********************************************************************
// Initialization & main functions
//***********************************************************************
int
mlacp_init(u_long  first_time)
{
    int status = 0;

    if (first_time != TRUE) {
        VLOG_ERR("Cannot handle revival from dead");
        status = -1;
        goto end;
    }

    if (lacp_init_done == TRUE) {
        VLOG_WARN("Already initialized");
        status = -1;
        goto end;
    }

    // Initialize super ports.
    mvlan_sport_init(first_time);

    // Initialize LACP data structures.
    NEMO_AVL_INIT_TREE(lacp_per_port_vars_tree, nemo_compare_port_handle);

    // Initialize LACP main task event receiver queue.
    if (ml_init_event_rcvr()) {
        VLOG_ERR("Failed to initialize event receiver.");
        status = -1;
        goto end;
    }

    lacp_init_done  = TRUE;

end:
    return status;
} // mlacp_init
