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
#include <pthread.h>
#include <semaphore.h>
#include <netinet/ether.h>

#include <nemo/lacp/lacp_cmn.h>
#include <nemo/lacp/mlacp_debug.h>
#include <nemo/protocol/lacp/api.h>

#include "lacp_halon_if.h"
#include "lacp.h"
#include "mlacp_fproto.h"
#include "vpm/mvlan_sport.h"

#include <unixctl.h>
#include <dynamic-string.h>
#include <openhalon-idl.h>
#include <openvswitch/vlog.h>
#include <poll-loop.h>
#include <hash.h>
#include <shash.h>

VLOG_DEFINE_THIS_MODULE(lacpd_ovsdb_if);

/**********************************//**
 * @ingroup lacpd_ovsdb_if
 * @{
 **************************************/

#define LACP_ENABLED_ON_PORT(m)    ((m) == PORT_LACP_PASSIVE || \
                                    (m) == PORT_LACP_ACTIVE)

/* Scale OVS interface speed number (bps) down to
 * that used by LACP state machine (Mbps). */
#define INTF_TO_LACP_LINK_SPEED(s)    ((s)/1000000)

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
    uint16_t            lag_id;             /*!< LAG ID of the port */
    struct shash        cfg_member_ifs;     /*!< configured member interfaces */
    struct shash        eligible_member_ifs;/*!< Interfaces eligible to form a LAG */
    enum ovsrec_port_lacp_e lacp_mode;      /*!< port's LACP mode */
    unsigned int        lag_member_speed;   /*!< link speed of LAG members */
};

/* NOTE: These LAG IDs are only used for LACP state machine.
 *       They are not necessarily the same as h/w LAG ID. */
#define LAG_ID_IN_USE   1
#define VALID_LAG_ID(x) ((x)>=min_lag_id && (x)<=max_lag_id)

const uint16_t min_lag_id = 1;
uint16_t max_lag_id = 0;
uint16_t *lag_id_pool = NULL;

/* To serialize updates to OVSDB.  Both LACP Cyclone and OVS
 * interface threads calls to update OVSDB states. */
pthread_mutex_t ovsdb_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Macros to lock and unlock mutexes in a verbose manner. */
#define OVSDB_LOCK { \
                VLOG_DBG("%s(%d): OVSDB_LOCK: taking lock...", __FUNCTION__, __LINE__); \
                pthread_mutex_lock(&ovsdb_mutex); \
}

#define OVSDB_UNLOCK { \
                VLOG_DBG("%s(%d): OVSDB_UNLOCK: releasing lock...", __FUNCTION__, __LINE__); \
                pthread_mutex_unlock(&ovsdb_mutex); \
}


static int update_interface_lag_eligibility(struct iface_data *idp);
static int update_interface_hw_bond_config_map_entry(struct iface_data *idp,
                                                     const char *key, const char *value);

/**********************************************************************/
/*                               UTILS                                */
/**********************************************************************/
static void
init_lag_id_pool(uint16_t count)
{
    if (lag_id_pool == NULL) {
        /* Track how many we're allocating. */
        max_lag_id = count;

        /* Allocate an extra one to skip LAG ID 0. */
        lag_id_pool = (uint16_t *)xcalloc(count+1, sizeof(uint16_t));
        VLOG_DBG("lacpd: allocated %d LAG IDs", count);
    }
} /* init_lag_id_pool */

static uint16_t
alloc_lag_id(void)
{
    if (lag_id_pool != NULL) {
        uint16_t id;

        for (id=min_lag_id; id<=max_lag_id; id++) {

            if (lag_id_pool[id] == LAG_ID_IN_USE) {
                continue;
            }

            /* Found an available LAG_ID. */
            lag_id_pool[id] = LAG_ID_IN_USE;
            return id;
        }
    } else {
        VLOG_ERR("LAG ID pool not initialized!");
    }

    /* No free LAG ID available if we get here. */
    return 0;

} /* alloc_lag_id */

