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
 * @addtogroup lacpd_ovsdb_if
 *
 * @details
 * This module implements OVSDB interface for lacpd daemon providing the
 * following primary functionality:
 *
 *     - Establish IDL session with OVSDB server.
 *     - Register for caching of tables and columns the daemon is interested in.
 *     - Maintain an internal cache of data important for this daemon so that
 *       change notifications can be handled appropriately.
 *     - Update OVSDB to report LAG membership status and inform vswitchd
 *       to carry out LAG configuration in hardware.
 *     - provide debug dump functions to dump internal state info using
 *       ovs-appctl interface.
 * @file
 * Source file for OVSDB interface functions.
 ***************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nemo/pm/pm_cmn.h>
#include <nemo/lacp/lacp_cmn.h>
#include <nemo/lacp/mlacp_debug.h>
#include <nemo/protocol/lacp/api.h>

#include "vpm/mvlan_sport.h"
#include "vpm/mvlan_lacp.h"

#include <dynamic-string.h>
#include <vswitch-idl.h>
#include <openhalon-idl.h>
#include <openvswitch/vlog.h>
#include <hash.h>
#include <shash.h>

VLOG_DEFINE_THIS_MODULE(ovsdb_if);

/**********************************//**
 * @ingroup lacpd_ovsdb_if
 * @{
 **************************************/

struct ovsdb_idl *idl;           /*!< Session handle for OVSDB IDL session. */

static unsigned int idl_seqno;

static int system_configured = false;

/**
 * A hash map of daemon's internal data for all the interfaces maintained by
 * lacpd.
 */
static struct shash all_interfaces = SHASH_INITIALIZER(&all_interfaces);

/**
 * A hash map of daemon's internal data for all the ports maintained by lacpd.
 */
static struct shash all_ports = SHASH_INITIALIZER(&all_ports);

/*************************************************************************//**
 * @ingroup lacpd_ovsdb_if
 * @brief lacpd's internal data structure to store per port data.
 ****************************************************************************/
struct port_data {
    char                *name;              /*!< Name of the port */
    struct shash        cfg_member_ifs;     /*!< configured member interfaces */
    struct shash        eligible_member_ifs;/*!< Interfaces eligible to form a LAG */
    enum ovsrec_port_lacp_e lacp_mode;      /*!< port's LACP mode */
    unsigned int        lag_member_speed;   /*!< link speed of LAG members */
};

/*************************************************************************//**
 * @ingroup lacpd_ovsdb_if
 * @brief lacpd's internal data strucuture to store per interface data.
 ****************************************************************************/
struct iface_data {
    char                *name;              /*!< Name of the interface */
    struct port_data    *port_datap;        /*!< Pointer to associated port's port_data */
    unsigned int        link_speed;         /*!< Operarational link speed of the interface */
    bool                lag_eligible;       /*!< indicates whether this interface is eligible to become member of configured LAG */
    enum ovsrec_interface_link_state_e link_state; /*!< operational link state */
    enum ovsrec_interface_duplex_e duplex;  /*!< operational link duplex */

    /* These members are valid only within lacpd_reconfigure(). */
    const struct ovsrec_interface *cfg;     /*!< pointer to corresponding row in IDL cache */
};

static int update_interface_lag_eligibility(struct iface_data *idp);
static int update_interface_hw_bond_config_map_entry(
    struct iface_data *idp, const char *key, const char *value);

/**
 * @details
 * Establishes an IDL session with OVSDB server. Registers the following
 * tables/columns for caching and change notification:
 *
 *     Open_vSwitch:cur_cfg
 *     Port:name, lacp, and interfaces columns.
 *     Interface:name, link_state, link_speed, hw_bond_config columns.
 */
