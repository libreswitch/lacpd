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

#include <stdio.h>
#include <signal.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include <nemo/mqueue.h>
#include <nemo/protocol/drivers/mlacp.h>
#include <nemo/pm/pm_cmn.h>
#include <nemo/lacp/lacp_cmn.h>
#include <nemo/lacp/mlacp_debug.h>
#include "lacp.h"
#include "mlacp_fproto.h"
#include "lacp_support.h"

#include <slog.h>
#include <hc-utils.h>

#include "lacp_halon_if.h"

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

// Forward declarations
extern int mvlan_sport_init(u_long  first_time);

//////////////////// Internal global defines ////////////////////

#define LACPD_ID             "lacpd"
#define LACPD_PID_FILE       "/var/run/lacpd.pid"
#define LACPD_DBG_FILE       "lacpd_dbg.log"

//***********************************************************************
// Event Receiver Functions
//***********************************************************************
int
hc_enet_init_event_rcvr(void)
{
    int rc;

    rc = mqueue_init(&lacpd_main_rcvq);
    if (rc) {
        RDEBUG(DL_ERROR, "Failed lacp main receive queue init: %s\n",
               strerror(rc));
    }

    return rc;
} // hc_enet_init_event_rcvr

int
ml_send_event(ML_event* event)
{
    int rc;

    rc = mqueue_send(&lacpd_main_rcvq, event);
    if (rc) {
        RDEBUG(DL_ERROR,
               "Failed to send to lacp main receive queue: %s\n",
               strerror(rc));
    }

    return rc;
} // ml_send_event

ML_event *
ml_wait_for_next_event(void)
{
    int rc;
    ML_event *event = NULL;

    rc = mqueue_wait(&lacpd_main_rcvq, (void**)(void *)&event);
    if (!rc) {
        /* Set up event->msg pointer to just after the event
         * structure itself. This must be done here since the
         * sender's event->msg pointer points sender's memory
         * space, and will result in fatal errors if we try to
         * access it in LACP process space.
         */
        event->msg = (void*)(event+1);
    } else {
        RDEBUG(DL_ERROR, "LACP main receive queue wait error, rc=%d\n",
               rc);
    }

    return event;
} // ml_wait_for_next_event

void
ml_event_free(ML_event* event)
{
    if (event != NULL) {
        free(event);
    }
} // ml_event_free

#if 0 // HALON_TODO
//***********************************************************************
//  LACPDU Send and Receive Functions
//***********************************************************************
void
register_mcast_addr(port_handle_t lport_handle)
{
    int port = PM_HANDLE2PORT(lport_handle);
    cycl_port_type[port] = PM_HANDLE2LTYPE(lport_handle);
    mcast_registered_pbm |= PORTNUM_TO_PBM(port);
} // register_mcast_addr

void
deregister_mcast_addr(port_handle_t lport_handle)
{
    int port = PM_HANDLE2PORT(lport_handle);
    mcast_registered_pbm &= ~PORTNUM_TO_PBM(port);
    cycl_port_type[port] = PM_LPORT_INVALID;
} // deregister_mcast_addr

int
mlacp_send(unsigned char* data, int length, port_handle_t portHandle)
{
    proto_packet_t pkt;
    int totalLength;

    // Set up PDU header.  This is from mlacp_send() in mlacp_klib.c.
    data[0] = 0x01;
    data[1] = 0x80;
    data[2] = 0xC2;
    data[3] = 0x00;
    data[4] = 0x00;
    data[5] = 0x02;

    data[6] = my_mac_addr[0];
    data[7] = my_mac_addr[1];
    data[8] = my_mac_addr[2];
    data[9] = my_mac_addr[3];
    data[10] = my_mac_addr[4];
    data[11] = my_mac_addr[5];

    data[12] = 0x88;
    data[13] = 0x09;

    pkt.header.port = PM_HANDLE2PORT(portHandle);

    memcpy(pkt.data, data, length);

    totalLength = length+sizeof(proto_pkt_header_t);

    // --- HALON - send via DAL interface ---
    halon_send_lacpdu((unsigned char *)&pkt, totalLength, pkt.header.port);

    return 0;
} // mlacp_send