#if 0
/* HALON_TODO */
static void
free_lag_id(uint16_t id)
{
    if ((lag_id_pool == NULL) && VALID_LAG_ID(id)) {
        if (lag_id_pool[id] == LAG_ID_IN_USE) {
            lag_id_pool[id] = 0;
        } else {
            VLOG_ERR("Trying to free an unused LAGID (%d)!", id);
        }
    } else {
        if (lag_id_pool == NULL) {
            VLOG_ERR("Attempt to free LAG ID when"
                     "pool is not initialized!");
        } else {
            VLOG_ERR("Attempt to free invalid LAG ID %d!", id);
        }
    }

} /* free_lag_id */
#endif

struct iface_data *
find_iface_data_by_index(int index)
{
    struct iface_data *idp;
    struct shash_node *sh_node;

    SHASH_FOR_EACH(sh_node, &all_interfaces) {
        idp = sh_node->data;
        if (idp) {
            if (idp->index == index) {
                return idp;
            }
        }
    }

    return NULL;
} /* find_iface_data_by_index */

const char *
find_ifname_by_index(int index)
{
    struct iface_data *idp;

    idp = find_iface_data_by_index(index);
    if (idp) {
        return idp->name;
    } else {
        return "";
    }

} /* find_ifname_by_index */

/**********************************************************************/
/*              Configuration Message Sending Utilities               */
/**********************************************************************/
static void *
alloc_msg(int size)
{
    void *msg;

    msg = xzalloc(size);

    if (msg == NULL) {
        VLOG_ERR("%s: malloc failed.",__FUNCTION__);
    }

    return msg;
} /* alloc_msg */

#if 0
/* HALON_TODO: OVS defines system_priority as part of Port table,
 * under other_config:lacp_system_priority.  Port table also has the
 * system ID (MAC) address under other_config:lacp_system_id.  These
 * two attributes should not be configurable on a per-Port (LAG) basis.
 * System ID & priority should be defined at the system level, e.g.
 * under Open_vSwitch, Subsystem, or Bridge table. TBD. */
static void
send_sys_pri_msg(int priority)
{
    ML_event *event;
    struct MLt_lacp_api__actorSysPriority *msg;
    int msgSize;

    VLOG_DBG("%s: priority=%d", __FUNCTION__, priority);

    msgSize = sizeof(ML_event)+sizeof(struct MLt_lacp_api__actorSysPriority);

    event = (ML_event*)alloc_msg(msgSize);

    if (event != NULL) {
        /*** From CfgMgr peer. ***/
        event->sender.peer = ml_cfgMgr_index;
        event->msgnum = MLm_lacp_api__setActorSysPriority;

        msg = (struct MLt_lacp_api__actorSysPriority *)(event+1);
        msg->actor_system_priority = priority;

        ml_send_event(event);
    }
} /* send_sys_pri_msg */
#endif

static void
send_sys_mac_msg(struct ether_addr *macAddr)
{
    ML_event *event;
    struct MLt_lacp_api__actorSysMac *macMsg;
    int msgSize;

    VLOG_DBG("%s: entry", __FUNCTION__);

    msgSize = sizeof(ML_event) + sizeof(struct MLt_lacp_api__actorSysMac);

    event = (ML_event *)alloc_msg(msgSize);

    if (event != NULL) {
        /*** From CfgMgr peer. ***/
        event->sender.peer = ml_cfgMgr_index;
        event->msgnum = MLm_lacp_api__setActorSysMac;

        macMsg = (struct MLt_lacp_api__actorSysMac *)(event+1);

        /* Copy MAC address. */
        memcpy(macMsg->actor_sys_mac, macAddr, ETH_ALEN);

        ml_send_event(event);
    }
} /* send_sys_mac_msg */

