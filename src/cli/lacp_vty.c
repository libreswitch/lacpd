/* LACP CLI commands
 *
 * Copyright (C) 1997, 98 Kunihiro Ishiguro
 * Copyright (C) 2015-2016 Hewlett Packard Enterprise Development LP
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * File: lacp_vty.c
 *
 * Purpose:  To add LACP CLI configuration and display commands.
 */

#include <sys/un.h>
#include <setjmp.h>
#include <sys/wait.h>
#include <pwd.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "vtysh/lib/version.h"
#include "getopt.h"
#include "vtysh/command.h"
#include "vtysh/memory.h"
#include "vtysh/vtysh.h"
#include "vtysh/vtysh_user.h"
#include "vswitch-idl.h"
#include "ovsdb-idl.h"
#include "smap.h"
#include "openvswitch/vlog.h"
#include "openswitch-idl.h"
#include "vtysh/vtysh_ovsdb_if.h"
#include "vtysh/vtysh_ovsdb_config.h"
#include "openswitch-dflt.h"
#include "lacp_vty.h"
#include "vtysh/utils/lacp_vtysh_utils.h"
#include "vtysh/utils/vrf_vtysh_utils.h"
#include "vtysh/utils/vlan_vtysh_utils.h"
#include "vtysh/utils/l3_vtysh_utils.h"
#include "vtysh_ovsdb_intf_lag_context.h"
#include "vrf-utils.h"

VLOG_DEFINE_THIS_MODULE(vtysh_lacp_cli);
extern struct ovsdb_idl *idl;

/**
 * This function will check if a given port row has any acl
 * configuration i.e. acl applied in hw or acl configured by
 * user.
 *
 * @param port_row port table row to check for acl configuration
 *
 * @return true if acl configuration is detected, otherwise returns
 *              false.
 */
bool
is_acl_configured(const struct ovsrec_port *port_row)
{

  ovs_assert(port_row);
  /* Check all acl columns,
   * aclv4_in_applied - indicates currently applied acls
   * aclv4_in_cfg - indicates acl configuration requested by user
   */
  if (port_row->aclv4_in_applied || port_row->aclv4_in_cfg) {

      return true;
  }

  return false;
}

bool
lacp_exceeded_maximum_lag()
{
  const struct ovsrec_port *port_row = NULL;
  int lags_found = 0;

  OVSREC_PORT_FOR_EACH(port_row, idl) {
      if (strncmp(port_row->name,
                  LAG_PORT_NAME_PREFIX,
                  LAG_PORT_NAME_PREFIX_LENGTH) == 0) {
          lags_found++;
      }
  }

  return lags_found >= MAX_LAG_INTERFACES;
}

char *
lacp_remove_lb_hash_suffix(const char * lb_hash) {
    char * temp_hash_suffix = NULL;
    char * temp_hash = NULL;

    if (lb_hash) {
        temp_hash = strdup(lb_hash);
        temp_hash_suffix = strstr(temp_hash, OVSDB_LB_HASH_SUFFIX);
        if (temp_hash_suffix) {
            temp_hash_suffix[0] = '\0';
        }
    }

    return temp_hash;
}

static struct cmd_node link_aggregation_node =
{
  LINK_AGGREGATION_NODE,
  "%s(config-lag-if)# ",
};

DEFUN (vtysh_intf_link_aggregation,
       vtysh_intf_link_aggregation_cmd,
       "interface lag <1-2000>",
       "Select an interface to configure\n"
       "Configure link-aggregation parameters\n"
       "LAG number ranges from 1 to 2000\n")
{
  const struct ovsrec_port *port_row = NULL;
  bool port_found = false;
  struct ovsdb_idl_txn *txn = NULL;
  enum ovsdb_idl_txn_status status_txn;
  static char lag_number[LAG_NAME_LENGTH]={0};
  const struct ovsrec_vrf *default_vrf_row = NULL;
  const struct ovsrec_vrf *vrf_row = NULL;
  int i=0;
  struct ovsrec_port **ports = NULL;

  snprintf(lag_number, LAG_NAME_LENGTH, "%s%s","lag", argv[0]);

  OVSREC_PORT_FOR_EACH(port_row, idl)
  {
    if (strcmp(port_row->name, lag_number) == 0)
    {
      port_found = true;
      break;
    }
  }

  if(!port_found)
  {
    if(lacp_exceeded_maximum_lag())
    {
      vty_out(vty, "Cannot create LAG interface."
              "Maximum LAG interface count is already reached.%s",VTY_NEWLINE);
      return CMD_SUCCESS;
    }
    txn = cli_do_config_start();
    if (txn == NULL)
    {
      VLOG_DBG("Transaction creation failed by %s. Function=%s, Line=%d",
               " cli_do_config_start()", __func__, __LINE__);
          cli_do_config_abort(txn);
          return CMD_OVSDB_FAILURE;
    }

    port_row = ovsrec_port_insert(txn);
    ovsrec_port_set_name(port_row, lag_number);

    OVSREC_VRF_FOR_EACH (vrf_row, idl)
    {
        if (strcmp(vrf_row->name, DEFAULT_VRF_NAME) == 0) {
           default_vrf_row = vrf_row;
            break;
        }
    }

    if(default_vrf_row == NULL)
    {
      assert(0);
      VLOG_DBG("Couldn't fetch default VRF row. Function=%s, Line=%d",
                __func__, __LINE__);
      cli_do_config_abort(txn);
      return CMD_OVSDB_FAILURE;
    }

    ports = xmalloc(sizeof *default_vrf_row->ports *
                   (default_vrf_row->n_ports + 1));
    for (i = 0; i < default_vrf_row->n_ports; i++)
    {
      ports[i] = default_vrf_row->ports[i];
    }
    ports[default_vrf_row->n_ports] = CONST_CAST(struct ovsrec_port*,port_row);
    ovsrec_vrf_set_ports(default_vrf_row, ports,
                         default_vrf_row->n_ports + 1);
    free(ports);

    status_txn = cli_do_config_finish(txn);
    if(status_txn == TXN_SUCCESS || status_txn == TXN_UNCHANGED)
    {
      vty->node = LINK_AGGREGATION_NODE;
      vty->index = lag_number;
      return CMD_SUCCESS;
    }
    else
    {
      VLOG_ERR("Transaction commit failed in function=%s, line=%d",__func__,__LINE__);
      return CMD_OVSDB_FAILURE;
    }
  }
  else
  {
    vty->node = LINK_AGGREGATION_NODE;
    vty->index = lag_number;
    return CMD_SUCCESS;
  }
}

DEFUN (vtysh_exit_lac_interface,
             vtysh_exit_lacp_interface_cmd,
             "exit",
             "Exit current mode and down to previous mode\n")
{
  return vtysh_exit (vty);
}

static int
delete_lag(const char *lag_name)
{
  const struct ovsrec_vrf *vrf_row = NULL;
  const struct ovsrec_bridge *bridge_row = NULL;
  const struct ovsrec_port *lag_port_row = NULL;
  const struct ovsrec_interface *interface_row = NULL;
  bool lag_to_vrf = false;
  bool lag_to_bridge = false;
  struct ovsrec_port **ports;
  int k=0, n=0, i=0, intf_index=0;
  struct ovsdb_idl_txn* status_txn = NULL;
  enum ovsdb_idl_txn_status status;
  bool port_found = false;

  struct smap smap = SMAP_INITIALIZER(&smap);

  /* Return if LAG port doesn't exit */
  OVSREC_PORT_FOR_EACH(lag_port_row, idl)
  {
    if (strcmp(lag_port_row->name, lag_name) == 0)
    {
       port_found = true;
    }
  }


  if(!port_found)
  {
    vty_out(vty, "Specified LAG port doesn't exist.%s",VTY_NEWLINE);
    return CMD_SUCCESS;
  }

  /* Check if the given LAG port is part of VRF */
  OVSREC_VRF_FOR_EACH (vrf_row, idl)
  {
    for (k = 0; k < vrf_row->n_ports; k++)
    {
       lag_port_row = vrf_row->ports[k];
       if(strcmp(lag_port_row->name, lag_name) == 0)
       {
          lag_to_vrf = true;
          goto port_attached;
       }
    }
  }

   /* Check if the given LAG port is part of bridge */
  OVSREC_BRIDGE_FOR_EACH (bridge_row, idl)
  {
    for (k = 0; k < bridge_row->n_ports; k++)
    {
       lag_port_row = bridge_row->ports[k];
       if(strcmp(lag_port_row->name, lag_name) == 0)
       {
          lag_to_bridge = true;
          goto port_attached;
       }
    }
  }

port_attached:
  if(lag_to_vrf || lag_to_bridge)
  {
    /* LAG port is attached to either VRF or bridge.
     * So create transaction.                    */
    status_txn = cli_do_config_start();
    if(status_txn == NULL)
    {
      VLOG_ERR(LACP_OVSDB_TXN_CREATE_ERROR,__func__,__LINE__);
      cli_do_config_abort(status_txn);
      return CMD_OVSDB_FAILURE;
    }
  }
  else
  {
    /* assert if the LAG port is not part of either VRF or bridge */
    assert(0);
    VLOG_ERR("Port table entry not found either in VRF or in bridge."
              "Function=%s, Line=%d", __func__,__LINE__);
    return CMD_OVSDB_FAILURE;
  }

  /* Remove Aggregation-key and user configuration*/
  for (intf_index=0; intf_index < lag_port_row->n_interfaces; intf_index++)
  {
      interface_row = lag_port_row->interfaces[intf_index];

      /* Remove Aggregation-key */
      smap_clone(&smap, &interface_row->other_config);
      smap_remove(&smap, INTERFACE_OTHER_CONFIG_MAP_LACP_AGGREGATION_KEY);
      ovsrec_interface_set_other_config(interface_row, &smap);
      smap_clear(&smap);

      /* Remove User configuration */
      smap_clone(&smap, &interface_row->user_config);
      smap_remove(&smap, INTERFACE_USER_CONFIG_MAP_ADMIN);
      ovsrec_interface_set_user_config(interface_row, &smap);
      smap_clear(&smap);
  }
  smap_destroy(&smap);

  if(lag_to_vrf)
  {
    /* Delete the LAG port reference from VRF */
    ports = xmalloc(sizeof *vrf_row->ports * (vrf_row->n_ports-1));
    for(i = n = 0; i < vrf_row->n_ports; i++)
    {
       if(vrf_row->ports[i] != lag_port_row)
       {
          ports[n++] = vrf_row->ports[i];
       }
    }
    ovsrec_vrf_set_ports(vrf_row, ports, n);
    free(ports);
  }
  else if(lag_to_bridge)
  {
    /* Delete the LAG port reference from Bridge */
    ports = xmalloc(sizeof *bridge_row->ports * (bridge_row->n_ports-1));
    for(i = n = 0; i < bridge_row->n_ports; i++)
    {
       if(bridge_row->ports[i] != lag_port_row)
       {
          ports[n++] = bridge_row->ports[i];
       }
    }
    ovsrec_bridge_set_ports(bridge_row, ports, n);
    free(ports);
  }

  /* Delete the port as we cleared references to it from VRF or bridge*/
  ovsrec_port_delete(lag_port_row);

  status = cli_do_config_finish(status_txn);

  if(status == TXN_SUCCESS || status == TXN_UNCHANGED)
  {
     return CMD_SUCCESS;
  }
  else
  {
     VLOG_ERR(LACP_OVSDB_TXN_COMMIT_ERROR,__func__,__LINE__);
     return CMD_OVSDB_FAILURE;
  }
}

DEFUN (vtysh_remove_lag,
       vtysh_remove_lag_cmd,
       "no interface lag <1-2000>",
        NO_STR
        INTERFACE_NO_STR
       "Configure link-aggregation parameters\n"
       "LAG number ranges from 1 to 2000\n")
{
  char lag_number[LAG_NAME_LENGTH]={0};
  /* Form lag name in the form of lag1,lag2,etc */
  snprintf(lag_number, LAG_NAME_LENGTH, "%s%s",LAG_PORT_NAME_PREFIX, argv[0]);
  return delete_lag(lag_number);
}


static int
lacp_set_mode(const char *lag_name, const char *mode_to_set, const char *present_mode)
{
  const struct ovsrec_port *port_row = NULL;
  bool port_found = false;
  struct ovsdb_idl_txn* txn = NULL;
  enum ovsdb_idl_txn_status status;

  txn = cli_do_config_start();
  if(txn == NULL)
  {
    VLOG_ERR(LACP_OVSDB_TXN_CREATE_ERROR,__func__,__LINE__);
    cli_do_config_abort(txn);
    return CMD_OVSDB_FAILURE;
  }

  OVSREC_PORT_FOR_EACH(port_row, idl)
  {
    if (strcmp(port_row->name, lag_name) == 0)
    {
      port_found = true;
      break;
    }
  }

  if(!port_found)
  {
    /* assert - as LAG port should be present in DB. */
    assert(0);
    VLOG_ERR("Port table entry not found in DB.Function=%s, Line=%d", __func__,__LINE__);
    cli_do_config_abort(txn);
    return CMD_OVSDB_FAILURE;
  }

  if (strcmp(OVSREC_PORT_LACP_OFF, mode_to_set) == 0)
  {
    if (port_row->lacp && strcmp(port_row->lacp, present_mode) == 0)
    {
      ovsrec_port_set_lacp(port_row, OVSREC_PORT_LACP_OFF);
    }
    else
    {
      vty_out(vty, "Enter the configured LACP mode.\n");
      cli_do_config_abort(txn);
      return CMD_SUCCESS;
    }
  }
  ovsrec_port_set_lacp(port_row, mode_to_set);

  status = cli_do_config_finish(txn);
  if(status == TXN_SUCCESS || status == TXN_UNCHANGED)
  {
    return CMD_SUCCESS;
  }
  else
  {
    VLOG_ERR("Transaction commit failed.Function=%s Line=%d", __func__,__LINE__);
    return CMD_OVSDB_FAILURE;
  }
}

