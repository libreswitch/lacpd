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

#include <lacp_cmn.h>
#include <mlacp_debug.h>
#include <lacp_fsm.h>

#include <ops-utils.h>
#include <eventlog.h>

#include "lacp_ops_if.h"
#include "lacp.h"
#include "lacp_support.h"
#include "mlacp_fproto.h"
#include "mvlan_sport.h"

#include <unixctl.h>
#include <dynamic-string.h>
#include <openswitch-idl.h>
#include <openswitch-dflt.h>
#include <openvswitch/vlog.h>
#include <poll-loop.h>
#include <hash.h>
#include <shash.h>

VLOG_DEFINE_THIS_MODULE(lacpd_ovsdb_if);

/**********************************//**
 * @ingroup lacpd_ovsdb_if
 * @{
 **************************************/

/*********************************
 *
 * Pool definitions
 *
 *********************************/
#define BITS_PER_BYTE           8
#define MAX_ENTRIES_IN_POOL     256
/* Wait up to 3 seconds when calling poll_block. */
#define LACP_POLL_INTERVAL      3000

#define IS_AVAILABLE(a, idx)  ((a[idx/BITS_PER_BYTE] & (1 << (idx % BITS_PER_BYTE))) == 0)

#define CLEAR(a, idx)   a[idx/BITS_PER_BYTE] &= ~(1 << (idx % BITS_PER_BYTE))
#define SET(a, idx)     a[idx/BITS_PER_BYTE] |= (1 << (idx % BITS_PER_BYTE))

#define POOL(name, size)     unsigned char name[size/BITS_PER_BYTE+1]

int allocate_next(unsigned char *pool, int size);
static void free_index(unsigned char *pool, int idx);

POOL(port_index, MAX_ENTRIES_IN_POOL);

/*********************************************************/

#define LACP_ENABLED_ON_PORT(lpm)    (((lpm) == PORT_LACP_PASSIVE) || \
                                      ((lpm) == PORT_LACP_ACTIVE))

#define IS_VALID_PORT_ID(id)        ((id) >= MIN_INTERFACE_OTHER_CONFIG_LACP_PORT_ID && \
                                     (id) <= MAX_INTERFACE_OTHER_CONFIG_LACP_PORT_ID)

#define IS_VALID_ACTOR_PRI(p)      (((p) >= MIN_INTERFACE_OTHER_CONFIG_LACP_PORT_PRIORITY) && \
                                    ((p) <= MAX_INTERFACE_OTHER_CONFIG_LACP_PORT_PRIORITY))

#define IS_VALID_AGGR_KEY(p)       (((p) >= MIN_INTERFACE_OTHER_CONFIG_LACP_AGGREGATION_KEY) && \
                                    ((p) <= MAX_INTERFACE_OTHER_CONFIG_LACP_AGGREGATION_KEY))

#define IS_VALID_SYS_PRIO(p)       (((p) >= MIN_SYSTEM_LACP_CONFIG_SYSTEM_PRIORITY) && \
                                    ((p) <= MAX_SYSTEM_LACP_CONFIG_SYSTEM_PRIORITY))

/* Scale OVS interface speed number (bps) down to
 * that used by LACP state machine (Mbps). */
#define MEGA_BITS_PER_SEC  1000000
#define INTF_TO_LACP_LINK_SPEED(s)    ((s)/MEGA_BITS_PER_SEC)

struct ovsdb_idl *idl;           /*!< Session handle for OVSDB IDL session. */
static unsigned int idl_seqno;
static int system_configured = false;
static char system_id[OPS_MAC_STR_SIZE] = {0};
static int system_priority = DFLT_SYSTEM_LACP_CONFIG_SYSTEM_PRIORITY;
static int prev_sys_prio = DFLT_SYSTEM_LACP_CONFIG_SYSTEM_PRIORITY;

/**
 * A hash map of daemon's internal data for all the interfaces maintained by
 * lacpd.
 */
static struct shash all_interfaces = SHASH_INITIALIZER(&all_interfaces);

/**
 * A hash map of daemon's internal data for the interfaces recently added to some port.
 * The idea of this hash is to prevent completely deleting an interface that was previously
 * added to another port.
 * This scenario can occur when an interface is moved from one LAG to another.
 */
static struct shash interfaces_recently_added = SHASH_INITIALIZER(&interfaces_recently_added);

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
    struct shash        cfg_member_ifs;     /*!< Configured member interfaces */
    struct shash        eligible_member_ifs;/*!< Interfaces eligible to form a LAG */
    struct shash        participant_ifs;    /*!< Interfaces currently in LAG */
    enum ovsrec_port_lacp_e lacp_mode;      /*!< port's LACP mode */
    unsigned int        lag_member_speed;   /*!< link speed of LAG members */
    const struct ovsrec_port *cfg;          /*!< Port's idl entry */
    char                *speed_str;         /*!< Most recent speed value */

    int                 current_status;     /*!< Currently recorded status of LAG */
    int                 timeout_mode;       /*!< 0=long, 1=short */
    int                 sys_prio;           /*!< Port override for system priority */
    char                *sys_id;            /*!< Port override for system mac */
    bool                fallback_enabled ;  /*!< Default = false*/
};

/* current_status values */
#define STATUS_UNINITIALIZED    0
#define STATUS_DOWN             1
#define STATUS_UP               2
#define STATUS_DEFAULTED        3
#define STATUS_LACP_DISABLED    4

/* NOTE: These LAG IDs are only used for LACP state machine.
 *       They are not necessarily the same as h/w LAG ID. */
#define LAG_ID_IN_USE   1
#define VALID_LAG_ID(x) ((x)>=min_lag_id && (x)<=max_lag_id)

const uint16_t min_lag_id = 1;
uint16_t max_lag_id = 0; // This will be set in init_lag_id_pool
uint16_t *lag_id_pool = NULL;

/* To serialize updates to OVSDB.  Both LACP and OVS
 * interface threads calls to update OVSDB states. */
pthread_mutex_t ovsdb_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Macros to lock and unlock mutexes in a verbose manner. */
#define OVSDB_LOCK { \
                VLOG_DBG("%s(%d): OVSDB_LOCK: taking lock...", __FUNCTION__, __LINE__); \
                if (!(pthread_mutex_lock(&ovsdb_mutex))) { \
                    VLOG_WARN("%s(%d): failed to  take OVSDB_LOCK lock...",\
                              __FUNCTION__, __LINE__); \
                } \
}

#define OVSDB_UNLOCK { \
                VLOG_DBG("%s(%d): OVSDB_UNLOCK: releasing lock...", __FUNCTION__, __LINE__); \
                if (!(pthread_mutex_unlock(&ovsdb_mutex))) { \
                    VLOG_WARN("%s(%d): failed to  release OVSDB_LOCK lock...",\
                              __FUNCTION__, __LINE__); \
                } \
}

static int update_interface_lag_eligibility(struct iface_data *idp);
static int update_interface_hw_bond_config_map_entry(struct iface_data *idp,
                                                     const char *key, const char *value);
static void update_member_interface_bond_status(struct port_data *portp);
static void update_interface_bond_status_map_entry(struct iface_data *idp);
static void update_port_bond_status_map_entry(struct port_data *portp);

static char *lacp_mode_str(enum ovsrec_port_lacp_e mode);
static void db_clear_interface(struct iface_data *idp);
static void db_update_port_status(struct port_data *portp);
void db_clear_lag_partner_info_port(struct port_data *portp);

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

static void
free_lag_id(uint16_t id)
{
    if ((lag_id_pool != NULL) && VALID_LAG_ID(id)) {
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

struct port_data *
find_port_data_by_lag_id(int lag_id)
{
    struct port_data *pdp;
    struct shash_node *sh_node;
    SHASH_FOR_EACH(sh_node, &all_ports) {
        pdp = sh_node->data;
        if (pdp) {
            if (pdp->lag_id == lag_id) {
                return pdp;
            }
        }
    }

    return NULL;
} /* find_port_data_by_lag_id */

static int
valid_lacp_timeout(const char *cp)
{
   if (cp) {
      if (!*cp || strcmp(cp, PORT_OTHER_CONFIG_LACP_TIME_SLOW) == 0) {
         return(LONG_TIMEOUT);
      } else if (strcmp(cp, PORT_OTHER_CONFIG_LACP_TIME_FAST) == 0) {
         return(SHORT_TIMEOUT);
      } else {
         return(-1);
      }
   }
   return(LONG_TIMEOUT);
} /* valid_lacp_timeout */

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

static void
set_port_overrides(struct port_data *portp, struct iface_data *idp)
{
    ML_event *event;
    struct MLt_lacp_api__set_lport_overrides *msg;
    int msgSize;

    msgSize = sizeof(ML_event)+sizeof(struct MLt_lacp_api__set_lport_overrides);

    event = (ML_event*)alloc_msg(msgSize);

    if (event != NULL) {
        event->sender.peer = ml_cfgMgr_index;
        event->msgnum = MLm_lacp_api__set_lport_overrides;

        /* Set up msg pointer to just after the event
         * structure itself. This must be done here since the
         * sender's event->msg pointer points sender's memory
         * space, and will result in fatal errors if we try to
         * access it in LACP process space.
         */
        msg = (struct MLt_lacp_api__set_lport_overrides *)(event+1);

        msg->lport_handle = PM_SMPT2HANDLE(0,0,idp->index,
                                           idp->cycl_port_type);
        msg->priority = portp->sys_prio;

        memset(msg->actor_sys_mac, 0, sizeof(msg->actor_sys_mac));

        if (portp->sys_id) {
            struct ether_addr *eth_addr_p;
            struct ether_addr eth_addr;

            eth_addr_p = ether_aton_r(portp->sys_id, &eth_addr);

            if (eth_addr_p != NULL) {
                memcpy(msg->actor_sys_mac, eth_addr_p, sizeof(msg->actor_sys_mac));
            }
        }

        ml_send_event(event);
    }
}

static void
clear_port_overrides(struct iface_data *idp)
{
    ML_event *event;
    struct MLt_lacp_api__set_lport_overrides *msg;
    int msgSize;

    msgSize = sizeof(ML_event)+sizeof(struct MLt_lacp_api__set_lport_overrides);

    event = (ML_event*)alloc_msg(msgSize);

    if (event != NULL) {
        event->sender.peer = ml_cfgMgr_index;
        event->msgnum = MLm_lacp_api__set_lport_overrides;

        /* Set up msg pointer to just after the event
         * structure itself. This must be done here since the
         * sender's event->msg pointer points sender's memory
         * space, and will result in fatal errors if we try to
         * access it in LACP process space.
         */
        msg = (struct MLt_lacp_api__set_lport_overrides *)(event+1);

        msg->lport_handle = PM_SMPT2HANDLE(0,0,idp->index,
                                           idp->cycl_port_type);
        msg->priority = 0;
        memset(msg->actor_sys_mac, 0, sizeof(msg->actor_sys_mac));

        ml_send_event(event);
    }
}

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

        /* Set up msg pointer to just after the event
         * structure itself. This must be done here since the
         * sender's event->msg pointer points sender's memory
         * space, and will result in fatal errors if we try to
         * access it in LACP process space.
         */
        msg = (struct MLt_lacp_api__actorSysPriority *)(event+1);
        msg->actor_system_priority = priority;

        ml_send_event(event);
    }
} /* send_sys_pri_msg */

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

        /* Set up macMsg pointer to just after the event
         * structure itself. This must be done here since the
         * sender's event->msg pointer points sender's memory
         * space, and will result in fatal errors if we try to
         * access it in LACP process space.
         */
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

        /* Set up msg pointer to just after the event
         * structure itself. This must be done here since the
         * sender's event->msg pointer points sender's memory
         * space, and will result in fatal errors if we try to
         * access it in LACP process space.
         */
        msg = (struct MLt_vpm_api__create_sport *)(event+1);
        msg->handle = PM_LAG2HANDLE(lag_id);
        msg->type = STYPE_802_3AD;

        ml_send_event(event);
    }
} /* send_lag_create_msg */

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

        /* Set up msg pointer to just after the event
         * structure itself. This must be done here since the
         * sender's event->msg pointer points sender's memory
         * space, and will result in fatal errors if we try to
         * access it in LACP process space.
         */
        msg = (struct MLt_vpm_api__delete_sport *)(event+1);
        msg->handle = PM_LAG2HANDLE(lag_id);

        ml_send_event(event);
    }
} /* send_lag_delete_msg */

