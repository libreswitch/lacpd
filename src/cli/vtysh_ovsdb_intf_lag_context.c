/*
 * Copyright (C) 2015-2016 Hewlett Packard Enterprise Development LP
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
/****************************************************************************
 * @ingroup cli
 *
 * @file vtysh_ovsdb_intf_lag_context.c
 * Source for registering client callback with interface lag context.
 *
 ***************************************************************************/

#include "vtysh/vty.h"
#include "vtysh/vector.h"
#include "vswitch-idl.h"
#include "openswitch-idl.h"
#include "vtysh/vtysh_ovsdb_if.h"
#include "vtysh/vtysh_ovsdb_config.h"
#include "vtysh/utils/lacp_vtysh_utils.h"
#include "vtysh/utils/vlan_vtysh_utils.h"
#include "vtysh/utils/intf_vtysh_utils.h"
#include "vtysh/utils/system_vtysh_utils.h"
#include "lacp_vty.h"
#include "qos_lag.h"
#include "ops-utils.h"
#include "mstp_lag.h"

/*-----------------------------------------------------------------------------
| Function : vtysh_ovsdb_intftable_parse_vlan
| Responsibility : Used for VLAN related config
| Parameters :
|     const char *if_name           : Name of interface
|     vtysh_ovsdb_cbmsg_ptr p_msg   : Used for idl operations
| Return : vtysh_ret_val
-----------------------------------------------------------------------------*/
static vtysh_ret_val
vtysh_ovsdb_porttable_parse_vlan(const char *if_name,
                                 vtysh_ovsdb_cbmsg_ptr p_msg)
{
    const struct ovsrec_port *port_row;
    int i;

    port_row = port_lookup(if_name, p_msg->idl);
    if (port_row == NULL)
    {
        return e_vtysh_ok;
    }

    if (port_row->vlan_mode == NULL)
    {
        return e_vtysh_ok;
    }
    else if (strcmp(port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_ACCESS) == 0)
    {
        if(port_row->vlan_tag != NULL)
        {
            vtysh_ovsdb_cli_print(p_msg, "%4s%s%d", "", "vlan access ",
                ops_port_get_tag(port_row));
        }
    }
    else if (strcmp(port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_TRUNK) == 0)
    {
        for (i = 0; i < port_row->n_vlan_trunks; i++)
        {
            vtysh_ovsdb_cli_print(p_msg, "%4s%s%d", "", "vlan trunk allowed ",
                ops_port_get_trunks(port_row, i));
        }
    }
    else if (strcmp(port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_UNTAGGED) == 0)
    {
        if (port_row->vlan_tag != NULL)
        {
            vtysh_ovsdb_cli_print(p_msg, "%4s%s%d", "", "vlan trunk native ",
                ops_port_get_tag(port_row));
        }
        for (i = 0; i < port_row->n_vlan_trunks; i++)
        {
            vtysh_ovsdb_cli_print(p_msg, "%4s%s%d", "", "vlan trunk allowed ",
                ops_port_get_trunks(port_row, i));
        }
    }
    else if (strcmp(port_row->vlan_mode, OVSREC_PORT_VLAN_MODE_NATIVE_TAGGED) == 0)
    {
        if (port_row->vlan_tag != NULL)
        {
            vtysh_ovsdb_cli_print(p_msg, "%4s%s%d", "", "vlan trunk native ",
                ops_port_get_tag(port_row));
        }
        vtysh_ovsdb_cli_print(p_msg, "%4s%s", "", "vlan trunk native tag");
        for (i = 0; i < port_row->n_vlan_trunks; i++)
        {
            vtysh_ovsdb_cli_print(p_msg, "%4s%s%d", "", "vlan trunk allowed ",
                ops_port_get_trunks(port_row, i));
        }
    }

    return e_vtysh_ok;
}