void
lacpd_ovsdb_if_init(const char *db_path)
{
    /* Initialize IDL through a new connection to the DB. */
    idl = ovsdb_idl_create(db_path, &ovsrec_idl_class, false, true);
    idl_seqno = ovsdb_idl_get_seqno(idl);
    ovsdb_idl_set_lock(idl, "halon_lacpd");
    ovsdb_idl_verify_write_only(idl);

    /* Cache Open_vSwitch table. */
    ovsdb_idl_add_table(idl, &ovsrec_table_open_vswitch);
    ovsdb_idl_add_column(idl, &ovsrec_open_vswitch_col_cur_cfg);

    /* Cache Port table and columns. */
    ovsdb_idl_add_table(idl, &ovsrec_table_port);
    ovsdb_idl_add_column(idl, &ovsrec_port_col_name);
    ovsdb_idl_add_column(idl, &ovsrec_port_col_lacp);
    ovsdb_idl_add_column(idl, &ovsrec_port_col_interfaces);

    /* Cache Inteface table and columns. */
    ovsdb_idl_add_table(idl, &ovsrec_table_interface);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_name);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_link_state);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_link_speed);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_hw_bond_config);
    ovsdb_idl_omit_alert(idl, &ovsrec_interface_col_hw_bond_config);

} /* lacpd_ovsdb_if_init */

void
lacpd_ovsdb_if_exit(void)
{
    shash_destroy_free_data(&all_ports);
    shash_destroy_free_data(&all_interfaces);
    ovsdb_idl_destroy(idl);
} /* lacpd_ovsdb_if_exit */

static void
del_old_interface(struct shash_node *sh_node)
{
    if (sh_node) {
        struct iface_data *idp = sh_node->data;
        free(idp->name);
        free(idp);
        shash_delete(&all_interfaces, sh_node);
    }
} /* del_old_interface */

/**
 * Adds a new interface to daemon's internal data structures.
 *
 * Allocates a new iface_data entry. Parses the ifrow and
 * copies data into new iface_data entry.
 * Adds the new iface_data entry into all_interfaces shash map.
 * @param ifrow pointer to interface configuration row in IDL cache.
 */
static void
add_new_interface(const struct ovsrec_interface *ifrow)
{
    struct iface_data *idp = NULL;

    VLOG_DBG("Interface %s being added!", ifrow->name);

    /* Allocate structure to save state information for this interface. */
    idp = xzalloc(sizeof *idp);

    if (!shash_add_once(&all_interfaces, ifrow->name, idp)) {
        VLOG_WARN("Interface %s specified twice", ifrow->name);
        free(idp);
    } else {
        idp->name = xstrdup(ifrow->name);

        idp->link_state = INTERFACE_LINK_STATE_DOWN;
        if (ifrow->link_state) {
            if (!strcmp(ifrow->link_state, OVSREC_INTERFACE_LINK_STATE_UP)) {
                idp->link_state = INTERFACE_LINK_STATE_UP;
            }
        }

        idp->duplex = INTERFACE_DUPLEX_FULL; /* HALON_TODO */
        idp->link_speed = 1000; /* HALON_TODO */
        idp->lag_eligible = false;
        idp->cfg = ifrow;

        /* Intialize the interface to be not part of any LAG.
           This column gets updated later. */
        update_interface_hw_bond_config_map_entry(
            idp,
            INTERFACE_HW_BOND_CONFIG_MAP_RX_ENABLED,
            INTERFACE_HW_BOND_CONFIG_MAP_ENABLED_FALSE);
        update_interface_hw_bond_config_map_entry(
            idp,
            INTERFACE_HW_BOND_CONFIG_MAP_TX_ENABLED,
            INTERFACE_HW_BOND_CONFIG_MAP_ENABLED_FALSE);

        VLOG_DBG("Created local data for interface %s", ifrow->name);
    }
} /* add_new_interface */

/**
 * Update daemon's internal interface data structures based on the latest
 * data from OVSDB.
 * Takes necessary actions to propagate database changes.
 *
 * @return positive integer if an ovsdb write is required 0 otherwise.
 */
