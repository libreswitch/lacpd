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

/***************************************************************************
 *    File               : mlacp_main.c
 *    Description        : Master (mcpu) LACP Manager's main entry point
 ***************************************************************************/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/filter.h>

#include <util.h>
#include <openvswitch/vlog.h>

#include <mqueue.h>
#include <pm_cmn.h>
#include <lacp_cmn.h>
#include <mlacp_debug.h>
#include <mvlan_sport.h>
#include "lacp.h"
#include "mlacp_recv.h"
#include "mlacp_fproto.h"
#include "lacp_support.h"
#include "lacp_ops_if.h"

VLOG_DEFINE_THIS_MODULE(mlacp_main);

/************************************************************************
 * Global Variables
 ************************************************************************/
static int lacp_init_done = FALSE;
int lacpd_shutdown = 0;

/* Message Queue for LACPD main protocol thread */
mqueue_t lacpd_main_rcvq;

/* epoll FD for LACPDU RX. */
int epfd = -1;

/* Max number of events returned by epoll_wait().
 * This number is arbitrary.  It's only used for
 * sizing the epoll events data structure. */
#define MAX_EVENTS 64

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

/************************************************************************
 * Event Receiver Functions
 ************************************************************************/
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

/************************************************************************
 * LACPDU Send and Receive Functions
 ************************************************************************/
void *
mlacp_rx_pdu_thread(void *data  __attribute__ ((unused)))
{
    /* Detach thread to avoid memory leak upon exit. */
    pthread_detach(pthread_self());

    epfd = epoll_create1(0);
    if (epfd == -1) {
        VLOG_ERR("Failed to create epoll object.  rc=%d", errno);
        return NULL;
    }

    for (;;) {
        int n;
        int nfds;
        struct epoll_event events[MAX_EVENTS];

        /* Wait infinite time (-1) for events on epfd */
        nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);

        if (nfds < 0) {
            VLOG_ERR("epoll_wait returned error %s", strerror(errno));
            break;
        } else {
            VLOG_DBG("epoll_wait returned, nfds=%d", nfds);
        }

        for (n = 0; n < nfds; n++) {
            int count;
            int clientlen;
            struct sockaddr_ll clientaddr;
            ML_event *event;
            int total_msg_size;
            struct MLt_drivers_mlacp__rxPdu *pkt_event;
            struct iface_data *idp = NULL;

            idp = (struct iface_data *)events[n].data.ptr;
            if (idp == NULL) {
                VLOG_ERR("Interface data missing for epoll event!");
                continue;
            } else {
                VLOG_DBG("epoll event #%d: events flags=0x%x, port=%d, sock=%d",
                         n, events[n].events, idp->index, idp->pdu_sockfd);
            }

            if (idp->pdu_registered == false) {
                /* Most likely just a race condition. */
                continue;
            }

            /* LACPDU size hard-coded to 124 max.
             * See MLt_drivers_mlacp__rxPdu in mlacp_recv.h
             */
            total_msg_size = sizeof(ML_event) + sizeof(struct MLt_drivers_mlacp__rxPdu);

            event = xzalloc(total_msg_size);
            event->sender.peer = ml_rx_pdu_index;

            /* Set up pkt_event pointer to just after the event
             * structure itself. This must be done here since the
             * sender's event->msg pointer points sender's memory
             * space, and will result in fatal errors if we try to
             * access it in LACP process space.
             */
            pkt_event = (struct MLt_drivers_mlacp__rxPdu *)(event+1);

            clientlen = sizeof(clientaddr);
            count = recvfrom(idp->pdu_sockfd, (void *)pkt_event->data,
                              LACP_PKT_SIZE, 0,
                             (struct sockaddr *)&clientaddr,
                             (unsigned int *)&clientlen);
            if (count < 0) {
                /* General socket error. */
                VLOG_ERR("Read failed, fd=%d: errno=%d",
                         idp->pdu_sockfd, errno);
                free(event);
                continue;

            } else if (!count) {
                /* Socket is closed.  Get out. */
                VLOG_ERR("socket=%d closed", idp->pdu_sockfd);
                free(event);
                continue;

            } else if (count <= LACP_PKT_SIZE) {
                pkt_event->lport_handle = PM_SMPT2HANDLE(0, 0, idp->index,
                                                         idp->cycl_port_type);
                pkt_event->pktLen = count;
                ml_send_event(event);
            }
        } /* for nfds */
    } /* for(;;) */

    return NULL;
} /* mlacp_rx_pdu_thread */

