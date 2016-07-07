# -*- coding: utf-8 -*-
#
# Copyright (C) 2016 Hewlett Packard Enterprise Development LP
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

##########################################################################
# Name:        test_ft_lacp_l3_ping.py
#
# Objective:   Verify a ping between 2 switches configured with L3 dynamic
#              LAGs works properly.
#
# Topology:    2 switches (DUT running Halon) connected by 3 interfaces
#
##########################################################################

from pytest import mark
from lacp_lib import (
    assign_ip_to_lag,
    associate_interface_to_lag,
    create_lag,
    LOCAL_STATE,
    REMOTE_STATE,
    set_lacp_rate_fast,
    turn_on_interface,
    validate_local_key,
    validate_remote_key,
    verify_connectivity_between_switches,
    verify_lag_config,
    verify_state_sync_lag,
    verify_turn_on_interfaces
)


TOPOLOGY = """
# +-------+     +-------+
# |  sw1  |-----|  sw2  |
# +-------+     +-------+

# Nodes
[type=openswitch name="Switch 1"] sw1
[type=openswitch name="Switch 2"] sw2

# Links
sw1:1 -- sw2:1
sw1:2 -- sw2:2
# Remove unused switch sw3
"""


@mark.gate
@mark.platform_incompatible(['docker'])
def test_l3_dynamic_lag_ping_case_1(topology, step):
    """
    Case 1:
        Verify a simple ping works properly between 2 switches configured
        with L3 dynamic LAGs. Each LAG having 3 interfaces.
    """
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')

    step('### Verifying switches are not None ###')
    assert sw1 is not None, 'Topology failed getting object sw1'
    assert sw2 is not None, 'Topology failed getting object sw2'

    lag_id = '1'
    sw1_lag_ip_address = '10.0.0.1'
    sw2_lag_ip_address = '10.0.0.2'
    ip_address_mask = '24'
    mode_active = 'active'
    mode_passive = 'passive'

    ports_sw1 = list()
    ports_sw2 = list()
    # Remove unused port 3
    port_labels = ['1', '2']

    step("### Mapping interfaces from Docker ###")
    for port in port_labels:
        ports_sw1.append(sw1.ports[port])
        ports_sw2.append(sw2.ports[port])

    step("Sorting the port list")
    ports_sw1.sort()
    ports_sw2.sort()

    step("### Turning on all interfaces used in this test ###")
    for port in ports_sw1:
        turn_on_interface(sw1, port)

    for port in ports_sw2:
        turn_on_interface(sw2, port)

    step("#### Validate interfaces are turned on ####")
    verify_turn_on_interfaces(sw1, ports_sw1)
    verify_turn_on_interfaces(sw2, ports_sw2)

    # Modify hardcoded interfaces for dynamic interfaces
    mac_addr_sw1 = sw1.libs.vtysh.show_interface('1')['mac_address']
    mac_addr_sw2 = sw2.libs.vtysh.show_interface('1')['mac_address']
    assert mac_addr_sw1 != mac_addr_sw2,\
        'Mac address of interfaces in sw1 is equal to mac address of ' +\
        'interfaces in sw2. This is a test framework problem. Dynamic ' +\
        'LAGs cannot work properly under this condition. Refer to Taiga ' +\
        'issue #1251.'

    step("### Create LAG in both switches ###")
    create_lag(sw1, lag_id, 'active')
    create_lag(sw2, lag_id, 'passive')

    step("### Set LACP rate to fast ###")
    set_lacp_rate_fast(sw1, lag_id)
    set_lacp_rate_fast(sw2, lag_id)

    step("#### Associate Interfaces to LAG ####")
    for intf in ports_sw1:
        associate_interface_to_lag(sw1, intf, lag_id)

    for intf in ports_sw2:
        associate_interface_to_lag(sw2, intf, lag_id)

    step("### Assign IP to LAGs ###")
    assign_ip_to_lag(sw1, lag_id, sw1_lag_ip_address, ip_address_mask)
    assign_ip_to_lag(sw2, lag_id, sw2_lag_ip_address, ip_address_mask)

    step("#### Verify LAG configuration ####")
    verify_lag_config(sw1, lag_id, ports_sw1,
                      heartbeat_rate='fast', mode=mode_active)
    verify_lag_config(sw2, lag_id, ports_sw2,
                      heartbeat_rate='fast', mode=mode_passive)

    step("### Verify if LAG is synchronized")
    verify_state_sync_lag(sw1, ports_sw1, LOCAL_STATE, mode_active)
    verify_state_sync_lag(sw1, ports_sw1, REMOTE_STATE, mode_passive)
    verify_state_sync_lag(sw2, ports_sw2, LOCAL_STATE, mode_passive)
    verify_state_sync_lag(sw2, ports_sw2, REMOTE_STATE, mode_active)

    step("### Get information for LAG in interface 1 with both switches ###")
    map_lacp_sw1 = sw1.libs.vtysh.show_lacp_interface(ports_sw1[0])
    map_lacp_sw2 = sw2.libs.vtysh.show_lacp_interface(ports_sw2[0])

    step("### Validate the LAG was created in both switches ###")
    validate_local_key(map_lacp_sw1, lag_id)
    validate_remote_key(map_lacp_sw1, lag_id)
    validate_local_key(map_lacp_sw2, lag_id)
    validate_remote_key(map_lacp_sw2, lag_id)

    step("#### Test ping between switches work ####")
    verify_connectivity_between_switches(sw1, sw1_lag_ip_address,
                                         sw2, sw2_lag_ip_address,
                                         success=True)