static int
update_interface_cache(void)
{
    struct shash sh_idl_interfaces;
    const struct ovsrec_interface *ifrow;
    struct shash_node *sh_node, *sh_next;
    int rc = 0;

    /* Collect all the interfaces in the DB. */
    shash_init(&sh_idl_interfaces);
    OVSREC_INTERFACE_FOR_EACH(ifrow, idl) {
        if (!shash_add_once(&sh_idl_interfaces, ifrow->name, ifrow)) {
            VLOG_WARN("interface %s specified twice", ifrow->name);
        }
    }

    /* Delete old interfaces. */
    SHASH_FOR_EACH_SAFE(sh_node, sh_next, &all_interfaces) {
        struct iface_data *idp =
            shash_find_data(&sh_idl_interfaces, sh_node->name);
        if (!idp) {
            VLOG_DBG("Found a deleted interface %s", sh_node->name);
            del_old_interface(sh_node);
        }
    }

    /* Add new interfaces. */
    SHASH_FOR_EACH(sh_node, &sh_idl_interfaces) {
        struct iface_data *idp =
            shash_find_data(&all_interfaces, sh_node->name);
        if (!idp) {
            VLOG_DBG("Found an added interface %s", sh_node->name);
            add_new_interface(sh_node->data);
            rc++;
        }
    }

    /* Check for changes in the interface row entries. */
    SHASH_FOR_EACH(sh_node, &all_interfaces) {
        struct iface_data *idp = sh_node->data;
        const struct ovsrec_interface *ifrow =
            shash_find_data(&sh_idl_interfaces, sh_node->name);
        enum ovsrec_interface_link_state_e new_link_state;

        /* Check for changes to row. */
        if (OVSREC_IDL_IS_ROW_INSERTED(ifrow, idl_seqno) ||
            OVSREC_IDL_IS_ROW_MODIFIED(ifrow, idl_seqno)) {
            new_link_state = INTERFACE_LINK_STATE_DOWN;
            if (ifrow->link_state) {
                if (!strcmp(ifrow->link_state, OVSREC_INTERFACE_LINK_STATE_UP)) {
                    new_link_state = INTERFACE_LINK_STATE_UP;
                }
            }
            if (new_link_state != idp->link_state) {
                VLOG_DBG("Interface %s link_state changed in DB: %s",
                         ifrow->name, ifrow->link_state);
                idp->link_state = new_link_state;
                if (update_interface_lag_eligibility(idp)) {
                    rc++;
                }
            }
        }
    }

    /* Destroy the shash of the IDL interfaces. */
    shash_destroy(&sh_idl_interfaces);

    return rc;
} /* update_interface_cache */

/**
 * Update hw_bond_config map column of interface row with entry_key and
 * entry_value pair.
 *
 * @param idp  iface_data pointer to the interface entry.
 * @param entry_key name of the key.
 * @param entry_value value for the key.
 *
 * @return 1 to indicate database transaction needs to be committed.
 */
static int
update_interface_hw_bond_config_map_entry(
    struct iface_data *idp,
    const char *entry_key,
    const char *entry_value)
{
    const struct ovsrec_interface *ifrow;
    struct smap smap;

    ifrow = idp->cfg;
    smap_clone(&smap, &ifrow->hw_bond_config);
    smap_replace(&smap, entry_key, entry_value);

    ovsrec_interface_set_hw_bond_config(ifrow, &smap);

    return 1;
} /* update_interface_hw_bond_config_map_entry */

/**
 * Common function to set interface's LAG eligibility status for all LAG types.
 * Depending on the LAG type, this function either updates the DB by writing
 * to interface:hw_bond_config column or initiate LACP protocol on the
 * interface.
 *
 * @param portp pointer to port_data data struct.
 * @param idp pointer to iface_data data struct.
 * @param eligible indicates whether the interface is eligible to be a LAG member.
 */