DEFUN (cli_lacp_set_mode,
       lacp_set_mode_cmd,
       "lacp mode (active | passive)",
        LACP_STR
       "Configure LACP mode(Default:off)\n"
       "Sets an interface as LACP active\n"
       "Sets an interface as LACP passive\n")
{
  return lacp_set_mode((char*) vty->index, argv[0], "");
}

DEFUN (cli_lacp_set_no_mode,
       lacp_set_mode_no_cmd,
       "no lacp mode (active | passive)",
       NO_STR
       LACP_STR
       "Configure LACP mode(Default:off)\n"
       "Sets an interface as LACP active\n"
       "Sets an interface as LACP passive\n")
{
  return lacp_set_mode((char*) vty->index, "off", argv[0]);
}

static int
lacp_set_hash(const char *lag_name, const char *hash)
{
  const struct ovsrec_port *port_row = NULL;
  bool port_found = false;
  struct smap smap = SMAP_INITIALIZER(&smap);
  struct ovsdb_idl_txn* txn = NULL;
  enum ovsdb_idl_txn_status status;

  txn = cli_do_config_start();
  if(txn == NULL)
  {
    VLOG_ERR(LACP_OVSDB_TXN_CREATE_ERROR,__func__,__LINE__);
    cli_do_config_abort(txn);
    return CMD_OVSDB_FAILURE;
  }
  OVSREC_PORT_FOR_EACH(port_row, idl)
  {
    if (strcmp(port_row->name, lag_name) == 0)
    {
      port_found = true;
      break;
    }
  }

  if(!port_found)
  {
    /* assert - as LAG port should be present in DB. */
    assert(0);
    cli_do_config_abort(txn);
    VLOG_ERR("Port table entry not found in DB.Function=%s Line=%d",__func__,__LINE__);
    return CMD_OVSDB_FAILURE;
  }
  smap_clone(&smap, &port_row->other_config);

  if(strncmp(OVSDB_LB_L3_HASH, hash, strlen(OVSDB_LB_L3_HASH)) == 0)
    smap_remove(&smap, "bond_mode");
  else
    smap_replace(&smap, "bond_mode", hash);

  ovsrec_port_set_other_config(port_row, &smap);
  smap_destroy(&smap);
  status = cli_do_config_finish(txn);
  if(status == TXN_SUCCESS || status == TXN_UNCHANGED)
  {
    return CMD_SUCCESS;
  }
  else
  {
    VLOG_ERR("Transaction commit failed.Function=%s Line=%d",__func__,__LINE__);
    return CMD_OVSDB_FAILURE;
  }
}

DEFUN (cli_lacp_set_l2_hash,
       lacp_set_l2_hash_cmd,
       "hash l2-src-dst",
       "The type of hash algorithm used for aggregated port (Default:l3-src-dst)\n"
       "Base the hash on l2-src-dst\n")
{
  return lacp_set_hash((char*) vty->index, OVSDB_LB_L2_HASH);
}

DEFUN (cli_lacp_set_l3_hash,
       lacp_set_l3_hash_cmd,
       "hash l3-src-dst",
       "The type of hash algorithm used for aggregated port (Default:l3-src-dst)\n"
       "Base the hash on l3-src-dst\n")
{
  return lacp_set_hash((char*) vty->index, OVSDB_LB_L3_HASH);
}

DEFUN (cli_lacp_set_l4_hash,
       lacp_set_l4_hash_cmd,
       "hash l4-src-dst",
       "The type of hash algorithm used for aggregated port (Default:l3-src-dst)\n"
       "Base the hash on l4-src-dst\n")
{
  return lacp_set_hash((char*) vty->index, OVSDB_LB_L4_HASH);
}

static int
lacp_set_fallback(const char *lag_name, bool fallback_enabled)
{
    const struct ovsrec_port *port_row = NULL;
    bool port_found = false;
    struct smap smap = SMAP_INITIALIZER(&smap);
    struct ovsdb_idl_txn* txn = NULL;
    enum ovsdb_idl_txn_status status;

    txn = cli_do_config_start();
    if (txn == NULL) {
        VLOG_ERR(LACP_OVSDB_TXN_CREATE_ERROR,__func__,__LINE__);
        cli_do_config_abort(txn);
        return CMD_OVSDB_FAILURE;
    }

    OVSREC_PORT_FOR_EACH(port_row, idl) {
        if (strncmp(lag_name,
                    port_row->name,
                    strlen(lag_name)) == 0) {
            port_found = true;
            break;
        }
    }

    if (!port_found) {
        /* assert - as LAG port should be present in DB. */
        assert(0);
        VLOG_ERR("Port table entry not found in DB.Function=%s Line=%d",
                 __func__,__LINE__);
        cli_do_config_abort(txn);
        return CMD_OVSDB_FAILURE;
    }

    smap_clone(&smap, &port_row->other_config);

    if (fallback_enabled) {
        smap_replace(&smap,
                     PORT_OTHER_CONFIG_LACP_FALLBACK,
                     PORT_OTHER_CONFIG_LACP_FALLBACK_ENABLED);
    } else {
        smap_remove(&smap, PORT_OTHER_CONFIG_LACP_FALLBACK);
    }

    ovsrec_port_set_other_config(port_row, &smap);
    smap_destroy(&smap);
    status = cli_do_config_finish(txn);

    if(status == TXN_SUCCESS || status == TXN_UNCHANGED) {
        return CMD_SUCCESS;
    } else {
        VLOG_ERR("Transaction commit failed.Function=%s Line=%d",
                 __func__,__LINE__);
        return CMD_OVSDB_FAILURE;
    }
}

DEFUN (cli_lacp_set_fallback,
       lacp_set_fallback_cmd,
       "lacp fallback",
       LACP_STR
       "Enable LACP fallback mode\n")
{
    return lacp_set_fallback((char*) vty->index, true);
}

DEFUN (cli_lacp_set_no_fallback,
       lacp_set_no_fallback_cmd,
       "no lacp fallback",
       NO_STR
       LACP_STR
       "Enable LACP fallback mode\n")
{
    return lacp_set_fallback((char*) vty->index, false);
}

static int
lacp_set_heartbeat_rate(const char *lag_name, const char *rate)
{
  const struct ovsrec_port *port_row = NULL;
  bool port_found = false;
  struct smap smap = SMAP_INITIALIZER(&smap);
  struct ovsdb_idl_txn* txn = NULL;
  enum ovsdb_idl_txn_status status;

  txn = cli_do_config_start();
  if(txn == NULL)
  {
    VLOG_ERR(LACP_OVSDB_TXN_CREATE_ERROR,__func__,__LINE__);
    cli_do_config_abort(txn);
    return CMD_OVSDB_FAILURE;
  }

  OVSREC_PORT_FOR_EACH(port_row, idl)
  {
    if (strcmp(port_row->name, lag_name) == 0)
    {
      port_found = true;
      break;
    }
  }

  if(!port_found)
  {
    /* assert - as LAG port should be present in DB. */
    assert(0);
    cli_do_config_abort(txn);
    VLOG_ERR("Port table entry not found in DB.Function=%s Line=%d",__func__,__LINE__);
    return CMD_OVSDB_FAILURE;
  }

  smap_clone(&smap, &port_row->other_config);

  if(strcmp(PORT_OTHER_CONFIG_LACP_TIME_SLOW, rate) == 0)
  {
    smap_remove(&smap, PORT_OTHER_CONFIG_MAP_LACP_TIME);
  }
  else
  {
    smap_replace(&smap, PORT_OTHER_CONFIG_MAP_LACP_TIME, rate);
  }

  ovsrec_port_set_other_config(port_row, &smap);
  smap_destroy(&smap);
  status = cli_do_config_finish(txn);
  if(status == TXN_SUCCESS || status == TXN_UNCHANGED)
  {
    return CMD_SUCCESS;
  }
  else
  {
    VLOG_ERR("Transaction commit failed.Function=%s Line=%d",__func__,__LINE__);
    return CMD_OVSDB_FAILURE;
  }
}

DEFUN (cli_lacp_set_heartbeat_rate,
       lacp_set_heartbeat_rate_cmd,
       "lacp rate (slow|fast)",
       LACP_STR
       "Set LACP heartbeat request time\n"
       "Default heartbeats rate, which is once every 30 seconds\nLACP \
        heartbeats are requested at the rate of one per second\n")
{
  if (strcmp(argv[0],PORT_OTHER_CONFIG_LACP_TIME_FAST) == 0)
      return lacp_set_heartbeat_rate((char*) vty->index, PORT_OTHER_CONFIG_LACP_TIME_FAST);
  else
      return lacp_set_heartbeat_rate((char*) vty->index, PORT_OTHER_CONFIG_LACP_TIME_SLOW);
}
DEFUN (cli_lacp_set_no_heartbeat_rate,
       lacp_set_no_heartbeat_rate_cmd,
       "no lacp rate",
       NO_STR
       LACP_STR
       "Set LACP heartbeat request time. Default is slow which is once every 30 seconds\n")
{
  return lacp_set_heartbeat_rate((char*) vty->index, PORT_OTHER_CONFIG_LACP_TIME_SLOW);
}

DEFUN (cli_lacp_set_no_heartbeat_rate_fast,
       lacp_set_no_heartbeat_rate_fast_cmd,
       "no lacp rate fast",
       NO_STR
       LACP_STR
       "Set LACP heartbeat request time. Default is slow which is once every 30 seconds\n"
       "LACP heartbeats are requested at the rate of one per second\n")
{
  return lacp_set_heartbeat_rate((char*) vty->index, PORT_OTHER_CONFIG_LACP_TIME_SLOW);
}


static int
lacp_set_global_sys_priority(const char *priority)
{
  const struct ovsrec_system *row = NULL;
  struct smap smap = SMAP_INITIALIZER(&smap);
  struct ovsdb_idl_txn* txn = NULL;
  enum ovsdb_idl_txn_status status;

  txn = cli_do_config_start();
  if(txn == NULL)
  {
    VLOG_ERR(LACP_OVSDB_TXN_CREATE_ERROR,__func__,__LINE__);
    cli_do_config_abort(txn);
    return CMD_OVSDB_FAILURE;
  }

  row = ovsrec_system_first(idl);

  if(!row)
  {
    VLOG_ERR(LACP_OVSDB_ROW_FETCH_ERROR,__func__,__LINE__);
    cli_do_config_abort(txn);
    return CMD_OVSDB_FAILURE;
  }

  smap_clone(&smap, &row->lacp_config);

  if(DFLT_SYSTEM_LACP_CONFIG_SYSTEM_PRIORITY == atoi(priority))
    smap_remove(&smap, PORT_OTHER_CONFIG_MAP_LACP_SYSTEM_PRIORITY);
  else
    smap_replace(&smap, PORT_OTHER_CONFIG_MAP_LACP_SYSTEM_PRIORITY, priority);

  ovsrec_system_set_lacp_config(row, &smap);
  smap_destroy(&smap);
  status = cli_do_config_finish(txn);
  if(status == TXN_SUCCESS || status == TXN_UNCHANGED)
  {
    return CMD_SUCCESS;
  }
  else
  {
    VLOG_ERR("Transaction commit failed.Function=%s Line=%d",__func__,__LINE__);
    return CMD_OVSDB_FAILURE;
  }
}

DEFUN (cli_lacp_set_global_sys_priority,
       lacp_set_global_sys_priority_cmd,
       "lacp system-priority <0-65535>",
       LACP_STR
       "Set LACP system priority\n"
       "The range is 0 to 65535.(Default:65534)\n")
{
  return lacp_set_global_sys_priority(argv[0]);
}

DEFUN (cli_lacp_set_no_global_sys_priority,
       lacp_set_no_global_sys_priority_cmd,
       "no lacp system-priority <0-65535>",
       NO_STR
       LACP_STR
       "Set LACP system priority\n"
       "The range is 0 to 65535.(Default:65534)\n")
{
  char def_sys_priority[LACP_DEFAULT_SYS_PRIORITY_LENGTH]={0};
  snprintf(def_sys_priority, LACP_DEFAULT_SYS_PRIORITY_LENGTH, "%d",
           DFLT_SYSTEM_LACP_CONFIG_SYSTEM_PRIORITY);
  return lacp_set_global_sys_priority(def_sys_priority);
}

DEFUN (cli_lacp_set_no_global_sys_priority_shortform,
       lacp_set_no_global_sys_priority_shortform_cmd,
       "no lacp system-priority",
       NO_STR
       LACP_STR
       "Set LACP system priority\n")
{
  char def_sys_priority[LACP_DEFAULT_SYS_PRIORITY_LENGTH]={0};
  snprintf(def_sys_priority, LACP_DEFAULT_SYS_PRIORITY_LENGTH, "%d",
           DFLT_SYSTEM_LACP_CONFIG_SYSTEM_PRIORITY);
  return lacp_set_global_sys_priority(def_sys_priority);
}

static int
lacp_intf_set_port_id(const char *if_name, const char *port_id_val)
{
   const struct ovsrec_interface * row = NULL;
   struct ovsdb_idl_txn* status_txn = NULL;
   enum ovsdb_idl_txn_status status;
   struct smap smap = SMAP_INITIALIZER(&smap);

   status_txn = cli_do_config_start();
   if(status_txn == NULL)
   {
      VLOG_ERR(LACP_OVSDB_TXN_CREATE_ERROR,__func__,__LINE__);
      cli_do_config_abort(status_txn);
      return CMD_OVSDB_FAILURE;
   }

   row = ovsrec_interface_first(idl);
   if(!row)
   {
      cli_do_config_abort(status_txn);
      return CMD_OVSDB_FAILURE;
   }

   OVSREC_INTERFACE_FOR_EACH(row, idl)
   {
      if(strcmp(row->name, if_name) == 0)
      {
         smap_clone(&smap, &row->other_config);
         smap_replace(&smap, INTERFACE_OTHER_CONFIG_MAP_LACP_PORT_ID, port_id_val);

         ovsrec_interface_set_other_config(row, &smap);
         smap_destroy(&smap);
         break;
      }
   }

   status = cli_do_config_finish(status_txn);

   if(status == TXN_SUCCESS || status == TXN_UNCHANGED)
   {
      return CMD_SUCCESS;
   }
   else
   {
      VLOG_ERR(LACP_OVSDB_TXN_COMMIT_ERROR, __func__,__LINE__);
      return CMD_OVSDB_FAILURE;
   }
}

