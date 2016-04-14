/****************************************************************************
 * (c) Copyright 2015-2016 Hewlett Packard Enterprise Development LP
 *
 *    Licensed under the Apache License, Version 2.0 (the "License"); you may
 *    not use this file except in compliance with the License. You may obtain
 *    a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *    License for the specific language governing permissions and limitations
 *    under the License.
 *
 ***************************************************************************/

#include "vtysh/command.h"
#include "vtysh/vtysh.h"
#include "vtysh/vtysh_user.h"
#include "vswitch-idl.h"
#include "ovsdb-idl.h"
#include "smap.h"
#include "memory.h"
#include "openvswitch/vlog.h"
#include "openswitch-idl.h"
#include "vtysh/vtysh_ovsdb_if.h"
#include "vtysh/vtysh_ovsdb_config.h"
#include "qos_lag.h"

extern struct ovsdb_idl *idl;

void qos_trust_lag_show_running_config(const struct ovsrec_port *port_row) {
    if (port_row == NULL) {
        return;
    }

    const char *qos_trust_name = smap_get(&port_row->qos_config, QOS_TRUST_KEY);
    if (qos_trust_name == NULL) {
        return;
    }

    vty_out(vty, "    qos trust %s%s", qos_trust_name, VTY_NEWLINE);
}

void qos_cos_lag_show_running_config(const struct ovsrec_port *port_row) {
    if (port_row == NULL) {
        return;
    }

    const char *cos_map_index = smap_get(&port_row->qos_config,
            QOS_COS_OVERRIDE_KEY);
    if (cos_map_index == NULL) {
        return;
    }

    vty_out(vty, "    qos cos %s%s", cos_map_index, VTY_NEWLINE);
}

void qos_apply_lag_show_running_config(const struct ovsrec_port *port_row) {
    if (port_row == NULL) {
        return;
    }

    /* Show the schedule profile. */
    if (port_row->qos != NULL) {
        const char *schedule_profile_name = port_row->qos->name;
        if (schedule_profile_name != NULL) {
            vty_out(vty, "    apply qos schedule-profile %s%s",
                    schedule_profile_name, VTY_NEWLINE);
        }
    }
}

void qos_dscp_lag_show_running_config(const struct ovsrec_port *port_row) {
    if (port_row == NULL) {
        return;
    }

    const char *dscp_map_index = smap_get(&port_row->qos_config,
            QOS_DSCP_OVERRIDE_KEY);
    if (dscp_map_index == NULL) {
        return;
    }

    vty_out(vty, "    qos dscp %s%s", dscp_map_index, VTY_NEWLINE);
}