static void
send_lag_create_msg(int lag_id)
{
    ML_event *event;
    struct MLt_vpm_api__create_sport *msg;
    int msgSize;

    VLOG_DBG("%s: lag_id=%d", __FUNCTION__, lag_id);

    msgSize = sizeof(ML_event) + sizeof(struct MLt_vpm_api__create_sport);

    event = (ML_event *)alloc_msg(msgSize);

    if (event != NULL) {
        /*** From CfgMgr peer. ***/
        event->sender.peer = ml_cfgMgr_index;
        event->msgnum = MLm_vpm_api__create_sport;

        msg = (struct MLt_vpm_api__create_sport *)(event+1);
        msg->handle = PM_LAG2HANDLE(lag_id);
        msg->type = STYPE_802_3AD;

        ml_send_event(event);
    }
} /* send_lag_create_msg */

#if 0
/* HALON_TODO */
static void
send_lag_delete_msg(int lag_id)
{
    ML_event *event;
    struct MLt_vpm_api__delete_sport *msg;
    int msgSize;

    VLOG_DBG("%s: lag_id=%d", __FUNCTION__, lag_id);

    msgSize = sizeof(ML_event) + sizeof(struct MLt_vpm_api__delete_sport);

    event = (ML_event *)alloc_msg(msgSize);

    if (event != NULL) {
        /*** From CfgMgr peer. ***/
        event->sender.peer = ml_cfgMgr_index;
        event->msgnum = MLm_vpm_api__delete_sport;

        msg = (struct MLt_vpm_api__delete_sport *)(event+1);
        msg->handle = PM_LAG2HANDLE(lag_id);

        ml_send_event(event);
    }
} /* send_lag_delete_msg */
#endif

static void
send_config_lag_msg(int lag_id, int actorKey, int cycl_ptype)
{
    ML_event *event;
    struct MLt_vpm_api__lacp_sport_params *msg;
    int msgSize;

    VLOG_DBG("%s: lag_id=%d, actor_key=%d, cycl_ptype=%d",
             __FUNCTION__, lag_id, actorKey, cycl_ptype);

    msgSize = sizeof(ML_event) + sizeof(struct MLt_vpm_api__lacp_sport_params);

    event = (ML_event *)alloc_msg(msgSize);

    if (event != NULL) {
        /*** From CfgMgr peer. (This is for the VPM side) ***/
        event->sender.peer = ml_cfgMgr_index;
        event->msgnum = MLm_vpm_api__set_lacp_sport_params;

        msg = (struct MLt_vpm_api__lacp_sport_params *)(event+1);
        msg->sport_handle = PM_LAG2HANDLE(lag_id);
        msg->flags = (LACP_LAG_PORT_TYPE_FIELD_PRESENT |
                      LACP_LAG_ACTOR_KEY_FIELD_PRESENT);

        msg->port_type = cycl_ptype;
        msg->actor_key = actorKey;

        ml_send_event(event);
    }
} /* send_config_lag_msg */

static void
send_config_lport_msg(struct iface_data *info_ptr)
{
    ML_event *event;
    struct MLt_vpm_api__lport_lacp_change *msg;
    int msgSize;

    VLOG_DBG("%s: port=%s, index=%d", __FUNCTION__,
             info_ptr->name, info_ptr->index);

    msgSize = sizeof(ML_event) + sizeof(struct MLt_vpm_api__lport_lacp_change);

    event = (ML_event *)alloc_msg(msgSize);

    if (event != NULL) {
        /*** From LPORT peer. ***/
        event->sender.peer = ml_lport_index;
        event->msgnum = MLm_vpm_api__set_lacp_lport_params_event;

        msg = (struct MLt_vpm_api__lport_lacp_change *)(event+1);
        msg->lport_handle = PM_SMPT2HANDLE(0,0,info_ptr->index,
                                           info_ptr->cycl_port_type);
        msg->link_state = (info_ptr->link_state == INTERFACE_LINK_STATE_UP) ? 1 : 0;
        msg->link_speed = INTF_TO_LACP_LINK_SPEED(info_ptr->link_speed);

        /* NOTE: 802.3ad requires port number to be non-zero.  So we'll
         *       just use 1-based port number, instead of 0-based.
         * (ANVL LACP Conformance Test numbers 4.11)
         */
        msg->port_id          = (info_ptr->index+1);
        msg->port_key         = info_ptr->actor_key;
        msg->port_priority    = info_ptr->actor_priority;
        msg->lacp_state       = info_ptr->lacp_state;
        msg->lacp_aggregation = info_ptr->aggregateable;
        msg->lacp_activity    = info_ptr->activity_mode;
        msg->lacp_timeout     = info_ptr->timeout_mode;
        msg->collecting_ready = info_ptr->collecting_ready;

        msg->flags = (LACP_LPORT_PORT_KEY_PRESENT |
                      LACP_LPORT_PORT_PRIORITY_PRESENT |
                      LACP_LPORT_ACTIVITY_FIELD_PRESENT |
                      LACP_LPORT_TIMEOUT_FIELD_PRESENT |
                      LACP_LPORT_AGGREGATION_FIELD_PRESENT |
                      LACP_LPORT_HW_COLL_STATUS_PRESENT);

        ml_send_event(event);
    }
} /* send_config_lport_msg */