static void
send_config_lag_msg(int lag_id, int actor_key, int cycl_ptype)
{
    ML_event *event;
    struct MLt_vpm_api__lacp_sport_params *msg;
    int msgSize;

    VLOG_DBG("%s: lag_id=%d, actor_key=%d, cycl_ptype=%d",
             __FUNCTION__, lag_id, actor_key, cycl_ptype);

    msgSize = sizeof(ML_event) + sizeof(struct MLt_vpm_api__lacp_sport_params);

    event = (ML_event *)alloc_msg(msgSize);

    if (event != NULL) {
        /*** From CfgMgr peer. (This is for the VPM side) ***/
        event->sender.peer = ml_cfgMgr_index;
        event->msgnum = MLm_vpm_api__set_lacp_sport_params;

        /* Set up msg pointer to just after the event
         * structure itself. This must be done here since the
         * sender's event->msg pointer points sender's memory
         * space, and will result in fatal errors if we try to
         * access it in LACP process space.
         */
        msg = (struct MLt_vpm_api__lacp_sport_params *)(event+1);
        msg->sport_handle = PM_LAG2HANDLE(lag_id);
        msg->flags = (LACP_LAG_PORT_TYPE_FIELD_PRESENT |
                      LACP_LAG_ACTOR_KEY_FIELD_PRESENT);

        msg->port_type = cycl_ptype;
        msg->actor_key = actor_key;

        ml_send_event(event);
    }
} /* send_config_lag_msg */

static void
send_unconfig_lag_msg(int lag_id)
{
    ML_event *event;
    struct MLt_vpm_api__lacp_sport_params *msg;
    int msgSize;

    VLOG_DBG("%s: lag_id=%d", __FUNCTION__, lag_id);

    msgSize = sizeof(ML_event) + sizeof(struct MLt_vpm_api__lacp_sport_params);

    event = (ML_event *)alloc_msg(msgSize);

    if (event != NULL) {
        /*** From CfgMgr peer. (This is for the VPM side) ***/
        event->sender.peer = ml_cfgMgr_index;
        event->msgnum = MLm_vpm_api__unset_lacp_sport_params;

        /* Set up msg pointer to just after the event
         * structure itself. This must be done here since the
         * sender's event->msg pointer points sender's memory
         * space, and will result in fatal errors if we try to
         * access it in LACP process space.
         */
        msg = (struct MLt_vpm_api__lacp_sport_params *)(event+1);
        msg->sport_handle = PM_LAG2HANDLE(lag_id);

        ml_send_event(event);
    }
} /* send_unconfig_lag_msg */

static void
send_config_lport_msg(struct iface_data *info_ptr)
{
    ML_event *event;
    struct MLt_vpm_api__lport_lacp_change *msg;
    int msgSize;
    struct port_data *portp;

    VLOG_DBG("%s: port=%s, hw_port=%d, index=%d", __FUNCTION__,
             info_ptr->name, info_ptr->hw_port_number, info_ptr->index);

    msgSize = sizeof(ML_event) + sizeof(struct MLt_vpm_api__lport_lacp_change);

    event = (ML_event *)alloc_msg(msgSize);

    if (event != NULL) {
        /*** From LPORT peer. ***/
        event->sender.peer = ml_lport_index;
        event->msgnum = MLm_vpm_api__set_lacp_lport_params_event;

        /* Set up msg pointer to just after the event
         * structure itself. This must be done here since the
         * sender's event->msg pointer points sender's memory
         * space, and will result in fatal errors if we try to
         * access it in LACP process space.
         */
        msg = (struct MLt_vpm_api__lport_lacp_change *)(event+1);
        msg->lport_handle = PM_SMPT2HANDLE(0,0,info_ptr->index,
                                           info_ptr->cycl_port_type);
        msg->link_state = info_ptr->link_state;  // INTERFACE_LINK_STATE_DOWN or INTERFACE_LINK_STATE_UP
        msg->link_speed = info_ptr->link_speed;

        /* NOTE: 802.3ad requires port number to be non-zero.  So we'll
         *       just use 1-based port number, instead of 0-based.
         * (ANVL LACP Conformance Test numbers 4.11)
         */
        msg->port_id          = (info_ptr->port_id == 0) ?
                                    info_ptr->index+1 : info_ptr->port_id;
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

        /* Handle per-port (not interface) overrides. */

        /*
         * These have to be handled here in case per-port override was
         * set before interface was added to port.
         */

        portp = info_ptr->port_datap;
        if (!portp) {
            VLOG_WARN("Port data is empty when trying to configure "
                      "System Priority and System ID");
        }
        else {
            if (portp->lacp_mode != PORT_LACP_OFF && portp->sys_prio != 0) {
                msg->flags |= LACP_LPORT_SYS_PRIORITY_FIELD_PRESENT;
                msg->sys_priority = portp->sys_prio;
            }

            if (portp->lacp_mode != PORT_LACP_OFF && portp->sys_id != NULL) {
                struct ether_addr *eth_addr_p;
                struct ether_addr eth_addr;

                eth_addr_p = ether_aton_r(portp->sys_id, &eth_addr);

                if (eth_addr_p != NULL) {
                    msg->flags |= LACP_LPORT_SYS_ID_FIELD_PRESENT;
                    memcpy(msg->sys_id, eth_addr_p, ETH_ALEN);
                }
            }
        }

        ml_send_event(event);
    }
} /* send_config_lport_msg */

static void
send_lport_lacp_change_msg(struct iface_data *info_ptr, unsigned int flags)
{
    ML_event *event;
    struct MLt_vpm_api__lport_lacp_change *msg;
    int msgSize;

    VLOG_DBG("%s: port=%s, hw_port=%d, index=%d, flags=0x%x", __FUNCTION__,
             info_ptr->name, info_ptr->hw_port_number, info_ptr->index, flags);

    msgSize = sizeof(ML_event)+sizeof(struct MLt_vpm_api__lport_lacp_change);

    event = (ML_event*)alloc_msg(msgSize);

    if (event != NULL) {
        /*** From LPORT peer. ***/
        event->sender.peer = ml_lport_index;
        event->msgnum = MLm_vpm_api__set_lacp_lport_params_event;

        /* Set up msg pointer to just after the event
         * structure itself. This must be done here since the
         * sender's event->msg pointer points sender's memory
         * space, and will result in fatal errors if we try to
         * access it in LACP process space.
         */
        msg = (struct MLt_vpm_api__lport_lacp_change *)(event+1);
        msg->lport_handle = PM_SMPT2HANDLE(0,0,info_ptr->index,
                                           info_ptr->cycl_port_type);

        /* NOTE: 802.3ad requires port number to be non-zero.  So we'll
         *       just use 1-based port number, instead of 0-based.
         * (ANVL LACP Conformance Test numbers 4.11)
         */
        msg->port_id          = (info_ptr->port_id == 0) ?
                                    info_ptr->index+1 : info_ptr->port_id;
        msg->lacp_state       = info_ptr->lacp_state;
        msg->lacp_timeout     = info_ptr->timeout_mode;
        msg->collecting_ready = info_ptr->collecting_ready;

        msg->flags = (flags | LACP_LPORT_DYNAMIC_FIELDS_PRESENT);

        ml_send_event(event);
    }
} /* send_lport_lacp_change_msg */

static void
send_link_state_change_msg(struct iface_data *info_ptr)
{
    ML_event *event;
    struct MLt_vpm_api__lport_state_change *msg;
    int msgSize;

    VLOG_DBG("%s: port=%s, state=%d, speed=%d", __FUNCTION__,
             info_ptr->name, info_ptr->link_state, info_ptr->link_speed);

    msgSize = sizeof(ML_event)+sizeof(struct MLt_vpm_api__lport_state_change);

    event = (ML_event*)alloc_msg(msgSize);

    if (event != NULL) {
        /*** From LPORT peer. ***/
        event->sender.peer = ml_lport_index;
        event->msgnum = ((info_ptr->link_state == INTERFACE_LINK_STATE_UP) ?
                         MLm_vpm_api__lport_state_up :
                         MLm_vpm_api__lport_state_down);

        /* Set up msg pointer to just after the event
         * structure itself. This must be done here since the
         * sender's event->msg pointer points sender's memory
         * space, and will result in fatal errors if we try to
         * access it in LACP process space.
         */
        msg = (struct MLt_vpm_api__lport_state_change *)(event+1);
        msg->lport_handle = PM_SMPT2HANDLE(0, 0, info_ptr->index,
                                           info_ptr->cycl_port_type);
        msg->link_speed = info_ptr->link_speed;

        ml_send_event(event);
    }
} /* send_link_state_change_msg */

static void
send_fallback_status_msg(struct iface_data *info_ptr, bool fallback_status)
{
    ML_event *event;
    struct MLt_vpm_api__lport_fallback_status *msg;
    int msgSize;

    VLOG_DBG("%s: interface=%s, fallback=%d",
             __FUNCTION__,
            info_ptr->name,
            fallback_status);

    msgSize = sizeof(ML_event)
              + sizeof(struct MLt_vpm_api__lport_fallback_status);

    event = (ML_event *)alloc_msg(msgSize);

    if (event != NULL) {
        /*** From CfgMgr peer. ***/
        event->sender.peer = ml_lport_index;
        event->msgnum = MLm_vpm_api__set_lport_fallback_status;

        /* Set up msg pointer to just after the event
         * structure itself. This must be done here since the
         * sender's event->msg pointer points sender's memory
         * space, and will result in fatal errors if we try to
         * access it in LACP process space.
         */
        msg = (struct MLt_vpm_api__lport_fallback_status *)(event+1);
        msg->lport_handle = PM_SMPT2HANDLE(0, 0, info_ptr->index,
                                           info_ptr->cycl_port_type);
        msg->status = fallback_status;

        ml_send_event(event);
    }
} /* send_fallback_status_msg */

static void
configure_lacp_on_interface(struct port_data *portp, struct iface_data *idp)
{
    VLOG_DBG("%s: lag_id=%d, i/f=%s", __FUNCTION__, portp->lag_id, idp->name);

    idp->cfg_lag_id = portp->lag_id;
    idp->lacp_state = (portp->lacp_mode == PORT_LACP_OFF ?
                       LACP_STATE_DISABLED : LACP_STATE_ENABLED);

#if 0
    idp->cycl_port_type = speed_to_lport_type(portp->port_max_speed);
#else
    /* OPS_TODO: temporary hard-code. */
    idp->cycl_port_type = PM_LPORT_10GIGE;
    idp->aggregateable = AGGREGATABLE;
    idp->collecting_ready = 0;
#endif

    idp->timeout_mode = portp->timeout_mode;
    switch (portp->lacp_mode) {
        case PORT_LACP_ACTIVE:
            idp->activity_mode = LACP_ACTIVE_MODE;
            break;
        case PORT_LACP_PASSIVE:
        case PORT_LACP_OFF:
            idp->activity_mode = LACP_PASSIVE_MODE;
            break;
    }

} /* configure_lacp_on_interface */

/**
 * @details
 * Establishes an IDL session with OVSDB server. Registers the following
 * tables/columns for caching and change notification:
 *
 *     System:cur_cfg
 *     Port:name, lacp, and interfaces columns.
 *     Interface:name, link_state, link_speed, hw_bond_config columns.
 */
void
lacpd_ovsdb_if_init(const char *db_path)
{
    /* Initialize IDL through a new connection to the DB. */
    idl = ovsdb_idl_create(db_path, &ovsrec_idl_class, false, true);
    idl_seqno = ovsdb_idl_get_seqno(idl);
    ovsdb_idl_set_lock(idl, "ops_lacpd");
    ovsdb_idl_verify_write_only(idl);

    /* Cache System table. */
    ovsdb_idl_add_table(idl, &ovsrec_table_system);
    ovsdb_idl_add_column(idl, &ovsrec_system_col_cur_cfg);
    ovsdb_idl_add_column(idl, &ovsrec_system_col_system_mac);
    ovsdb_idl_add_column(idl, &ovsrec_system_col_lacp_config);

    /* Cache Subsystem table. */
    ovsdb_idl_add_table(idl, &ovsrec_table_subsystem);
    ovsdb_idl_add_column(idl, &ovsrec_subsystem_col_other_info);

    /* Cache Port table and columns. */
    ovsdb_idl_add_table(idl, &ovsrec_table_port);
    ovsdb_idl_add_column(idl, &ovsrec_port_col_name);
    ovsdb_idl_add_column(idl, &ovsrec_port_col_lacp);
    ovsdb_idl_add_column(idl, &ovsrec_port_col_interfaces);
    ovsdb_idl_add_column(idl, &ovsrec_port_col_lacp_status);
    ovsdb_idl_omit_alert(idl, &ovsrec_port_col_lacp_status);
    ovsdb_idl_add_column(idl, &ovsrec_port_col_other_config);
    ovsdb_idl_add_column(idl, &ovsrec_port_col_bond_status);
    ovsdb_idl_omit_alert(idl, &ovsrec_port_col_bond_status);

    /* Cache Interface table and columns. */
    ovsdb_idl_add_table(idl, &ovsrec_table_interface);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_name);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_type);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_duplex);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_link_state);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_link_speed);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_hw_bond_config);
    ovsdb_idl_omit_alert(idl, &ovsrec_interface_col_hw_bond_config);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_hw_intf_info);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_lacp_current);
    ovsdb_idl_omit_alert(idl, &ovsrec_interface_col_lacp_current);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_lacp_status);
    ovsdb_idl_omit_alert(idl, &ovsrec_interface_col_lacp_status);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_other_config);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_bond_status);
    ovsdb_idl_omit_alert(idl, &ovsrec_interface_col_bond_status);

    /* Initialize LAG ID pool. */
    /* OPS_TODO: read # of LAGs from somewhere? */
    init_lag_id_pool(128);

} /* lacpd_ovsdb_if_init */