static void
set_interface_lag_eligibility(
    struct port_data *portp,
    struct iface_data *idp,
    bool eligible)
{
    if (eligible == idp->lag_eligible) {
        return;
    }

    if (portp->lacp_mode == PORT_LACP_OFF) {
        /* static LAG configuration in hardware. */
        update_interface_hw_bond_config_map_entry(
            idp,
            INTERFACE_HW_BOND_CONFIG_MAP_RX_ENABLED,
            eligible == true
            ? INTERFACE_HW_BOND_CONFIG_MAP_ENABLED_TRUE
            : INTERFACE_HW_BOND_CONFIG_MAP_ENABLED_FALSE);

        update_interface_hw_bond_config_map_entry(
            idp,
            INTERFACE_HW_BOND_CONFIG_MAP_TX_ENABLED,
            eligible == true
            ? INTERFACE_HW_BOND_CONFIG_MAP_ENABLED_TRUE
            : INTERFACE_HW_BOND_CONFIG_MAP_ENABLED_FALSE);
    } else {
        /* HALON_TODO: LACP LAG support. */
    }

    /* update eligible LAG member list. */
    if (eligible) {
            shash_add(&portp->eligible_member_ifs, idp->name, (void *)idp);
    } else {
            shash_find_and_delete(&portp->eligible_member_ifs, idp->name);
    }
    idp->lag_eligible = eligible;
} /* set_interface_lag_eligibility */

/**
 * Checks whether the interface is eligibile to become a member of
 * configured LAG by applying the following rules:
 *     - interface configured to be member of a LAG
 *     - interface is linked up
 *     - interface is in full duplex mode
 *     - interface link speed is same as the lag_member_speed. The link
 *       speed of LAG members is determined by the link speed of the first
 *       interface to join the LAG.
 *
 * If all the conditions are met, the interface is made eligible to
 * become member of the LAG.
 * For static LAG, the interface is attached to LAG in hardware by updating
 * hw_bond_config column immediatly.
 * For LACP LAGs, the LACP protocol is activated on this interface. The
 * outcome of LACP protocol will determine whether the interface will be
 * added to LAG in hardware or not.
 * @param idp pointer to interface's data.
 *
 * @return non-zero when changes to database transaction are made.
 */
static int
update_interface_lag_eligibility(struct iface_data *idp)
{
    struct port_data *portp;
    bool old_eligible = false;
    bool new_eligible = true;
    int rc = 0;

    if (!idp || !idp->port_datap) {
        return 0;
    }
    portp = idp->port_datap;

    /* Check if the interface is currently eligible. */
    if (shash_find_data(&portp->eligible_member_ifs, idp->name)) {
        old_eligible = true;
    }

    if (!shash_find_data(&portp->cfg_member_ifs, idp->name)) {
        new_eligible = false;
    }

    if (INTERFACE_LINK_STATE_UP != idp->link_state) {
            new_eligible = false;
    }

    if (INTERFACE_DUPLEX_FULL != idp->duplex) {
            new_eligible = false;
    }

    if ((portp->lag_member_speed == 0) && new_eligible) {
        /* First member to join the LAG decides LAG member speed. */
        portp->lag_member_speed = idp->link_speed;
    }

    if (portp->lag_member_speed != idp->link_speed) {
        new_eligible = false;
    }

    VLOG_DBG("%s member %s old_eligible=%d new_eligible=%d",
             __FUNCTION__, idp->name,
             old_eligible, new_eligible);

    if (old_eligible != new_eligible) {
        set_interface_lag_eligibility(portp, idp, new_eligible);
        rc++;
    }
    return rc;
} /* update_interface_lag_eligibility */

/**
 * Handles LAG related configuration changes for a given port table entry.
 *
 * @param row pointer to port row in IDL.
 * @param portp pointer to daemon's internal port data struct.
 *
 * @return
 */