DEFUN (cli_lacp_intf_set_port_id,
      cli_lacp_intf_set_port_id_cmd,
      "lacp port-id <1-65535>",
      LACP_STR
      "Set port ID used in LACP negotiation\n."
      "The range is 1 to 65535\n")
{
  return lacp_intf_set_port_id((char*)vty->index, argv[0]);
}

/*****************************************************************************
 * When the interface is modified using "lacp port-id #" a new key will be
 * generated ("lacp-port-id=#") within the other_config section.
 *
 * This function will search for the interface and then remove the key.
 * By default smap will do nothing if the key was not found.
 ****************************************************************************/
static int
lacp_intf_set_no_port_id(const char *if_name, const char *port_id_val)
{
    const struct ovsrec_interface * row = NULL;
    struct ovsdb_idl_txn* status_txn = NULL;
    enum ovsdb_idl_txn_status status;
    struct smap smap = SMAP_INITIALIZER(&smap);

    bool error = false;
    const char *port_id = NULL;

    status_txn = cli_do_config_start();

    if(status_txn == NULL) {
        VLOG_ERR(LACP_OVSDB_TXN_CREATE_ERROR,__func__,__LINE__);
        cli_do_config_abort(status_txn);
        return CMD_OVSDB_FAILURE;
    }

    OVSREC_INTERFACE_FOR_EACH(row, idl)
    {
        if(strcmp(row->name, if_name) == 0)
        {
            smap_clone(&smap, &row->other_config);

            if (port_id_val) {
                port_id = smap_get(&smap,
                                   INTERFACE_OTHER_CONFIG_MAP_LACP_PORT_ID);

                if (!port_id || strcmp(port_id, port_id_val) != 0) {
                    error = true;
                    break;
                }
            }

            smap_remove(&smap, INTERFACE_OTHER_CONFIG_MAP_LACP_PORT_ID);
            ovsrec_interface_set_other_config(row, &smap);
            smap_destroy(&smap);
        }
    }

    if (error) {
        cli_do_config_abort(status_txn);
        return CMD_OVSDB_FAILURE;
    }

    status = cli_do_config_finish(status_txn);

    if(status == TXN_SUCCESS || status == TXN_UNCHANGED) {
        return CMD_SUCCESS;
    } else {
        VLOG_ERR(LACP_OVSDB_TXN_COMMIT_ERROR, __func__,__LINE__);
        return CMD_OVSDB_FAILURE;
    }
}

DEFUN (cli_lacp_intf_set_no_port_id,
       cli_lacp_intf_set_no_port_id_cmd,
       "no lacp port-id <1-65535>",
       NO_STR
       "Set port ID used in LACP negotiation\n"
       "The range is 1 to 65535\n")
{
    return lacp_intf_set_no_port_id((char*)vty->index, argv[0]);
}

DEFUN (cli_lacp_intf_set_no_port_id_short,
       cli_lacp_intf_set_no_port_id_short_cmd,
       "no lacp port-id",
       NO_STR
       "Set port ID used in LACP negotiation\n"
       "The range is 1 to 65535\n")
{
    return lacp_intf_set_no_port_id((char*)vty->index, NULL);
}

static int
lacp_intf_set_port_priority(const char *if_name, const char *port_priority_val)
{
   const struct ovsrec_interface * row = NULL;
   struct ovsdb_idl_txn* status_txn = NULL;
   enum ovsdb_idl_txn_status status;
   struct smap smap = SMAP_INITIALIZER(&smap);

   status_txn = cli_do_config_start();
   if(status_txn == NULL)
   {
      VLOG_ERR(LACP_OVSDB_TXN_CREATE_ERROR,__func__,__LINE__);
      cli_do_config_abort(status_txn);
      return CMD_OVSDB_FAILURE;
   }

   row = ovsrec_interface_first(idl);
   if(!row)
   {
      cli_do_config_abort(status_txn);
      return CMD_OVSDB_FAILURE;
   }

   OVSREC_INTERFACE_FOR_EACH(row, idl)
   {
      if(strcmp(row->name, if_name) == 0)
      {
         smap_clone(&smap, &row->other_config);
         smap_replace(&smap, INTERFACE_OTHER_CONFIG_MAP_LACP_PORT_PRIORITY, port_priority_val);

         ovsrec_interface_set_other_config(row, &smap);
         smap_destroy(&smap);
         break;
      }
   }

   status = cli_do_config_finish(status_txn);

   if(status == TXN_SUCCESS || status == TXN_UNCHANGED)
   {
      return CMD_SUCCESS;
   }
   else
   {
      VLOG_ERR(LACP_OVSDB_TXN_COMMIT_ERROR,__func__,__LINE__);
      return CMD_OVSDB_FAILURE;
   }
}

DEFUN (cli_lacp_intf_set_port_priority,
      cli_lacp_intf_set_port_priority_cmd,
      "lacp port-priority <1-65535>",
      LACP_STR
      "Set port priority is used in LACP negotiation\n"
      "The range is 1 to 65535\n")
{
  return lacp_intf_set_port_priority((char*)vty->index, argv[0]);
}

/*****************************************************************************
 * When the interface is modified using "lacp port-priority #" a new key will
 * be generated ("lacp-port-priority=#") within the other_config section.
 *
 * This function will search for the interface and then remove the key.
 * By default smap will do nothing if the key was not found.
 ****************************************************************************/
static int
lacp_intf_set_no_port_priority(const char *if_name,
                               const char *port_priority_val)
{
    const struct ovsrec_interface * row = NULL;
    struct ovsdb_idl_txn* status_txn = NULL;
    enum ovsdb_idl_txn_status status;
    struct smap smap = SMAP_INITIALIZER(&smap);

    bool error = false;
    const char *port_priority = NULL;

    status_txn = cli_do_config_start();
    if (status_txn == NULL) {
        VLOG_ERR(LACP_OVSDB_TXN_CREATE_ERROR,__func__,__LINE__);
        cli_do_config_abort(status_txn);
        return CMD_OVSDB_FAILURE;
    }

    OVSREC_INTERFACE_FOR_EACH(row, idl)
    {
        if (strcmp(row->name, if_name) == 0)
        {
            smap_clone(&smap, &row->other_config);

            if (port_priority_val) {
                port_priority = smap_get(&smap,
                                INTERFACE_OTHER_CONFIG_MAP_LACP_PORT_PRIORITY);

                if (!port_priority
                    || strcmp(port_priority, port_priority_val) != 0) {
                    error = true;
                    break;
                }
            }

            smap_remove(&smap, INTERFACE_OTHER_CONFIG_MAP_LACP_PORT_PRIORITY);
            ovsrec_interface_set_other_config(row, &smap);
            smap_destroy(&smap);
            break;
        }
    }

    if (error) {
        cli_do_config_abort(status_txn);
        return CMD_OVSDB_FAILURE;
    }

    status = cli_do_config_finish(status_txn);

    if (status == TXN_SUCCESS || status == TXN_UNCHANGED) {
        return CMD_SUCCESS;
    } else {
        VLOG_ERR(LACP_OVSDB_TXN_COMMIT_ERROR, __func__,__LINE__);
        return CMD_OVSDB_FAILURE;
    }
}

DEFUN (cli_lacp_intf_set_no_port_priority,
       cli_lacp_intf_set_no_port_priority_cmd,
       "no lacp port-priority <1-65535>",
       NO_STR
       "Set port priority is used in LACP negotiation\n"
       "The range is 1 to 65535\n")
{
    return lacp_intf_set_no_port_priority((char*)vty->index, argv[0]);
}

DEFUN (cli_lacp_intf_set_no_port_priority_short,
       cli_lacp_intf_set_no_port_priority_short_cmd,
       "no lacp port-priority",
       NO_STR
       "Set port priority is used in LACP negotiation\n"
       "The range is 1 to 65535\n")
{
    return lacp_intf_set_no_port_priority((char*)vty->index, NULL);
}


/*
 * Function : remove_port_reference
 * Responsibility : Remove port reference from VRF / bridge
 *
 * Parameters :
 *   const struct ovsrec_port *port_row: port to be deleted
 */
static void
remove_port_reference (const struct ovsrec_port *port_row)
{
    struct ovsrec_port **ports;
    const struct ovsrec_vrf *vrf_row = NULL;
    const struct ovsrec_bridge *default_bridge_row = NULL;
    struct ovsrec_port *row = NULL;
    int i,n;
    bool port_in_vrf = false;

    OVSREC_VRF_FOR_EACH (vrf_row, idl)
    {
        for (i = 0; i < vrf_row->n_ports; i++)
        {
            row = vrf_row->ports[i];
            if(strcmp(row->name, port_row->name) == 0)
            {
                port_in_vrf = true;
                break;
            }
        }
        if(port_in_vrf)
            break;
    }

    if (port_in_vrf)
    {
        ports = xmalloc (sizeof *vrf_row->ports * (vrf_row->n_ports - 1));
        for (i = n = 0; i < vrf_row->n_ports; i++)
        {
            if (vrf_row->ports[i] != port_row)
                ports[n++] = vrf_row->ports[i];
        }
        ovsrec_vrf_set_ports (vrf_row, ports, n);
        free(ports);
    }

    if (check_port_in_bridge(port_row->name))
    {
        default_bridge_row = ovsrec_bridge_first (idl);
        ports = xmalloc (sizeof *default_bridge_row->ports * (default_bridge_row->n_ports - 1));
        for (i = n = 0; i < default_bridge_row->n_ports; i++)
        {
            if (default_bridge_row->ports[i] != port_row)
                ports[n++] = default_bridge_row->ports[i];
        }
        ovsrec_bridge_set_ports (default_bridge_row, ports, n);
        free(ports);
    }
}

static int
lacp_add_intf_to_lag(const char *if_name, const char *lag_number)
{
   const struct ovsrec_interface *row = NULL;
   const struct ovsrec_interface *interface_row = NULL;
   struct ovsdb_idl_txn* status_txn = NULL;
   enum ovsdb_idl_txn_status status;
   char lag_name[LAG_NAME_LENGTH]={0};
   const struct ovsrec_port *port_row = NULL;
   bool port_found = false;
   const struct ovsrec_interface *if_row = NULL;
   const struct ovsrec_port *port_row_found = NULL;
   struct ovsrec_interface **interfaces;
   const struct ovsrec_port *lag_port = NULL;
   int i=0, k=0, n=0;
   const char* split_state = NULL;

   struct smap smap = SMAP_INITIALIZER(&smap);

   snprintf(lag_name, LAG_NAME_LENGTH, "%s%s", LAG_PORT_NAME_PREFIX, lag_number);

   /* Check if the LAG port is present or not. */
   OVSREC_PORT_FOR_EACH(lag_port, idl)
   {
     if (strcmp(lag_port->name, lag_name) == 0)
     {
       port_found = true;
       if(lag_port->n_interfaces == MAX_INTF_TO_LAG)
       {
         vty_out(vty, "Cannot add more interfaces to LAG. Maximum interface count is reached.\n");
         return CMD_SUCCESS;
       }
       break;
     }
   }
   if(!port_found)
   {
     vty_out(vty, "Specified LAG port doesn't exist.\n");
     return CMD_SUCCESS;
   }

   status_txn = cli_do_config_start();
   if(status_txn == NULL)
   {
      VLOG_ERR(LACP_OVSDB_TXN_CREATE_ERROR,__func__,__LINE__);
      cli_do_config_abort(status_txn);
      return CMD_OVSDB_FAILURE;
   }

   /* Delete the port entry of interface if already exists.
    * This can happen if the interface is attached to VLAN.
    * Remove the port reference from VRF and Bridge before.
    */
   OVSREC_PORT_FOR_EACH(port_row, idl)
   {
     if(strcmp(port_row->name, if_name) == 0)
     {
        /* check if ACL is configured on this port
         * if ACL is applied then we don't allow adding
         * this port to lag
         */
        if (is_acl_configured(port_row))
        {
            vty_out(vty, "Unable to add interface %s to lag %s, "
                         "ACL is configured on interface.\n",if_name, lag_name);
            cli_do_config_abort(status_txn);
            /* ovs-appctl return success since this is a business logic
             * validation and above message should help user
             * understand the issue.
             */
            return CMD_SUCCESS;
        }

        remove_port_reference(port_row);
        ovsrec_port_delete(port_row);
        break;
     }
   }

   /* Fetch the interface row to "interface_row" variable. */
   OVSREC_INTERFACE_FOR_EACH(row, idl)
   {
      if(strcmp(row->name, if_name) == 0)
      {
         interface_row = row;
         break;
      }
   }

   /*
    * Search if this interface is split or not. A parent split interface
    * can't be added to a LAG
    */
   split_state =  smap_get(&interface_row->user_config, INTERFACE_USER_CONFIG_MAP_LANE_SPLIT);
   if ((NULL != split_state) &&
       (strncmp(split_state, INTERFACE_USER_CONFIG_MAP_LANE_SPLIT_SPLIT, strlen(split_state)) == 0)) {
      vty_out(vty, "Split interface can't be part of a LAG\n");
      cli_do_config_abort(status_txn);
      return CMD_SUCCESS;
   }

   /* Search if the interface is part of any LAG or not.
    * If it is part of LAG port which is is specified in CLI then
    * return with SUCCESS.
    * If it is part of any other LAG then exit and remove the
    * reference from that LAG port.*/
   OVSREC_PORT_FOR_EACH(port_row, idl)
   {
     if (strncmp(port_row->name, LAG_PORT_NAME_PREFIX, LAG_PORT_NAME_PREFIX_LENGTH) == 0)
     {
        for (k = 0; k < port_row->n_interfaces; k++)
        {
           if_row = port_row->interfaces[k];
           if(strcmp(if_name, if_row->name) == 0)
           {
              if (strcmp(port_row->name, lag_name) == 0)
              {
                 vty_out(vty, "Interface %s is already part of %s.\n", if_name, port_row->name);
                 cli_do_config_abort(status_txn);
                 return CMD_SUCCESS;
              }
              else
              {
                 /* Unlink interface from "port_row_found" port entry. */
                 port_row_found = port_row;
                 goto exit_loop;
              }
           }
        }
     }
   }

exit_loop:
   if(port_row_found)
   {
       /* Unlink the interface from the Port row found*/
      interfaces = xmalloc(sizeof *port_row_found->interfaces * (port_row_found->n_interfaces-1));
      for(i = n = 0; i < port_row_found->n_interfaces; i++)
      {
         if(port_row_found->interfaces[i] != interface_row)
         {
            interfaces[n++] = port_row_found->interfaces[i];
         }
      }
      ovsrec_port_set_interfaces(port_row_found, interfaces, n);
      free(interfaces);
   }

   /* Link the interface to the LAG port specified. */
   interfaces = xmalloc(sizeof *lag_port->interfaces * (lag_port->n_interfaces + 1));
   for(i = 0; i < lag_port->n_interfaces; i++)
   {
      interfaces[i] = lag_port->interfaces[i];
   }
   interfaces[lag_port->n_interfaces] = (struct ovsrec_interface *)interface_row;
   ovsrec_port_set_interfaces(lag_port, interfaces, lag_port->n_interfaces + 1);
   free(interfaces);

   /* Add Aggregation-Key */
   smap_clone(&smap, &interface_row->other_config);
   smap_replace(&smap,
                INTERFACE_OTHER_CONFIG_MAP_LACP_AGGREGATION_KEY,
                lag_number);
   ovsrec_interface_set_other_config(interface_row, &smap);

   smap_destroy(&smap);

   /* Execute Transaction */
   status = cli_do_config_finish(status_txn);

   if(status == TXN_SUCCESS || status == TXN_UNCHANGED)
   {
      return CMD_SUCCESS;
   }
   else
   {
      VLOG_ERR(LACP_OVSDB_TXN_COMMIT_ERROR,__func__,__LINE__);
      return CMD_OVSDB_FAILURE;
   }
}

