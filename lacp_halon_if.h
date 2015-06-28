/*
 * Copyright (C) 2015 Hewlett-Packard Development Company, L.P.
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
 * @defgroup lacpd LACP Daemon
 * The lacpd platform daemon manages Link Aggregation Groups (LAGs) on
 * Open Halon platform.
 *
 * The lacpd daemon is responsible for handling user level LAG (bond)
 * configuration and managing the LAG membership. This deamon manages two
 * types of LAGs:
 *     - static LAGs, in which the LAG membership remains constant as long the
 *       interface's operational state meets basic LAG membership eligibility
 *       criterion.
 *     - dynamic LAGs using LACP protocol, in which the LAG membership is
 *       determined based on LACP protocol exchange with link partner in
 *       addition to basic LAG membership eligibility criterion.
 *
 * @{
 *
 * @file
 * Header for platform LACP daemon
 *
 * @defgroup lacpd_public Public Interface
 * Public API for the platform LACP daemon.
 *
 * @{
 *
 * Public APIs
 *
 * Command line options:
 *
 *     lacpd: Halon LACP Daemon
 *     usage: lacpd [OPTIONS] [DATABASE]
 *     where DATABASE is a socket on which ovsdb-server is listening
 *           (default: "unix:/var/run/openvswitch/db.sock").
 *
 *      Daemon options:
 *        --detach                run in background as daemon
 *        --no-chdir              do not chdir to '/'
 *        --pidfile[=FILE]        create pidfile (default: /var/run/openvswitch/lacpd.pid)
 *        --overwrite-pidfile     with --pidfile, start even if already running
 *
 *      Logging options:
 *        -vSPEC, --verbose=SPEC   set logging levels
 *        -v, --verbose            set maximum verbosity level
 *        --log-file[=FILE]        enable logging to specified FILE
 *                                 (default: /var/log/openvswitch/lacpd.log)
 *        --syslog-target=HOST:PORT  also send syslog msgs to HOST:PORT via UDP
 *
 *      Other options:
 *        --unixctl=SOCKET        override default control socket name
 *        -h, --help              display this help message
 *
 *
 * Available ovs-apptcl command options are:
 *
 *      coverage/show
 *      exit
 *      list-commands
 *      version
 *      lacpd/dump [{interface [interface name]} | {port [port name]}]
 *      vlog/disable-rate-limit [module]...
 *      vlog/enable-rate-limit  [module]...
 *      vlog/list
 *      vlog/reopen
 *      vlog/set                {spec | PATTERN:destination:pattern}
 *
 *
 * OVSDB elements usage
 *
 *  The following columns are READ by lacpd:
 *
 *      Open_vSwitch:cur_cfg
 *      Port:name
 *      Port:lacp
 *      Port:interfaces
 *      Interface:name
 *      Interface:link_state
 *      Interface:link_speed
 *
 *  The following columns are WRITTEN by lacpd:
 *
 *      Interface:hw_bond_config:rx_enabled
 *      Interface:hw_bond_config:tx_enabled
 *
 * Linux Files:
 *
 *  The following files are written by lacpd:
 *
 *      /var/run/openvswitch/lacpd.pid: Process ID for the lacpd daemon
 *      /var/run/openvswitch/lacpd.<pid>.ctl: Control file for ovs-appctl
 *
 ***************************************************************************/

/** @} end of group lacpd_public */

#ifndef __LACP_HALON_IF__H__
#define __LACP_HALON_IF__H__

#include "vpm/mvlan_lacp.h"
#include <dynamic-string.h>

/**
 * @defgroup lacpd_ovsdb_if OVSDB Interface
 * OVSDB interface functions for lacpd.
 * @{
 */
// H/W Configuration function
extern void halon_create_lag_in_hw(int lag_id);
extern void halon_destroy_lag_in_hw(int lag_id);
extern void halon_trunk_port_egr_enable(int lag_id, int port);
extern void halon_attach_port_in_hw(int lag_id, int port);
extern void halon_detach_port_in_hw(int lag_id, int port);
extern void halon_send_lacpdu(unsigned char* data, int len, int port);

// LAG status update functions
extern void db_update_lag_partner_info(int lag_id, lacp_sport_params_t *param);
extern void db_clear_lag_partner_info(int lag_id);
extern void db_add_lag_port(int lag_id, int port);
extern void db_delete_lag_port(int lag_id, int port);

/**************************************************************************//**
 * Initializes OVSDB interface.
 * Called by lacpd's initialization code to create a connection to OVSDB
 * server and register for caching the tables and columns.
 *
 * @param[in] db_path is the pathname for OVSDB connection.
 *
 *****************************************************************************/
extern void lacpd_ovsdb_if_init(const char *db_path);

/**************************************************************************//**
 * Cleanup OVSDB interface before daemon exit.
 * Called by lacpd's shutdown code to cleanup OVSDB interface.
 *
 *****************************************************************************/
extern void lacpd_ovsdb_if_exit(void);

/**************************************************************************//**
 * Setup file descriptors for IDL's poll function.
 * Called by lacpd's main loop to setup daemon specific wait criterion for
 * blocking.
 *
 *****************************************************************************/
extern void lacpd_wait(void);

/**************************************************************************//**
 * Process data on lacpd daemon's file descriptors.
 * Called by lacpd's main loop to process daemon specific handlers to
 * process data received on any of the polling file descriptors.
 *
 *****************************************************************************/
extern void lacpd_run(void);

/**************************************************************************//**
 * Debug function to dump lacpd's internal data structures.
 * Called by lacpd's appctl interface.
 *
 * @param[in,out] ds pointer to struct ds that holds the debug output.
 * @param[in] argc number of arguments passed to this function.
 * @param[in] argv variable argument list.
 *
 *****************************************************************************/
extern void lacpd_debug_dump(struct ds *ds, int argc, const char *argv[]);

/** @} end of group lacpd_ovsdb_if */

#endif /* __LACP_HALON_IF__H__ */

/** @} end of group lacpd */
