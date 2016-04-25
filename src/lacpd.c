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

/************************************************************************//**
 * @ingroup lacpd
 *
 * @file
 * Main source file for the LACP daemon.
 *
 *    The lacpd daemon operates as an overall OpenSwitch Link Aggregation (LAG)
 *    Daemon supporting both static LAGs and LACP based dynamic LAGs.
 *
 *    Its purpose in life is:
 *
 *       1. During start up, read port and interface related
 *          configuration data and maintain local cache.
 *       2. During operations, receive administrative
 *          configuration changes and apply to the hardware.
 *       3. Manage static LAG configuration and apply to the hardware.
 *       4. Manage LACP protocol operation for LACP LAGs.
 *       5. Dynamically configure hardware based on
 *          operational state changes as needed.
 ***************************************************************************/
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#include <util.h>
#include <daemon.h>
#include <dirs.h>
#include <unixctl.h>
#include <fatal-signal.h>
#include <command-line.h>
#include <vswitch-idl.h>
#include <openvswitch/vlog.h>
#include  <diag_dump.h>

#include <pm_cmn.h>
#include <lacp_cmn.h>
#include <mlacp_debug.h>
#include <eventlog.h>

#include "lacp.h"
#include "mlacp_fproto.h"
#include "lacp_ops_if.h"

VLOG_DEFINE_THIS_MODULE(lacpd);

#define DIAGNOSTIC_BUFFER_LEN 16000

bool exiting = false;
static unixctl_cb_func lacpd_unixctl_dump;
static unixctl_cb_func lacpd_unixctl_getlacpinterfaces;
static unixctl_cb_func lacpd_unixctl_getlacpcounters;
static unixctl_cb_func lacpd_unixctl_getlacpstate;
static unixctl_cb_func ops_lacpd_exit;

extern int lacpd_shutdown;

/**
 * ovs-appctl interface callback function to dump internal debug information.
 * This top level debug dump function calls other functions to dump lacpd
 * daemon's internal data. The function arguments in argv are used to
 * control the debug output.
 *
 * @param conn connection to ovs-appctl interface.
 * @param argc number of arguments.
 * @param argv array of arguments.
 * @param OVS_UNUSED aux argument not used.
 */
static void
lacpd_unixctl_dump(struct unixctl_conn *conn, int argc,
                   const char *argv[], void *aux OVS_UNUSED)
{
    struct ds ds = DS_EMPTY_INITIALIZER;

    lacpd_debug_dump(&ds, argc, argv);

    unixctl_command_reply(conn, ds_cstr(&ds));
    ds_destroy(&ds);
} /* lacpd_unixctl_dump */


/**
 * ovs-appctl interface callback function to dump the interfaces member of LAGs.
 * This top level debug dump function calls other functions to dump lacpd
 * daemon's internal data. The function arguments in argv are used to
 * control the debug output.
 *
 * @param conn connection to ovs-appctl interface.
 * @param argc number of arguments.
 * @param argv array of arguments.
 * @param OVS_UNUSED aux argument not used.
 */
static void
lacpd_unixctl_getlacpinterfaces(struct unixctl_conn *conn, int argc,
                                const char *argv[], void *aux OVS_UNUSED)
{
    struct ds ds = DS_EMPTY_INITIALIZER;
    lacpd_lag_ports_dump(&ds, argc, argv);

    unixctl_command_reply(conn, ds_cstr(&ds));
    ds_destroy(&ds);
} /* lacpd_unixctl_getlacpinterfaces */

/**
 * ovs-appctl interface callback function to dump LACP counters of LACP_PDUs.
 * This top level debug dump function calls other functions to dump lacpd
 * daemon's internal data. The function arguments in argv are used to
 * control the debug output.
 *
 * @param conn connection to ovs-appctl interface.
 * @param argc number of arguments.
 * @param argv array of arguments.
 * @param OVS_UNUSED aux argument not used.
 */