DEFUN (cli_lacp_add_intf_to_lag,
      cli_lacp_add_intf_to_lag_cmd,
      "lag <1-2000>",
      "Add the current interface to link aggregation\n"
      "LAG number ranges from 1 to 2000\n")
{
  return lacp_add_intf_to_lag((char*)vty->index, argv[0]);
}

static int
lacp_remove_intf_from_lag(const char *if_name, const char *lag_number)
{
   const struct ovsrec_interface *row = NULL;
   const struct ovsrec_interface *interface_row = NULL;
   const struct ovsrec_interface *if_row = NULL;
   struct ovsdb_idl_txn* status_txn = NULL;
   enum ovsdb_idl_txn_status status;
   char lag_name[8]={0};
   bool port_found = false;
   struct ovsrec_interface **interfaces;
   const struct ovsrec_port *lag_port = NULL;
   int i=0, n=0, k=0;
   bool interface_found = false;
   struct smap smap = SMAP_INITIALIZER(&smap);

   /* LAG name should be in the format of lag1,lag2,etc  */
   snprintf(lag_name,
            LAG_NAME_LENGTH,
            "%s%s",
            LAG_PORT_NAME_PREFIX,
            lag_number);

   /* Check if the LAG port is present in DB. */
   OVSREC_PORT_FOR_EACH(lag_port, idl)
   {
     if (strcmp(lag_port->name, lag_name) == 0)
     {
       for (k = 0; k < lag_port->n_interfaces; k++)
       {
         if_row = lag_port->interfaces[k];
         if(strcmp(if_name, if_row->name) == 0)
         {
           interface_found = true;
         }
       }
       port_found = true;
       break;
     }
   }

   if(!port_found)
   {
     vty_out(vty, "Specified LAG port doesn't exist.\n");
     return CMD_SUCCESS;
   }
   if(!interface_found)
   {
     vty_out(vty, "Interface %s is not part of %s.\n", if_name, lag_name);
     return CMD_SUCCESS;
   }

   status_txn = cli_do_config_start();
   if(status_txn == NULL)
   {
      VLOG_ERR(LACP_OVSDB_TXN_CREATE_ERROR,__func__,__LINE__);
      cli_do_config_abort(status_txn);
      return CMD_OVSDB_FAILURE;
   }

   /* Fetch the interface row to "interface_row" */
   OVSREC_INTERFACE_FOR_EACH(row, idl)
   {
      if(strcmp(row->name, if_name) == 0)
      {
         interface_row = row;
         break;
      }
   }

   /* Remove Aggregation Key */
   smap_clone(&smap, &interface_row->other_config);
   smap_remove(&smap, INTERFACE_OTHER_CONFIG_MAP_LACP_AGGREGATION_KEY);
   ovsrec_interface_set_other_config(interface_row, &smap);
   smap_clear(&smap);

   /* Remove User config */
   smap_clone(&smap, &interface_row->user_config);
   smap_remove(&smap, INTERFACE_USER_CONFIG_MAP_ADMIN);
   ovsrec_interface_set_user_config(interface_row, &smap);
   smap_destroy(&smap);

   /* Unlink the interface from the Port row found*/
   interfaces = xmalloc(sizeof *lag_port->interfaces * (lag_port->n_interfaces-1));
   for(i = n = 0; i < lag_port->n_interfaces; i++)
   {
      if(lag_port->interfaces[i] != interface_row)
      {
         interfaces[n++] = lag_port->interfaces[i];
      }
   }
   ovsrec_port_set_interfaces(lag_port, interfaces, n);
   free(interfaces);

   status = cli_do_config_finish(status_txn);

   if(status == TXN_SUCCESS || status == TXN_UNCHANGED)
   {
      return CMD_SUCCESS;
   }
   else
   {
      VLOG_ERR(LACP_OVSDB_TXN_COMMIT_ERROR,__func__,__LINE__);
      return CMD_OVSDB_FAILURE;
   }
}



DEFUN (cli_lacp_remove_intf_from_lag,
      cli_lacp_remove_intf_from_lag_cmd,
      "no lag <1-2000>",
      NO_STR
      "Add the current interface to link aggregation\n"
      "LAG number ranges from 1 to 2000\n")
{
  return lacp_remove_intf_from_lag((char*)vty->index, argv[0]);
}

static int
lacp_show_configuration()
{
   const struct ovsrec_system *row = NULL;
   const char *system_id = NULL;
   const char *system_priority = NULL;

   row = ovsrec_system_first(idl);
   if(!row)
   {
      return CMD_OVSDB_FAILURE;
   }
   system_id = smap_get(&row->lacp_config, PORT_OTHER_CONFIG_MAP_LACP_SYSTEM_ID);
   if(NULL == system_id)
       vty_out(vty,"System-id       : %s", row->system_mac);
   else
       vty_out(vty,"System-id       : %s", system_id);
   vty_out(vty,"%s",VTY_NEWLINE);
   system_priority = smap_get(&row->lacp_config, PORT_OTHER_CONFIG_MAP_LACP_SYSTEM_PRIORITY);
   if(NULL == system_priority)
      vty_out(vty, "System-priority : %d", DFLT_SYSTEM_LACP_CONFIG_SYSTEM_PRIORITY);
   else
      vty_out(vty, "System-priority : %s", system_priority);
   vty_out(vty,"%s",VTY_NEWLINE);
   return CMD_SUCCESS;
}

DEFUN (cli_lacp_show_configuration,
      cli_lacp_show_configuration_cmd,
      "show lacp configuration",
      SHOW_STR
      "Show various LACP settings\n"
      "Show LACP system-wide configuration\n")
{
  return lacp_show_configuration();
}

static int
lacp_show_aggregates(const char *lag_name)
{
   const struct ovsrec_port *lag_port = NULL;
   const struct ovsrec_interface *if_row = NULL;
   const char *heartbeat_rate = NULL;
   bool fallback = false;
   const char *fallback_mode = NULL;
   const char *fallback_timeout = NULL;
   const char *aggregate_mode = NULL;
   const char *hash = NULL;
   char * tmp_hash = NULL;
   bool show_all = false;
   bool port_found = false;
   int k = 0;

   if(strncmp("all", lag_name, 3) == 0)
   {
      show_all = true;
   }

   OVSREC_PORT_FOR_EACH(lag_port, idl)
   {
      if(((strncmp(lag_port->name, LAG_PORT_NAME_PREFIX, LAG_PORT_NAME_PREFIX_LENGTH) == 0) && show_all)
          || (strcmp(lag_port->name, lag_name) == 0))
      {
         vty_out(vty, "%s", VTY_NEWLINE);
         vty_out(vty, "%s%s%s","Aggregate-name        : ", lag_port->name, VTY_NEWLINE);
         vty_out(vty, "%s","Aggregated-interfaces : ");
         for (k = 0; k < lag_port->n_interfaces; k++)
         {
            if_row = lag_port->interfaces[k];
            vty_out(vty, "%s ", if_row->name);
         }
         vty_out(vty, "%s", VTY_NEWLINE);
         heartbeat_rate = smap_get(&lag_port->other_config, "lacp-time");
         if(heartbeat_rate)
            vty_out(vty, "%s%s%s", "Heartbeat rate        : ",heartbeat_rate, VTY_NEWLINE);
         else
            vty_out(vty, "%s%s%s", "Heartbeat rate        : ",
                         PORT_OTHER_CONFIG_LACP_TIME_SLOW, VTY_NEWLINE);

         fallback = smap_get_bool(&lag_port->other_config, "lacp-fallback-ab", false);
         vty_out(vty, "%s%s%s", "Fallback              : ",(fallback)?"true":"false", VTY_NEWLINE);

         fallback_mode = smap_get(&lag_port->other_config,
                                  PORT_OTHER_CONFIG_MAP_LACP_FALLBACK_MODE);
         vty_out(vty,
                 "%s%s%s",
                 "Fallback mode         : ",
                 fallback_mode ? fallback_mode : "priority",
                 VTY_NEWLINE);

         fallback_timeout = smap_get(&lag_port->other_config,
                                     PORT_OTHER_CONFIG_MAP_LACP_FALLBACK_TIMEOUT);
         vty_out(vty,
                 "%s%s%s",
                 "Fallback timeout      : ",
                 fallback_timeout ? fallback_timeout : "0",
                 VTY_NEWLINE);

         hash = smap_get(&lag_port->other_config, "bond_mode");
         if(hash) {
            tmp_hash = lacp_remove_lb_hash_suffix(hash);
            if (tmp_hash) {
                vty_out(vty, "%s%s%s", "Hash                  : ",tmp_hash, VTY_NEWLINE);
                free(tmp_hash);
                tmp_hash = NULL;
            }
         }
         else {
            vty_out(vty, "%s%s%s", "Hash                  : ", LAG_LB_ALG_L3, VTY_NEWLINE);
         }

         aggregate_mode = lag_port->lacp;
         if(aggregate_mode)
            vty_out(vty, "%s%s%s", "Aggregate mode        : ",aggregate_mode, VTY_NEWLINE);
         else
            vty_out(vty, "%s%s%s", "Aggregate mode        : ","off", VTY_NEWLINE);
         vty_out(vty, "%s", VTY_NEWLINE);

         if(!show_all)
         {
            port_found = true;
            break;
         }
      }
   }

   if(!show_all && !port_found)
      vty_out(vty, "Specified LAG port doesn't exist.\n");

   return CMD_SUCCESS;
}

DEFUN (cli_lacp_show_all_aggregates,
      cli_lacp_show_all_aggregates_cmd,
      "show lacp aggregates",
      SHOW_STR
      "Show various LACP settings\n"
      "Show LACP aggregates\n")
{
  return lacp_show_aggregates("all");
}

DEFUN (cli_lacp_show_aggregates,
      cli_lacp_show_aggregates_cmd,
      "show lacp aggregates WORD",
      SHOW_STR
      "Show various LACP settings\n"
      "Show LACP aggregates\n"
      "Link-aggregate name\n")
{
  return lacp_show_aggregates(argv[0]);
}

static char *
get_lacp_state(const int *state)
{
   /* +1 for the event where all flags are ON then we have a place to store \0 */
   static char ret_state[LACP_STATUS_FIELD_COUNT+1]={0};
   int n = 0;

   memset(ret_state, 0, LACP_STATUS_FIELD_COUNT+1);
   if(state == NULL) return ret_state;
   ret_state[n++] = state[0]? 'A':'P';
   ret_state[n++] = state[1]? 'S':'L';
   ret_state[n++] = state[2]? 'F':'I';
   ret_state[n++] = state[3]? 'N':'O';
   if (state[4]) ret_state[n++] = 'C';
   if (state[5]) ret_state[n++] = 'D';
   if (state[6]) ret_state[n++] = 'E';
   if (state[7]) ret_state[n++] = 'X';

   return ret_state;
}


void
parse_id_from_db (char *str, char **value1, char **value2)
{
  *value1 = strsep(&str, ",");
  *value2 = strsep(&str, ",");
}

/*
 * Expected format from DB (e.g):
 * "Actv:1,TmOut:1,Aggr:1,Sync:1,Col:1,Dist:1,Def:0,Exp:0"
 * Expected output (e.g):
 * [1,1,1,1,1,1,0,0]
 * Returns the number of fields read
 *
 */

int
parse_state_from_db(const char *str, int *ret_str)
{
 return sscanf(str, "%*[^01]%d%*[^01]%d%*[^01]%d%*[^01]%d%*[^01]%d%*[^01]%d%*[^01]%d%*[^01]%d",
               &ret_str[0], &ret_str[1], &ret_str[2], &ret_str[3], &ret_str[4], &ret_str[5],
               &ret_str[6], &ret_str[7]);
}