void
lacpd_ovsdb_if_exit(void)
{
    shash_destroy_free_data(&all_ports);
    shash_destroy_free_data(&all_interfaces);
    shash_destroy_free_data(&interfaces_recently_added);
    ovsdb_idl_destroy(idl);
} /* lacpd_ovsdb_if_exit */


static void
del_old_interface(struct shash_node *sh_node)
{
    if (sh_node) {
        struct iface_data *idp = sh_node->data;
        free(idp->name);
        free_index(port_index, idp->index);
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
        int port_priority = 0;

        /* Save the interface name. */
        idp->name = xstrdup(ifrow->name);

        /* Allocate interface index. */
        /* -- use hw_intf_info:switch_intf_id for now.
         * -- may be overridden with OVS's other_config:lacp-port-id. */
        idp->index = allocate_next(port_index, MAX_ENTRIES_IN_POOL);
        if (idp->index < 0) {
            VLOG_ERR("Invalid interface index=%d", idp->index);
        }

        /* Save the reference to IDL row. */
        idp->cfg = ifrow;

        idp->lag_eligible = false;
        idp->lacp_current = false;
        idp->lacp_current_set = false;

        int key = smap_get_int(&(ifrow->other_config),
                              INTERFACE_OTHER_CONFIG_MAP_LACP_AGGREGATION_KEY,
                              -1);
        idp->actor_key = IS_VALID_AGGR_KEY(key) ? key : -1;

        /* Get the interface type. The default interface type is system. */
        if ((ifrow->type == NULL) ||
            (ifrow->type[0] == '\0') ||
            (strcmp(ifrow->type, OVSREC_INTERFACE_TYPE_SYSTEM) == 0)) {

            idp->intf_type = INTERFACE_TYPE_SYSTEM;

        } else if (strcmp(ifrow->type, OVSREC_INTERFACE_TYPE_INTERNAL) == 0) {

            idp->intf_type = INTERFACE_TYPE_INTERNAL;
        }

        /* The following variables are applicable only for System interfaces. */
        if (idp->intf_type == INTERFACE_TYPE_SYSTEM) {

            idp->hw_port_number = smap_get_int(&(ifrow->hw_intf_info),
                                               INTERFACE_HW_INTF_INFO_MAP_SWITCH_INTF_ID, -1);
            if (idp->hw_port_number <= 0) {
                VLOG_ERR("Invalid switch interface ID. Name=%s, ID=%d", ifrow->name, idp->hw_port_number);
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
                idp->link_speed = INTF_TO_LACP_LINK_SPEED(ifrow->link_speed[0]);
            }

            /* Set actor_priority */
            port_priority = smap_get_int(&(ifrow->other_config),
                                         INTERFACE_OTHER_CONFIG_MAP_LACP_PORT_PRIORITY, -1);
            /* If not supplied by user, default is set 1 */
            idp->actor_priority = IS_VALID_ACTOR_PRI(port_priority) ? port_priority : DEFAULT_PORT_PRIORITY;
        }

        /* Initialize the interface to be not part of any LAG.
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
        unsigned int flag = 0;

        /* Internal interfaces doesn't participate in LAGs. */
        if (idp->intf_type == INTERFACE_TYPE_INTERNAL) {
            VLOG_INFO("Skipping the interface %s ", ifrow->name);
            continue;
        }

        /* Check for changes to row. */
        if (OVSREC_IDL_IS_ROW_INSERTED(ifrow, idl_seqno) ||
            OVSREC_IDL_IS_ROW_MODIFIED(ifrow, idl_seqno)) {

            int val;
            int key = 0;
            const char* key_str = NULL;
            unsigned int new_speed;
            enum ovsrec_interface_duplex_e new_duplex;
            enum ovsrec_interface_link_state_e new_link_state;
            int new_port_id;

            /* Update actor_priority */
            val = smap_get_int(&(ifrow->other_config),
                               INTERFACE_OTHER_CONFIG_MAP_LACP_PORT_PRIORITY, 1);
            if (!IS_VALID_ACTOR_PRI(val)) {
                val = DEFAULT_PORT_PRIORITY;
            }

            if (val != idp->actor_priority) {
                idp->actor_priority = val;
                flag = 1;
            }

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
                new_speed = INTF_TO_LACP_LINK_SPEED(ifrow->link_speed[0]);
            }

            new_duplex = INTERFACE_DUPLEX_HALF;
            if (ifrow->duplex) {
                if (!strcmp(ifrow->duplex, OVSREC_INTERFACE_DUPLEX_FULL)) {
                    new_duplex = INTERFACE_DUPLEX_FULL;
                }
            }
            /* Update Port Id*/
            new_port_id = smap_get_int(&(ifrow->other_config),
                                        INTERFACE_OTHER_CONFIG_MAP_LACP_PORT_ID,
                                        0);

            if (!IS_VALID_PORT_ID(new_port_id)) {
                new_port_id = 0;
            }

            if (new_port_id != idp->port_id) {
                VLOG_DBG("Interface %s port_id changed in DB: "
                         "new port_id=%d",
                         ifrow->name, new_port_id);
                idp->port_id = new_port_id;
                flag = 1;
            }

            /* Update Aggregation Key */
            key_str = smap_get(&(ifrow->other_config),
                      INTERFACE_OTHER_CONFIG_MAP_LACP_AGGREGATION_KEY);
            if(key_str)
            {
                key = atoi(key_str);

                if (!IS_VALID_AGGR_KEY(key)) {
                    key = -1;
                }

                if (key != idp->actor_key) {
                    VLOG_DBG("Interface %s actor_key change in DB: "
                            "new actor_key=%d",
                            ifrow->name, key);
                    idp->actor_key = key;
                    flag = 1;
                }
            }

            if (flag) {
                send_config_lport_msg(idp);
            }

            if ((new_link_state != idp->link_state) ||
                (new_speed != idp->link_speed) ||
                (new_duplex != idp->duplex)) {

                idp->link_state = new_link_state;
                idp->link_speed = new_speed;
                idp->duplex = new_duplex;
                if (idp->port_datap != NULL) {
                    update_member_interface_bond_status(idp->port_datap);
                    update_port_bond_status_map_entry(idp->port_datap);
                    rc++;
                }

                VLOG_DBG("Interface %s link state changed in DB: "
                         "new_speed=%d, new_link=%s, new_duplex=%s, ",
                         ifrow->name, idp->link_speed,
                         (idp->link_state == INTERFACE_LINK_STATE_UP ? "up" : "down"),
                         (idp->duplex == INTERFACE_DUPLEX_FULL ? "full" : "half"));

                if (update_interface_lag_eligibility(idp)) {
                    rc++;
                } else {
                    /* If no change to eligibility, but this interface is
                     * part of a dynamic LAG, then we need to send link
                     * change notification message to the state machine. */
                    if (idp->lag_eligible && idp->port_datap &&
                        LACP_ENABLED_ON_PORT(idp->port_datap->lacp_mode)) {

                        send_link_state_change_msg(idp);

                        /* OPS_TODO: need to trigger the LACP state machine
                         * to advance to collecting/distributing state.  In the
                         * past, this was triggered by stpd when it set the port
                         * to forwarding.  In OpenSwitch environment, this really
                         * needs to be done only after the interface has been
                         * attached to the h/w LAG, thus collecting-ready.
                         * For now, force this trigger here. */

                        send_lport_lacp_change_msg(idp, (LACP_LPORT_TIMEOUT_FIELD_PRESENT |
                                                         LACP_LPORT_HW_COLL_STATUS_PRESENT));
                    }
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

    smap_destroy(&smap);

    return 1;
} /* update_interface_hw_bond_config_map_entry */

/**
 * Common function to update port's fallback flag status.
 * When you turn off a LAG, all the interfaces and super port are removed, so
 * if you turn LAG again fallback flag will be lost and will be reset to
 * default value, even when OVSDB the key is present and 'true'.
 *
 * @param row pointer to port row in IDL.
 * @param portp pointer to daemon's internal port data struct.
 */
static void
update_port_fallback_flag(const struct ovsrec_port *row,
                          struct port_data *portp, bool lacp_changed)
{
    const char *ovs_fallback = NULL;
    bool ovs_fallback_enabled = false;

    struct shash_node *node, *next;
    struct iface_data *idp = NULL;

    /* We need to verify if fallback flag switched value between OVSDB and
     * local data.
     *
     * You can overpass CLI by using 'ovs-vsctl' and toggle fallback flag
     * manually. Bad user, bad!
     */
    ovs_fallback = smap_get(&(row->other_config),
                            PORT_OTHER_CONFIG_LACP_FALLBACK);

    if (ovs_fallback) {
        if (strncmp(ovs_fallback,
                    PORT_OTHER_CONFIG_LACP_FALLBACK_ENABLED,
                    strlen(PORT_OTHER_CONFIG_LACP_FALLBACK_ENABLED)) == 0) {
            ovs_fallback_enabled = true;
        }
    }
    if (ovs_fallback_enabled != portp->fallback_enabled || lacp_changed) {
        portp->fallback_enabled = ovs_fallback_enabled;
        SHASH_FOR_EACH_SAFE(node, next, &portp->cfg_member_ifs) {
            idp = shash_find_data(&all_interfaces, node->name);
            if (idp) {
                send_fallback_status_msg(idp, ovs_fallback_enabled);
            }
        }
    }
}

/**
 * Update bond_status configuration for the configured member interfaces
 * of one LAG.
 *
 * NOTE: ovsdb_mutex must be taken prior to calling this function.
 *
 * @param portp port_data pointer to the port entry.
 */
static void
update_member_interface_bond_status(struct port_data *portp)
{
    struct shash_node *node, *next;

    /* If the port is NULL, then return */
    if(!portp) {
        VLOG_WARN("Calling update_member_interface_bond_status with portp NULL.");
        return;
    }

    if (!strncmp(portp->name,
                 LAG_PORT_NAME_PREFIX,
                 LAG_PORT_NAME_PREFIX_LENGTH)) {
        SHASH_FOR_EACH_SAFE(node, next, &portp->cfg_member_ifs) {
            struct iface_data *idp = shash_find_data(&all_interfaces,
                                                     node->name);
            update_interface_bond_status_map_entry(idp);
        }
    }
} /* update_member_interface_bond_status */

/**
 * Update bond_status configuration for a given interface
 *
 * NOTE: ovsdb_mutex must be taken prior to calling this function.
 *
 * @param idp  iface_data pointer to the interface entry.
 */
static void
update_interface_bond_status_map_entry(struct iface_data *idp)
{
    const struct ovsrec_interface *ifrow;
    struct smap smap;
    struct smap_node *node;
    bool rx_enable = false;
    bool tx_enable = false;

    ifrow = idp->cfg;
    smap_init(&smap);

    if (idp->link_state == INTERFACE_LINK_STATE_UP) {

        SMAP_FOR_EACH(node, &ifrow->hw_bond_config) {

            if (!strncmp(node->key,
                         INTERFACE_HW_BOND_CONFIG_MAP_RX_ENABLED,
                         strlen(INTERFACE_HW_BOND_CONFIG_MAP_RX_ENABLED))
                && !strncmp(node->value,
                            INTERFACE_HW_BOND_CONFIG_MAP_ENABLED_TRUE,
                            strlen(INTERFACE_HW_BOND_CONFIG_MAP_ENABLED_TRUE))) {
                rx_enable = true;
            }
            else if (!strncmp(node->key,
                     INTERFACE_HW_BOND_CONFIG_MAP_TX_ENABLED,
                     strlen(INTERFACE_HW_BOND_CONFIG_MAP_TX_ENABLED))
                     && !strncmp(node->value,
                                 INTERFACE_HW_BOND_CONFIG_MAP_ENABLED_TRUE,
                                 strlen(INTERFACE_HW_BOND_CONFIG_MAP_ENABLED_TRUE))) {
                tx_enable = true;
            }
        }

        if (tx_enable && rx_enable) {
            smap_replace(&smap,
                         INTERFACE_BOND_STATUS_UP,
                         INTERFACE_BOND_STATUS_ENABLED_TRUE);
        } else {
            smap_replace(&smap,
                         INTERFACE_BOND_STATUS_BLOCKED,
                         INTERFACE_BOND_STATUS_ENABLED_TRUE);
        }
    }
    /* Interface link is down */
    else {
        smap_replace(&smap,
                     INTERFACE_BOND_STATUS_DOWN,
                     INTERFACE_BOND_STATUS_ENABLED_TRUE);
    }

    ovsrec_interface_set_bond_status(ifrow, &smap);
    smap_destroy(&smap);
} /* update_interface_bond_status_map_entry */

/**
 * Remove bond_status configuration for a given interface that is
 * not part of any LAG.
 *
 * NOTE: ovsdb_mutex must be taken prior to calling this function.
 *
 * @param idp  iface_data pointer to the interface entry.
 */
static void
remove_interface_bond_status_map_entry(struct iface_data *idp)
{
    const struct ovsrec_interface *ifrow;
    struct smap smap;

    ifrow = idp->cfg;
    smap_init(&smap);
    smap_remove(&smap, INTERFACE_BOND_STATUS_DOWN);
    smap_remove(&smap, INTERFACE_BOND_STATUS_BLOCKED);
    smap_remove(&smap, INTERFACE_BOND_STATUS_UP);

    ovsrec_interface_set_bond_status(ifrow, &smap);
    smap_destroy(&smap);
} /* remove_interface_bond_status_map_entry */

/**
 * Update bond_status configuration for a given LAG port
 *
 * NOTE: this function checks the configuration of different interfaces for a
 * LAG port and determines the global status:
 *
 * - Up: At least one member interface is eligible and should be
 *     forwarding traffic according to LACP
 * - Blocked:  All member interfaces are either not eligible or should be
 *     blocked according to LACP.
 *     If LACP-fallback-ab is enabled, and the <ref column="lacp_status"/> is
 *     defaulted, then the bond state is forwarding.  If the LACP-fallback-ab
 *     is disable, then the state is blocked.
 * - Down:  All member interfaces configured to be a member of a LAG are either
 *     administratively or operatively down
 *
 * @param portp port_data pointer to the port entry.
 */
static void
update_port_bond_status_map_entry(struct port_data *portp)
{
    const struct ovsrec_interface *ifrow;
    struct smap smap;
    struct shash_node *node, *next;
    struct smap_node *snode;
    int total_intf = 0;
    int blocked_intf = 0;
    int up_intf = 0;
    int down_intf = 0;
    char* speed_str;

    /* If the port is NULL, then return */
    if(!portp) {
        VLOG_WARN("Calling update_port_bond_status_map_entry with portp NULL.");
        return;
    }

    /* If the port is not a LAG then return */
    if (strncmp(portp->name, LAG_PORT_NAME_PREFIX, LAG_PORT_NAME_PREFIX_LENGTH)) {
        return;
    }

    smap_init(&smap);

    SHASH_FOR_EACH_SAFE(node, next, &portp->cfg_member_ifs) {

        struct iface_data *idp = shash_find_data(&all_interfaces, node->name);
        if (idp) {
            ifrow = idp->cfg;
            SMAP_FOR_EACH(snode, &ifrow->bond_status) {

                if (!strncmp(snode->key,
                             INTERFACE_BOND_STATUS_UP,
                             strlen(INTERFACE_BOND_STATUS_UP))) {
                    up_intf++;
                } else if (!strncmp(snode->key,
                           INTERFACE_BOND_STATUS_BLOCKED,
                           strlen(INTERFACE_BOND_STATUS_BLOCKED))) {
                    blocked_intf++;
                } else if (!strncmp(snode->key,
                           INTERFACE_BOND_STATUS_DOWN,
                           strlen(INTERFACE_BOND_STATUS_DOWN))) {
                    down_intf++;
                }

                total_intf++;
            }
        }
    }

    if (down_intf == total_intf) {
           smap_replace(&smap,
                        PORT_BOND_STATUS_DOWN,
                        PORT_BOND_STATUS_ENABLED_TRUE);
    } else if (blocked_intf == total_intf) {
        smap_replace(&smap,
                     PORT_BOND_STATUS_BLOCKED,
                     PORT_BOND_STATUS_ENABLED_TRUE);
    } else if (up_intf > 0) {
        smap_replace(&smap,
                     PORT_BOND_STATUS_UP,
                     PORT_BOND_STATUS_ENABLED_TRUE);
    }

    /* Update bond_speed */
    /* If the LAG has no member interfaces, then bond_speed is empty. */
    if (total_intf == 0) {
        smap_remove(&smap, PORT_BOND_STATUS_MAP_BOND_SPEED);
    } else {
        long speed_in_bps = (long)portp->lag_member_speed * MEGA_BITS_PER_SEC;
        asprintf(&speed_str, "%ld", speed_in_bps);
        smap_replace(&smap, PORT_BOND_STATUS_MAP_BOND_SPEED, speed_str);
    }

    ovsrec_port_set_bond_status(portp->cfg, &smap);
    smap_destroy(&smap);
} /* update_port_bond_status_map_entry */

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

        /* If lacp mode, and state is disabled, set RX/TX to disabled */
        if (idp->lacp_state == LACP_STATE_DISABLED) {
            update_interface_hw_bond_config_map_entry(
                idp,
                INTERFACE_HW_BOND_CONFIG_MAP_RX_ENABLED,
                INTERFACE_HW_BOND_CONFIG_MAP_ENABLED_FALSE);

            update_interface_hw_bond_config_map_entry(
                idp,
                INTERFACE_HW_BOND_CONFIG_MAP_TX_ENABLED,
                INTERFACE_HW_BOND_CONFIG_MAP_ENABLED_FALSE);
        }
        /* If this interface is not in the configured member list,
         * and is in the recently added list, we do NOT need to call
         * send_config_lport_msg because that would delete internal port data.
         * If those 2 conditions are not met, we call send_config_lport_msg. */
        if (shash_find_data(&portp->cfg_member_ifs, idp->name) != NULL ||
            shash_find_data(&interfaces_recently_added, idp->name) == NULL) {
            send_config_lport_msg(idp);
        }
    }

    /* update eligible LAG member list. */
    if (eligible) {
        shash_add(&portp->eligible_member_ifs, idp->name, (void *)idp);
        update_member_interface_bond_status(portp);
        update_port_bond_status_map_entry(portp);
    } else {
        update_member_interface_bond_status(portp);
        update_port_bond_status_map_entry(portp);
        shash_find_and_delete(&portp->eligible_member_ifs, idp->name);
    }
    idp->lag_eligible = eligible;

} /* set_interface_lag_eligibility */

/**
 * Checks whether the interface is eligibile to become a member of
 * configured LAG by applying the following rules:
 *     - interface configured to be member of a LAG
 * For static LAGs, additional rules apply:
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

    if (!IS_VALID_AGGR_KEY(idp->actor_key)) {
        idp->actor_key = portp->lag_id;
    }

    /* Check if the interface is currently eligible. */
    if (shash_find_data(&portp->eligible_member_ifs, idp->name)) {
        old_eligible = true;
    }

    if (!shash_find_data(&portp->cfg_member_ifs, idp->name)) {
        /* Interface must be configured as part of the Port first. */
        new_eligible = false;

    } else if (LACP_ENABLED_ON_PORT(portp->lacp_mode)) {
        /* For dynamic LAGs, interface is considered eligible as long
         * as the interface is a configured member of the port. */
        new_eligible = true;

    } else {
        /* For static LAGs, interface eligibility is based on
         * additional link status. */
        if (INTERFACE_LINK_STATE_UP != idp->link_state) {
            new_eligible = false;
        }

        if (INTERFACE_DUPLEX_FULL != idp->duplex) {
            new_eligible = false;
        }

        if (shash_count(&portp->eligible_member_ifs) == 0 && new_eligible) {
            /* First member to join the LAG decides LAG member speed. */
            portp->lag_member_speed = idp->link_speed;
        }

        if (portp->lag_member_speed != idp->link_speed) {
            new_eligible = false;
        }
    }

    VLOG_DBG("%s: interface %s - old_eligible=%d new_eligible=%d",
             __FUNCTION__, idp->name,
             old_eligible, new_eligible);

    if (old_eligible != new_eligible) {
        set_interface_lag_eligibility(portp, idp, new_eligible);
        rc++;
    }

    return rc;
} /* update_interface_lag_eligibility */