#if 0
/* HALON_TODO */
static void
send_lport_lacp_change_msg(struct iface_data *info_ptr, unsigned int flags)
{
    ML_event *event;
    struct MLt_vpm_api__lport_lacp_change *msg;
    int msgSize;

    VLOG_DBG("%s: port=%s, index=%d, flags=0x%x", __FUNCTION__,
             info_ptr->name, info_ptr->index, flags);

    msgSize = sizeof(ML_event)+sizeof(struct MLt_vpm_api__lport_lacp_change);

    event = (ML_event*)alloc_msg(msgSize);

    if (event != NULL) {
        /*** From LPORT peer. ***/
        event->sender.peer = ml_lport_index;
        event->msgnum = MLm_vpm_api__set_lacp_lport_params_event;

        msg = (struct MLt_vpm_api__lport_lacp_change *)(event+1);
        msg->lport_handle = PM_SMPT2HANDLE(0,0,info_ptr->index,
                                           info_ptr->cycl_port_type);

        /* NOTE: 802.3ad requires port number to be non-zero.  So we'll
         *       just use 1-based port number, instead of 0-based.
         * (ANVL LACP Conformance Test numbers 4.11)
         */
        msg->port_id          = (info_ptr->index+1);
        msg->lacp_state       = info_ptr->lacp_state;
        msg->lacp_timeout     = info_ptr->timeout_mode;
        msg->collecting_ready = info_ptr->collecting_ready;

        msg->flags        = (flags | LACP_LPORT_DYNAMIC_FIELDS_PRESENT);

        ml_send_event(event);
    }
} /* send_lport_lacp_change_msg */
#endif

#if 0
/* HALON_TODO */
static void
send_link_state_change_msg(struct iface_data *info_ptr, int state, int speed)
{
    ML_event *event;
    struct MLt_vpm_api__lport_state_change *msg;
    int msgSize;

    VLOG_DBG("%s: port=%s, state=%d, speed=%d", __FUNCTION__,
             info_ptr->name, state, speed);

    msgSize = sizeof(ML_event)+sizeof(struct MLt_vpm_api__lport_state_change);

    event = (ML_event*)alloc_msg(msgSize);

    if (event != NULL) {
        /*** From LPORT peer. ***/
        event->sender.peer = ml_lport_index;
        event->msgnum = (state ? MLm_vpm_api__lport_state_up :
                                 MLm_vpm_api__lport_state_down);

        msg = (struct MLt_vpm_api__lport_state_change *)(event+1);
        msg->lport_handle = PM_SMPT2HANDLE(0, 0, info_ptr->index,
                                           info_ptr->cycl_port_type);
        msg->link_speed = speed;

        ml_send_event(event);
    }
} /* send_link_state_change_msg */
#endif