void
register_mcast_addr(port_handle_t lport_handle)
{
    int rc;
    int port;
    int sockfd;
    int if_idx = 0;
    struct iface_data *idp = NULL;
    struct sockaddr_ll addr;
    struct epoll_event event;

    /* Find the interface data first. */
    port = PM_HANDLE2PORT(lport_handle);
    idp = find_iface_data_by_index(port);

    if (idp == NULL) {
        VLOG_ERR("Failed to find interface data for register mcast addr! "
                 "lport=0x%llx", lport_handle);
        return;
    }

    if (idp->pdu_registered == true) {
        VLOG_ERR("Duplicated registration for mcast addr? port=%s", idp->name);
        return;
    }

    /* TODO: This is a temporal solution for a race condition between the initialization of the interfaces
     * and LACP daemon trying to get the index of the member interfaces.
     * The real solution should check if the interfaces are completely initialized before starting LACP daemon.
     * This can be done by checking some value in OVSDB which indicates when an interface is ready to be used by the different daemons. */

    /* Max number of retries to see if function if_nametoindex returns something
     * different than zero, which means the interface has been initialized. */
    const int MAX_NUMBER_RETRIES_NAMETOINDEX = 1000;

    /* Number of microseconds to wait before calling if_nametoindex again. */
    const int SLEEP_TIME_NAMETOINDEX = 10000;

    int number_retries = 0;
    do {
        if_idx = if_nametoindex(idp->name);
        if (if_idx != 0) {
            break;
        }
        usleep(SLEEP_TIME_NAMETOINDEX);
        number_retries++;
    }
    while (number_retries < MAX_NUMBER_RETRIES_NAMETOINDEX);

    if (if_idx == 0) {
        VLOG_ERR("FATAL ERROR when getting ifindex for port %d (if_name=%s)!. "
                  "This means that the interface was not initialized and LACP daemon cannot send LACPDUs through this interface",
                  port, idp->name);
        return;
    }

    VLOG_DBG("%s: port %s, ifindex=%d\n", __FUNCTION__, idp->name, if_idx);

    /* Create raw socket on interface to receive LACPDUs. */
    if ((sockfd = socket(PF_PACKET, SOCK_RAW, 0)) < 0) {
        rc = errno;
        VLOG_ERR("Failed to open datagram socket for %s, rc=%s",
                 idp->name, strerror(rc));
        return;
    }

    rc = setsockopt(sockfd, SOL_SOCKET, SO_ATTACH_FILTER,
                    &lacpd_fprog, sizeof(lacpd_fprog));
    if (rc < 0) {
        VLOG_ERR("Failed to attach socket filter for %s, rc=%s",
                 idp->name, strerror(rc));
        close(sockfd);
        return;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sll_family = AF_PACKET;
    addr.sll_ifindex = if_idx;
    addr.sll_protocol = htons(ETH_P_SLOW); /* IEEE802.3 slow protocol (LACP) */

    rc = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0) {
        VLOG_ERR("Failed to bind socket to addr for %s, rc=%s",
                 idp->name, strerror(rc));
        close(sockfd);
        return;
    }

    /* Save sockfd information in interface data. */
    idp->pdu_sockfd = sockfd;
    idp->pdu_registered = true;

    /* Add new FD to epoll.  Save interface data pointer.
     * NOTE: assumption is that interfaces are not deleted in h/w switch! */
    /* OPS_TODO: OVSDB allows interface to be deleted.  Maybe it's better
     * to save port index & find data?  Or in OVSDB interface delete code,
     * modify the epoll event data to remove the pointer. */
    event.events = EPOLLIN;
    event.data.ptr = (void *)idp;

    rc = epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &event);
    if (rc == 0) {
        VLOG_DBG("Registered sockfd %d for interface %s with epoll loop.",
                 sockfd, idp->name);
    } else {
        VLOG_ERR("Failed to register sockfd for interface %s with epoll "
                 "loop.  err=%s", idp->name, strerror(errno));
    }
} /* register_mcast_addr */

void
deregister_mcast_addr(port_handle_t lport_handle)
{
    int rc;
    int port;
    struct iface_data *idp = NULL;

    /* Find the interface data first. */
    port = PM_HANDLE2PORT(lport_handle);
    idp = find_iface_data_by_index(port);

    if (idp == NULL) {
        VLOG_ERR("Failed to find interface data for deregister mcast addr! "
                 "lport=0x%llx", lport_handle);
        return;
    }

    if (idp->pdu_registered != true) {
        VLOG_ERR("Deregistering for mcast addr when not registered? "
                 "port=%s", idp->name);
        return;
    }

    rc = epoll_ctl(epfd, EPOLL_CTL_DEL, idp->pdu_sockfd, NULL);
    if (rc == 0) {
        VLOG_DBG("Deregistered sockfd %d for interface %s with epoll loop.",
                 idp->pdu_sockfd, idp->name);
    } else {
        VLOG_ERR("Failed to deregister sockfd for interface %s with epoll "
                 "loop.  err=%s", idp->name, strerror(errno));
    }

    close(idp->pdu_sockfd);
    idp->pdu_sockfd = 0;
    idp->pdu_registered = false;

} /* deregister_mcast_addr */