static void
lacpd_unixctl_getlacpcounters(struct unixctl_conn *conn, int argc,
                              const char *argv[], void *aux OVS_UNUSED)
{
    struct ds ds = DS_EMPTY_INITIALIZER;

    lacpd_pdus_counters_dump(&ds, argc, argv);

    unixctl_command_reply(conn, ds_cstr(&ds));
    ds_destroy(&ds);
} /* lacpd_unixctl_getlacpcounters */

/**
 * ovs-appctl interface callback function to dump LACP state.
 * This top level debug dump function calls other functions to dump lacpd
 * daemon's internal data. The function arguments in argv are used to
 * control the debug output.
 *
 * @param conn connection to ovs-appctl interface.
 * @param argc number of arguments.
 * @param argv array of arguments.
 * @param OVS_UNUSED aux argument not used.
 */
static void
lacpd_unixctl_getlacpstate(struct unixctl_conn *conn, int argc,
                           const char *argv[], void *aux OVS_UNUSED)
{
    struct ds ds = DS_EMPTY_INITIALIZER;

    lacpd_state_dump(&ds, argc, argv);

    unixctl_command_reply(conn, ds_cstr(&ds));
    ds_destroy(&ds);
} /* lacpd_unixctl_getlacpstate */


/**
 * callback handler function for diagnostic dump basic
 * it allocates memory as per requirement and populates data.
 * INIT_DIAG_DUMP_BASIC will free allocated memory.
 *
 * @param feature name of the feature.
 * @param buf pointer to the buffer.
 */
static void lacpd_diag_dump_basic_cb(const char *feature , char **buf)
{
    struct ds ds = DS_EMPTY_INITIALIZER;

    if (!buf)
        return;
    *buf =  xcalloc(1,DIAGNOSTIC_BUFFER_LEN);
    if (*buf) {
        /* populate basic diagnostic data to buffer  */
        ds_put_format(&ds, "System Ports: \n");
        const char* argv[] = {"", "port"};
        lacpd_debug_dump(&ds, 2, argv);
        sprintf(*buf, "%s", ds_cstr(&ds));

        ds_put_format(&ds, "\nLAG interfaces: \n");
        lacpd_lag_ports_dump(&ds, 0, NULL);
        sprintf(*buf, "%s", ds_cstr(&ds));

        ds_put_format(&ds, "\nLACP PDUs counters: \n");
        lacpd_pdus_counters_dump(&ds, 0, NULL);
        sprintf(*buf, "%s", ds_cstr(&ds));

        ds_put_format(&ds, "\nLACP state: \n");

        lacpd_state_dump(&ds, 0, NULL);
        sprintf(*buf, "%s", ds_cstr(&ds));
        VLOG_INFO("basic diag-dump data populated for feature %s",feature);
    } else{
        VLOG_ERR("Memory allocation failed for feature %s , %d bytes",
                 feature , DIAGNOSTIC_BUFFER_LEN);
    }
    return ;
} /* lacpd_diag_dump_basic_cb */


/**
 * lacpd daemon's timer handler function.
 */
static void
timerHandler(void)
{
    ML_event *timerEvent;

    timerEvent = (ML_event *)malloc(sizeof(ML_event));
    if (NULL == timerEvent) {
        VLOG_ERR("Out of memory for LACP timer message.");
        return;
    }
    memset(timerEvent, 0, sizeof(ML_event));
    timerEvent->sender.peer = ml_timer_index;

    ml_send_event(timerEvent);
} /* timerHandler */

/**
 * lacpd daemon's main initialization function.  Responsible for
 * creating various protocol & OVSDB interface threads.
 *
 * @param db_path pathname for OVSDB connection.
 */
