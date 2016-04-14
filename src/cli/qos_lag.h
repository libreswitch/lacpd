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

#ifndef _QOS_LAG_H_
#define _QOS_LAG_H_

#define QOS_TRUST_KEY "qos_trust"
#define QOS_COS_OVERRIDE_KEY "cos_override"
#define QOS_DSCP_OVERRIDE_KEY "dscp_override"

void qos_trust_lag_show_running_config(const struct ovsrec_port *port_row);

void qos_cos_lag_show_running_config(const struct ovsrec_port *port_row);

void qos_apply_lag_show_running_config(const struct ovsrec_port *port_row);

void qos_dscp_lag_show_running_config(const struct ovsrec_port *port_row);

#endif /* _QOS_LAG_H_ */