static int
handle_lag_config(const struct ovsrec_port *row, struct port_data *portp)
{
    enum ovsrec_port_lacp_e lacp_mode;
    struct ovsrec_interface *intf;
    struct shash sh_idl_port_intfs;
    struct shash_node *node, *next;
    int rc = 0;
    size_t i;

    lacp_mode = PORT_LACP_OFF;
    if (row->lacp) {
        if (strcmp(OVSREC_PORT_LACP_ACTIVE, row->lacp) == 0) {
            lacp_mode = PORT_LACP_ACTIVE;
        } else if (strcmp(OVSREC_PORT_LACP_PASSIVE, row->lacp) == 0) {
            lacp_mode = PORT_LACP_PASSIVE;
        }
    }
    if (portp->lacp_mode != lacp_mode) {
        VLOG_DBG("port %s:lacp_mode changed  %d -> %d",
                 row->name, portp->lacp_mode, lacp_mode);
        portp->lacp_mode = lacp_mode;
        /* HALON_TODO: Handle lacp_mode changes. */
    }

    VLOG_DBG("%s: port %s n_interfaces=%d",
             __FUNCTION__, row->name, (int)row->n_interfaces);

    /* Build a new map for this port's interfaces in idl. */
    shash_init(&sh_idl_port_intfs);
    for (i = 0; i < row->n_interfaces; i++) {
        intf = row->interfaces[i];
        if (!shash_add_once(&sh_idl_port_intfs, intf->name, intf)) {
            VLOG_WARN("interface %s specified twice", intf->name);
        }
    }

    /* Look for deleted interfaces. */
    SHASH_FOR_EACH_SAFE(node, next, &portp->cfg_member_ifs) {
        struct ovsrec_interface *ifrow =
            shash_find_data(&sh_idl_port_intfs, node->name);
        if (!ifrow) {
            struct iface_data *idp =
                shash_find_data(&all_interfaces, node->name);
            if (idp) {
                VLOG_DBG("Found a deleted interface %s", node->name);
                shash_delete(&portp->cfg_member_ifs, node);
                set_interface_lag_eligibility(portp, idp, false);
                idp->port_datap = NULL;
                rc++;
            }
        }
    }
    /* Look for newly added interfaces. */
    SHASH_FOR_EACH(node, &sh_idl_port_intfs) {
        struct ovsrec_interface *ifrow =
            shash_find_data(&portp->cfg_member_ifs, node->name);
        if (!ifrow) {
            VLOG_DBG("Found an added interface %s", node->name);
            struct iface_data *idp =
                shash_find_data(&all_interfaces, node->name);
            if (!idp) {
                VLOG_ERR("Error adding interface to port %s."
                         "Interface %s not found.",
                         portp->name, node->name);
                continue;
            }
            shash_add(&portp->cfg_member_ifs, node->name, (void *)idp);
            idp->port_datap = portp;
        }
    }

    /* Update LAG member eligibilty for configured member intefaces. */
    SHASH_FOR_EACH_SAFE(node, next, &portp->cfg_member_ifs) {
        struct iface_data *idp = shash_find_data(&all_interfaces, node->name);
        if (idp) {
            if (update_interface_lag_eligibility(idp)) {
                rc++;
            }
        }
    }

    /* Destroy the shash of the IDL interfaces. */
    shash_destroy(&sh_idl_port_intfs);

    return rc;
} /* handle_lag_config */

static int
del_old_port(struct shash_node *sh_node)
{
    int rc = 0;
    if (sh_node) {
        struct port_data *portp = sh_node->data;
        struct shash_node *node, *next;

        SHASH_FOR_EACH_SAFE(node, next, &portp->cfg_member_ifs) {
            struct iface_data *idp =
                shash_find_data(&all_interfaces, node->name);
            if (idp) {
                VLOG_DBG("Removing interface %s from port %s hash map",
                         idp->name, portp->name);

                shash_delete(&portp->cfg_member_ifs, node);
                set_interface_lag_eligibility(portp, idp, false);
                idp->port_datap = NULL;
                rc++;
            }
        }
        free(portp->name);
        free(portp);
        shash_delete(&all_ports, sh_node);
    }
    return rc;
} /* del_old_port */