static void
lacpd_init(const char *db_path, struct unixctl_server *appctl)
{
    int rc;
    sigset_t sigset;
    pthread_t ovs_if_thread;
    pthread_t lacpd_thread;
    pthread_t lacpdu_rx_thread;

    /* Block all signals so the spawned threads don't receive any. */
    sigemptyset(&sigset);
    sigfillset(&sigset);
    pthread_sigmask(SIG_BLOCK, &sigset, NULL);

    /* Spawn off the main LACP protocol thread. */
    rc = pthread_create(&lacpd_thread,
                        (pthread_attr_t *)NULL,
                        lacpd_protocol_thread,
                        NULL);
    if (rc) {
        VLOG_ERR("pthread_create for LACPD protocol thread failed! rc=%d", rc);
        exit(-rc);
    }

    /* Initialize IDL through a new connection to the dB. */
    lacpd_ovsdb_if_init(db_path);

    /* Register diagnostic callback function */
    INIT_DIAG_DUMP_BASIC(lacpd_diag_dump_basic_cb);

    /* Register ovs-appctl commands for this daemon. */
    unixctl_command_register("lacpd/dump", "", 0, 2, lacpd_unixctl_dump, NULL);
    unixctl_command_register("lacpd/getlacpinterfaces", "", 0, 1,
                             lacpd_unixctl_getlacpinterfaces, NULL);
    unixctl_command_register("lacpd/getlacpcounters", "", 0, 1,
                             lacpd_unixctl_getlacpcounters, NULL);
    unixctl_command_register("lacpd/getlacpstate", "", 0, 1,
                             lacpd_unixctl_getlacpstate, NULL);

    /* Spawn off the OVSDB interface thread. */
    rc = pthread_create(&ovs_if_thread,
                        (pthread_attr_t *)NULL,
                        lacpd_ovs_main_thread,
                        (void *)appctl);
    if (rc) {
        VLOG_ERR("pthread_create for OVSDB i/f thread failed! rc=%d", rc);
        exit(-rc);
    }

    /* Spawn off LACPDU RX thread. */
    rc = pthread_create(&lacpdu_rx_thread,
                        (pthread_attr_t *)NULL,
                        mlacp_rx_pdu_thread,
                        NULL);
    if (rc) {
        VLOG_ERR("pthread_create for LACDU RX thread failed! rc=%d", rc);
        exit(-rc);
    }

    /* Init events for LACP. */
    if (event_log_init("LACP") < 0) {
        VLOG_ERR("Could not init event log for LACP");
    }

} /* lacpd_init */

/**
 * lacpd usage help function.
 *
 */
static void
usage(void)
{
    printf("%s: OpenSwitch LACP daemon\n"
           "usage: %s [OPTIONS] [DATABASE]\n"
           "where DATABASE is a socket on which ovsdb-server is listening\n"
           "      (default: \"unix:%s/db.sock\").\n",
           program_name, program_name, ovs_rundir());
    daemon_usage();
    vlog_usage();
    printf("\nOther options:\n"
           "  --unixctl=SOCKET        override default control socket name\n"
           "  -h, --help              display this help message\n");
    exit(EXIT_SUCCESS);
} /* usage */

static char *
parse_options(int argc, char *argv[], char **unixctl_pathp)
{
    enum {
        OPT_UNIXCTL = UCHAR_MAX + 1,
        VLOG_OPTION_ENUMS,
        DAEMON_OPTION_ENUMS,
    };
    static const struct option long_options[] = {
        {"help",        no_argument, NULL, 'h'},
        {"unixctl",     required_argument, NULL, OPT_UNIXCTL},
        DAEMON_LONG_OPTIONS,
        VLOG_LONG_OPTIONS,
        {NULL, 0, NULL, 0},
    };
    char *short_options = long_options_to_short_options(long_options);

    for (;;) {
        int c;

        c = getopt_long(argc, argv, short_options, long_options, NULL);
        if (c == -1) {
            break;
        }

        switch (c) {
        case 'h':
            usage();

        case OPT_UNIXCTL:
            *unixctl_pathp = optarg;
            break;

        VLOG_OPTION_HANDLERS
        DAEMON_OPTION_HANDLERS

        case '?':
            exit(EXIT_FAILURE);

        default:
            abort();
        }
    }
    free(short_options);

    argc -= optind;
    argv += optind;

    switch (argc) {
    case 0:
        return xasprintf("unix:%s/db.sock", ovs_rundir());

    case 1:
        return xstrdup(argv[0]);

    default:
        VLOG_FATAL("at most one non-option argument accepted; "
                   "use --help for usage");
    }
} /* parse_options */