/*-----------------------------------------------------------------------------
| Function : vtysh_intf_lag_context_clientcallback
| Responsibility : client callback routine
| Parameters :
|     void *p_private : void type object typecast to required
| Return : vtysh_ret_val
-----------------------------------------------------------------------------*/
vtysh_ret_val
vtysh_intf_lag_context_clientcallback(void *p_private)
{
  vtysh_ovsdb_cbmsg_ptr p_msg = (vtysh_ovsdb_cbmsg *)p_private;
  const char *data = NULL;
  const struct ovsrec_port *port_row = NULL;
  char * hash_prefix = NULL;
  int i;

  OVSREC_PORT_FOR_EACH(port_row, p_msg->idl)
  {
    if(strncmp(port_row->name, LAG_PORT_NAME_PREFIX, LAG_PORT_NAME_PREFIX_LENGTH) == 0)
    {
      /* Print the LAG port name because lag port is present. */
      vtysh_ovsdb_cli_print(p_msg, "interface lag %d",
                          atoi(&port_row->name[LAG_PORT_NAME_PREFIX_LENGTH]));

      data = port_row->admin;
      if(data && strncmp(data,
                         OVSREC_PORT_ADMIN_UP,
                         strlen(OVSREC_PORT_ADMIN_UP)) == 0) {
          vtysh_ovsdb_cli_print(p_msg, "%4s%s", "", "no shutdown");
      }

      if (check_port_in_bridge(port_row->name))
      {
          vtysh_ovsdb_cli_print(p_msg, "%4s%s", "", "no routing");
          vtysh_ovsdb_porttable_parse_vlan(port_row->name, p_msg);
      }
      data = port_row->lacp;
      if(data && strcmp(data, OVSREC_PORT_LACP_OFF) != 0)
      {
        vtysh_ovsdb_cli_print(p_msg, "%4slacp mode %s"," ",data);
      }

      data = NULL;
      data = smap_get(&port_row->other_config, "bond_mode");
      if (data) {
          hash_prefix = lacp_remove_lb_hash_suffix(data);
            if (hash_prefix) {
                vtysh_ovsdb_cli_print(p_msg, "%4shash %s"," ",hash_prefix);
                free(hash_prefix);
                hash_prefix = NULL;
            }
      }

      data = NULL;
      data = smap_get(&port_row->other_config, "lacp-fallback-ab");
      if (data) {
          if (VTYSH_STR_EQ(data, "true")) {
              vtysh_ovsdb_cli_print(p_msg, "%4slacp fallback"," ");
          }
      }

      data = NULL;
      data = smap_get(&port_row->other_config,
                      PORT_OTHER_CONFIG_MAP_LACP_FALLBACK_MODE);
      if (data) {
          if (VTYSH_STR_EQ(data,
                           PORT_OTHER_CONFIG_LACP_FALLBACK_MODE_ALL_ACTIVE)) {
              vtysh_ovsdb_cli_print(p_msg,
                                    "%4slacp fallback mode all_active",
                                    " ");
          }
      }

      data = NULL;
      data = smap_get(&port_row->other_config,
                      PORT_OTHER_CONFIG_MAP_LACP_FALLBACK_TIMEOUT);
      if (data) {
          vtysh_ovsdb_cli_print(p_msg, "%4slacp fallback timeout %s", " ", data);
      }

      data = NULL;
      data = smap_get(&port_row->other_config, "lacp-time");
      if (data) {
          vtysh_ovsdb_cli_print(p_msg, "%4slacp rate %s"," ",data);
      }

      data = NULL;
      data = port_row->ip4_address;
      if (data) {
          vtysh_ovsdb_cli_print(p_msg, "%4sip address %s"," ",data);
      }

      for (i = 0; i < port_row->n_ip4_address_secondary; i++) {
         vtysh_ovsdb_cli_print(p_msg, "%4sip address %s secondary"," ",
                               port_row->ip4_address_secondary[i]);
      }

      data = NULL;
      data = port_row->ip6_address;
      if (data) {
          vtysh_ovsdb_cli_print(p_msg, "%4sipv6 address %s", " ",data);
      }

      for (i = 0; i < port_row->n_ip6_address_secondary; i++) {
         vtysh_ovsdb_cli_print(p_msg, "%4sipv6 address %s secondary", " ",
                               port_row->ip6_address_secondary[i]);
      }

      qos_trust_lag_show_running_config(port_row);
      qos_cos_lag_show_running_config(port_row);
      qos_dscp_lag_show_running_config(port_row);
      qos_apply_lag_show_running_config(port_row);
      mstp_lag_show_running_config(port_row);
    }

  }

  return e_vtysh_ok;
}