void
halon_lacpdu_rx(proto_packet_t *shalPkt, int count)
{
    if (count < 0) {
        RDEBUG(DL_ERROR, "Invalid LACPDU length len=%d", count);
        return;
    }

    if (!shal_port_is_valid(shalPkt->header.port)) {
        // Bad port number.  Ignore this packet.
        RDEBUG(DL_ERROR,
               "LACP rx processing, Invalid port number=%d",
               shalPkt->header.port);
        return;
    }

    RDEBUG(DL_LACPDU,
           " --- LACP Proto Rx Thread: rx pkt from port=%d  length=%d\n",
           shalPkt->header.port, count);

    // Ignore any LACPDU from ports that aren't ready to handle LACPDUs.
    if (!(mcast_registered_pbm & PORTNUM_TO_PBM(shalPkt->header.port))) {
        return;
    }

    //*********************************************
    //*           LACPDU Packet                   *
    //*********************************************
    if (shalPkt->header.msgType == PROTO_DATA_PKT) {
        count -= sizeof(proto_pkt_header_t);

        RDEBUG(DL_LACPDU, " --- LACPDU Received, PDU length=%d\n", count);

        // Allocate a new PDU event message and send it to LACP main.
        struct MLt_drivers_mlacp__rxPdu *pkt_event;
        ML_event *event;
        int total_msg_size;

        if (count <= MAX_LACPDU_PKT_SIZE) {
            // Cyclone LACPDU size hard-coded to 124 max.
            // See MLt_drivers_mlacp__rxPdu in include/nemo/protocol/drivers/mlacp.h

            total_msg_size = sizeof(ML_event) + sizeof(struct MLt_drivers_mlacp__rxPdu);
            event = (ML_event*)malloc(total_msg_size);

            if (event != NULL) {
                event->sender.peer = ml_bolton_index;
                // pkt_event = (struct MLt_drivers_mlacp__rxPdu *) &(event->msgBody[0]);
                pkt_event = (struct MLt_drivers_mlacp__rxPdu *) (event+1);

                // NOT NEEDED.
                //event->msg = pkt_event;

                pkt_event->lport_handle = PM_SMPT2HANDLE(0, 0, shalPkt->header.port,
                                     cycl_port_type[shalPkt->header.port]);
                pkt_event->pktLen = count;
                memcpy(pkt_event->data, shalPkt->data, count);

                ml_send_event(event);
            } else {
                RDEBUG(DL_ERROR, "No memory to send PDU!|n");
            }
        } else {
            RDEBUG(DL_ERROR, "LACPDU packet size greater than %d!\n",
                   MAX_LACPDU_PKT_SIZE);
        }
    } else {
        RDEBUG(DL_ERROR, "Unknown protocol msgType=%d\n", shalPkt->header.msgType);
    }

    return;
} // halon_lacpdu_rx
#else
void
register_mcast_addr(port_handle_t lport_handle __attribute__ ((unused)))
{
} // register_mcast_addr

void
deregister_mcast_addr(port_handle_t lport_handle __attribute__ ((unused)))
{
} // deregister_mcast_addr

int
mlacp_send(unsigned char* data __attribute__ ((unused)),
       int length __attribute__ ((unused)),
       int portHandle __attribute__ ((unused)) )
{
    return 0;
}
void
halon_lacpdu_rx(void *shalPkt __attribute__((unused)),
        int count __attribute__((unused)))
{
}
#endif // 0 HALON_TODO

