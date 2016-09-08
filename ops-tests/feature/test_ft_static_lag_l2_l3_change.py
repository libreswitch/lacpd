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
# Name:        test_ft_static_lag_l2_l3_change.py
#
# Objective:   Verify a static LAG switches properly from L2 to L3 and
#              viceversa.
#
# Topology:    2 switches (DUT running Halon) connected by 2 interfaces
#              2 workstations connected by the 2 switches
#
##########################################################################

from pytest import mark
from lacp_lib import (
    assign_ip_to_lag,
    associate_interface_to_lag,
    associate_vlan_to_l2_interface,
    associate_vlan_to_lag,
    check_connectivity_between_hosts,
    check_connectivity_between_switches,
    create_lag,
    create_vlan,
    turn_on_interface,
    verify_connectivity_between_hosts,
    verify_turn_on_interfaces
)
from time import sleep
import pytest

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
hs1:1 -- sw1:1
sw1:2 -- sw2:2
sw1:3 -- sw2:3
sw2:1 -- hs2:1
"""


@mark.gate
@pytest.mark.skipif(True, reason="Skipping due to instability")
def test_l2_l3_switch_case_1(topology):
    """
    Case 1:
        Verify the correct communication of 2 switches connected first by a
        L2 LAG, then by a L3 LAG and finally connected by a L2 LAG again.
    """
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')
    ip_address_mask = '24'
    hs1_ip_address = '10.0.10.1'
    hs2_ip_address = '10.0.10.2'
    hs1_ip_address_with_mask = hs1_ip_address + '/' + ip_address_mask
    hs2_ip_address_with_mask = hs2_ip_address + '/' + ip_address_mask
    sw1_lag_ip_address = '10.0.0.1'
    sw2_lag_ip_address = '10.0.0.2'
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

    step("Verify all interface are up")
    verify_turn_on_interfaces(sw1, ports_sw1)
    verify_turn_on_interfaces(sw2, ports_sw2)

    step("Assign an IP address on the same range to each workstation")
    hs1.libs.ip.interface('1', addr=hs1_ip_address_with_mask, up=True)
    hs2.libs.ip.interface('1', addr=hs2_ip_address_with_mask, up=True)

    step('Creating VLAN in both switches')
    create_vlan(sw1, vlan_identifier)
    create_vlan(sw2, vlan_identifier)

    step("Create LAG in both switches")
    create_lag(sw1, sw1_lag_id, 'off')
    create_lag(sw2, sw2_lag_id, 'off')

    step("Associate interfaces [2, 3] to LAG in both switches")
    for intf in ports_sw1[1:3]:
        associate_interface_to_lag(sw1, intf, sw1_lag_id)

    for intf in ports_sw2[1:3]:
        associate_interface_to_lag(sw2, intf, sw2_lag_id)

    step("Configure LAGs and workstations interfaces with same VLAN")
    associate_vlan_to_lag(sw1, vlan_identifier, sw1_lag_id)
    associate_vlan_to_lag(sw2, vlan_identifier, sw2_lag_id)
    associate_vlan_to_l2_interface(sw1, vlan_identifier, ports_sw1[0])
    associate_vlan_to_l2_interface(sw2, vlan_identifier, ports_sw2[0])

    step("Test ping between clients")
    verify_connectivity_between_hosts(hs1, hs1_ip_address, hs2, hs2_ip_address)

    step("Assign IP on the same range to LAG in both switches")
    assign_ip_to_lag(sw1, sw1_lag_id, sw1_lag_ip_address, ip_address_mask)
    assign_ip_to_lag(sw2, sw2_lag_id, sw2_lag_ip_address, ip_address_mask)

    step(" Ping between workstations should fail")
    check_connectivity_between_hosts(hs1, hs1_ip_address, hs2, hs2_ip_address,
                                     number_pings, False)

    step("Ping between switches should succeed")
    check_connectivity_between_switches(sw1, sw1_lag_ip_address, sw2,
                                        sw2_lag_ip_address, number_pings,
                                        True)

    step("Configure LAGs with VLAN")
    associate_vlan_to_lag(sw1, vlan_identifier, sw1_lag_id)
    associate_vlan_to_lag(sw2, vlan_identifier, sw2_lag_id)

    step("Ping between workstations should succeed")
    verify_connectivity_between_hosts(hs1, hs1_ip_address, hs2, hs2_ip_address)

    step("Ping between switches should fail")
    check_connectivity_between_switches(sw1, sw1_lag_ip_address, sw2,
                                        sw2_lag_ip_address, number_pings,
                                        False)