static void
add_new_port(const struct ovsrec_port *port_row)
{
    struct port_data *portp = NULL;

    VLOG_DBG("Port %s being added!", port_row->name);

    /* Allocate structure to save state information for this interface. */
    portp = xcalloc(1, sizeof *portp);

    if (!shash_add_once(&all_ports, port_row->name, portp)) {
        VLOG_WARN("Port %s specified twice", port_row->name);
        free(portp);
    } else {
        size_t i;

        portp->name = xstrdup(port_row->name);
        shash_init(&portp->cfg_member_ifs);
        shash_init(&portp->eligible_member_ifs);

        for (i = 0; i < port_row->n_interfaces; i++) {
            struct ovsrec_interface *intf;
            struct iface_data *idp;

            intf = port_row->interfaces[i];
            idp = shash_find_data(&all_interfaces, intf->name);
            if (!idp) {
                VLOG_ERR("Error adding interface to new port %s."
                         "Interface %s not found.",
                         portp->name, intf->name);
                continue;
            }
            VLOG_DBG("Storing interface %s in port %s hash map",
                     intf->name, portp->name);
            shash_add(&portp->cfg_member_ifs, intf->name, (void *)idp);
            idp->port_datap = portp;
        }
        VLOG_DBG("Created local data for Port %s", port_row->name);
    }
} /* add_new_port */

/**
 * Update daemon's internal port data structures based on the latest
 * data from OVSDB.
 * Takes necessary actions to propagate database changes.
 *
 * @return positive integer if an ovsdb write is required 0 otherwise.
 */
static int
update_port_cache(void)
{
    struct shash sh_idl_ports;
    const struct ovsrec_port *row;
    struct shash_node *sh_node, *sh_next;
    int rc = 0;

    /* Collect all the ports in the DB. */
    shash_init(&sh_idl_ports);
    OVSREC_PORT_FOR_EACH(row, idl) {
        if (!shash_add_once(&sh_idl_ports, row->name, row)) {
            VLOG_WARN("port %s specified twice", row->name);
        }
    }

    /* Delete old ports. */
    SHASH_FOR_EACH_SAFE(sh_node, sh_next, &all_ports) {
        struct port_data *portp = shash_find_data(&sh_idl_ports, sh_node->name);
        if (!portp) {
            VLOG_DBG("Found a deleted port %s", sh_node->name);
            del_old_port(sh_node);
            rc++;
        }
    }

    /* Add new ports. */
    SHASH_FOR_EACH(sh_node, &sh_idl_ports) {
        struct port_data *portp = shash_find_data(&all_ports, sh_node->name);
        if (!portp) {
            VLOG_DBG("Found an added port %s", sh_node->name);
            add_new_port(sh_node->data);
        }
    }

    /* Check for changes in the port row entries. */
    SHASH_FOR_EACH(sh_node, &all_ports) {
        const struct ovsrec_port *row = shash_find_data(&sh_idl_ports,
                                                        sh_node->name);

        /* Check for changes to row. */
        if (OVSREC_IDL_IS_ROW_INSERTED(row, idl_seqno) ||
            OVSREC_IDL_IS_ROW_MODIFIED(row, idl_seqno)) {
            struct port_data *portp = sh_node->data;

            /* Handle LAG config update. */
            if (handle_lag_config(row, portp)) {
                rc++;
            }
        }
    }

    /* Destroy the shash of the IDL ports. */
    shash_destroy(&sh_idl_ports);

    return rc;
} /* update_port_cache */

static int
lacpd_reconfigure(void)
{
    int rc = 0;
    unsigned int new_idl_seqno = ovsdb_idl_get_seqno(idl);

    if (new_idl_seqno == idl_seqno) {
        /* There was no change in the DB. */
        return 0;
    }

    /* Update lacpd's Interfaces table cache. */
    if (update_interface_cache()) {
        rc++;
    }

    /* Update lacpd's Ports table cache. */
    if (update_port_cache()) {
        rc++;
    }

    /* Update IDL sequence # after we've handled everything. */
    idl_seqno = new_idl_seqno;

    return rc;
} /* lacpd_reconfigure */