int
mlacp_tx_pdu(unsigned char* data, int length, port_handle_t lport_handle)
{
    int rc;
    int port;
    struct iface_data *idp = NULL;

    /* Find the interface data first. */
    port = PM_HANDLE2PORT(lport_handle);
    idp = find_iface_data_by_index(port);

    if (idp == NULL) {
        VLOG_ERR("Failed to find interface data for LACPDU TX! "
                 "lport=0x%llx", lport_handle);
        return 1;
    }

    if (idp->pdu_registered != true) {
        VLOG_ERR("Trying to send LACPDU before registering, "
                 "port=%s", idp->name);
        return 1;
    }

    VLOG_DBG("%s: lport 0x%llx, port=%s, data=%p, len=%d",
             __FUNCTION__, lport_handle, idp->name, data, length);

    /* Set up LACPDU header dest/src MAC addresses. */
    memcpy(data, lacp_mcast_addr, MAC_ADDR_LENGTH);
    memcpy(&data[MAC_ADDR_LENGTH], my_mac_addr, MAC_ADDR_LENGTH);

    /* Add Ethernet Type to the header. According to standard
     * IEEE802.1AX Slow Protocols EtherType is 88-09 hexadecimal*/
    data[12] = SLOW_PROTOCOLS_ETHERTYPE_PART1;
    data[13] = SLOW_PROTOCOLS_ETHERTYPE_PART2;

    rc = sendto(idp->pdu_sockfd, data, length, 0, NULL, 0);
    if (rc == -1) {
        VLOG_ERR("Failed to send LACPDU for interface=%s, rc=%d",
                 idp->name, errno);
        return 1;
    }

    return 0;
} /* mlacp_tx_pdu */

/************************************************************************
 * LACP Protocol Thread
 ************************************************************************/
void *
lacpd_protocol_thread(void *arg  __attribute__ ((unused)))
{
    ML_event *pevent;

    /* Detach thread to avoid memory leak upon exit. */
    pthread_detach(pthread_self());

    VLOG_DBG("%s : waiting for events in the main loop", __FUNCTION__);

    /*******************************************************************
     * The main receive loop.
     *******************************************************************/
    while (1) {

        pevent = ml_wait_for_next_event();

        if (lacpd_shutdown) {
            break;
        }

        if (!pevent) {
            VLOG_ERR("LACPD protocol: Received NULL event!");
            continue;
        }

        if (pevent->sender.peer == ml_lport_index) {
            /***********************************************************
             * Msg from OVSDB interface for lports.
             ***********************************************************/
            mlacp_process_vlan_msg(pevent);

        } else if (pevent->sender.peer == ml_cfgMgr_index) {
            /***********************************************************
             * Msg from Cfg Manager.
             ***********************************************************/
            mlacp_process_api_msg(pevent);

        } else if (pevent->sender.peer == ml_timer_index) {
            /***********************************************************
             * Msg from LACP timers.
             ***********************************************************/
            mlacp_process_timer();

        } else if (pevent->sender.peer == ml_rx_pdu_index) {
            /***********************************************************
             * Packet has arrived through interface socket.
             ************************************************************/
            VLOG_DBG("%s : LACPDU Packet (%d) arrived from interface socket",
                   __FUNCTION__, pevent->msgnum);

            mlacp_process_rx_pdu(pevent);

        } else {
            /***********************************************************
             * Unknown/unregistered sender.
             ************************************************************/
            VLOG_ERR("%s : message %d from unknown sender %d",
                     __FUNCTION__, pevent->msgnum, pevent->sender.peer);
        }

        ml_event_free(pevent);

    } /* while loop */

    return NULL;
} /* lacpd_protocol_thread */

/************************************************************************
 * Initialization & main functions
 ************************************************************************/
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

    /* Initialize super ports. */
    mvlan_sport_init(first_time);

    /* Initialize LACP data structures. */
    LACP_AVL_INIT_TREE(lacp_per_port_vars_tree, lacp_compare_port_handle);

    /* Initialize LACP main task event receiver queue. */
    if (ml_init_event_rcvr()) {
        VLOG_ERR("Failed to initialize event receiver.");
        status = -1;
        goto end;
    }

    lacp_init_done  = TRUE;

end:
    return status;
} /* mlacp_init */