/**
 * Handles Port related configuration changes for a given port table entry.
 *
 * @param row pointer to port row in IDL.
 * @param portp pointer to daemon's internal port data struct.
 *
 * @return
 */
static int
handle_port_config(const struct ovsrec_port *row, struct port_data *portp)
{
    enum ovsrec_port_lacp_e lacp_mode;
    struct ovsrec_interface *intf;
    struct shash sh_idl_port_intfs;
    struct shash_node *node, *next;
    int rc = 0;
    int timeout;
    bool timeout_changed = false;
    bool lacp_mode_switched = false;
    size_t i;
    const char *cp;
    char agg_key[AGG_KEY_MAX_LENGTH];
    bool lacp_changed = false;

    VLOG_DBG("%s: port %s, n_interfaces=%d",
             __FUNCTION__, row->name, (int)row->n_interfaces);

    if (portp == NULL) {
        VLOG_WARN("Function: handle_port_config parameter portp is NULL");
        return rc;
    }

    /* Set timeout-mode */
    cp = smap_get(&(row->other_config), PORT_OTHER_CONFIG_MAP_LACP_TIME);
    timeout = valid_lacp_timeout(cp);
    if ((timeout != -1) && (timeout != portp->timeout_mode)) {
        portp->timeout_mode = timeout;
        timeout_changed = true;

        if (log_event("LACP_RATE_SET",
                      EV_KV("lag_id", "%s",
                            portp->name + LAG_PORT_NAME_PREFIX_LENGTH),
                      EV_KV("lacp_rate", "%s", cp)) < 0) {
            VLOG_ERR("Could not log event LACP_RATE_SET");
        }
    }

    /* Build a new map for this port's interfaces in idl. */
    shash_init(&sh_idl_port_intfs);
    for (i = 0; i < row->n_interfaces; i++) {
        intf = row->interfaces[i];
        if (!shash_add_once(&sh_idl_port_intfs, intf->name, intf)) {
            VLOG_WARN("interface %s specified twice", intf->name);
        }
    }

    /* Process deleted interfaces first. */
    SHASH_FOR_EACH_SAFE(node, next, &portp->cfg_member_ifs) {
        struct ovsrec_interface *ifrow =
            shash_find_data(&sh_idl_port_intfs, node->name);
        if (!ifrow) {
            struct iface_data *idp =
                shash_find_data(&all_interfaces, node->name);
            if (idp) {
                VLOG_DBG("Found a deleted interface %s", node->name);

                set_interface_lag_eligibility(portp, idp, false);
                /* If this interface was added to another port in this same cycle
                 * of SHASH_FOR_EACH(sh_node, &all_ports), then we don't have to
                 * delete it.*/
                if (shash_find_data(&interfaces_recently_added, node->name) == NULL) {
                    db_clear_interface(idp);
                    idp->port_datap = NULL;
                    clear_port_overrides(idp);
                    rc++;
                } else {
                    /* Update lag eligibility for this interface which is now part of other lag. */
                    if (update_interface_lag_eligibility(idp)) {
                        rc++;
                    }
                }

                if (log_event("LAG_INTERFACE_REMOVE",
                              EV_KV("lag_id", "%s",
                                    portp->name + LAG_PORT_NAME_PREFIX_LENGTH),
                              EV_KV("intf_id", "%s", node->name)) < 0) {
                    VLOG_ERR("Could not log event LAG_INTERFACE_REMOVE");
                }
                shash_delete(&portp->cfg_member_ifs, node);
            }
        }
    }

    /* Update LACP mode for existing interfaces. */
    lacp_mode = PORT_LACP_OFF;
    if (row->lacp) {
        if (strcmp(OVSREC_PORT_LACP_ACTIVE, row->lacp) == 0) {
            lacp_mode = PORT_LACP_ACTIVE;
        } else if (strcmp(OVSREC_PORT_LACP_PASSIVE, row->lacp) == 0) {
            lacp_mode = PORT_LACP_PASSIVE;
        }
    }

    if (portp->lacp_mode != lacp_mode) {
        VLOG_DBG("port %s:lacp_mode changed  %s -> %s",
                 row->name, lacp_mode_str(portp->lacp_mode),
                 lacp_mode_str(lacp_mode));

        lacp_changed = true;
        /* LACP mode changed.  In either case, mark all existing interfaces
         * as ineligible to detach them first.  Then the interfaces will be
         * reconfigured based on the new LACP mode. */
        if (log_event("LACP_MODE_SET",
                      EV_KV("lag_id", "%s",
                            portp->name + LAG_PORT_NAME_PREFIX_LENGTH),
                      EV_KV("lacp_mode", "%s",
                            lacp_mode_str(lacp_mode))) < 0) {
            VLOG_ERR("Could not log event LACP_MODE_SET");
        }

        SHASH_FOR_EACH_SAFE(node, next, &portp->eligible_member_ifs) {
            struct iface_data *idp =
                shash_find_data(&all_interfaces, node->name);
            set_interface_lag_eligibility(portp, idp, false);
        }

        if (!LACP_ENABLED_ON_PORT(portp->lacp_mode)) {
            /* LACP was not on (static LAG).  Need to turn on LACP. */

            /* Set port override values in each configured member. */
            SHASH_FOR_EACH_SAFE(node, next, &portp->cfg_member_ifs) {
                struct iface_data *idp = shash_find_data(&all_interfaces, node->name);
                if (idp) {
                    set_port_overrides(portp, idp);
                }
            }

            /* Create super port in LACP state machine. */
            if (!portp->lag_id) {
                portp->lag_id = alloc_lag_id();
            }

            if (portp->lag_id) {

                /* Send LAG creation information. */
                send_lag_create_msg(portp->lag_id);

                if (log_event("LAG_CREATE",
                              EV_KV("lag_id", "%s", portp->name +
                                    LAG_PORT_NAME_PREFIX_LENGTH)) < 0) {
                    VLOG_ERR("Could not log event LAG_CREATE");
                }

                /* Send LAG configuration information.
                 * We control actor_key and port type parameters,
                 * so we'll simply initialize them with default values.
                 * Use aggregation key (agg_key) for actor_key during
                 * creation.
                 */
                snprintf(agg_key, AGG_KEY_MAX_LENGTH, "%s",
                         portp->name + LAG_PORT_NAME_PREFIX_LENGTH);
                send_config_lag_msg(portp->lag_id,
                                    atoi(agg_key),
                                    PM_LPORT_INVALID);
            } else {
                VLOG_ERR("Failed to allocate LAGID for port %s!", portp->name);
            }

        } else {
            /* LACP was on (dynamic LAG). */

            /* If LACP switched from active to passive or passive to active
             * we just want to reconfigure the interfaces.
             */
            if (LACP_ENABLED_ON_PORT(lacp_mode)) {
                lacp_mode_switched = true;
            } else {
                /* Else, we need to turn off LACP */
                /* clear port lacp_status */
                db_clear_lag_partner_info_port(portp);

                /* clear interface lacp_status */
                SHASH_FOR_EACH_SAFE(node, next, &portp->cfg_member_ifs) {
                    struct iface_data *idp =
                           shash_find_data(&all_interfaces, node->name);
                    if (idp) {
                        db_clear_interface(idp);
                        clear_port_overrides(idp);
                    }
                }

                /* Delete super port in LACP state machine. */
                if (portp->lag_id) {
                    send_unconfig_lag_msg(portp->lag_id);
                    send_lag_delete_msg(portp->lag_id);
                    if (log_event("LAG_DELETE",
                                  EV_KV("lag_id", "%s", portp->name +
                                        LAG_PORT_NAME_PREFIX_LENGTH)) < 0) {
                        VLOG_ERR("Could not log event LAG_DELETE");
                    }

                    free_lag_id(portp->lag_id);
                    portp->lag_id = 0;
                }
                rc++;
            }
        }

        /* Save new LACP mode. */
        portp->lacp_mode = lacp_mode;
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
                VLOG_ERR("Error adding interface to port %s. "
                         "Interface %s not found.",
                         portp->name, node->name);
                continue;
            }
            shash_add(&portp->cfg_member_ifs, node->name, (void *)idp);
            /* Add interface to recently added list */
            shash_add(&interfaces_recently_added, node->name, (void *)idp);
            idp->port_datap = portp;
            set_port_overrides(portp, idp);
            if (log_event("LAG_INTERFACE_ADD",
                          EV_KV("lag_id", "%s",
                                portp->name + LAG_PORT_NAME_PREFIX_LENGTH),
                          EV_KV("intf_id", "%s", node->name)) < 0) {
                VLOG_ERR("Could not log event LAG_INTERFACE_ADD");
            }
            update_member_interface_bond_status(portp);
            rc++;
        }
    }

    /* Update LAG member eligibility for configured member interfaces. */
    SHASH_FOR_EACH_SAFE(node, next, &portp->cfg_member_ifs) {
        struct iface_data *idp = shash_find_data(&all_interfaces, node->name);
        if (idp) {
            if (lacp_mode_switched) {

                /* If mode switched reconfigure the port */
                send_config_lport_msg(idp);

            } else if (timeout_changed) {

                /* Else, just update the port if any dynamic fields changed */

                /* If user changed timeout mode, send update */
                idp->timeout_mode = portp->timeout_mode;
                send_lport_lacp_change_msg(idp, (LACP_LPORT_DYNAMIC_FIELDS_PRESENT |
                                                 LACP_LPORT_TIMEOUT_FIELD_PRESENT));
            }
            if (update_interface_lag_eligibility(idp)) {
                rc++;
            }
        }
    }

    if (lacp_mode != PORT_LACP_OFF) {
        /* other_config:lacp-system-id and other_config:lacp-system-priority
         * only make sense if lacp is enabled.
         */
        const char *sys_id;
        int sys_prio;
        struct ether_addr *eth_addr_p;
        struct ether_addr eth_addr;
        bool changed = false;

        sys_id = smap_get(&(row->other_config),
                      PORT_OTHER_CONFIG_MAP_LACP_SYSTEM_ID);
        sys_prio = smap_get_int(&(row->other_config),
                      PORT_OTHER_CONFIG_MAP_LACP_SYSTEM_PRIORITY,
                      0);

        /* If there's a change in the system-priority, send the update. */
        if (sys_prio != portp->sys_prio) {
            if (sys_prio == 0 || IS_VALID_SYS_PRIO(sys_prio)) {
                changed = true;
                portp->sys_prio = sys_prio;
            }
        }

        /* If there's a change in the system-id, send the update. */
        if ((sys_id == NULL && portp->sys_id != NULL) ||
            (sys_id != NULL && portp->sys_id == NULL) ||
            (sys_id != NULL && portp->sys_id != NULL &&
                strcmp(sys_id, portp->sys_id) != 0)) {
            if (sys_id == NULL) {
                free(portp->sys_id);
                portp->sys_id = NULL;
                changed = true;
            } else {
                /* Convert the string to a mac address. */
                eth_addr_p = ether_aton_r(sys_id, &eth_addr);
                if (eth_addr_p) {
                    /* Save the system-id *after* it's been validated. */
                    free(portp->sys_id);
                    portp->sys_id = strdup(sys_id);
                    changed = true;
                }
            }
        }
        if (changed) {
            SHASH_FOR_EACH_SAFE(node, next, &portp->cfg_member_ifs) {
                struct iface_data *idp = shash_find_data(&all_interfaces,
                                                         node->name);
                if (idp) {
                    set_port_overrides(portp, idp);
                }
            }
        }

        /* Update Fallback flag, it could be toggled on OVSDB */
        update_port_fallback_flag(row, portp, lacp_changed);
    }

    update_port_bond_status_map_entry(portp);

    /* Destroy the shash of the IDL interfaces. */
    shash_destroy(&sh_idl_port_intfs);

    return rc;
} /* handle_port_config */

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
                /* There is no need to update eligibility if we are deleting a LAG port. */
                if (strncmp(portp->name, LAG_PORT_NAME_PREFIX, LAG_PORT_NAME_PREFIX_LENGTH)) {
                    set_interface_lag_eligibility(portp, idp, false);
                }
                db_clear_interface(idp);
                idp->port_datap = NULL;
                rc++;
            }
        }
        if (portp->lag_id) {
            send_unconfig_lag_msg(portp->lag_id);
            send_lag_delete_msg(portp->lag_id);
            free_lag_id(portp->lag_id);
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

        portp->cfg = port_row;
        if ((portp->name = xstrdup(port_row->name)) == NULL) {
            free(portp);
            VLOG_FATAL("%s : out of memory", __FUNCTION__);
            return;
        }
        portp->lacp_mode = PORT_LACP_OFF;

        shash_init(&portp->cfg_member_ifs);
        shash_init(&portp->eligible_member_ifs);
        shash_init(&portp->participant_ifs);

        for (i = 0; i < port_row->n_interfaces; i++) {
            struct ovsrec_interface *intf;
            struct iface_data *idp;

            intf = port_row->interfaces[i];
            idp = shash_find_data(&all_interfaces, intf->name);
            if (!idp) {
                VLOG_ERR("Error adding interface to new port %s. "
                         "Interface %s not found.",
                         portp->name, intf->name);
                continue;
            }
            VLOG_DBG("Storing interface %s in port %s hash map",
                     intf->name, portp->name);

            shash_add(&portp->cfg_member_ifs, intf->name, (void *)idp);
            update_member_interface_bond_status(portp);
            idp->port_datap = portp;
            idp->fallback_enabled = false;
        }
        VLOG_DBG("Created local data for Port %s", port_row->name);

        /* update bond status */
        update_port_bond_status_map_entry(portp);
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

            if (OVSREC_IDL_IS_ROW_INSERTED(row, idl_seqno)) {
                portp->timeout_mode = LACP_PORT_TIMEOUT_DEFAULT;
            }

            /* Handle Port config update. */
            if (handle_port_config(row, portp)) {
                rc++;
            }
        }
    }

    /* Remove the interfaces added to interfaces_recently_added. */
    SHASH_FOR_EACH_SAFE(sh_node, sh_next, &interfaces_recently_added) {
        shash_delete(&interfaces_recently_added, sh_node);
    }

    /* Destroy the shash of the IDL ports. */
    shash_destroy(&sh_idl_ports);

    return rc;
} /* update_port_cache */

