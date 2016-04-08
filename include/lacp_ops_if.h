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
 * @defgroup lacpd LACP Daemon
 * The lacpd platform daemon manages Link Aggregation Groups (LAGs) on
 * OpenSwitch platform.
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
 *     lacpd: OpenSwitch LACP Daemon
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
 *      System:cur_cfg
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

#ifndef __LACP_OPS_IF__H__
#define __LACP_OPS_IF__H__

#include <dynamic-string.h>
#include <vswitch-idl.h>

#include <pm_cmn.h>

#include "mvlan_lacp.h"
#include "lacp.h"

struct lacp_status_values {
    char *system_id;
    char *port_id;
    char *key;
    char *state;
};

/*************************************************************************//**
 * @ingroup lacpd_ovsdb_if
 * @brief lacpd's internal data strucuture to store per interface data.
 ****************************************************************************/
struct iface_data {
    char                *name;              /*!< Name of the interface */
    enum ovsrec_interface_type_e intf_type; /*!< Interface type */
    struct port_data    *port_datap;        /*!< Pointer to associated port's port_data */
    unsigned int        link_speed;         /*!< Operarational link speed of the interface */
    bool                lag_eligible;       /*!< indicates whether this interface is eligible
                                              to become member of configured LAG */
    enum ovsrec_interface_link_state_e link_state; /*!< operational link state */
    enum ovsrec_interface_duplex_e duplex;  /*!< operational link duplex */

    /* These members are valid only within lacpd_reconfigure(). */
    const struct ovsrec_interface *cfg;     /*!< pointer to corresponding row in IDL cache */

    int                 index;              /*!< Allocated index for interface */
    int                 hw_port_number;     /*!< Hardware port number */
    enum PM_lport_type  cycl_port_type;     /*!< port type */

    /* Configuration information from LACP element. */
    uint16_t            cfg_lag_id;         /*!< Configured LAG_ID */
    int                 lacp_state;         /*!< 0=disabled, 1=enabled */
    int                 actor_priority;     /*!< Integer */
    int                 actor_key;          /*!< Integer */
    int                 aggregateable;      /*!< 0=no, 1=yes */
    int                 activity_mode;      /*!< 0=passive, 1=active */
    int                 timeout_mode;       /*!< 0=long, 1=short */
    int                 collecting_ready;   /*!< hardware is ready to collect */
    int                 port_id;            /*!< port id */
    bool                fallback_enabled;   /*!< Default = false */

    /* LACPDU send/receive related. */
    int                 pdu_sockfd;         /*!< Socket FD for LACPDU rx/tx */
    bool                pdu_registered;     /*!< Indicates if port is registered to receive LACPDU */

    /* LACP status values formatted */
    struct lacp_status_values actor;        /*!< Currently set lacp status values - actor */
    struct lacp_status_values partner;      /*!< Currently set lacp status values - partner */
    bool                      lacp_current; /*!< Currently set lacp_current value */
    bool                      lacp_current_set; /*!< false=lacp_current is not set, true=lacp_current is set */
    struct state_parameters   local_state;
};

/**
 * @defgroup lacpd_ovsdb_if OVSDB Interface
 * OVSDB interface functions for lacpd.
 * @{
 */
// H/W Configuration function
extern void ops_trunk_port_egr_enable(uint16_t lag_id, int port);
extern void ops_attach_port_in_hw(uint16_t lag_id, int port);
extern void ops_detach_port_in_hw(uint16_t lag_id, int port);
extern void ops_send_lacpdu(unsigned char* data, int len, int port);

// LAG status update functions
extern void db_update_lag_partner_info(uint16_t lag_id);
extern void db_clear_lag_partner_info(uint16_t lag_id);
extern void db_add_lag_port(uint16_t lag_id, int port, lacp_per_port_variables_t *plpinfo);
extern void db_delete_lag_port(uint16_t lag_id, int port, lacp_per_port_variables_t *plpinfo);

extern void db_update_interface(lacp_per_port_variables_t *plpinfo);

// Utility functions
extern struct iface_data *find_iface_data_by_index(int index);

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

/**************************************************************************//**
 * Debug function to dump the interfaces member of LAGs.
 * Called by lacpd's appctl interface.
 *
 * @param[in,out] ds pointer to struct ds that holds the debug output.
 * @param[in] argc number of arguments passed to this function.
 * @param[in] argv variable argument list.
 *
 *****************************************************************************/
extern void lacpd_lag_ports_dump(struct ds *ds, int argc, const char *argv[]);

/**************************************************************************//**
 * Debug function to dump the PDU counters for all the LAG ports in the daemon or for
 * an individual specified port.
 * Called by lacpd's appctl interface.
 *
 * @param[in,out] ds pointer to struct ds that holds the debug output.
 * @param[in] argc number of arguments passed to this function.
 * @param[in] argv variable argument list.
 *
 *****************************************************************************/
extern void lacpd_pdus_counters_dump(struct ds *ds, int argc, const char *argv[]);

/**************************************************************************//**
 * Debug function to dump the LACP state machine status for all the LAG ports in
 * the daemon or for an individual specified port.
 * Called by lacpd's appctl interface.
 *
 * @param[in,out] ds pointer to struct ds that holds the debug output.
 * @param[in] argc number of arguments passed to this function.
 * @param[in] argv variable argument list.
 *
 *****************************************************************************/
extern void lacpd_state_dump(struct ds *ds, int argc, const char *argv[]);

/**************************************************************************//**
 * lacpd daemon's main OVS interface function.
 *
 * @param arg pointer to ovs-appctl server struct.
 *
 *****************************************************************************/
extern void *lacpd_ovs_main_thread(void *arg);

/** @} end of group lacpd_ovsdb_if */

#endif /* __LACP_OPS_IF__H__ */

/** @} end of group lacpd */