static inline void
lacpd_chk_for_system_configured(void)
{
    const struct ovsrec_open_vswitch *ovs_vsw = NULL;

    if (system_configured) {
        /* Nothing to do if we're already configured. */
        return;
    }

    ovs_vsw = ovsrec_open_vswitch_first(idl);

    if (ovs_vsw && ovs_vsw->cur_cfg > (int64_t) 0) {
        system_configured = true;
        VLOG_INFO("System is now configured (cur_cfg=%d).",
                  (int)ovs_vsw->cur_cfg);
    }

} /* lacpd_chk_for_system_configured */

/**@} end of lacpd_ovsdb_if group */

/***
 * @ingroup lacpd
 * @{
 */
void
lacpd_run(void)
{
    struct ovsdb_idl_txn *txn;

    /* Process a batch of messages from OVSDB. */
    ovsdb_idl_run(idl);

    if (ovsdb_idl_is_lock_contended(idl)) {
        static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 1);

        VLOG_ERR_RL(&rl, "Another lacpd process is running, "
                    "disabling this process until it goes away");

        return;
    } else if (!ovsdb_idl_has_lock(idl)) {
        return;
    }

    /* Update the local configuration and push any changes to the DB. */
    lacpd_chk_for_system_configured();

    if (system_configured) {
        txn = ovsdb_idl_txn_create(idl);
        if (lacpd_reconfigure()) {
            /* Some OVSDB write needs to happen. */
            ovsdb_idl_txn_commit_block(txn);
        }
        ovsdb_idl_txn_destroy(txn);
    }

    return;
} /* lacpd_run */

void
lacpd_wait(void)
{
    ovsdb_idl_wait(idl);
} /* lacpd_wait */

void
halon_create_lag_in_hw(uint16_t lag_id __attribute__((unused)))
{
}

void
halon_destroy_lag_in_hw(uint16_t lag_id __attribute__((unused)))
{
}

void
halon_attach_port_in_hw(uint16_t lag_id __attribute__((unused)),
                        int port __attribute__((unused)))
{
}
void
halon_detach_port_in_hw(uint16_t lag_id __attribute__((unused)),
 int port_index __attribute__((unused)))
{
}

void
db_delete_lag_port(uint16_t lag_id __attribute__((unused)),
 int port __attribute__((unused)))
{
}

void
halon_trunk_port_egr_enable(uint16_t lag_id __attribute__((unused)),
 int port __attribute__((unused)))
{
}

void
db_add_lag_port(uint16_t lag_id __attribute__((unused)),
 int port __attribute__((unused)))
{
}

void
db_clear_lag_partner_info(uint16_t lag_id)
{
    VLOG_DBG("*** %s TODO lag_id %d***", __FUNCTION__, lag_id);
}

void
db_update_lag_partner_info(uint16_t lag_id,
   lacp_sport_params_t *param __attribute__((unused)))
{
    VLOG_DBG("*** %s TODO lag_id %d***", __FUNCTION__, lag_id);
}

/**********************************************************************/
/*                               DEBUG                                */
/**********************************************************************/
static void
lacpd_interface_dump(struct ds *ds, struct iface_data *idp)
{
    ds_put_format(ds, "Interface %s:\n", idp->name);
    ds_put_format(ds, "    link_state           : %s\n",
                  idp->link_state == INTERFACE_LINK_STATE_UP
                  ? OVSREC_INTERFACE_LINK_STATE_UP :
                  OVSREC_INTERFACE_LINK_STATE_DOWN);
    ds_put_format(ds, "    link_speed           : %d\n",
                  idp->link_speed);
    ds_put_format(ds, "    duplex               : %s\n",
                  idp->duplex == INTERFACE_DUPLEX_FULL
                  ? OVSREC_INTERFACE_DUPLEX_FULL :
                  OVSREC_INTERFACE_DUPLEX_HALF);
    if (idp->port_datap) {
        ds_put_format(ds, "    configured LAG       : %s\n",
                      idp->port_datap->name);
        ds_put_format(ds, "    LAG eligible         : %s\n",
                      idp->lag_eligible ? "true" : "false");
    }
} /* lacpd_interface_dump */

