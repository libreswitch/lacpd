/*
 * Copyright (C) 2015 Hewlett-Packard Development Company, L.P.
 * Copyright (c) 2008, 2009, 2010, 2011, 2012, 2013, 2014 Nicira, Inc.
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

/************************************************************************//**
 * @ingroup lacpd
 *
 * @file
 * Main source file for the LACP daemon.
 *
 *    The lacpd daemon operates as an overall Halon Link Aggregation (LAG)
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
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <config.h>
#include <command-line.h>
#include <compiler.h>
#include <daemon.h>
#include <dirs.h>
#include <dynamic-string.h>
#include <fatal-signal.h>
#include <ovsdb-idl.h>
#include <poll-loop.h>
#include <unixctl.h>
#include <util.h>
#include <openvswitch/vconn.h>
#include <openvswitch/vlog.h>
#include <vswitch-idl.h>
#include <openhalon-idl.h>
#include <hash.h>
#include <shash.h>

#include "lacp_halon_if.h"

VLOG_DEFINE_THIS_MODULE(lacpd);

static unixctl_cb_func lacpd_unixctl_dump;
static unixctl_cb_func halon_lacpd_exit;

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
 * lacpd daemons main initialization function.
 *
 * @param db_path pathname for OVSDB connection.
 */
static void
lacpd_init(const char *db_path)
{
    /* HALON_TODO: Initialize LACP protocol */
    /* halon_lacp_proto_init(); */

    /* Initialize IDL through a new connection to the dB. */
    lacpd_ovsdb_if_init(db_path);

    /* Register ovs-appctl commands for this daemon. */
    unixctl_command_register("lacpd/dump", "", 0, 2, lacpd_unixctl_dump, NULL);
} /* lacpd_init */

/**
 * Cleanup function at daemon shutdown time.
 *
 */
static void
lacpd_exit(void)
{
    lacpd_ovsdb_if_exit();
} /* lacpd_exit */

/**
 * lacpd usage help function.
 *
 */
static void
usage(void)
{
    printf("%s: Halon LACP daemon\n"
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
halon_lacpd_exit(struct unixctl_conn *conn, int argc OVS_UNUSED,
                 const char *argv[] OVS_UNUSED, void *exiting_)
{
    bool *exiting = exiting_;
    *exiting = true;
    unixctl_command_reply(conn, NULL);
} /* halon_lacpd_exit */

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
    char *appctl_path = NULL;
    struct unixctl_server *appctl;
    char *ovsdb_sock;
    bool exiting;
    int retval;

    set_program_name(argv[0]);
    proctitle_init(argc, argv);
    fatal_ignore_sigpipe();

    /* Parse command line args and get the name of the OVSDB socket. */
    ovsdb_sock = parse_options(argc, argv, &appctl_path);

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
    unixctl_command_register("exit", "", 0, 0, halon_lacpd_exit, &exiting);

    /* Create the IDL cache of the dB at ovsdb_sock. */
    lacpd_init(ovsdb_sock);
    free(ovsdb_sock);

    /* Notify parent of startup completion. */
    daemonize_complete();

    /* Enable asynch log writes to disk. */
    vlog_enable_async();

    VLOG_INFO_ONCE("%s (Halon Link Aggregation Daemon) started", program_name);

    exiting = false;
    while (!exiting) {
        lacpd_run();
        unixctl_server_run(appctl);

        lacpd_wait();
        unixctl_server_wait(appctl);
        if (exiting) {
            poll_immediate_wake();
        } else {
            poll_block();
        }
    }

    lacpd_exit();
    unixctl_server_destroy(appctl);

    return 0;
} /* main */