static void
update_system_prio_n_id(const struct ovsrec_system *sys, bool lacpd_init)
{
    bool sys_mac_changed = false;
    const char *sys_mac = NULL;
    int sys_prio = -1;
    struct ether_addr *eth_addr_p;
    struct ether_addr eth_addr;

    if (sys) {

        /* See if user set lacp-system-id */
        sys_mac = smap_get(&(sys->lacp_config),
                           SYSTEM_LACP_CONFIG_MAP_LACP_SYSTEM_ID);
        /* If LACP system ID is not configured, then use system mac. */
        if (sys_mac == NULL || (strlen(sys_mac) != OPS_MAC_STR_SIZE - 1)) {
            sys_mac = sys->system_mac;
        }

        if (sys_mac == NULL || (strlen(sys_mac) != OPS_MAC_STR_SIZE - 1)) {
            VLOG_FATAL("LACP System ID is not available.");
        }

        if (strcmp(sys_mac, system_id) != 0) {
            sys_mac_changed = true;
        }

        if (sys_mac_changed || lacpd_init) {
            eth_addr_p = ether_aton_r(sys_mac, &eth_addr);
            if (eth_addr_p) {

                send_sys_mac_msg(eth_addr_p);

                /* Only save after we know it is a valid MAC addr */
                strncpy(system_id, sys_mac, OPS_MAC_STR_SIZE);

                if (log_event("LACP_SYSTEM_ID_SET",
                              EV_KV("system_id", "%s", system_id)) < 0) {
                    VLOG_ERR("Could not log event LACP_SYSTEM_ID_SET");
                }

            }
        }

        /* Send system priority */

        /* See if user set lacp-system-priority */
        sys_prio = smap_get_int(&(sys->lacp_config),
                                SYSTEM_LACP_CONFIG_MAP_LACP_SYSTEM_PRIORITY,
                                DFLT_SYSTEM_LACP_CONFIG_SYSTEM_PRIORITY);

        if (IS_VALID_SYS_PRIO(sys_prio) || lacpd_init) {

            if (IS_VALID_SYS_PRIO(sys_prio)) {
                system_priority = sys_prio;
            }
            send_sys_pri_msg(system_priority);
            if (system_priority != prev_sys_prio) {
                if (log_event("LACP_SYSTEM_PRIORITY_SET",
                              EV_KV("system_priority", "%d",
                                    system_priority)) < 0) {
                    VLOG_ERR("Could not log event LACP_SYSTEM_PRIORITY_SET");
                }
                prev_sys_prio = system_priority;
            }
        }
    }
}
/* update_system_prio_n_id */


static int
lacpd_reconfigure(void)
{
    int rc = 0;
    unsigned int new_idl_seqno = ovsdb_idl_get_seqno(idl);
    const struct ovsrec_system *sys = NULL;

    if (new_idl_seqno == idl_seqno) {
        /* There was no change in the DB. */
        return 0;
    }

    /* Update system priority and system id */
    sys = ovsrec_system_first(idl);
    update_system_prio_n_id(sys, false);

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
    const struct ovsrec_system *sys = NULL;

    if (system_configured) {
        /* Nothing to do if we're already configured. */
        return;
    }

    sys = ovsrec_system_first(idl);
    if (sys && sys->cur_cfg > (int64_t)0) {

        /* Send system id (MAC) and system priority */
        update_system_prio_n_id(sys, true);

        system_configured = true;
    }

} /* lacpd_chk_for_system_configured */

static char *
format_system_id(system_variables_t *system_id)
{
    char *result = NULL;
    asprintf(&result, "%d,%02x:%02x:%02x:%02x:%02x:%02x",
             ntohs(system_id->system_priority),
             htons(system_id->system_mac_addr[0]) >> 8,
             htons(system_id->system_mac_addr[0]) & 0xff,
             htons(system_id->system_mac_addr[1]) >> 8,
             htons(system_id->system_mac_addr[1]) & 0xff,
             htons(system_id->system_mac_addr[2]) >> 8,
             htons(system_id->system_mac_addr[2]) & 0xff);

    return result;
}

