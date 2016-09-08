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
# Name:        test_ft_lacp_l2_ping.py
#
# Objective:   Verify a ping between 2 workstations connected by 2 switches
#              configured with L2 dynamic LAGs works properly.
#
# Topology:    2 switches (DUT running Halon) connected by 2 interfaces
#              2 workstations connected by the 2 switches
#
##########################################################################

from pytest import mark
from lacp_lib import (
    associate_interface_to_lag,
    associate_vlan_to_l2_interface,
    associate_vlan_to_lag,
    check_connectivity_between_hosts,
    create_lag,
    create_vlan,
    find_device_label,
    LOCAL_STATE,
    REMOTE_STATE,
    turn_on_interface,
    validate_lag_name,
    validate_lag_state_sync,
    validate_local_key,
    validate_remote_key,
    verify_lag_config,
    verify_state_sync_lag,
    verify_turn_on_interfaces
)

TOPOLOGY = """
# +-----------------+
# |                 |
# |  Workstation 1  |
# |                 |
# +-------+---------+
#         |
#         |
#   +-----+------+
#   |            |
#   |    sw1     |
#   |            |
#   +---+---+----+
#       |   |
#       |   |     LAG 1
#       |   |
#   +---+---+----+
#   |            |
#   |     sw2    |
#   |            |
#   +-----+------+
#         |
#         |
# +-------+---------+
# |                 |
# |  Workstation 2  |
# |                 |
# +-----------------+

# Nodes
[type=openswitch name="OpenSwitch 1"] sw1
[type=openswitch name="OpenSwitch 2"] sw2
[type=host name="Host 1"] hs1
[type=host name="Host 2"] hs2

# Links
hs1:1 -- sw1:3
sw1:1 -- sw2:1
sw1:2 -- sw2:2
sw2:3 -- hs2:1
"""


@mark.gate
@mark.platform_incompatible(['docker'])
def test_l2_dynamic_lag_ping_case_1(topology, step):
    """
    Case 1:
        Verify a ping between 2 workstations connected by 2 switches configured
        with L2 dynamic LAGs works properly.
    """
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')
    hs1_ip_address_with_mask = '10.0.10.1/24'
    hs2_ip_address_with_mask = '10.0.10.2/24'
    hs1_ip_address = '10.0.10.1'
    hs2_ip_address = '10.0.10.2'
    sw1_lag_id = '10'
    sw2_lag_id = '20'
    vlan_identifier = '8'
    number_pings = 5

    assert sw1 is not None
    assert sw2 is not None
    assert hs1 is not None
    assert hs2 is not None

    ports_sw1 = list()
    ports_sw2 = list()
    port_labels = ['1', '2', '3']

    step("Mapping interfaces")
    for port in port_labels:
        ports_sw1.append(sw1.ports[port])
        ports_sw2.append(sw2.ports[port])

    step("Sorting the port list")
    ports_sw1.sort()
    ports_sw2.sort()

    step("Turning on all interfaces used in this test")
    for port in ports_sw1:
        turn_on_interface(sw1, port)

    for port in ports_sw2:
        turn_on_interface(sw2, port)

    step("Validate interfaces are turn on")
    verify_turn_on_interfaces(sw1, ports_sw1)
    verify_turn_on_interfaces(sw1, ports_sw2)

    step("Assign an IP address on the same range to each workstation")
    hs1.libs.ip.interface('1', addr=hs1_ip_address_with_mask, up=True)
    hs2.libs.ip.interface('1', addr=hs2_ip_address_with_mask, up=True)

    step('Creating VLAN in both switches')
    create_vlan(sw1, vlan_identifier)
    create_vlan(sw2, vlan_identifier)

    step("Create LAG in both switches")
    create_lag(sw1, sw1_lag_id, 'active')
    create_lag(sw2, sw2_lag_id, 'active')

    step("Associate interfaces [2, 3] to LAG in both switches")
    for port in ports_sw1[1:3]:
        associate_interface_to_lag(sw1, port, sw1_lag_id)
    for port in ports_sw2[1:3]:
        associate_interface_to_lag(sw2, port, sw2_lag_id)

    step("Verify LAG configuration")
    verify_lag_config(sw1, sw1_lag_id, ports_sw1[1:3], mode='active')
    verify_lag_config(sw2, sw2_lag_id, ports_sw2[1:3], mode='active')

    step("Configure LAGs and workstations interfaces with same VLAN")
    associate_vlan_to_lag(sw1, vlan_identifier, sw1_lag_id)
    associate_vlan_to_lag(sw2, vlan_identifier, sw2_lag_id)
    associate_vlan_to_l2_interface(sw1, vlan_identifier, ports_sw1[0])
    associate_vlan_to_l2_interface(sw2, vlan_identifier, ports_sw2[0])

    step("Verify if LAG is synchronized")
    verify_state_sync_lag(sw1, ports_sw1[1:3], LOCAL_STATE, 'active')
    verify_state_sync_lag(sw1, ports_sw1[1:3], REMOTE_STATE, 'active')

    step("Get information for LAG in interface 2 with both switches")
    map_lacp_sw1 = sw1.libs.vtysh.show_lacp_interface(
                        find_device_label(sw1, ports_sw1[1]))
    map_lacp_sw2 = sw2.libs.vtysh.show_lacp_interface(
                        find_device_label(sw2, ports_sw2[1]))

    step("Validate the LAG was created in both switches")
    validate_lag_name(map_lacp_sw1, sw1_lag_id)
    validate_local_key(map_lacp_sw1, sw1_lag_id)
    validate_remote_key(map_lacp_sw1, sw2_lag_id)
    validate_lag_state_sync(map_lacp_sw1, LOCAL_STATE)
    validate_lag_state_sync(map_lacp_sw1, REMOTE_STATE)

    validate_lag_name(map_lacp_sw2, sw2_lag_id)
    validate_local_key(map_lacp_sw2, sw2_lag_id)
    validate_remote_key(map_lacp_sw2, sw1_lag_id)
    validate_lag_state_sync(map_lacp_sw2, LOCAL_STATE)
    validate_lag_state_sync(map_lacp_sw2, REMOTE_STATE)

    step("Ping workstation 2 from workstation 1 and viceversa")
    check_connectivity_between_hosts(hs1, hs1_ip_address, hs2, hs2_ip_address,
                                     number_pings, True)

    # ops-lacpd is stopped so that it produces the gcov coverage data
    #
    # Daemons from both switches will dump the coverage data to the
    # same file but the data write is done on daemon exit only.
    # The systemctl command waits until the process exits to return the
    # prompt and the shell function waits for the command to return,
    # therefore it is safe to stop the ops-lacpd daemons sequentially
    # This ensures that data from both processes is captured.
    sw1("systemctl stop ops-lacpd", shell="bash")
    sw2("systemctl stop ops-lacpd", shell="bash")