//***********************************************************************
// LACP Protocol Thread
//***********************************************************************
void *
lacpd_protocol_thread(void *arg __attribute__ ((unused)))
{
    ML_event *pevent;

    // Detach thread to avoid memory leak upon exit.
    pthread_detach(pthread_self());

    RDEBUG(DL_INFO, "%s : waiting for events in the main loop\n", __FUNCTION__);

    //*******************************************************************
    // The main receive loop starts
    //*******************************************************************
    while (1) {

        pevent = ml_wait_for_next_event();

        if (lacpd_shutdown) {
            break;
        }

        if (!pevent) {
            RDEBUG(DL_ERROR, "Received NULL event!\n");
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
            // XXX Use this later for various LACP timers
            //***********************************************************
            RDEBUG(DL_TIMERS, "%s : got timer event\n", __FUNCTION__);

            mlacp_process_timer(tevent);

        } else if (pevent->sender.peer == ml_bolton_index) {
            //***********************************************************
            // Packet has arrived thru socket
            //***********************************************************
            RDEBUG(DL_LACPDU, "%s : Packet (%d) arrived from bolton\n",
                   __FUNCTION__, pevent->msgnum);

            // Halon: this is really coming from DAL interface.
            mlacpBoltonRxPdu(pevent);

        } else {
            //***********************************************
            // unknown/unregistered sender
            //***********************************************
            RDEBUG(DL_ERROR, "%s : message %d from unknown sender %d\n",
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

    RENTRY();

    if (first_time != TRUE) {
        RDEBUG(DL_FATAL, "Cannot handle revival from dead");
        status = -1;
        goto end;
    }

    if (lacp_init_done == TRUE) {
        RDEBUG(DL_WARNING, "Already initialized");
        status = -1;
        goto end;
    }

    // Halon: Initialize super ports
    mvlan_sport_init(first_time);

    // Initialize LACP data structures
    NEMO_AVL_INIT_TREE(lacp_per_port_vars_tree, nemo_compare_port_handle);

    lacp_init_done  = TRUE;

end:
    REXIT();

    return(status);
} // mlacp_init

// HALON_TODO Cleanup the unused code.
#ifndef HALON
static int
hc_enet_init_lacp(void)
{
    int rc;
    pthread_t lacpd_thread;
    sigset_t sigset;

    // Initialize LACP main task event receiver sockets.
    hc_enet_init_event_rcvr();

    /**************** Thread Related ***************/

    // Block all signals so the spawned threads don't receive any.
    sigemptyset(&sigset);
    sigfillset(&sigset);
    pthread_sigmask(SIG_BLOCK, &sigset, NULL);

    // Spawn off the main Cyclone LACP protocol thread.
    rc = pthread_create(&lacpd_thread,
                        (pthread_attr_t *)NULL,
                        lacpd_protocol_thread,
                        NULL);
    if (rc) {
        SLOG_EXIT(2, "pthread_create for LACPD main protocol thread failed!\n");
    }

    // Initialize HALON interface.
    rc = lacp_halon_init();
    if (rc) {
        SLOG(SLOG_ERR, "LACP Halon init failed, rc=%d", rc);
    }

    return rc;
} // hc_enet_init_lacp

static void
usage(int argc __attribute__ ((unused)), char *argv[])
{
    //Original Cyclone option
    //printf("USAGE : %s [-l <log_level>]\n", argv[0]);
    printf("USAGE : %s [-d] [-l]\n", argv[0]);
    printf("  -d : Debug.  Does not daemonize LACP process.\n");
    printf("  -l : Logging. Provides a system logging enable mask.\n");
    printf(SLOG_USAGE);
} // usage

int
main(int argc, char *argv[])
{
    int debug  = 0;
    int option;
    sigset_t sigset;
    int signum;
    struct itimerval timerVal;

    RENTRY();
    // slog_level = (DBG_FATAL|DBG_ERROR|DBG_WARNING);

    SLOG_INIT(LACPD_ID);

    //Halon: Removed all Cyclone specific initialization code,
    //         e.g. msgLib, logging facilities, heartbeat, etc.

    while ((option = getopt(argc, argv, "dl:")) != EOF) {
        switch (option) {
        case 'd':
            debug = 1;
            break;
        case 'l':
            slog_level=strtol(optarg, NULL, 0);
            SLOG(SLOG_NOTICE,
                 "slog_level changed from 0x%x, now 0x%x",
                 DBG_BASIC, slog_level);
            break;
        default:
            usage(argc, argv);
            exit(EXIT_FAILURE);
        }
    }

    // If there's any extra arguments, fail it.
    if (optind < argc) {
        usage(argc, argv);
        exit(EXIT_FAILURE);
    }

    // Daemonize process.
    if (!debug) {
        hc_daemonize(HC_DAEMON_LACPD);
    }

    // Record our PID so we can signal the daemon.
    hc_record_pid(LACPD_PID_FILE);

    // Initialize Cyclone LACP data structures.
    (void)mlacp_init(TRUE);

    // Initialize various protocol and event sockets.
    hc_enet_init_lacp();

    // Set up timer to fire off every second.
    timerVal.it_interval.tv_sec  = 1;
    timerVal.it_interval.tv_usec = 0;
    timerVal.it_value.tv_sec  = 1;
    timerVal.it_value.tv_usec = 0;

    if ((mlacp_tindex = setitimer(ITIMER_REAL, &timerVal, NULL)) != 0) {
        RDEBUG(DL_ERROR, "mlacp_init: Timer start failed!\n");
    }

    // Wait for all signals in an infinite loop.
    sigfillset(&sigset);
    while (!lacpd_shutdown) {

        sigwait(&sigset, &signum);
        switch (signum) {

        case SIGALRM:
            timerHandler();
            break;

        case SIGTERM:
        case SIGINT:
            RDEBUG(DL_WARNING, "%s, sig %d caught", __FUNCTION__, signum);
            lacpd_shutdown = 1;
            break;

        case SIGUSR1:
            dump_lacp_halon_info();
            break;

        default:
            RDEBUG(DL_INFO, "Ignoring signal %d.\n", signum);
            break;
        }
    }

    // ---HALON---
    lacp_halon_cleanup();

    exit(EXIT_SUCCESS);

} // main
#endif /* HALON */