/*-----------------------------------------------------------------------------
| Function : vtysh_intf_context_lacp_clientcallback
| Responsibility : Interface context, LACP sub-context callback routine.
| Parameters :
|     p_private: Void pointer for holding address of vtysh_ovsdb_cbmsg_ptr
|                structure object.
| Return : vtysh_ret_val
-----------------------------------------------------------------------------*/
vtysh_ret_val
vtysh_intf_context_lacp_clientcallback(void *p_private)
{
  const char *data = NULL;
  vtysh_ovsdb_cbmsg_ptr p_msg = (vtysh_ovsdb_cbmsg *)p_private;
  const struct ovsrec_interface *ifrow = NULL;

  ifrow = (struct ovsrec_interface *)p_msg->feature_row;
  data = smap_get(&ifrow->other_config,
                  INTERFACE_OTHER_CONFIG_MAP_LACP_PORT_ID);

  if (data)
  {
    PRINT_INTERFACE_NAME(p_msg->disp_header_cfg, p_msg, ifrow->name)
    vtysh_ovsdb_cli_print(p_msg, "%4s%s %d", "", "lacp port-id", atoi(data));
  }
  data = NULL;
  data = smap_get(&ifrow->other_config,
                  INTERFACE_OTHER_CONFIG_MAP_LACP_PORT_PRIORITY);
  if (data)
  {
    if (atoi(data) != LACP_DEFAULT_PORT_PRIORITY) {
      PRINT_INTERFACE_NAME(p_msg->disp_header_cfg, p_msg, ifrow->name)
      vtysh_ovsdb_cli_print(p_msg, "%4s%s %d", "", "lacp port-priority",
                            atoi(data));
    }
  }

  return e_vtysh_ok;
}

/*-----------------------------------------------------------------------------
| Function : vtysh_intf_context_lag_clientcallback
| Responsibility : Interface context, LAG sub-context callback routine.
| Parameters :
|     p_private: Void pointer for holding address of vtysh_ovsdb_cbmsg_ptr
|                structure object.
| Return : vtysh_ret_val
-----------------------------------------------------------------------------*/
vtysh_ret_val
vtysh_intf_context_lag_clientcallback(void *p_private)
{
  const struct ovsrec_port *port_row = NULL;
  const struct ovsrec_interface *if_row = NULL;
  vtysh_ovsdb_cbmsg_ptr p_msg = (vtysh_ovsdb_cbmsg *)p_private;
  const struct ovsrec_interface *ifrow = NULL;
  int k=0;

  ifrow = (struct ovsrec_interface *)p_msg->feature_row;
  OVSREC_PORT_FOR_EACH(port_row, p_msg->idl)
  {
    if (strncmp(port_row->name, LAG_PORT_NAME_PREFIX,
                LAG_PORT_NAME_PREFIX_LENGTH) == 0)
    {
      for (k = 0; k < port_row->n_interfaces; k++)
      {
        if_row = port_row->interfaces[k];
        if(strcmp(ifrow->name, if_row->name) == 0)
        {
          PRINT_INTERFACE_NAME(p_msg->disp_header_cfg, p_msg, ifrow->name)
          vtysh_ovsdb_cli_print(p_msg, "%4s%s %d", " ", "lag",
                                atoi(&port_row->name[LAG_PORT_NAME_PREFIX_LENGTH]));
        }
      }
    }
  }

  return e_vtysh_ok;
}
