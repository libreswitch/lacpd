/* LACP CLI commands header file
 *
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
 *
 * File: lacp_vty.h
 *
 * Purpose:  To add declarations required for lacp_vty.c
 */

#ifndef _LACP_VTY_H
#define _LACP_VTY_H


#define LAG_LB_ALG_L2     "l2-src-dst"
#define LAG_LB_ALG_L3     "l3-src-dst"
#define LAG_LB_ALG_L4     "l4-src-dst"

#define OVSDB_LB_HASH_SUFFIX  "-hash"

#define OVSDB_LB_L2_HASH    (LAG_LB_ALG_L2 OVSDB_LB_HASH_SUFFIX)
#define OVSDB_LB_L3_HASH    (LAG_LB_ALG_L3 OVSDB_LB_HASH_SUFFIX)
#define OVSDB_LB_L4_HASH    (LAG_LB_ALG_L4 OVSDB_LB_HASH_SUFFIX)

void cli_pre_init(void);
void cli_post_init(void);
bool lacp_exceeded_maximum_lag(void);
int vtysh_init_intf_lag_context_clients();
char * lacp_remove_lb_hash_suffix(const char * lb_hash);

#endif /* _LACP_VTY_H */