static int
lacp_show_interfaces_all()
{
   const struct ovsrec_port *lag_port = NULL;
   const struct ovsrec_interface *if_row = NULL;
   int k = 0;
   int lacp_state_ovsdb[LACP_STATUS_FIELD_COUNT];
   const char *lacp_state = NULL;
   const char *agg_key = NULL;
   char *port_priority = NULL, *port_id = NULL;
   char *port_priority_id_ovsdb = NULL, *system_priority_id_ovsdb = NULL;
   char *system_id = NULL, *system_priority = NULL;
   const char *data_in_db = NULL;

   const char columns[] = "%-5s%-10s%-8s%-9s%-8s%-18s%-9s%-8s";
   const char *delimiter = "---------------------------------------"
                           "---------------------------------------";

   vty_out(vty,"%s", VTY_NEWLINE);
   vty_out(vty, "State abbreviations :%s", VTY_NEWLINE);
   vty_out(vty, "A - Active        P - Passive      F - Aggregable I - Individual");
   vty_out(vty,"%s", VTY_NEWLINE);
   vty_out(vty, "S - Short-timeout L - Long-timeout N - InSync     O - OutofSync");
   vty_out(vty,"%s", VTY_NEWLINE);
   vty_out(vty, "C - Collecting    D - Distributing ");
   vty_out(vty,"%s", VTY_NEWLINE);
   vty_out(vty, "X - State m/c expired              E - Default neighbor state");
   vty_out(vty,"%s%s", VTY_NEWLINE, VTY_NEWLINE);

   vty_out(vty, "Actor details of all interfaces:%s",VTY_NEWLINE);
   vty_out(vty, delimiter);
   vty_out(vty,"%s", VTY_NEWLINE);
   vty_out(vty, columns, "Intf", "Aggregate", "Port", "Port", "State",
           "System-id", "System", "Aggr");
   vty_out(vty,"%s", VTY_NEWLINE);
   vty_out(vty, columns, "", "name", "id", "Priority", "", "", "Priority", "Key");
   vty_out(vty,"%s", VTY_NEWLINE);
   vty_out(vty, delimiter);
   vty_out(vty,"%s", VTY_NEWLINE);

   OVSREC_PORT_FOR_EACH(lag_port, idl)
   {
      if (strncmp(lag_port->name, LAG_PORT_NAME_PREFIX, LAG_PORT_NAME_PREFIX_LENGTH) == 0)
      {
         for (k = 0; k < lag_port->n_interfaces; k++)
         {
            if_row = lag_port->interfaces[k];
            port_id = port_priority = system_id = system_priority = NULL;
            agg_key = lacp_state = NULL;
            data_in_db = smap_get(&if_row->lacp_status, INTERFACE_LACP_STATUS_MAP_ACTOR_STATE);
            if (data_in_db)
            {
              if (parse_state_from_db(data_in_db, lacp_state_ovsdb) == LACP_STATUS_FIELD_COUNT)
              {
                lacp_state = get_lacp_state(lacp_state_ovsdb);
              }
              agg_key = smap_get(&if_row->other_config,
                            INTERFACE_OTHER_CONFIG_MAP_LACP_AGGREGATION_KEY);
              /*
              * The system and port priority are kept in the lacp_status column
              * as part of the id fields separated by commas
              * e.g port_id = 1,18 where 1 = priority and 18 = id
              */
              port_priority_id_ovsdb = strdup(smap_get(&if_row->lacp_status,
                                   INTERFACE_LACP_STATUS_MAP_ACTOR_PORT_ID));
              parse_id_from_db(port_priority_id_ovsdb, &port_priority, &port_id);
              system_priority_id_ovsdb = strdup(smap_get(&if_row->lacp_status,
                                    INTERFACE_LACP_STATUS_MAP_ACTOR_SYSTEM_ID));
              parse_id_from_db(system_priority_id_ovsdb, &system_priority, &system_id);
            }

            /* Display information */
            vty_out(vty, columns,
                       if_row->name,
                       lag_port->name,
                       port_id ? port_id : " ",
                       port_priority ? port_priority : " ",
                       lacp_state ? lacp_state : " ",
                       system_id ? system_id : " ",
                       system_priority ? system_priority : " ",
                       agg_key ? agg_key : " ");
            vty_out(vty,"%s", VTY_NEWLINE);

            if (port_priority_id_ovsdb){
                free(port_priority_id_ovsdb);
                port_priority_id_ovsdb = NULL;
            }
            if (system_priority_id_ovsdb){
                free(system_priority_id_ovsdb);
                system_priority_id_ovsdb = NULL;
            }
         }
      }
   }

   vty_out(vty,"%s%s", VTY_NEWLINE, VTY_NEWLINE);

   vty_out(vty, "Partner details of all interfaces:%s",VTY_NEWLINE);
   vty_out(vty, delimiter);
   vty_out(vty,"%s", VTY_NEWLINE);
   vty_out(vty, columns, "Intf", "Aggregate", "Partner", "Port",
           "State", "System-id", "System", "Aggr");
   vty_out(vty,"%s", VTY_NEWLINE);
   vty_out(vty, columns, "", "name", "Port-id", "Priority", "", "",
           "Priority", "Key");
   vty_out(vty,"%s", VTY_NEWLINE);
   vty_out(vty, delimiter);
   vty_out(vty,"%s", VTY_NEWLINE);

   OVSREC_PORT_FOR_EACH(lag_port, idl)
   {
      if (strncmp(lag_port->name, LAG_PORT_NAME_PREFIX, LAG_PORT_NAME_PREFIX_LENGTH) == 0)
      {
         for (k = 0; k < lag_port->n_interfaces; k++)
         {
            if_row = lag_port->interfaces[k];
            port_id = port_priority = system_id = system_priority = NULL;
            agg_key = lacp_state = NULL;
            data_in_db = smap_get(&if_row->lacp_status, INTERFACE_LACP_STATUS_MAP_PARTNER_STATE);
            if (data_in_db)
            {
              if (parse_state_from_db(data_in_db, lacp_state_ovsdb) == LACP_STATUS_FIELD_COUNT)
              {
                lacp_state = get_lacp_state(lacp_state_ovsdb);
              }
              agg_key = smap_get(&if_row->other_config,
                            INTERFACE_OTHER_CONFIG_MAP_LACP_AGGREGATION_KEY);
              port_priority_id_ovsdb = strdup(smap_get(&if_row->lacp_status,
                                       INTERFACE_LACP_STATUS_MAP_PARTNER_PORT_ID));
              parse_id_from_db(port_priority_id_ovsdb, &port_priority, &port_id);
              system_priority_id_ovsdb = strdup(smap_get(&if_row->lacp_status,
                                        INTERFACE_LACP_STATUS_MAP_PARTNER_SYSTEM_ID));
              parse_id_from_db(system_priority_id_ovsdb, &system_priority, &system_id);
            }
            vty_out(vty, columns,
                       if_row->name,
                       lag_port->name,
                       port_id ? port_id : " ",
                       port_priority ? port_priority : " ",
                       lacp_state ? lacp_state : " ",
                       system_id ? system_id : " ",
                       system_priority ? system_priority : " ",
                       agg_key ? agg_key : " ");
            vty_out(vty,"%s", VTY_NEWLINE);
            if (port_priority_id_ovsdb){
                free(port_priority_id_ovsdb);
                port_priority_id_ovsdb = NULL;
            }
            if (system_priority_id_ovsdb){
                free(system_priority_id_ovsdb);
                system_priority_id_ovsdb = NULL;
            }
        }
      }
   }

   return CMD_SUCCESS;
}

DEFUN (cli_lacp_show_all_interfaces,
      cli_lacp_show_all_interfaces_cmd,
      "show lacp interfaces",
      SHOW_STR
      "Show various LACP settings\n"
      "Show LACP interfaces\n")
{
  return lacp_show_interfaces_all();
}

static int
lacp_show_interfaces(const char *if_name)
{
   const struct ovsrec_port *port_row = NULL;
   const struct ovsrec_interface *if_row = NULL;
   int k = 0;
   int a_lacp_state_ovsdb[LACP_STATUS_FIELD_COUNT] = {0};
   int p_lacp_state_ovsdb[LACP_STATUS_FIELD_COUNT] = {0};
   const char *a_lacp_state = NULL, *p_lacp_state = NULL;
   const char *a_key = NULL;
   char *a_system_id = NULL, *a_system_priority = NULL, *a_port_id = NULL, *a_port_priority = NULL;
   const char *p_key = NULL;
   char *p_system_id = NULL, *p_system_priority = NULL, *p_port_id = NULL, *p_port_priority = NULL;
   char *a_port_priority_id_ovsdb = NULL, *a_system_priority_id_ovsdb = NULL;
   char *p_port_priority_id_ovsdb = NULL, *p_system_priority_id_ovsdb = NULL;
   const char *data_in_db = NULL;
   const char *columns = "%-18s | %-18s | %-18s %s";
   bool port_row_round = false;

   vty_out(vty,"%s", VTY_NEWLINE);
   vty_out(vty, "State abbreviations :%s", VTY_NEWLINE);
   vty_out(vty, "A - Active        P - Passive      F - Aggregable I - Individual");
   vty_out(vty,"%s", VTY_NEWLINE);
   vty_out(vty, "S - Short-timeout L - Long-timeout N - InSync     O - OutofSync");
   vty_out(vty,"%s", VTY_NEWLINE);
   vty_out(vty, "C - Collecting    D - Distributing ");
   vty_out(vty,"%s", VTY_NEWLINE);
   vty_out(vty, "X - State m/c expired              E - Default neighbor state");
   vty_out(vty,"%s%s", VTY_NEWLINE, VTY_NEWLINE);

   OVSREC_PORT_FOR_EACH(port_row, idl)
   {
     if (strncmp(port_row->name, LAG_PORT_NAME_PREFIX, LAG_PORT_NAME_PREFIX_LENGTH) == 0)
     {
        for (k = 0; k < port_row->n_interfaces; k++)
        {
           if_row = port_row->interfaces[k];
           if(strcmp(if_name, if_row->name) == 0)
           {
             a_port_id = a_port_priority = a_system_id = a_system_priority = NULL;
             a_key = a_lacp_state = NULL;
             p_port_id = p_port_priority = p_system_id = p_system_priority = NULL;
             p_key = p_lacp_state = NULL;
             data_in_db = smap_get(&if_row->lacp_status, INTERFACE_LACP_STATUS_MAP_ACTOR_STATE);
             if (data_in_db)
             {
               /*
                * get_lacp_state() returns a static char*,
                * unless copied it will be overwritten by partner state
                */
               if (parse_state_from_db(data_in_db, a_lacp_state_ovsdb) == LACP_STATUS_FIELD_COUNT)
                   a_lacp_state = strdup(get_lacp_state(a_lacp_state_ovsdb));
               a_key = smap_get(&if_row->lacp_status, INTERFACE_LACP_STATUS_MAP_ACTOR_KEY);
               a_port_priority_id_ovsdb = strdup(smap_get(&if_row->lacp_status,
                                          INTERFACE_LACP_STATUS_MAP_ACTOR_PORT_ID));
               parse_id_from_db(a_port_priority_id_ovsdb, &a_port_priority, &a_port_id);
               a_system_priority_id_ovsdb = strdup(smap_get(&if_row->lacp_status,
                                            INTERFACE_LACP_STATUS_MAP_ACTOR_SYSTEM_ID));
               parse_id_from_db(a_system_priority_id_ovsdb, &a_system_priority, &a_system_id);

               data_in_db = smap_get(&if_row->lacp_status, INTERFACE_LACP_STATUS_MAP_PARTNER_STATE);
               if (parse_state_from_db(data_in_db, p_lacp_state_ovsdb) == LACP_STATUS_FIELD_COUNT)
                   p_lacp_state = strdup(get_lacp_state(p_lacp_state_ovsdb));
               p_key = smap_get(&if_row->lacp_status, INTERFACE_LACP_STATUS_MAP_PARTNER_KEY);
               p_port_priority_id_ovsdb = strdup(smap_get(&if_row->lacp_status,
                                          INTERFACE_LACP_STATUS_MAP_PARTNER_PORT_ID));
               parse_id_from_db(p_port_priority_id_ovsdb, &p_port_priority, &p_port_id);
               p_system_priority_id_ovsdb = strdup(smap_get(&if_row->lacp_status,
                                            INTERFACE_LACP_STATUS_MAP_PARTNER_SYSTEM_ID));
               parse_id_from_db(p_system_priority_id_ovsdb, &p_system_priority, &p_system_id);
             }
             port_row_round = true;
             goto Exit;
           }
        }
     }
   }
Exit:
   vty_out(vty,"%s",VTY_NEWLINE);
   vty_out(vty, "Aggregate-name : %s%s", port_row_round?port_row->name:" ", VTY_NEWLINE);
   vty_out(vty, "-------------------------------------------------");
   vty_out(vty,"%s",VTY_NEWLINE);
   vty_out(vty, "                       Actor             Partner");
   vty_out(vty,"%s",VTY_NEWLINE);
   vty_out(vty, "-------------------------------------------------");
   vty_out(vty,"%s",VTY_NEWLINE);
   vty_out(vty,columns,
               "Port-id", a_port_id?a_port_id:" ", p_port_id?p_port_id:" ", VTY_NEWLINE);
   vty_out(vty,columns,
               "Port-priority", a_port_priority?a_port_priority:" ",
                p_port_priority?p_port_priority:" ", VTY_NEWLINE);
   vty_out(vty,columns,
               "Key", a_key?a_key:" ", p_key?p_key:" ", VTY_NEWLINE);
   vty_out(vty,columns,
               "State", a_lacp_state?a_lacp_state:" ", p_lacp_state?p_lacp_state:" ", VTY_NEWLINE);
   vty_out(vty,columns,
               "System-id",a_system_id?a_system_id:" ", p_system_id?p_system_id:" ", VTY_NEWLINE);
   vty_out(vty,columns,
               "System-priority",a_system_priority?a_system_priority:" ",
               p_system_priority?p_system_priority:" ", VTY_NEWLINE);
   vty_out(vty,"%s",VTY_NEWLINE);
   free(a_port_priority_id_ovsdb);
   free(p_port_priority_id_ovsdb);
   free(a_system_priority_id_ovsdb);
   free(p_system_priority_id_ovsdb);
   free((char *)a_lacp_state);
   free((char *)p_lacp_state);
   return CMD_SUCCESS;
}