static void
configure_lacp_on_interface(struct port_data *portp, struct iface_data *idp)
{
    VLOG_DBG("%s: lag_id=%d, i/f=%s", __FUNCTION__, portp->lag_id, idp->name);

    idp->cfg_lag_id = portp->lag_id;
    idp->lacp_state = (portp->lacp_mode == PORT_LACP_OFF ?
                       LACP_STATE_DISABLED : LACP_STATE_ENABLED);

#if 0
    idp->cycl_port_type = speed_to_lport_type(portp->port_max_speed);
    idp->actor_priority = portp->actor_priority;
    idp->actor_key = portp->actor_key;
    idp->activity_mode = lag_entry->activity_mode;
    idp->timeout_mode = lag_entry->timeout_mode;
#else
    /* HALON_TODO: temporary hard-code. */
    idp->cycl_port_type = PM_LPORT_10GIGE;
    idp->actor_priority = 1;
    idp->actor_key = portp->lag_id;
    idp->aggregateable = AGGREGATABLE;
    idp->activity_mode = LACP_ACTIVE_MODE;
    idp->timeout_mode = LONG_TIMEOUT;
    idp->collecting_ready = 1;
#endif
} /* configure_lacp_on_interface */

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

    /* Cache Subsystem table. */
    ovsdb_idl_add_table(idl, &ovsrec_table_subsystem);
    ovsdb_idl_add_column(idl, &ovsrec_subsystem_col_other_info);

    /* Cache Port table and columns. */
    ovsdb_idl_add_table(idl, &ovsrec_table_port);
    ovsdb_idl_add_column(idl, &ovsrec_port_col_name);
    ovsdb_idl_add_column(idl, &ovsrec_port_col_lacp);
    ovsdb_idl_add_column(idl, &ovsrec_port_col_interfaces);

    /* Cache Inteface table and columns. */
    ovsdb_idl_add_table(idl, &ovsrec_table_interface);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_name);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_duplex);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_link_state);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_link_speed);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_hw_bond_config);
    ovsdb_idl_omit_alert(idl, &ovsrec_interface_col_hw_bond_config);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_hw_intf_info);

    /* Initialize LAG ID pool. */
    /* HALON_TODO: read # of LAGs from somewhere? */
    init_lag_id_pool(128);

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
        const char *hw_id = smap_get(&(ifrow->hw_intf_info),
                                     INTERFACE_HW_INTF_INFO_MAP_SWITCH_INTF_ID);

        idp->name = xstrdup(ifrow->name);

        /* Allocate interface index. */
        /* -- use hw_intf_info:switch_intf_id for now.
         * -- may be overridden with OVS's other_config:lacp-port-id. */
        idp->index = hw_id ? atoi(hw_id) : -1;
        if (idp->index <= 0) {
            VLOG_ERR("Invalid interface index, hw_id=%p, id=%d", hw_id, idp->index);
        }

        idp->link_state = INTERFACE_LINK_STATE_DOWN;
        if (ifrow->link_state) {
            if (!strcmp(ifrow->link_state, OVSREC_INTERFACE_LINK_STATE_UP)) {
                idp->link_state = INTERFACE_LINK_STATE_UP;
            }
        }

        idp->duplex = INTERFACE_DUPLEX_HALF;
        if (ifrow->duplex) {
            if (!strcmp(ifrow->duplex, OVSREC_INTERFACE_DUPLEX_FULL)) {
                idp->duplex = INTERFACE_DUPLEX_FULL;
            }
        }

        idp->link_speed = 0;
        if (ifrow->n_link_speed > 0) {
            /* There should only be one speed. */
            idp->link_speed = ifrow->link_speed[0];
        }

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

        /* Check for changes to row. */
        if (OVSREC_IDL_IS_ROW_INSERTED(ifrow, idl_seqno) ||
            OVSREC_IDL_IS_ROW_MODIFIED(ifrow, idl_seqno)) {
            unsigned int new_speed;
            enum ovsrec_interface_duplex_e new_duplex;
            enum ovsrec_interface_link_state_e new_link_state;

            new_link_state = INTERFACE_LINK_STATE_DOWN;
            if (ifrow->link_state) {
                if (!strcmp(ifrow->link_state, OVSREC_INTERFACE_LINK_STATE_UP)) {
                    new_link_state = INTERFACE_LINK_STATE_UP;
                }
            }

            /* Although speed & duplex should only change if link state
               has changed, the IDL change notices may not all come at
               the same time! */
            new_speed = 0;
            if (ifrow->n_link_speed > 0) {
                /* There should only be one speed. */
                new_speed = ifrow->link_speed[0];
            }

            new_duplex = INTERFACE_DUPLEX_HALF;
            if (ifrow->duplex) {
                if (!strcmp(ifrow->duplex, OVSREC_INTERFACE_DUPLEX_FULL)) {
                    new_duplex = INTERFACE_DUPLEX_FULL;
                }
            }

            if ((new_link_state != idp->link_state) ||
                (new_speed != idp->link_speed) ||
                (new_duplex != idp->duplex)) {

                idp->link_state = new_link_state;
                idp->link_speed = new_speed;
                idp->duplex = new_duplex;

                VLOG_DBG("Interface %s link state changed in DB: "
                         "new_speed=%d, new_link=%s, new_duplex=%s, ",
                         ifrow->name, idp->link_speed,
                         (idp->link_state == INTERFACE_LINK_STATE_UP ? "up" : "down"),
                         (idp->duplex == INTERFACE_DUPLEX_FULL ? "full" : "half"));

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
 * NOTE: ovsdb_mutex must be taken prior to calling this function.
 *
 * @param idp  iface_data pointer to the interface entry.
 * @param entry_key name of the key.
 * @param entry_value value for the key.
 *
 * @return 1 to indicate database transaction needs to be committed.
 */
static int
update_interface_hw_bond_config_map_entry(struct iface_data *idp,
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
set_interface_lag_eligibility(struct port_data *portp, struct iface_data *idp,
                              bool eligible)
{
    if (eligible == idp->lag_eligible) {
        return;
    }

    if (portp->lacp_mode == PORT_LACP_OFF) {
        /* Static LAG configuration in hardware. */
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
        /* NOTE: For Ports in dynamic LACP mode, eligible interfaces
         * mean the LACP is run on the interfaces.  LACP state machine
         * will then decide if the interfaces' RX/TX should be enabled.
         */
        configure_lacp_on_interface(portp, idp);

        /* Override lacp_state based on eligibility. */
        idp->lacp_state = (eligible? LACP_STATE_ENABLED :
                                     LACP_STATE_DISABLED);
        send_config_lport_msg(idp);
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

        /* LACP mode changed.  In either case, mark all existing interfaces
         * as ineligible to detach them first.  Then the interfaces will be
         * reconfigured based on the new LACP mode. */
        SHASH_FOR_EACH_SAFE(node, next, &portp->eligible_member_ifs) {
            struct iface_data *idp =
                shash_find_data(&all_interfaces, node->name);
            set_interface_lag_eligibility(portp, idp, false);
        }

        if (!LACP_ENABLED_ON_PORT(portp->lacp_mode)) {
            /* LACP was not on (static LAG).  Need to turn on LACP. */

            /* Create super port in LACP state machine. */
            if (!portp->lag_id) {
                portp->lag_id = alloc_lag_id();
            }

            if (portp->lag_id) {
                /* Send LAG creation information. */
                send_lag_create_msg(portp->lag_id);

                /* Send LAG configuration information
                 * NOTE: Halon now controls actor_key and port type
                 *       parameters, so we'll simply initialize them
                 *       with default values.
                 * HALON_TODO: Allow the user to configure actor_key.
                 */
                send_config_lag_msg(portp->lag_id,
                                    portp->lag_id, /* Just use LAGID as actor key...*/
                                    PM_LPORT_INVALID);
            } else {
                VLOG_ERR("Failed to allocate LAGID for port %s!", portp->name);
            }
        }

        /* Save new LACP mode. */
        portp->lacp_mode = lacp_mode;
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
    portp = xzalloc(sizeof *portp);

    if (!shash_add_once(&all_ports, port_row->name, portp)) {
        VLOG_WARN("Port %s specified twice", port_row->name);
        free(portp);
    } else {
        size_t i;

        portp->lag_id = 0;
        portp->name = xstrdup(port_row->name);
        portp->lacp_mode = PORT_LACP_OFF;
        portp->lag_member_speed = 0;
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
    if (ovs_vsw && ovs_vsw->cur_cfg > (int64_t)0) {
        const char *sys_mac;
        const struct ovsrec_subsystem *ovs_subsys = NULL;

        /* HALON_TODO: temprarily using base_mac_address, which is
         * used for interface MACs.  We really need to reserve one
         * dedicated to the entire switching system. */
        /* HALON_TODO: handle multiple subsystems. */
        ovs_subsys = ovsrec_subsystem_first(idl);

        if (ovs_subsys) {
            sys_mac = smap_get(&ovs_subsys->other_info, "base_mac_address");
            if (sys_mac) {
                struct ether_addr *eth_addr;
                eth_addr = ether_aton(sys_mac);
                send_sys_mac_msg(eth_addr);

                system_configured = true;
                VLOG_INFO("System is now configured (cur_cfg=%d, sys_mac=%s).",
                          (int)ovs_vsw->cur_cfg, sys_mac);
            }
        }
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

    OVSDB_LOCK;

    /* Process a batch of messages from OVSDB. */
    ovsdb_idl_run(idl);

    if (ovsdb_idl_is_lock_contended(idl)) {
        static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 1);
        VLOG_ERR_RL(&rl, "Another lacpd process is running, "
                    "disabling this process until it goes away");
        OVSDB_UNLOCK;
        return;
    } else if (!ovsdb_idl_has_lock(idl)) {
        OVSDB_UNLOCK;
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

    OVSDB_UNLOCK;

    return;
} /* lacpd_run */

void
lacpd_wait(void)
{
    ovsdb_idl_wait(idl);
} /* lacpd_wait */

static void
lacpd_thread_intf_update_hw_bond_config(struct iface_data *idp,
                                        bool update_rx, bool rx_enabled,
                                        bool update_tx, bool tx_enabled)
{
    struct ovsdb_idl_txn *txn;

    OVSDB_LOCK;
    txn = ovsdb_idl_txn_create(idl);
    if (update_rx) {
        update_interface_hw_bond_config_map_entry(
            idp,
            INTERFACE_HW_BOND_CONFIG_MAP_RX_ENABLED,
            (rx_enabled ?
             INTERFACE_HW_BOND_CONFIG_MAP_ENABLED_TRUE :
             INTERFACE_HW_BOND_CONFIG_MAP_ENABLED_FALSE));
    }
    if (update_tx) {
        update_interface_hw_bond_config_map_entry(
            idp,
            INTERFACE_HW_BOND_CONFIG_MAP_TX_ENABLED,
            (tx_enabled ?
             INTERFACE_HW_BOND_CONFIG_MAP_ENABLED_TRUE :
             INTERFACE_HW_BOND_CONFIG_MAP_ENABLED_FALSE));
    }
    ovsdb_idl_txn_commit_block(txn);
    ovsdb_idl_txn_destroy(txn);
    OVSDB_UNLOCK;

} /* halon_intf_update_hw_bond_config */

void
halon_attach_port_in_hw(uint16_t lag_id, int port)
{
    struct iface_data *idp = NULL;

    idp = find_iface_data_by_index(port);
    if (idp) {
        /* Attaching port means just RX. */
        lacpd_thread_intf_update_hw_bond_config(idp,
                                                true,   /* update_rx */
                                                true,   /* rx_enabled */
                                                false,  /* update_tx */
                                                false); /* tx_enabled */
    } else {
        VLOG_ERR("Failed to find interface data for attaching port in hw. "
                 "port index=%d", port);
    }
} /* halon_attach_port_in_hw */

void
halon_detach_port_in_hw(uint16_t lag_id, int port)
{
    struct iface_data *idp = NULL;

    idp = find_iface_data_by_index(port);
    if (idp) {
        /* Detaching port means both RX/TX are disabled. */
        lacpd_thread_intf_update_hw_bond_config(idp,
                                                true,   /* update_rx */
                                                false,  /* rx_enabled */
                                                true,   /* update_tx */
                                                false); /* tx_enabled */
    } else {
        VLOG_ERR("Failed to find interface data for attaching port in hw. "
                 "port index=%d", port);
    }

} /* halon_detach_port_in_hw */

void
db_delete_lag_port(uint16_t lag_id, int port)
{
    VLOG_DBG("%s: HALON_TODO lag_id=%d, port=%d", __FUNCTION__, lag_id, port);
} /* db_delete_lag_port */

void
halon_trunk_port_egr_enable(uint16_t lag_id, int port)
{
    struct iface_data *idp = NULL;

    idp = find_iface_data_by_index(port);
    if (idp) {
        /* Egress enable means TX. */
        lacpd_thread_intf_update_hw_bond_config(idp,
                                                false, /* update_rx */
                                                false, /* rx_enabled */
                                                true,  /* update_tx */
                                                true); /* tx_enabled */
    } else {
        VLOG_ERR("Failed to find interface data for egress enable. "
                 "port index=%d", port);
    }
} /* halon_trunk_port_egr_enable */

void
db_add_lag_port(uint16_t lag_id, int port)
{
    VLOG_DBG("%s: HALON_TODO lag_id=%d, port=%d", __FUNCTION__, lag_id, port);
} /* db_add_lag_port */

void
db_clear_lag_partner_info(uint16_t lag_id)
{
    VLOG_DBG("%s: HALON_TODO lag_id %d", __FUNCTION__, lag_id);
} /* db_clear_lag_partner_info */

void
db_update_lag_partner_info(uint16_t lag_id,
                           lacp_sport_params_t *param __attribute__((unused)))
{
    VLOG_DBG("%s: HALON_TODO lag_id %d", __FUNCTION__, lag_id);
} /* db_update_lag_partner_info */

/**********************************************************************/
/*                               DEBUG                                */
/**********************************************************************/
#define MEGA_BITS_PER_SEC  1000000

static void
lacpd_interface_dump(struct ds *ds, struct iface_data *idp)
{
    ds_put_format(ds, "Interface %s:\n", idp->name);
    ds_put_format(ds, "    link_state           : %s\n",
                  idp->link_state == INTERFACE_LINK_STATE_UP
                  ? OVSREC_INTERFACE_LINK_STATE_UP :
                  OVSREC_INTERFACE_LINK_STATE_DOWN);
    /* Convert OVS's bps to Mbps to make it easier to read. */
    ds_put_format(ds, "    link_speed           : %d Mbps\n",
                  (idp->link_speed/MEGA_BITS_PER_SEC));
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

/**********************************************************************/
/*                        OVS Main Thread                             */
/**********************************************************************/

/**
 * Cleanup function at daemon shutdown time.
 */
static void
lacpd_exit(void)
{
    lacpd_ovsdb_if_exit();
    VLOG_INFO("lacpd OVSDB thread exiting...");
} /* lacpd_exit */

/**
 * @details
 * lacpd daemon's main OVS interface function.  Repeat loop that
 * calls run, wait, poll_block, etc. functions for lacpd.
 *
 * @param arg pointer to ovs-appctl server struct.
 */
void *
lacpd_ovs_main_thread(void *arg)
{
    struct unixctl_server *appctl;

    /* Detach thread to avoid memory leak upon exit. */
    pthread_detach(pthread_self());

    appctl = (struct unixctl_server *)arg;

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

    /* HALON_TODO -- need to tell main loop to exit... */

    return NULL;

} /* lacpd_ovs_main_thread */

/**@} end of lacpd group */