static char *
format_port_id(u_short port_priority, u_short port_number)
{
    char *result = NULL;
    asprintf(&result, "%d,%d", ntohs(port_priority), ntohs(port_number));

    return result;
}

static char *
format_key(u_short key)
{
    char *result = NULL;
    asprintf(&result, "%d", ntohs(key));

    return result;
}

static char *
format_state(state_parameters_t state)
{
    char *result = NULL;
    asprintf(&result,
             INTERFACE_LACP_STATUS_STATE_ACTIVE
             ":%c,"
             INTERFACE_LACP_STATUS_STATE_TIMEOUT
             ":%c,"
             INTERFACE_LACP_STATUS_STATE_AGGREGATION
             ":%c,"
             INTERFACE_LACP_STATUS_STATE_SYNCHRONIZATION
             ":%c,"
             INTERFACE_LACP_STATUS_STATE_COLLECTING
             ":%c,"
             INTERFACE_LACP_STATUS_STATE_DISTRIBUTING
             ":%c,"
             INTERFACE_LACP_STATUS_STATE_DEFAULTED
             ":%c,"
             INTERFACE_LACP_STATUS_STATE_EXPIRED
             ":%c",
             state.lacp_activity ? '1' : '0',
             state.lacp_timeout ? '1' : '0',
             state.aggregation ? '1' : '0',
             state.synchronization ? '1' : '0',
             state.collecting ? '1' : '0',
             state.distributing ? '1' : '0',
             state.defaulted ? '1' : '0',
             state.expired ? '1' : '0');

    return result;
}

static void
db_clear_interface(struct iface_data *idp)
{
    bool *bptr = NULL;
    const struct ovsrec_interface *ifrow;
    struct smap smap;

    /*
     * Note that this can only be called in the context of
     * the idl loop, so we are already holding the OVSDB_LOCK.
     * More specifically, we've scheduled a txn, so we don't
     * need to call ovsdb_idl_txn_create(), either.
     */

    VLOG_DBG("clearing interface %s lacpd status", idp->name);

    ifrow = idp->cfg;

    smap_init(&smap);

    ovsrec_interface_set_lacp_status(ifrow, &smap);
    ovsrec_interface_set_lacp_current(ifrow, bptr, 0);
    remove_interface_bond_status_map_entry(idp);

    idp->lacp_current = false;
    idp->lacp_current_set = false;

    free(idp->actor.system_id);
    idp->actor.system_id = NULL;
    free(idp->actor.port_id);
    idp->actor.port_id = NULL;
    free(idp->actor.key);
    idp->actor.key = NULL;
    free(idp->actor.state);
    idp->actor.state = NULL;

    free(idp->partner.system_id);
    idp->partner.system_id = NULL;
    free(idp->partner.port_id);
    idp->partner.port_id = NULL;
    free(idp->partner.key);
    idp->partner.key = NULL;
    free(idp->partner.state);
    idp->partner.state = NULL;

    smap_destroy(&smap);
}

void
db_update_interface(lacp_per_port_variables_t *plpinfo)
{
    struct iface_data *idp = NULL;
    int port = PM_HANDLE2PORT(plpinfo->lport_handle);
    const struct ovsrec_interface *ifrow;
    bool lacp_current;
    struct ovsdb_idl_txn *txn = NULL;
    bool changes = false;
    bool smap_changes = false;
    char *system_id, *port_id, *key, *state;
    struct smap smap;
    struct port_data *portp;

    OVSDB_LOCK;

    /* get interface data */
    idp = find_iface_data_by_index(port);

    if (idp == NULL) {
        VLOG_WARN("Unable to find interface for hardware index %d", port);
        goto end;
    }

    portp = idp->port_datap;

    if (!portp) {
        VLOG_WARN("Interface doesn't have any port");
        goto end;
    }

    if (portp->lacp_mode == PORT_LACP_OFF) {
        VLOG_WARN("Interface lacp mode is off");
        goto end;
    }

    idp->local_state = plpinfo->actor_oper_port_state;

    ifrow = idp->cfg;

    txn = ovsdb_idl_txn_create(idl);

    smap_clone(&smap, &ifrow->lacp_status);

    /* actor data */
    system_id = format_system_id(&plpinfo->actor_oper_system_variables);
    port_id = format_port_id(plpinfo->actor_oper_port_priority, plpinfo->actor_oper_port_number);
    key = format_key(plpinfo->actor_oper_port_key);
    state = format_state(plpinfo->actor_oper_port_state);

    if (idp->actor.system_id == NULL ||
        strcmp(idp->actor.system_id, system_id) != 0) {
        smap_replace(&smap, INTERFACE_LACP_STATUS_MAP_ACTOR_SYSTEM_ID, system_id);
        free(idp->actor.system_id);
        idp->actor.system_id = system_id;
        smap_changes = true;
        VLOG_DBG("updating interface %s (lacp_status:actor_system_id = %s)",
            idp->name, system_id);
    } else {
        free(system_id);
    }

    if (idp->actor.port_id == NULL ||
        strcmp(idp->actor.port_id, port_id) != 0) {
        smap_replace(&smap, INTERFACE_LACP_STATUS_MAP_ACTOR_PORT_ID, port_id);
        free(idp->actor.port_id);
        idp->actor.port_id = port_id;
        smap_changes = true;
        VLOG_DBG("updating interface %s (lacp_status:actor_port_id = %s)",
            idp->name, port_id);
    } else {
        free(port_id);
    }

    if (idp->actor.key == NULL ||
        strcmp(idp->actor.key, key) != 0) {
        smap_replace(&smap, INTERFACE_LACP_STATUS_MAP_ACTOR_KEY, key);
        free(idp->actor.key);
        idp->actor.key = key;
        smap_changes = true;
        VLOG_DBG("updating interface %s (lacp_status:actor_key = %s)",
            idp->name, key);
    } else {
        free(key);
    }

    if (idp->actor.state == NULL ||
        strcmp(idp->actor.state, state) != 0) {
        smap_replace(&smap, INTERFACE_LACP_STATUS_MAP_ACTOR_STATE, state);
        free(idp->actor.state);
        idp->actor.state = state;
        smap_changes = true;
        VLOG_DBG("updating interface %s (lacp_status:actor_state = %s)",
            idp->name, state);
    } else {
        free(state);
    }

    /* partner data */
    system_id = format_system_id(&plpinfo->partner_oper_system_variables);
    port_id = format_port_id(plpinfo->partner_oper_port_priority, plpinfo->partner_oper_port_number);
    key = format_key(plpinfo->partner_oper_key);
    state = format_state(plpinfo->partner_oper_port_state);

    if (idp->partner.system_id == NULL ||
        strcmp(idp->partner.system_id, system_id) != 0) {
        if (strncmp(system_id, NO_SYSTEM_ID, strlen(NO_SYSTEM_ID))) {
            if (portp &&
                log_event("LACP_PARTNER_DETECTED",
                          EV_KV("intf_id", "%s", idp->name),
                          EV_KV("lag_id", "%s",
                                portp->name +
                                    LAG_PORT_NAME_PREFIX_LENGTH),
                          EV_KV("partner_sys_id", "%s", system_id)) < 0) {
                VLOG_ERR("Could not log event LACP_PARTNER_DETECTED");
            }
        }

        smap_replace(&smap, INTERFACE_LACP_STATUS_MAP_PARTNER_SYSTEM_ID, system_id);
        free(idp->partner.system_id);
        idp->partner.system_id = system_id;
        smap_changes = true;
        VLOG_DBG("updating interface %s (lacp_status:partner_system_id = %s)",
            idp->name, system_id);
    } else {
        free(system_id);
    }

    if (idp->partner.port_id == NULL ||
        strcmp(idp->partner.port_id, port_id) != 0) {
        smap_replace(&smap, INTERFACE_LACP_STATUS_MAP_PARTNER_PORT_ID, port_id);
        free(idp->partner.port_id);
        idp->partner.port_id = port_id;
        smap_changes = true;
        VLOG_DBG("updating interface %s (lacp_status:partner_port_id = %s)",
            idp->name, port_id);
    } else {
        free(port_id);
    }

    if (idp->partner.key == NULL ||
        strcmp(idp->partner.key, key) != 0) {
        smap_replace(&smap, INTERFACE_LACP_STATUS_MAP_PARTNER_KEY, key);
        free(idp->partner.key);
        idp->partner.key = key;
        smap_changes = true;
        VLOG_DBG("updating interface %s (lacp_status:partner_key = %s)",
            idp->name, key);
    } else {
        free(key);
    }

    if (idp->partner.state == NULL ||
        strcmp(idp->partner.state, state) != 0) {
        smap_replace(&smap, INTERFACE_LACP_STATUS_MAP_PARTNER_STATE, state);
        free(idp->partner.state);
        idp->partner.state = state;
        smap_changes = true;
        VLOG_DBG("updating interface %s (lacp_status:partner_state = %s)",
            idp->name, state);
    } else {
        free(state);
    }

    if (smap_changes) {
        ovsrec_interface_set_lacp_status(ifrow, &smap);
        changes = true;
    }

    smap_destroy(&smap);

    /* lacp_current data */
    lacp_current = (plpinfo->recv_fsm_state == RECV_FSM_CURRENT_STATE);

    if (idp->lacp_current_set == false || idp->lacp_current != lacp_current) {
        ovsrec_interface_set_lacp_current(ifrow, &lacp_current, 1);
        VLOG_DBG("updating interface %s (lacp_current = %s)", idp->name,
            lacp_current ? "true" : "false");
        changes = true;
        idp->lacp_current = lacp_current;
        idp->lacp_current_set = true;
    }

    if (changes) {
        ovsdb_idl_txn_commit_block(txn);
    } else {
        ovsdb_idl_txn_abort(txn);
    }
    ovsdb_idl_txn_destroy(txn);

    if (portp) {
        if (plpinfo->lag != NULL) {
            portp->lag_member_speed = lport_type_to_speed(ntohs(plpinfo->lag->port_type));
        }
        db_update_port_status(portp);
    }

end:
    OVSDB_UNLOCK;
} /* db_update_interface */

/**********************************************************************
 * Pool implementation: this is diferrent from the LAG pool manager.
 * This is currently only used for allocating interface indexes.
 **********************************************************************/
int
allocate_next(unsigned char *pool, int size)
{
    int idx = 0;

    while (pool[idx] == 0xff && (idx * BITS_PER_BYTE) < size) {
        idx++;
    }

    if ((idx * BITS_PER_BYTE) < size) {
        idx *= BITS_PER_BYTE;

        while (idx < size && !IS_AVAILABLE(pool, idx)) {
            idx++;
        }

        if (idx < size && IS_AVAILABLE(pool, idx)) {
            SET(pool, idx);
            return idx;
        }
    }

    return -1;
} /* allocate_next */

static void
free_index(unsigned char *pool, int idx)
{
    CLEAR(pool, idx);
} /* free_index */

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
    poll_timer_wait(LACP_POLL_INTERVAL);
} /* lacpd_wait */

/**********************************************************************/
/* Interface attach/detach functions called from LACP state machine.  */
/**********************************************************************/
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

        update_member_interface_bond_status(idp->port_datap);
        update_port_bond_status_map_entry(idp->port_datap);
    }
    if (update_tx) {
        update_interface_hw_bond_config_map_entry(
            idp,
            INTERFACE_HW_BOND_CONFIG_MAP_TX_ENABLED,
            (tx_enabled ?
             INTERFACE_HW_BOND_CONFIG_MAP_ENABLED_TRUE :
             INTERFACE_HW_BOND_CONFIG_MAP_ENABLED_FALSE));

        update_member_interface_bond_status(idp->port_datap);
        update_port_bond_status_map_entry(idp->port_datap);
    }

    ovsdb_idl_txn_commit_block(txn);
    ovsdb_idl_txn_destroy(txn);
    OVSDB_UNLOCK;

} /* ops_intf_update_hw_bond_config */

void
ops_attach_port_in_hw(uint16_t lag_id, int port)
{
    struct iface_data *idp = NULL;

    VLOG_DBG("%s: lag_id=%d, port=%d", __FUNCTION__, lag_id, port);

    idp = find_iface_data_by_index(port);
    if (idp) {
        if (idp->lacp_state == LACP_STATE_ENABLED) {
            /* Attaching port means just RX. */
            lacpd_thread_intf_update_hw_bond_config(idp,
                                                    true,   /* update_rx */
                                                    true,   /* rx_enabled */
                                                    false,  /* update_tx */
                                                    false); /* tx_enabled */
        } else {
            VLOG_ERR("LACP state machine trying to attach port %d "
                     "when LACP is not enabled!", port);
        }
    } else {
        VLOG_ERR("Failed to find interface data for attaching port in hw. "
                 "port index=%d", port);
    }
} /* ops_attach_port_in_hw */