DEFUN (cli_lacp_show_interfaces,
      cli_lacp_show_interfaces_cmd,
      "show lacp interfaces IFNAME",
      SHOW_STR
      "Show various LACP settings\n"
      "Show LACP interfaces\n"
      "Interface's name\n")
{
   if(strncmp(argv[0], LAG_PORT_NAME_PREFIX, LAG_PORT_NAME_PREFIX_LENGTH) != 0) {
      return lacp_show_interfaces(argv[0]);
   } else {
       vty_out(vty, "%% Unknown command.%s", VTY_NEWLINE);
       return CMD_SUCCESS;
   }
}

/*
* This function is used to make an LAG L3.
* It attaches the port to the default VRF.
*/
static int lag_routing(const char *port_name)
{
    const struct ovsrec_port *port_row = NULL;
    const struct ovsrec_vrf *default_vrf_row = NULL;
    const struct ovsrec_bridge *default_bridge_row = NULL;
    struct ovsdb_idl_txn *status_txn = NULL;
    enum ovsdb_idl_txn_status status;
    struct ovsrec_port **ports;
    size_t i, n;

    status_txn = cli_do_config_start();

    if (status_txn == NULL) {
        VLOG_DBG(
            "%s Got an error when trying to create a transaction using"
            " cli_do_config_start()", __func__);
        cli_do_config_abort(status_txn);
        return CMD_OVSDB_FAILURE;
    }

    port_row = port_check_and_add(port_name, false, false, status_txn);

    if (check_port_in_vrf(port_name)) {
        VLOG_DBG(
            "%s Interface \"%s\" is already L3. No change required.",
            __func__, port_name);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    //Clean vlan configuration
    ovsrec_port_set_vlan_mode(port_row, NULL);
    ops_port_set_tag(0, port_row, idl);
    ops_port_set_trunks(NULL, 0, port_row, idl);

    default_bridge_row = ovsrec_bridge_first(idl);
    ports = xmalloc(sizeof *default_bridge_row->ports *
        (default_bridge_row->n_ports - 1));
    for (i = n = 0; i < default_bridge_row->n_ports; i++) {
        if (default_bridge_row->ports[i] != port_row) {
            ports[n++] = default_bridge_row->ports[i];
        }
    }
    ovsrec_bridge_set_ports(default_bridge_row, ports, n);
    free(ports);

    default_vrf_row = get_default_vrf(idl);
    ports = xmalloc(sizeof *default_vrf_row->ports *
        (default_vrf_row->n_ports + 1));
    for (i = 0; i < default_vrf_row->n_ports; i++) {
        ports[i] = default_vrf_row->ports[i];
    }
    struct ovsrec_port *temp_port_row = CONST_CAST(struct ovsrec_port*, port_row);
    ports[default_vrf_row->n_ports] = temp_port_row;
    ovsrec_vrf_set_ports(default_vrf_row, ports, default_vrf_row->n_ports + 1);
    free(ports);

    status = cli_do_config_finish(status_txn);

    if (status == TXN_SUCCESS) {
        VLOG_DBG(
            "%s The command succeeded and interface \"%s\" is now L3"
            " and attached to default VRF", __func__, port_name);
        return CMD_SUCCESS;
    }
    else if (status == TXN_UNCHANGED) {
        VLOG_DBG(
            "%s The command resulted in no change. Check if"
            " LAG \"%s\" is already L3",
            __func__, port_name);
        return CMD_SUCCESS;
    }
    else {
        VLOG_DBG(
            "%s While trying to commit transaction to DB, got a status"
            " response : %s", __func__,
            ovsdb_idl_txn_status_to_string(status));
        return CMD_OVSDB_FAILURE;
    }
}

/*
* This function is used to make an LAG L2.
* It attaches the port to the default VRF.
* It also removes all L3 related configuration like IP addresses.
*/
static int lag_no_routing(const char *port_name)
{
    const struct ovsrec_port *port_row = NULL;
    const struct ovsrec_vrf *vrf_row = NULL;
    const struct ovsrec_bridge *default_bridge_row = NULL;
    struct ovsdb_idl_txn *status_txn = NULL;
    enum ovsdb_idl_txn_status status;
    struct ovsrec_port **vrf_ports;
    struct ovsrec_port **bridge_ports;
    struct smap smap = SMAP_INITIALIZER(&smap);
    size_t i, n;

    status_txn = cli_do_config_start();

    if (status_txn == NULL) {
        VLOG_DBG(
            "%s Got an error when trying to create a transaction using"
            " cli_do_config_start()", __func__);
        cli_do_config_abort(status_txn);
        return CMD_OVSDB_FAILURE;
    }

    port_row = port_check_and_add(port_name, true, false, status_txn);
    if (check_port_in_bridge(port_name)) {
        vty_out(vty,"Interface \"%s\" is already L2. No change required. %s",
                port_name, VTY_NEWLINE);
        VLOG_DBG(
            "%s Interface \"%s\" is already L2. No change required.",
            __func__, port_name);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }
    else if ((vrf_row = port_vrf_lookup(port_row)) != NULL) {
        vrf_ports = xmalloc(sizeof *vrf_row->ports * (vrf_row->n_ports - 1));
        for (i = n = 0; i < vrf_row->n_ports; i++){
            if (vrf_row->ports[i] != port_row) {
                vrf_ports[n++] = vrf_row->ports[i];
            }
        }
        ovsrec_vrf_set_ports(vrf_row, vrf_ports, n);
        free(vrf_ports);
    }

    /* assign external vlan */
    ovsrec_port_set_vlan_mode(port_row, OVSREC_PORT_VLAN_MODE_ACCESS);
    ops_port_set_tag(DEFAULT_VLAN, port_row, idl);
    ops_port_set_trunks(NULL, 0, port_row, idl);

    smap_clone(&smap, &port_row->other_config);
    smap_remove(&smap, PORT_HW_CONFIG_MAP_INTERNAL_VLAN_ID);
    ovsrec_port_set_hw_config(port_row, &smap);
    smap_destroy(&smap);

    default_bridge_row = ovsrec_bridge_first(idl);
    bridge_ports = xmalloc(sizeof *default_bridge_row->ports
        * (default_bridge_row->n_ports + 1));
    for (i = 0; i < default_bridge_row->n_ports; i++) {
        bridge_ports[i] = default_bridge_row->ports[i];
    }
    struct ovsrec_port *temp_port_row = CONST_CAST(struct ovsrec_port*, port_row);
    bridge_ports[default_bridge_row->n_ports] = temp_port_row;
    ovsrec_bridge_set_ports(default_bridge_row, bridge_ports,
        (default_bridge_row->n_ports + 1));
    free(bridge_ports);
    ovsrec_port_set_ip4_address(port_row, NULL);
    ovsrec_port_set_ip4_address_secondary(port_row, NULL, 0);
    ovsrec_port_set_ip6_address(port_row, NULL);
    ovsrec_port_set_ip6_address_secondary(port_row, NULL, 0);

    status = cli_do_config_finish(status_txn);
    if (status == TXN_SUCCESS) {
        VLOG_DBG(
            "%s The command succeeded and interface \"%s\" is now L2"
            " and attached to default bridge", __func__, port_name);
        return CMD_SUCCESS;
    }
    else if (status == TXN_UNCHANGED) {
        VLOG_DBG(
            "%s The command resulted in no change. Check if"
            " interface \"%s\" is already L2",
            __func__, port_name);
        return CMD_SUCCESS;
    }
    else {
        VLOG_DBG(
            "%s While trying to commit transaction to DB, got a status"
            " response : %s", __func__,
            ovsdb_idl_txn_status_to_string(status));
        return CMD_OVSDB_FAILURE;
    }
}

DEFUN(cli_lag_routing,
    cli_lag_routing_cmd,
    "routing",
    "Configure LAG as L3\n")
{
    return lag_routing((char*) vty->index);
}

DEFUN(cli_lag_no_routing,
    cli_lag_no_routing_cmd,
    "no routing",
    NO_STR
    "Configure LAG as L3\n")
{
    return lag_no_routing((char*) vty->index);
}

/*
 * CLI "shutdown"
 * default : enabled
 */
DEFUN (cli_lag_shutdown,
        cli_lag_shutdown_cmd,
        "shutdown",
        "Enable/disable a LAG\n")
{
    const struct ovsrec_interface * intf_row = NULL;
    const struct ovsrec_port *port_row = NULL;
    struct ovsdb_idl_txn* status_txn = cli_do_config_start();
    enum ovsdb_idl_txn_status status;
    struct smap smap_user_config;
    int i;

    if (status_txn == NULL) {
        VLOG_ERR(OVSDB_TXN_CREATE_ERROR);
        cli_do_config_abort(status_txn);
        return CMD_OVSDB_FAILURE;
    }

    OVSREC_PORT_FOR_EACH(port_row, idl) {

        if(strncmp(port_row->name,
                   (char*)vty->index,
                   strlen(port_row->name)) == 0) {

            if(vty_flags & CMD_FLAG_NO_CMD) {
                ovsrec_port_set_admin(port_row,
                                      OVSREC_INTERFACE_ADMIN_STATE_UP);
            } else {
                ovsrec_port_set_admin(port_row,
                                      OVSREC_INTERFACE_ADMIN_STATE_DOWN);
            }

            for (i = 0; i < port_row->n_interfaces; i++) {
                intf_row = port_row->interfaces[i];
                smap_clone(&smap_user_config, &intf_row->user_config);
                if (vty_flags & CMD_FLAG_NO_CMD) {
                    smap_replace(&smap_user_config,
                                 INTERFACE_USER_CONFIG_MAP_ADMIN,
                                 OVSREC_INTERFACE_USER_CONFIG_ADMIN_UP);
                } else {
                    smap_remove(&smap_user_config,
                                INTERFACE_USER_CONFIG_MAP_ADMIN);
                }
                ovsrec_interface_set_user_config(intf_row, &smap_user_config);
                smap_destroy(&smap_user_config);
            }
            break;
        }
    }

    status = cli_do_config_finish(status_txn);

    if (status == TXN_SUCCESS || status == TXN_UNCHANGED) {
        return CMD_SUCCESS;
    } else {
        VLOG_ERR(OVSDB_TXN_COMMIT_ERROR);
    }

    return CMD_OVSDB_FAILURE;
}

DEFUN_NO_FORM (cli_lag_shutdown,
        cli_lag_shutdown_cmd,
        "shutdown",
        "Enable/disable a LAG\n");

/*
 * This function is used to configure an IP address for a port
 * which is attached to a sub interface.
 * This function accepts 2 arguments both as const char type.
 * Parameter 1 : intf lag name
 * Parameter 2 : ipv4 address
 * Return      : On Success it returns CMD_SUCCESS
 *               On Failure it returns CMD_OVSDB_FAILURE
 */
static int
lag_intf_config_ip (const char *if_name, const char *ip4, bool secondary)
{
    const struct ovsrec_port *port_row = NULL;
    struct ovsdb_idl_txn *status_txn = NULL;
    enum ovsdb_idl_txn_status status;
    bool port_found;
    char **secondary_ip4_addresses;
    size_t i;

    if (!is_valid_ip_address(ip4)) {
      vty_out(vty, "Invalid IP address. %s", VTY_NEWLINE);
      return CMD_SUCCESS;
    }

    if (!is_ip_configurable(vty, ip4, if_name, AF_INET, secondary))
    {
        VLOG_DBG("%s  An interface with the same IP address or "
                 "subnet or an overlapping network%s"
                 "%s already exists.", __func__, VTY_NEWLINE, ip4);
        return CMD_SUCCESS;
    }

    OVSREC_PORT_FOR_EACH(port_row, idl) {
        if (strncmp(port_row->name, if_name, strlen(if_name)) == 0) {
          port_found = true;
          break;
        }
    }
    if (!port_found) {
      vty_out(vty, "Port %s not found.%s", if_name, VTY_NEWLINE);
      return CMD_SUCCESS;
    }

    if (check_iface_in_bridge (if_name) && (VERIFY_VLAN_IFNAME (if_name) != 0)) {
      vty_out (vty, "Interface %s is not L3.%s", if_name, VTY_NEWLINE);
      VLOG_DBG ("%s Interface \"%s\" is not attached to any SUB_IF.%s"
                "It is attached to default bridge",
                __func__, if_name, VTY_NEWLINE);
      return CMD_SUCCESS;
    }

    port_row = port_check_and_add (if_name, true, true, status_txn);

    status_txn = cli_do_config_start ();
    if (status_txn == NULL) {
      VLOG_ERR (OVSDB_TXN_CREATE_ERROR);
      cli_do_config_abort (status_txn);
      return CMD_OVSDB_FAILURE;
    }

    if (!secondary) {
      ovsrec_port_set_ip4_address(port_row, ip4);
    } else {
        /*
         * Duplicate entries are taken care of set function.
         * Refer to ovsdb_datum_sort_unique() in vswitch-idl.c
         */
        secondary_ip4_addresses = xmalloc(
            IP_ADDRESS_LENGTH * (port_row->n_ip4_address_secondary + 1));
        for (i = 0; i < port_row->n_ip4_address_secondary; i++) {
          secondary_ip4_addresses[i] = port_row->ip4_address_secondary[i];
        }
        secondary_ip4_addresses[port_row->n_ip4_address_secondary] = (char *) ip4;
        ovsrec_port_set_ip4_address_secondary(port_row, secondary_ip4_addresses,
                                              port_row->n_ip4_address_secondary + 1);
        free (secondary_ip4_addresses);
    }

    status = cli_do_config_finish(status_txn);
    if ((status == TXN_SUCCESS) || (status == TXN_UNCHANGED)) {
      return CMD_SUCCESS;
    }
    else {
      VLOG_ERR (OVSDB_TXN_COMMIT_ERROR);
      return CMD_OVSDB_FAILURE;
    }
}

DEFUN (cli_lag_intf_config_ip4,
        cli_lag_intf_config_ip4_cmd,
        "ip address A.B.C.D/M {secondary}",
        IP_STR
        "Set IP address\n"
        "LAG Interface IP address\n")
{
    return lag_intf_config_ip((char*) vty->index, argv[0],
      (argv[1] != NULL) ? true : false);
}

DEFUN (cli_lag_intf_del_ip4,
        cli_lag_intf_del_ip4_cmd,
        "no ip address A.B.C.D/M {secondary}",
        NO_STR
        IP_STR
        "Delete IP address\n"
        "Delete Sub Interface IP address\n")
{
    const struct ovsrec_port *port_row = NULL;
    struct ovsdb_idl_txn *status_txn = NULL;
    enum ovsdb_idl_txn_status status;
    const char *if_name = (char*)vty->index;
    char ip4[IP_ADDRESS_LENGTH];
    char **secondary_ip4_addresses;
    size_t i, n;
    bool ip4_address_match = false;
    bool secondary = argv[1] != NULL ? true : false;

    status_txn = cli_do_config_start();

    if (NULL != argv[0]) {
        sprintf(ip4,"%s",argv[0]);
    }

    if (status_txn == NULL) {
        VLOG_ERR(OVSDB_TXN_CREATE_ERROR);
        cli_do_config_abort(status_txn);
        return CMD_OVSDB_FAILURE;
    }

    port_row = port_check_and_add(if_name, false, false, status_txn);

    if (!port_row) {
        vty_out(vty,"Port %s not found.%s", if_name, VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_OVSDB_FAILURE;
    }

    if (check_port_in_bridge (if_name)) {
        vty_out(vty, "Port %s is not L3.%s", if_name, VTY_NEWLINE);
        VLOG_DBG("%s Port \"%s\" is not attached to any VRF. "
                  "It is attached to default bridge",
                  __func__, if_name);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    if (!secondary) {
        if (!port_row->ip4_address) {
            vty_out(vty, "No IP address configured on port %s.%s", if_name,
                    VTY_NEWLINE);
            VLOG_DBG("%s No IP address configured on port \"%s\".",
                     __func__, if_name);
            cli_do_config_abort(status_txn);
            return CMD_SUCCESS;
        }

        if (port_row->n_ip4_address_secondary) {
            vty_out(vty, "Delete all secondary IP addresses before deleting"
                    " primary.%s",
                    VTY_NEWLINE);
            VLOG_DBG("%s Port \"%s\" has secondary IP addresses"
                     " assigned to it. Delete them before deleting primary.",
                     __func__, if_name);
            cli_do_config_abort(status_txn);
            return CMD_SUCCESS;
        }

        if (strncmp(port_row->ip4_address, ip4, IP_ADDRESS_LENGTH) != 0) {
            vty_out(vty, "IP address %s not found.%s", ip4, VTY_NEWLINE);
            VLOG_DBG("%s IP address \"%s\" not configured on interface "
                     "\"%s\".",
                     __func__, ip4, if_name);
            cli_do_config_abort(status_txn);
            return CMD_SUCCESS;
        }
        ovsrec_port_set_ip4_address(port_row, NULL);
    }
    else {
        if (!port_row->n_ip4_address_secondary) {
            vty_out(vty, "No secondary IP address configured on"
                    " port %s.%s",
                    if_name, VTY_NEWLINE);
            VLOG_DBG("%s No secondary IP address configured on port"
                     " \"%s\".",
                     __func__, if_name);
            cli_do_config_abort(status_txn);
            return CMD_SUCCESS;
        }
        for (i = 0; i < port_row->n_ip4_address_secondary; i++) {
            if (strncmp(ip4, port_row->ip4_address_secondary[i],
                        IP_ADDRESS_LENGTH) == 0) {
                ip4_address_match = true;
                break;
            }
        }

        if (!ip4_address_match) {
            vty_out(vty, "IP address %s not found.%s", ip4, VTY_NEWLINE);
            VLOG_DBG("%s IP address \"%s\" not configured on port \"%s\".",
                     __func__, ip4, if_name);
            cli_do_config_abort(status_txn);
            return CMD_SUCCESS;
        }
        secondary_ip4_addresses = xmalloc(
           IP_ADDRESS_LENGTH * (port_row->n_ip4_address_secondary - 1));
        for (i = n = 0; i < port_row->n_ip4_address_secondary; i++) {
            if (strncmp(ip4, port_row->ip4_address_secondary[i],
                        IP_ADDRESS_LENGTH) != 0) {
                secondary_ip4_addresses[n++] = port_row->ip4_address_secondary[i];
            }
        }
        ovsrec_port_set_ip4_address_secondary(port_row, secondary_ip4_addresses, n);
        free(secondary_ip4_addresses);
    }

    status = cli_do_config_finish(status_txn);

    if ((status == TXN_SUCCESS) || (status == TXN_UNCHANGED)) {
        return CMD_SUCCESS;
    } else {
        VLOG_ERR (OVSDB_TXN_COMMIT_ERROR);
        return CMD_OVSDB_FAILURE;
    }
}

DEFUN (cli_lag_intf_config_ipv6,
        cli_lag_intf_config_ipv6_cmd,
        "ipv6 address X:X::X:X/M {secondary}",
        IPV6_STR
        "Set IPv6 address\n"
        "LAG Interface IPv6 address\n")
{
    const struct ovsrec_port *port_row = NULL;
    struct ovsdb_idl_txn *status_txn = NULL;
    enum ovsdb_idl_txn_status status;
    bool secondary = false;
    const char *if_name = (char*) vty->index;
    const char *ipv6 = argv[0];
    char **secondary_ipv6_addresses;
    size_t i;

    if (!is_valid_ip_address(argv[0])) {
        vty_out(vty, "Invalid IP address.%s", VTY_NEWLINE);
        return CMD_SUCCESS;
    }

    if (argv[1] != NULL)
        secondary = true;

    status_txn = cli_do_config_start ();
    if (status_txn == NULL) {
        VLOG_ERR(OVSDB_TXN_CREATE_ERROR);
        cli_do_config_abort(status_txn);
        return CMD_OVSDB_FAILURE;
    }

    port_row = port_check_and_add(if_name, true, true, status_txn);
    if (check_iface_in_bridge (if_name) && (VERIFY_VLAN_IFNAME (if_name) != 0)) {
        vty_out(vty, "Interface %s is not L3.%s", if_name, VTY_NEWLINE);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    if (!is_ip_configurable(vty, ipv6, if_name, AF_INET6, secondary))
    {
        VLOG_DBG("%s  An interface with the same IP address or "
                 "subnet or an overlapping network%s"
                 "%s already exists.", __func__,  VTY_NEWLINE, ipv6);
        cli_do_config_abort(status_txn);
        return CMD_SUCCESS;
    }

    if (!(argv[1] != NULL)) {
      ovsrec_port_set_ip6_address(port_row, ipv6);
    } else {
         /*
          * Duplicate entries are taken care of of set function.
          * Refer to ovsdb_datum_sort_unique() in vswitch-idl.c
          */
         secondary_ipv6_addresses = xmalloc(
             IPV6_ADDRESS_LENGTH * (port_row->n_ip6_address_secondary + 1));
         for (i = 0; i < port_row->n_ip6_address_secondary; i++) {
           secondary_ipv6_addresses[i] = port_row->ip6_address_secondary[i];
         }
         secondary_ipv6_addresses[port_row->n_ip6_address_secondary] =
             (char *) ipv6;
         ovsrec_port_set_ip6_address_secondary (
             port_row, secondary_ipv6_addresses,
             port_row->n_ip6_address_secondary + 1);
         free (secondary_ipv6_addresses);
    }

    status = cli_do_config_finish (status_txn);
    if ((status == TXN_SUCCESS) || (status == TXN_UNCHANGED)) {
       return CMD_SUCCESS;
    } else {
        VLOG_ERR(OVSDB_TXN_COMMIT_ERROR);
        return CMD_OVSDB_FAILURE;
      }
}

DEFUN (cli_lag_intf_del_ipv6,
        cli_lag_intf_del_ipv6_cmd,
        "no ipv6 address X:X::X:X/M {secondary}",
        NO_STR
        IPV6_STR
        "Delete IPv6 address\n"
        "Delete LAG Interface IPv6 address\n")
{
    const struct ovsrec_port *port_row = NULL;
    struct ovsdb_idl_txn *status_txn = NULL;
    enum ovsdb_idl_txn_status status;
    const char *if_name = (char*) vty->index;
    const char *ipv6 = NULL;
    char **secondary_ipv6_addresses;
    size_t i, n;

    if (argv[0] != NULL) {
        ipv6 = argv[0];
    }

    status_txn = cli_do_config_start ();

    if (status_txn == NULL) {
        VLOG_ERR(OVSDB_TXN_CREATE_ERROR);
        cli_do_config_abort(status_txn);
        return CMD_OVSDB_FAILURE;
    }

    port_row = port_check_and_add(if_name, false, false, status_txn);

    if (!port_row) {
        VLOG_DBG("%s Interface \"%s\" does not have any port configuration.%s",
                 __func__, if_name, VTY_NEWLINE);
        cli_do_config_abort (status_txn);
        return CMD_SUCCESS;
    }

    if (!(argv[1] != NULL)) {
       if (!port_row->ip6_address) {
           vty_out(vty, "No IPv6 address configured on interface"
                   " %s.%s", if_name, VTY_NEWLINE);
           VLOG_DBG("%s No IPv6 address configured on interface"
                    " \"%s\".%s", __func__, if_name, VTY_NEWLINE);
           cli_do_config_abort(status_txn);
           return CMD_SUCCESS;
       }

       if ((NULL != ipv6) && (strncmp(port_row->ip6_address,
           ipv6, strlen(ipv6)) != 0)) {
           vty_out(vty, "IPv6 address %s not found.%s", ipv6, VTY_NEWLINE);
           VLOG_DBG("%s IPv6 address \"%s\" not configured on interface"
                    " \"%s\".%s", __func__, ipv6, if_name, VTY_NEWLINE);
           cli_do_config_abort (status_txn);
           return CMD_SUCCESS;
       }
       ovsrec_port_set_ip6_address(port_row, NULL);
    } else {
          if (!port_row->n_ip6_address_secondary) {
              vty_out(vty, "No secondary IPv6 address configured on interface"
                      " lag%s.%s",
                      if_name, VTY_NEWLINE);
              VLOG_DBG("%s No secondary IPv6 address configured on interface"
                       " lag\"%s\".",
                       __func__, if_name);
              cli_do_config_abort (status_txn);
              return CMD_SUCCESS;
          }
          bool ipv6_address_match = false;
          for (i = 0; i < port_row->n_ip6_address_secondary; i++) {
              if (strncmp(ipv6, port_row->ip6_address_secondary[i],
                  strlen(port_row->ip6_address_secondary[i])) == 0) {
                  ipv6_address_match = true;
                  break;
              }
          }

          if (!ipv6_address_match) {
              vty_out(vty, "IPv6 address %s not found.%s", ipv6, VTY_NEWLINE);
              VLOG_DBG("%s IPv6 address \"%s\" not configured on interface"
                       " \"%s\".",
                       __func__, ipv6, if_name);
              cli_do_config_abort(status_txn);
              return CMD_SUCCESS;
          }
          secondary_ipv6_addresses = xmalloc(
              IPV6_ADDRESS_LENGTH * (port_row->n_ip6_address_secondary - 1));
          for (i = n = 0; i < port_row->n_ip6_address_secondary; i++) {
              if (strncmp (ipv6, port_row->ip6_address_secondary[i],
                  strlen(port_row->ip6_address_secondary[i])) != 0) {
                  secondary_ipv6_addresses[n++] = port_row->ip6_address_secondary[i];
              }
            }
            ovsrec_port_set_ip6_address_secondary(port_row,
                                                  secondary_ipv6_addresses, n);
            free(secondary_ipv6_addresses);
    }

    status = cli_do_config_finish (status_txn);

    if ((status == TXN_SUCCESS) || (status == TXN_UNCHANGED)) {
        return CMD_SUCCESS;
    } else {
          VLOG_ERR (OVSDB_TXN_COMMIT_ERROR);
          return CMD_OVSDB_FAILURE;
      }
}


/* Note: Disabling fallback mode CLI commands until all_active mode is implemented */
/**
 * This function sets the lacp fallback mode for a given lag port
 *
 * @param lag_name name of the lag port
 * @param fallback_mode name mode to be set (priority or all_active)
 *
 * @return the status of the ovsdb transaction
 */
//static int
//lacp_set_fallback_mode(const char *lag_name, const char *fallback_mode)
//{
//    const struct ovsrec_port *port_row = NULL;
//    bool port_found = false;
//    struct smap smap = SMAP_INITIALIZER(&smap);
//    struct ovsdb_idl_txn* txn = NULL;
//    enum ovsdb_idl_txn_status status;
//
//    txn = cli_do_config_start();
//    if (txn == NULL) {
//        VLOG_ERR(LACP_OVSDB_TXN_CREATE_ERROR,__func__,__LINE__);
//        cli_do_config_abort(txn);
//        return CMD_OVSDB_FAILURE;
//    }
//
//    OVSREC_PORT_FOR_EACH (port_row, idl) {
//        if (strncmp(port_row->name, lag_name, strlen(lag_name)) == 0) {
//            port_found = true;
//            break;
//        }
//    }
//
//    if (!port_found) {
//        /* assert - as LAG port should be present in DB. */
//        assert(0);
//        cli_do_config_abort(txn);
//        VLOG_ERR("Port table entry not found in DB.Function=%s Line=%d",__func__,__LINE__);
//        return CMD_OVSDB_FAILURE;
//    }
//
//    smap_clone(&smap, &port_row->other_config);
//
//    if (strncmp(fallback_mode,
//                PORT_OTHER_CONFIG_LACP_FALLBACK_MODE_PRIORITY,
//                strlen(PORT_OTHER_CONFIG_LACP_FALLBACK_MODE_PRIORITY)) == 0) {
//        smap_remove(&smap, PORT_OTHER_CONFIG_MAP_LACP_FALLBACK_MODE);
//    } else {
//        smap_replace(&smap, PORT_OTHER_CONFIG_MAP_LACP_FALLBACK_MODE, fallback_mode);
//    }
//
//    ovsrec_port_set_other_config(port_row, &smap);
//    smap_destroy(&smap);
//    status = cli_do_config_finish(txn);
//    if (status == TXN_SUCCESS || status == TXN_UNCHANGED) {
//        return CMD_SUCCESS;
//    } else {
//        VLOG_ERR("Transaction commit failed.Function=%s Line=%d",__func__,__LINE__);
//        return CMD_OVSDB_FAILURE;
//    }
//}

//DEFUN(cli_lacp_set_fallback_mode,
//    cli_lacp_set_fallback_mode_cmd,
//    "lacp fallback mode (priority | all_active)",
//    LACP_STR
//    "Set LACP fallback mode\n"
//    "Default LACP fallback mode\n"
//    "Sets LACP fallback mode to all_active\n")
//{
//    return lacp_set_fallback_mode((char*) vty->index, argv[0]);
//}
//
//DEFUN(cli_lacp_set_no_fallback_mode,
//    cli_lacp_set_no_fallback_mode_cmd,
//    "no lacp fallback mode all_active",
//    LACP_STR
//    "Set LACP fallback mode to default value which is priority\n")
//{
//    return lacp_set_fallback_mode((char*) vty->index,
//                                  PORT_OTHER_CONFIG_LACP_FALLBACK_MODE_PRIORITY);
//}


/**
 * This function sets the lacp fallback timeout for a given lag port
 *
 * @param lag_name name of the lag port
 * @param fallback_timeout timeout to be set
 *
 * @return the status of the OVSDB transaction
 */
static int
lacp_set_fallback_timeout(const char *lag_name, const char *fallback_timeout)
{
    const struct ovsrec_port *port_row = NULL;
    bool port_found = false;
    struct smap smap = SMAP_INITIALIZER(&smap);
    struct ovsdb_idl_txn* txn = NULL;
    enum ovsdb_idl_txn_status status;

    txn = cli_do_config_start();
    if (txn == NULL) {
        VLOG_ERR(LACP_OVSDB_TXN_CREATE_ERROR,__func__,__LINE__);
        cli_do_config_abort(txn);
        return CMD_OVSDB_FAILURE;
    }

    OVSREC_PORT_FOR_EACH (port_row, idl) {
        if (strncmp(port_row->name, lag_name, strlen(lag_name)) == 0) {
            port_found = true;
            break;
        }
    }

    if (!port_found) {
        /* LAG port should be present in DB. */
        cli_do_config_abort(txn);
        VLOG_ERR("Port table entry not found in DB.Function=%s Line=%d",__func__,__LINE__);
        return CMD_OVSDB_FAILURE;
    }

    smap_clone(&smap, &port_row->other_config);
    smap_replace(&smap, PORT_OTHER_CONFIG_MAP_LACP_FALLBACK_TIMEOUT, fallback_timeout);


    ovsrec_port_set_other_config(port_row, &smap);
    smap_destroy(&smap);
    status = cli_do_config_finish(txn);
    if (status == TXN_SUCCESS || status == TXN_UNCHANGED) {
        return CMD_SUCCESS;
    } else {
        VLOG_ERR("Transaction commit failed.Function=%s Line=%d",__func__,__LINE__);
        return CMD_OVSDB_FAILURE;
    }
}


DEFUN(cli_lacp_set_fallback_timeout,
    cli_lacp_set_fallback_timeout_cmd,
    "lacp fallback timeout <1-900>",
    LACP_STR
    "Set LACP fallback timeout (time in seconds during which fallback remains active)\n"
    "The range is 1 to 900\n")
{
    return lacp_set_fallback_timeout((char*) vty->index, argv[0]);
}


/**
 * This function sets the lacp fallback timeout for a given lag port to the
 * default value (0)
 *
 * @param lag_name name of the lag port
 * @param fallback_timeout previous fallback timeout
 *
 * @return the status of the OVSDB transaction
 */
static int
lacp_set_no_fallback_timeout(const char *lag_name, const char *fallback_timeout)
{
    const struct ovsrec_port *port_row = NULL;
    bool port_found = false;
    const char* actual_fallback_timeout = NULL;
    struct smap smap = SMAP_INITIALIZER(&smap);
    struct ovsdb_idl_txn* txn = NULL;
    enum ovsdb_idl_txn_status status;

    txn = cli_do_config_start();
    if (txn == NULL) {
        VLOG_ERR(LACP_OVSDB_TXN_CREATE_ERROR,__func__,__LINE__);
        cli_do_config_abort(txn);
        return CMD_OVSDB_FAILURE;
    }

    OVSREC_PORT_FOR_EACH (port_row, idl) {
        if (strncmp(port_row->name, lag_name, strlen(lag_name)) == 0) {
            port_found = true;
            break;
        }
    }

    if (!port_found) {
        /* LAG port should be present in DB. */
        cli_do_config_abort(txn);
        VLOG_ERR("Port table entry not found in DB.Function=%s Line=%d",__func__,__LINE__);
        return CMD_OVSDB_FAILURE;
    }

    smap_clone(&smap, &port_row->other_config);
    actual_fallback_timeout = smap_get(&smap,
            PORT_OTHER_CONFIG_MAP_LACP_FALLBACK_TIMEOUT);

    /* Check if the fallback timeout configured is equal to the one passed as parameter. */
    if (!actual_fallback_timeout
        || strncmp(actual_fallback_timeout,
                   fallback_timeout,
                   strlen(fallback_timeout)) != 0) {
        cli_do_config_abort(txn);
        return CMD_OVSDB_FAILURE;
    }

    smap_remove(&smap, PORT_OTHER_CONFIG_MAP_LACP_FALLBACK_TIMEOUT);

    ovsrec_port_set_other_config(port_row, &smap);
    smap_destroy(&smap);
    status = cli_do_config_finish(txn);
    if (status == TXN_SUCCESS || status == TXN_UNCHANGED) {
        return CMD_SUCCESS;
    } else {
        VLOG_ERR("Transaction commit failed.Function=%s Line=%d",__func__,__LINE__);
        return CMD_OVSDB_FAILURE;
    }
}

DEFUN(cli_lacp_set_no_fallback_timeout,
    cli_lacp_set_no_fallback_timeout_cmd,
    "no lacp fallback timeout <1-900>",
    LACP_STR
    "Set LACP fallback timeout to default value which is 0\n")
{
    return lacp_set_no_fallback_timeout((char*) vty->index, argv[0]);
}


/*
 * Function: lacp_ovsdb_init
 * Responsibility : Add lacp related column ops-cli idl cache.
 *
 */
static void
lacp_ovsdb_init()
{
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_lacp_status);

    ovsdb_idl_add_column(idl, &ovsrec_system_col_lacp_config);
    ovsdb_idl_add_column(idl, &ovsrec_port_col_other_config);
    ovsdb_idl_add_column(idl, &ovsrec_interface_col_other_config);
    ovsdb_idl_add_column(idl, &ovsrec_port_col_lacp);
    return;
}
/*
 * Function: cli_pre_init
 * Responsibility : Initialize LACP cli node.
 */
void cli_pre_init(void)
{
  vtysh_ret_val retval = e_vtysh_error;
  install_node (&link_aggregation_node, NULL);
  vtysh_install_default (LINK_AGGREGATION_NODE);

  /* Add tables/columns needed for LACP config commands. */
  lacp_ovsdb_init();

  retval = install_show_run_config_context(e_vtysh_interface_lag_context,
                                  &vtysh_intf_lag_context_clientcallback,
                                  NULL, NULL);
  if(e_vtysh_ok != retval)
  {
    vtysh_ovsdb_config_logmsg(VTYSH_OVSDB_CONFIG_ERR,
                        "Interface LAG context unable to add config callback");
    assert(0);
    return;
  }
}
/*
 * Function: cli_post_init
 * Responsibility: Initialize LACP cli element.
 */
void cli_post_init(void)
{
  vtysh_ret_val retval = e_vtysh_error;
  install_element (LINK_AGGREGATION_NODE, &lacp_set_mode_cmd);
  install_element (LINK_AGGREGATION_NODE, &lacp_set_mode_no_cmd);
  install_element (LINK_AGGREGATION_NODE, &cli_lag_routing_cmd);
  install_element (LINK_AGGREGATION_NODE, &cli_lag_no_routing_cmd);
  install_element (LINK_AGGREGATION_NODE, &lacp_set_l2_hash_cmd);
  install_element (LINK_AGGREGATION_NODE, &lacp_set_l3_hash_cmd);
  install_element (LINK_AGGREGATION_NODE, &lacp_set_l4_hash_cmd);
  install_element (LINK_AGGREGATION_NODE, &lacp_set_fallback_cmd);
  install_element (LINK_AGGREGATION_NODE, &lacp_set_no_fallback_cmd);
  install_element (LINK_AGGREGATION_NODE, &lacp_set_heartbeat_rate_cmd);
  install_element (LINK_AGGREGATION_NODE, &lacp_set_no_heartbeat_rate_cmd);
  install_element (LINK_AGGREGATION_NODE, &lacp_set_no_heartbeat_rate_fast_cmd);
  install_element (LINK_AGGREGATION_NODE, &vtysh_exit_lacp_interface_cmd);
  install_element (LINK_AGGREGATION_NODE, &vtysh_end_all_cmd);
  install_element (LINK_AGGREGATION_NODE, &cli_lag_shutdown_cmd);
  install_element (LINK_AGGREGATION_NODE, &no_cli_lag_shutdown_cmd);

  install_element (LINK_AGGREGATION_NODE, &cli_lag_intf_config_ip4_cmd);
  install_element (LINK_AGGREGATION_NODE, &cli_lag_intf_del_ip4_cmd);
  install_element (LINK_AGGREGATION_NODE, &cli_lag_intf_config_ipv6_cmd);
  install_element (LINK_AGGREGATION_NODE, &cli_lag_intf_del_ipv6_cmd);

  /*install_element (LINK_AGGREGATION_NODE, &cli_lacp_set_fallback_mode_cmd);
  install_element (LINK_AGGREGATION_NODE, &cli_lacp_set_no_fallback_mode_cmd);*/
  install_element (LINK_AGGREGATION_NODE, &cli_lacp_set_fallback_timeout_cmd);
  install_element (LINK_AGGREGATION_NODE, &cli_lacp_set_no_fallback_timeout_cmd);

  install_element (CONFIG_NODE, &lacp_set_global_sys_priority_cmd);
  install_element (CONFIG_NODE, &lacp_set_no_global_sys_priority_cmd);
  install_element (CONFIG_NODE, &lacp_set_no_global_sys_priority_shortform_cmd);
  install_element (CONFIG_NODE, &vtysh_remove_lag_cmd);
  install_element (CONFIG_NODE, &vtysh_intf_link_aggregation_cmd);

  install_element (INTERFACE_NODE, &cli_lacp_intf_set_port_id_cmd);
  install_element (INTERFACE_NODE, &cli_lacp_intf_set_no_port_id_cmd);
  install_element (INTERFACE_NODE, &cli_lacp_intf_set_no_port_id_short_cmd);
  install_element (INTERFACE_NODE, &cli_lacp_intf_set_port_priority_cmd);
  install_element (INTERFACE_NODE, &cli_lacp_intf_set_no_port_priority_cmd);
  install_element (INTERFACE_NODE,
                   &cli_lacp_intf_set_no_port_priority_short_cmd);
  install_element (INTERFACE_NODE, &cli_lacp_add_intf_to_lag_cmd);
  install_element (INTERFACE_NODE, &cli_lacp_remove_intf_from_lag_cmd);
  install_element (ENABLE_NODE, &cli_lacp_show_configuration_cmd);
  install_element (ENABLE_NODE, &cli_lacp_show_all_aggregates_cmd);
  install_element (ENABLE_NODE, &cli_lacp_show_aggregates_cmd);
  install_element (ENABLE_NODE, &cli_lacp_show_all_interfaces_cmd);
  install_element (ENABLE_NODE, &cli_lacp_show_interfaces_cmd);

  /* Initialize lacp context show running client callback function. */
  retval = e_vtysh_error;
  retval = install_show_run_config_subcontext(e_vtysh_interface_context,
                                       e_vtysh_interface_context_lacp,
                                       &vtysh_intf_context_lacp_clientcallback,
                                       NULL, NULL);
  if(e_vtysh_ok != retval)
  {
    vtysh_ovsdb_config_logmsg(VTYSH_OVSDB_CONFIG_ERR,
                           "Interface context unable to add lacp client callback");
    assert(0);
    return;
  }

  retval = e_vtysh_error;
  retval = install_show_run_config_subcontext(e_vtysh_interface_context,
                                     e_vtysh_interface_context_lag,
                                     &vtysh_intf_context_lag_clientcallback,
                                     NULL, NULL);
  if(e_vtysh_ok != retval)
  {
    vtysh_ovsdb_config_logmsg(VTYSH_OVSDB_CONFIG_ERR,
                           "Interface context unable to add lag client callback");
    assert(0);
    return;
  }
}