/**
 * lacpd daemon's ovs-appctl callback function for exit command.
 *
 * @param conn is pointer appctl connection data struct.
 * @param argc OVS_UNUSED
 * @param argv OVS_UNUSED
 * @param exiting_ is pointer to a flag that reports exit status.
 */
static void
ops_lacpd_exit(struct unixctl_conn *conn, int argc OVS_UNUSED,
                 const char *argv[] OVS_UNUSED, void *exiting_)
{
    bool *exiting = exiting_;
    *exiting = true;
    unixctl_command_reply(conn, NULL);
} /* ops_lacpd_exit */

/**
 * Main function for lacpd daemon.
 *
 * @param argc is the number of command line arguments.
 * @param argv is an array of command line arguments.
 *
 * @return 0 for success or exit status on daemon exit.
 */
int
main(int argc, char *argv[])
{
    int mlacp_tindex;
    struct itimerval timerVal;
    char *appctl_path = NULL;
    struct unixctl_server *appctl;
    char *ovsdb_sock;
    int retval;
    sigset_t sigset;
    int signum;

    set_program_name(argv[0]);
    proctitle_init(argc, argv);
    fatal_ignore_sigpipe();

    /* Parse command line args and get the name of the OVSDB socket. */
    ovsdb_sock = parse_options(argc, argv, &appctl_path);
    if (ovsdb_sock == NULL) {
        exit(EXIT_FAILURE);
    }

    /* Initialize the metadata for the IDL cache. */
    ovsrec_init();

    /* Fork and return in child process; but don't notify parent of
     * startup completion yet. */
    daemonize_start();

    /* Create UDS connection for ovs-appctl. */
    retval = unixctl_server_create(appctl_path, &appctl);
    if (retval) {
        exit(EXIT_FAILURE);
    }

    /* Register the ovs-appctl "exit" command for this daemon. */
    unixctl_command_register("exit", "", 0, 0, ops_lacpd_exit, &exiting);

    /* Main LACP protocol state machine related initialization. */
    retval = mlacp_init(TRUE);
    if (retval) {
        exit(EXIT_FAILURE);
    }

    /* Initialize various protocol and event sockets, and create
     * the IDL cache of the dB at ovsdb_sock. */
    lacpd_init(ovsdb_sock, appctl);
    free(ovsdb_sock);

    /* Notify parent of startup completion. */
    daemonize_complete();

    /* Enable asynch log writes to disk. */
    vlog_enable_async();

    VLOG_INFO_ONCE("%s (OpenSwitch Link Aggregation Daemon) started", program_name);

    /* Set up timer to fire off every second. */
    timerVal.it_interval.tv_sec  = 1;
    timerVal.it_interval.tv_usec = 0;
    timerVal.it_value.tv_sec  = 1;
    timerVal.it_value.tv_usec = 0;

    if ((mlacp_tindex = setitimer(ITIMER_REAL, &timerVal, NULL)) != 0) {
        VLOG_ERR("lacpd main: Timer start failed!\n");
    }

    /* Wait for all signals in an infinite loop. */
    sigfillset(&sigset);
    while (!lacpd_shutdown) {

        sigwait(&sigset, &signum);
        switch (signum) {

        case SIGALRM:
            timerHandler();
            break;

        case SIGTERM:
        case SIGINT:
            VLOG_WARN("%s, sig %d caught", __FUNCTION__, signum);
            lacpd_shutdown = 1;
            break;

        default:
            VLOG_INFO("Ignoring signal %d.\n", signum);
            break;
        }
    }

    /* OPS_TODO - clean up various threads. */
    /* lacp_ops_cleanup(); */

    return 0;
} /* main */