static void
lacpd_interfaces_dump(struct ds *ds, int argc, const char *argv[])
{
    struct shash_node *sh_node;
    struct iface_data *idp = NULL;

    if (argc > 2) {
        struct iface_data *idp =
            shash_find_data(&all_interfaces, argv[2]);
        if (idp){
            lacpd_interface_dump(ds, idp);
        }
    } else {
        ds_put_cstr(ds, "================ Interfaces ================\n");

        SHASH_FOR_EACH(sh_node, &all_interfaces) {
            idp = sh_node->data;
            if (idp){
                lacpd_interface_dump(ds, idp);
            }
        }
    }
} /* lacpd_interfaces_dump */

static void
lacpd_port_dump(struct ds *ds, struct port_data *portp)
{
    struct shash_node *node;

    ds_put_format(ds, "Port %s:\n", portp->name);
    ds_put_format(ds, "    lacp                 : %s\n",
                  (portp->lacp_mode == PORT_LACP_ACTIVE)
                  ? OVSREC_PORT_LACP_ACTIVE :
                  ((portp->lacp_mode == PORT_LACP_PASSIVE)
                   ? OVSREC_PORT_LACP_PASSIVE :
                   OVSREC_PORT_LACP_OFF));
    ds_put_format(ds, "    lag_member_speed     : %d\n",
                  portp->lag_member_speed);
    ds_put_format(ds, "    configured_members   :");
    SHASH_FOR_EACH(node, &portp->cfg_member_ifs) {
        struct iface_data *idp = shash_find_data(&all_interfaces, node->name);
        if (idp) {
            ds_put_format(ds, " %s", idp->name);
        }
    }
    ds_put_format(ds, "\n");

    ds_put_format(ds, "    eligible_members     :");
    SHASH_FOR_EACH(node, &portp->eligible_member_ifs) {
        struct iface_data *idp = shash_find_data(&all_interfaces, node->name);
        if (idp) {
            ds_put_format(ds, " %s", idp->name);
        }
    }
    ds_put_format(ds, "\n");
} /* lacpd_port_dump */

static void
lacpd_ports_dump(struct ds *ds, int argc, const char *argv[])
{
    struct shash_node *sh_node;
    struct port_data *portp = NULL;

    if (argc > 2) {
        portp = shash_find_data(&all_ports, argv[2]);
        if (portp){
            lacpd_port_dump(ds, portp);
        }
    } else {
        ds_put_cstr(ds, "================ Ports ================\n");

        SHASH_FOR_EACH(sh_node, &all_ports) {
            portp = sh_node->data;
            if (portp){
                lacpd_port_dump(ds, portp);
            }
        }
    }
} /* lacpd_ports_dump */

/**
 * @details
 * Dumps debug data for entire daemon or for individual component specified
 * on command line.
 */
void
lacpd_debug_dump(struct ds *ds, int argc, const char *argv[])
{
    const char *table_name = NULL;

    if (argc > 1) {
        table_name = argv[1];

        if (!strcmp(table_name, "interface")) {
            lacpd_interfaces_dump(ds, argc, argv);
        } else if (!strcmp(table_name, "port")) {
            lacpd_ports_dump(ds, argc, argv);
        }
    } else {
        lacpd_interfaces_dump(ds, 0, NULL);
        lacpd_ports_dump(ds, 0, NULL);
    }
} /* lacpd_debug_dump */

/**@} end of lacpd group */