void
ops_detach_port_in_hw(uint16_t lag_id, int port)
{
    struct iface_data *idp = NULL;

    VLOG_DBG("%s: lag_id=%d, port=%d", __FUNCTION__, lag_id, port);

    idp = find_iface_data_by_index(port);
    if (idp) {
        if (idp->lacp_state == LACP_STATE_ENABLED) {
            /* Detaching port means both RX/TX are disabled. */
            lacpd_thread_intf_update_hw_bond_config(idp,
                                                    true,   /* update_rx */
                                                    false,  /* rx_enabled */
                                                    true,   /* update_tx */
                                                    false); /* tx_enabled */
        } else {
            /* Probably just a race condition between static <-> dynamic
             * LAG conversion.  Ignore the request. */
            VLOG_DBG("Ignoring detach port request from LACP state "
                     "machine. LACP is not enabled on %d", port);
        }
    } else {
        VLOG_ERR("Failed to find interface data for attaching port in hw. "
                 "port index=%d", port);
    }

} /* ops_detach_port_in_hw */

void
ops_trunk_port_egr_enable(uint16_t lag_id, int port)
{
    struct iface_data *idp = NULL;

    VLOG_DBG("%s: lag_id=%d, port=%d", __FUNCTION__, lag_id, port);

    idp = find_iface_data_by_index(port);
    if (idp) {
        if (idp->lacp_state == LACP_STATE_ENABLED) {
            /* Egress enable means TX. */
            lacpd_thread_intf_update_hw_bond_config(idp,
                                                    false, /* update_rx */
                                                    false, /* rx_enabled */
                                                    true,  /* update_tx */
                                                    true); /* tx_enabled */
        } else {
            VLOG_ERR("LACP state machine trying to enable egress on "
                     "port %d when LACP is not enabled!", port);
        }
    } else {
        VLOG_ERR("Failed to find interface data for egress enable. "
                 "port index=%d", port);
    }
} /* ops_trunk_port_egr_enable */

static void
db_update_port_status(struct port_data *portp)
{
    const struct ovsrec_port *prow;
    struct smap smap;
    struct ovsdb_idl_txn *txn;
    bool changed = false;
    char *speed_str;

    prow = portp->cfg;

    smap_clone(&smap, &prow->lacp_status);

    if (portp->lacp_mode == PORT_LACP_OFF && portp->current_status != STATUS_LACP_DISABLED)
    {
        smap_remove(&smap, PORT_LACP_STATUS_MAP_BOND_STATUS_REASON);
        smap_remove(&smap, PORT_LACP_STATUS_MAP_BOND_STATUS);

        if (shash_count(&portp->participant_ifs) == 0) {
            portp->lag_member_speed = 0;
        }

        portp->current_status = STATUS_LACP_DISABLED;
        changed = true;
    } else if (shash_count(&portp->participant_ifs) == 0) {

        if (portp->current_status != STATUS_DOWN) {
            /* record port as non-operational */
            VLOG_WARN("Port %s isn't operational - no interfaces working",
                      portp->name);

            smap_replace(&smap,
                         PORT_LACP_STATUS_MAP_BOND_STATUS_REASON,
                         "No operational interfaces in bond");

            smap_replace(&smap,
                         PORT_LACP_STATUS_MAP_BOND_STATUS,
                         PORT_LACP_STATUS_BOND_STATUS_DOWN);

            portp->lag_member_speed = 0;

            portp->current_status = STATUS_DOWN;
            changed = true;
        }

    } else if (shash_count(&portp->participant_ifs) == 1) {
        /* determine if interface is defaulted or not */
        struct shash_node *node;
        struct iface_data *idp;

        /* get interface name and data */
        node = shash_first(&portp->participant_ifs);
        idp = (struct iface_data *)node->data;

        if (idp->local_state.defaulted) {
            if (portp->current_status != STATUS_DEFAULTED) {
                smap_replace(&smap,
                             PORT_LACP_STATUS_MAP_BOND_STATUS_REASON,
                             "Remote LACP not responding on interfaces");

                smap_replace(&smap,
                             PORT_LACP_STATUS_MAP_BOND_STATUS,
                             PORT_LACP_STATUS_BOND_STATUS_DEFAULTED);

                portp->current_status = STATUS_DEFAULTED;
                changed = true;
            }
        } else {
            if (portp->current_status != STATUS_UP) {
                smap_remove(&smap,
                            PORT_LACP_STATUS_MAP_BOND_STATUS_REASON);

                smap_replace(&smap,
                             PORT_LACP_STATUS_MAP_BOND_STATUS,
                             PORT_LACP_STATUS_BOND_STATUS_OK);

                portp->current_status = STATUS_UP;
                changed = true;
            }
        }
    } else {
        /* more than one participant -> operational */
        if (portp->current_status != STATUS_UP) {
            smap_remove(&smap,
                        PORT_LACP_STATUS_MAP_BOND_STATUS_REASON);

            smap_replace(&smap,
                         PORT_LACP_STATUS_MAP_BOND_STATUS,
                         PORT_LACP_STATUS_BOND_STATUS_OK);

            portp->current_status = STATUS_UP;
            changed = true;
        }
    }

    /* update speed */
    asprintf(&speed_str, "%d", portp->lag_member_speed);
    if (portp->speed_str == NULL || strcmp(speed_str, portp->speed_str) != 0) {
        free(portp->speed_str);
        portp->speed_str = speed_str;
        smap_replace(&smap, PORT_LACP_STATUS_MAP_BOND_SPEED, speed_str);
        changed = true;
    } else {
        free(speed_str);
    }

    if (changed) {
        txn = ovsdb_idl_txn_create(idl);

        ovsrec_port_set_lacp_status(prow, &smap);
        update_port_bond_status_map_entry(portp);

        ovsdb_idl_txn_commit_block(txn);
        ovsdb_idl_txn_destroy(txn);
    }

    smap_destroy(&smap);
} /* db_update_port_status */

void
db_add_lag_port(uint16_t lag_id, int port, lacp_per_port_variables_t *plpinfo)
{
    struct port_data *portp;
    struct iface_data *idp;
    int index;

    OVSDB_LOCK;

    /* get port data */
    portp = find_port_data_by_lag_id(lag_id);

    if (portp == NULL) {
        VLOG_WARN("Port not configured for LACP! lag_id = %d", lag_id);
        goto end;
    }

    index = PM_HANDLE2PORT(plpinfo->lport_handle);
    idp = find_iface_data_by_index(index);

    if (idp == NULL) {
        VLOG_WARN("Interface not configured in LAG. lag_id = %d, port = %d",
                  lag_id, port);
        goto end;
    }

    idp->local_state = plpinfo->actor_oper_port_state;

    shash_add_once(&portp->participant_ifs, idp->name, idp);

    VLOG_DBG("Added interface (%d) to lag (%d): %d participants", port, lag_id, (int)shash_count(&portp->participant_ifs));

    if (plpinfo->lag != NULL) {
        portp->lag_member_speed = lport_type_to_speed(ntohs(plpinfo->lag->port_type));
        VLOG_DBG("setting speed: %d\n", portp->lag_member_speed);
    }

    db_update_port_status(portp);
end:
    OVSDB_UNLOCK;

} /* db_add_lag_port */

void
db_delete_lag_port(uint16_t lag_id, int port, lacp_per_port_variables_t *plpinfo)
{
    struct port_data *portp;
    struct iface_data *idp;
    struct shash_node *node;
    int index;
    struct ovsdb_idl_txn *txn = NULL;

    OVSDB_LOCK;

    index = PM_HANDLE2PORT(plpinfo->lport_handle);
    idp = find_iface_data_by_index(index);

    if (idp == NULL) {
        VLOG_WARN("Interface not configured in LAG. lag_id = %d, port = %d",
                  lag_id, port);
        goto end;
    }

    /* get port data */
    portp = find_port_data_by_lag_id(lag_id);

    if (portp == NULL) {
        VLOG_WARN("Port not configured for LACP! lag_id = %d", lag_id);
        txn = ovsdb_idl_txn_create(idl);

        db_clear_interface(idp);

        ovsdb_idl_txn_commit_block(txn);
        ovsdb_idl_txn_destroy(txn);

        goto end;
    }

    node = shash_find(&portp->participant_ifs, idp->name);
    if (!node) {
        VLOG_WARN("Interface %s is not in participant list for lag_id = %d", idp->name, lag_id);
        goto end;
    }
    shash_delete(&portp->participant_ifs, node);

    VLOG_DBG("Removed interface (%d) from lag (%d): %d participants",
             port, lag_id, (int)shash_count(&portp->participant_ifs));

    if (plpinfo->lag != NULL) {
        portp->lag_member_speed = lport_type_to_speed(plpinfo->lag->port_type);
        VLOG_DBG("setting speed: %d\n", portp->lag_member_speed);
    }

    db_update_port_status(portp);

end:
    OVSDB_UNLOCK;

} /* db_delete_lag_port */

void
db_clear_lag_partner_info_port(struct port_data *portp)
{
    const struct ovsrec_port *prow;
    struct smap smap;

    prow = portp->cfg;

    smap_init(&smap);

    /* set everything to empty */
    ovsrec_port_set_lacp_status(prow, &smap);

    smap_destroy(&smap);

    free(portp->speed_str);
    portp->speed_str = NULL;
    portp->current_status = STATUS_UNINITIALIZED;
}

void
db_clear_lag_partner_info(uint16_t lag_id)
{
    struct port_data *portp;
    struct ovsdb_idl_txn *txn = NULL;

    /* acquire lock */
    OVSDB_LOCK;

    /* get port */
    portp = find_port_data_by_lag_id(lag_id);

    if (portp == NULL) {
        VLOG_WARN("Updating port not configured for LACP! lag_id = %d", lag_id);
        goto end;
    }

    txn = ovsdb_idl_txn_create(idl);

    db_clear_lag_partner_info_port(portp);

    ovsdb_idl_txn_commit_block(txn);
    ovsdb_idl_txn_destroy(txn);

end:
    OVSDB_UNLOCK;
} /* db_clear_lag_partner_info */

void
db_update_lag_partner_info(uint16_t lag_id)
{
    const struct ovsrec_port *prow;
    struct port_data *portp;
    struct smap smap;
    struct ovsdb_idl_txn *txn = NULL;
    bool changes = false;
    char *speed_str;

    /* acquire lock */
    OVSDB_LOCK;

    /* get port */
    portp = find_port_data_by_lag_id(lag_id);

    if (portp == NULL) {
        VLOG_WARN("Updating port not configured for LACP! lag_id = %d", lag_id);
        goto end;
    }

    prow = portp->cfg;

    smap_clone(&smap, &prow->lacp_status);

    txn = ovsdb_idl_txn_create(idl);

    /* update speed */
    asprintf(&speed_str, "%d", portp->lag_member_speed);
    if (portp->speed_str == NULL || strcmp(speed_str, portp->speed_str) != 0) {
        free(portp->speed_str);
        portp->speed_str = speed_str;
        smap_replace(&smap, PORT_LACP_STATUS_MAP_BOND_SPEED, speed_str);
        changes = true;
    } else {
        free(speed_str);
    }

    if (changes) {
        ovsrec_port_set_lacp_status(prow, &smap);
        ovsdb_idl_txn_commit_block(txn);
    } else {
        ovsdb_idl_txn_abort(txn);
    }

    ovsdb_idl_txn_destroy(txn);

    smap_destroy(&smap);

end:
    OVSDB_UNLOCK;

} /* db_update_lag_partner_info */

/**********************************************************************/
/*                               DEBUG                                */
/**********************************************************************/
static char *
lacp_mode_str(enum ovsrec_port_lacp_e mode)
{
    switch (mode) {
    case PORT_LACP_OFF:     return OVSREC_PORT_LACP_OFF;
    case PORT_LACP_ACTIVE:  return OVSREC_PORT_LACP_ACTIVE;
    case PORT_LACP_PASSIVE: return OVSREC_PORT_LACP_PASSIVE;
    default:                return "???";
    }
} /* lacp_mode_str */

static void
lacpd_interface_dump(struct ds *ds, struct iface_data *idp)
{
    ds_put_format(ds, "Interface %s:\n", idp->name);
    ds_put_format(ds, "    link_state           : %s\n",
                  idp->link_state == INTERFACE_LINK_STATE_UP
                  ? OVSREC_INTERFACE_LINK_STATE_UP :
                  OVSREC_INTERFACE_LINK_STATE_DOWN);
    ds_put_format(ds, "    link_speed           : %d Mbps\n",
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

    if (argc > 2) { /* an interface is specified in argv */
        struct iface_data *idp =
            shash_find_data(&all_interfaces, argv[2]);
        if (idp){
            lacpd_interface_dump(ds, idp);
        }
    } else { /* dump all interfaces */
        ds_put_cstr(ds, "================ Interfaces ================\n");

        SHASH_FOR_EACH(sh_node, &all_interfaces) {
            idp = sh_node->data;
            if (idp){
                lacpd_interface_dump(ds, idp);
            }
        }
    }
} /* lacpd_interfaces_dump */

/**
 * @details
 * Dumps the configured, eligible and participant interfaces of one lag
 * specified in portp parameter.
 */
static void
lacpd_lag_member_interfaces_dump(struct ds *ds, struct port_data *portp)
{
    struct shash_node *node;

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

    ds_put_format(ds, "    participant_members  :");
    SHASH_FOR_EACH(node, &portp->participant_ifs) {
        struct iface_data *idp = shash_find_data(&all_interfaces, node->name);
        if (idp) {
            ds_put_format(ds, " %s", idp->name);
        }
    }
    ds_put_format(ds, "\n");
} /* lacpd_lag_member_interfaces_dump */

static void
lacpd_port_dump(struct ds *ds, struct port_data *portp)
{
    ds_put_format(ds, "Port %s:\n", portp->name);
    ds_put_format(ds, "    lacp                 : %s\n",
                  lacp_mode_str(portp->lacp_mode));
    ds_put_format(ds, "    lag_member_speed     : %d\n",
                  portp->lag_member_speed);
    lacpd_lag_member_interfaces_dump(ds, portp);
    ds_put_format(ds, "    interface_count      : %d\n",
                  (int)shash_count(&portp->participant_ifs));
} /* lacpd_port_dump */

static void
lacpd_ports_dump(struct ds *ds, int argc, const char *argv[])
{
    struct shash_node *sh_node;
    struct port_data *portp = NULL;

    if (argc > 2) { /* a port is specified in argv */
        portp = shash_find_data(&all_ports, argv[2]);
        if (portp){
            lacpd_port_dump(ds, portp);
        }
    } else { /* dump all ports */
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

/**
 * @details
 * Dumps debug data for all the LAG ports in the daemon or for an individual
 * specified port.
 */
void
lacpd_lag_ports_dump(struct ds *ds, int argc, const char *argv[])
{
    struct shash_node *sh_node;
    struct port_data *portp = NULL;

    if (argc > 1) { /* a lag is specified in argv */
        portp = shash_find_data(&all_ports, argv[1]);
        if (portp) {
            if (!strncmp(portp->name, LAG_PORT_NAME_PREFIX, LAG_PORT_NAME_PREFIX_LENGTH)) {
                ds_put_format(ds, "Port %s:\n", portp->name);
                lacpd_lag_member_interfaces_dump(ds, portp);
            }
        }
    } else { /* dump all ports */
        SHASH_FOR_EACH(sh_node, &all_ports) {
            portp = sh_node->data;
            if (portp) {
                if (!strncmp(portp->name, LAG_PORT_NAME_PREFIX, LAG_PORT_NAME_PREFIX_LENGTH)) {
                    ds_put_format(ds, "Port %s:\n", portp->name);
                    lacpd_lag_member_interfaces_dump(ds, portp);
                }
            }
        }
    }
} /* lacpd_dump_lag_interfaces */


/**
 * @details
 * The idea of this code is to make the match between two structs:
 *   1. port_data which contains the port data of a LAG including the list of
 *      all the configured interfaces.
 *   2. lacp_per_port_variables_t which contains the pdu counters for each
 *      interface member of a lag.
 * We go through all the configured interfaces for the lag specified in the
 * parameter portp, and we look for an interface in lacp_per_port_vars_tree
 * with the same port number. Once we find it we print the pdu counters.
*/
void lacpd_dump_pdus_per_interface(struct ds *ds, struct port_data *portp)
{
    struct shash_node *node;
    lacp_per_port_variables_t *lacp_port_variable;
    int port_number_iface_data;
    int port_number_lacp_port_variable;

    ds_put_format(ds, " Configured interfaces:\n");

    /*Go through all the configured interfaces*/
    SHASH_FOR_EACH(node, &portp->cfg_member_ifs) {
        struct iface_data *idp = shash_find_data(&all_interfaces, node->name);
        if (idp) {
            /* This port_id has this format "port_priority,port_id" and
             * is formed using the function format_port_id defined in this file. */
            const char* port_id_char = idp->actor.port_id;
            int temp_port_priority;
            sscanf(port_id_char, "%d,%d", &temp_port_priority, &port_number_iface_data);
            /* The port_number is equal to the port_id minus one*/
            port_number_iface_data--;

            RENTRY();
            /* Go through lacp_per_port_vars_tree looking for a
             * lacp_per_port_variables_t corresponding to the port number we
             * found before. */
            lacp_port_variable = LACP_AVL_FIRST(lacp_per_port_vars_tree);
            while (lacp_port_variable) {

                port_number_lacp_port_variable = PM_HANDLE2PORT(lacp_port_variable->lport_handle);

                if (port_number_lacp_port_variable == port_number_iface_data) {
                    ds_put_format(ds, "  Interface: %s\n", idp->name);
                    ds_put_format(ds, "    lacp_pdus_sent: %d\n",
                                  lacp_port_variable->lacp_pdus_sent);
                    ds_put_format(ds, "    marker_response_pdus_sent: %d\n",
                                  lacp_port_variable->marker_response_pdus_sent);
                    ds_put_format(ds, "    lacp_pdus_received: %d\n",
                                  lacp_port_variable->lacp_pdus_received);
                    ds_put_format(ds, "    marker_pdus_received: %d\n",
                                  lacp_port_variable->marker_pdus_received);
                    break;
                }
                lacp_port_variable = LACP_AVL_NEXT(lacp_port_variable->avlnode);
            }
            REXIT();
        }
    }
}/* lacpd_dump_pdus_per_interface */

/**
 * @details
 * Dumps the PDU counters for all the LAG ports in the daemon or for
 * an individual specified port.
 */
void
lacpd_pdus_counters_dump(struct ds *ds, int argc, const char *argv[])
{
    struct shash_node *sh_node;
    struct port_data *portp = NULL;

    if (argc > 1) { /* a lag is specified in argv */
        portp = shash_find_data(&all_ports, argv[1]);
        if (portp){
            if (!strncmp(portp->name,
                         LAG_PORT_NAME_PREFIX,
                         LAG_PORT_NAME_PREFIX_LENGTH)
                && portp->lacp_mode != PORT_LACP_OFF) {

                ds_put_format(ds, "LAG %s:\n", portp->name);
                lacpd_dump_pdus_per_interface(ds, portp);
            }
        }
    } else { /* dump all ports */
        SHASH_FOR_EACH(sh_node, &all_ports) {
            portp = sh_node->data;
            if (portp) {
                if (!strncmp(portp->name,
                             LAG_PORT_NAME_PREFIX,
                             LAG_PORT_NAME_PREFIX_LENGTH)
                    && portp->lacp_mode != PORT_LACP_OFF) {

                    ds_put_format(ds, "LAG %s:\n", portp->name);
                    lacpd_dump_pdus_per_interface(ds, portp);
                }
            }
        }
    }
}/* lacpd_pdus_counters_dump */

/**
 * @details
 * The idea of this code is to make the match between two structs:
 *   1. port_data which contains the port data of a LAG including the list of
 *      all the configured interfaces.
 *   2. lacp_per_port_variables_t which contains lacpd state machine control
 *      variables and the state parameters for each interface member of a lag.
 * We go through all the configured interfaces for the lag specified in
 * the parameter portp, and we look for an interface in
 * lacp_per_port_vars_tree with the same port number. Once we find it we print
 * the lacpd state.
*/
void lacpd_dump_state_per_interface(struct ds *ds, struct port_data *portp)
{
    struct shash_node *node;
    lacp_per_port_variables_t *lacp_port_variable;
    struct iface_data *idp;
    const char* port_id_char;
    int port_number_iface_data;
    int port_number_lacp_port_variable;
    int temp_port_priority;

    ds_put_format(ds, " Configured interfaces:\n");

    /* Go through all the configured interfaces */
    SHASH_FOR_EACH(node, &portp->cfg_member_ifs) {
        idp = shash_find_data(&all_interfaces, node->name);
        if (idp) {
            /* This port_id has this format "port_priority,port_id" and
             * is formed using the function format_port_id defined in this file. */
            port_id_char = idp->actor.port_id;
            sscanf(port_id_char, "%d,%d", &temp_port_priority, &port_number_iface_data);
            /* The port_number is equal to the port_id minus one*/
            port_number_iface_data--;

            RENTRY();
            /* Go through lacp_per_port_vars_tree looking for a lacp_per_port_variables_t
             * corresponding to the port number we found before. */
            lacp_port_variable = LACP_AVL_FIRST(lacp_per_port_vars_tree);
            while (lacp_port_variable) {

                port_number_lacp_port_variable = PM_HANDLE2PORT(lacp_port_variable->lport_handle);

                if (port_number_lacp_port_variable == port_number_iface_data) {
                    ds_put_format(ds, "  Interface: %s\n", idp->name);

                    ds_put_format(ds, "    actor_oper_port_state \n");
                    ds_put_format(ds, "       lacp_activity:%d time_out:%d aggregation:%d sync:%d collecting:%d distributing:%d defaulted:%d expired:%d\n",
                                    lacp_port_variable->actor_oper_port_state.lacp_activity,
                                    lacp_port_variable->actor_oper_port_state.lacp_timeout,
                                    lacp_port_variable->actor_oper_port_state.aggregation,
                                    lacp_port_variable->actor_oper_port_state.synchronization,
                                    lacp_port_variable->actor_oper_port_state.collecting,
                                    lacp_port_variable->actor_oper_port_state.distributing,
                                    lacp_port_variable->actor_oper_port_state.defaulted,
                                    lacp_port_variable->actor_oper_port_state.expired);
                    ds_put_format(ds, "    partner_oper_port_state \n");
                    ds_put_format(ds, "       lacp_activity:%d time_out:%d aggregation:%d sync:%d collecting:%d distributing:%d defaulted:%d expired:%d\n",
                                    lacp_port_variable->partner_oper_port_state.lacp_activity,
                                    lacp_port_variable->partner_oper_port_state.lacp_timeout,
                                    lacp_port_variable->partner_oper_port_state.aggregation,
                                    lacp_port_variable->partner_oper_port_state.synchronization,
                                    lacp_port_variable->partner_oper_port_state.collecting,
                                    lacp_port_variable->partner_oper_port_state.distributing,
                                    lacp_port_variable->partner_oper_port_state.defaulted,
                                    lacp_port_variable->partner_oper_port_state.expired);
                    ds_put_format(ds, "    lacp_control\n");
                    ds_put_format(ds, "       begin:%d actor_churn:%d partner_churn:%d ready_n:%d selected:%d port_moved:%d ntt:%d port_enabled:%d\n",
                                    lacp_port_variable->lacp_control.begin,
                                    lacp_port_variable->lacp_control.actor_churn,
                                    lacp_port_variable->lacp_control.partner_churn,
                                    lacp_port_variable->lacp_control.ready_n,
                                    lacp_port_variable->lacp_control.selected,
                                    lacp_port_variable->lacp_control.port_moved,
                                    lacp_port_variable->lacp_control.ntt,
                                    lacp_port_variable->lacp_control.port_enabled);
                    break;
                }
                lacp_port_variable = LACP_AVL_NEXT(lacp_port_variable->avlnode);
            }
            REXIT();
        }
    }
}/* lacpd_dump_state_per_interface */

/**
 * @details
 * Dumps the LACP state machine status for all the LAG ports in the daemon or for
 * an individual specified port.
 */
void
lacpd_state_dump(struct ds *ds, int argc, const char *argv[])
{
    struct shash_node *sh_node;
    struct port_data *portp = NULL;

    if (argc > 1) { /* a lag is specified in argv */
        portp = shash_find_data(&all_ports, argv[1]);
        if (portp) {
            if (!strncmp(portp->name,
                         LAG_PORT_NAME_PREFIX,
                         LAG_PORT_NAME_PREFIX_LENGTH)
                && portp->lacp_mode != PORT_LACP_OFF) {

                ds_put_format(ds, "LAG %s:\n", portp->name);
                lacpd_dump_state_per_interface(ds, portp);
            }
        }
    } else { /* dump all ports */
        SHASH_FOR_EACH(sh_node, &all_ports) {
            portp = sh_node->data;
            if (portp) {
                if (!strncmp(portp->name,
                             LAG_PORT_NAME_PREFIX,
                             LAG_PORT_NAME_PREFIX_LENGTH)
                    && portp->lacp_mode != PORT_LACP_OFF) {

                    ds_put_format(ds, "LAG %s:\n", portp->name);
                    lacpd_dump_state_per_interface(ds, portp);
                }
            }
        }
    }
}/* lacpd_state_dump */

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

    /* OPS_TODO -- need to tell main loop to exit... */

    return NULL;

} /* lacpd_ovs_main_thread */

/**@} end of lacpd group */
