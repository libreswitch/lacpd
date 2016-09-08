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
# Name:        test_ft_static_lag_interface_l2_l3_change.py
#
# Objective:   Verify an interface switches correctly from L2 LAG to L3 LAG
#              and viceversa
#
# Topology:    2 switches (DUT running Halon) connected by 3 interfaces
#              2 workstations connected by the 2 switches
#
##########################################################################

from pytest import mark
from lacp_lib import(
    assign_ip_to_lag,
    associate_interface_to_lag,
    associate_vlan_to_l2_interface,
    associate_vlan_to_lag,
    create_lag,
    create_vlan,
    remove_interface_from_lag,
    turn_on_interface,
    verify_connectivity_between_hosts,
    verify_connectivity_between_switches,
    verify_lag_config,
    verify_turn_on_interfaces
)

TOPOLOGY = """
#  +-----------------+
#  |                 |
#  |  Workstation 1  |
#  |                 |
#  +-------+---------+
#          |
#          |
#    +-----+------+
#    |            |
#    |  Switch 1  |
#    |  1  2  3   |
#    +--+--+--+---+
#       |  |  |
#  L2   |  |  |  L3
#  LAG  |  |  |  LAG
#    +--+--+--+---+
#    |  1  2  3   |
#    |  Switch 2  |
#    |            |
#    +-----+------+
#          |
#          |
#  +-------+---------+
#  |                 |
#  |  Workstation 2  |
#  |                 |
#  +-----------------+

# Nodes
[type=openswitch name="OpenSwitch 1"] sw1
[type=openswitch name="OpenSwitch 2"] sw2
[type=host name="Host 1"] hs1
[type=host name="Host 2"] hs2

# Links
hs1:1 -- sw1:4
sw1:1 -- sw2:1
sw1:2 -- sw2:2
sw1:3 -- sw2:3
sw2:4 -- hs2:1
"""

# Ports
port_labels = ['1', '2', '3', '4']


@mark.platform_incompatible(['docker'])
@mark.gate
@mark.platform_incompatible(['docker'])
def test_l2_l3_interface_switch_case_1(topology, step):
    """
    Case 1:
        This test verifies the functionality of 2 LAGs when an interface is
        changed from a L2 LAG to a L3 LAG and viceversa.
    """
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')

    assert sw1 is not None
    assert sw2 is not None
    assert hs1 is not None
    assert hs2 is not None

    ip_address_mask = '24'
    hs1_ip_address = '10.0.10.1'
    hs2_ip_address = '10.0.10.2'
    hs1_ip_address_with_mask = hs1_ip_address + '/' + ip_address_mask
    hs2_ip_address_with_mask = hs2_ip_address + '/' + ip_address_mask
    sw1_l3_lag_ip_address = '10.0.0.1'
    sw2_l3_lag_ip_address = '10.0.0.2'
    l2_lag_id = '2'
    l3_lag_id = '3'
    vlan_identifier = '8'

    ports_sw1 = list()
    ports_sw2 = list()

    step("Mapping interfaces")
    for port in port_labels:
        ports_sw1.append(sw1.ports[port])
        ports_sw2.append(sw2.ports[port])
    step("Sorting the port list")
    ports_sw1.sort()
    ports_sw2.sort()

    p11 = ports_sw1[0]
    p12 = ports_sw1[1]
    p13 = ports_sw1[2]
    p14 = ports_sw1[3]
    p21 = ports_sw2[0]
    p22 = ports_sw2[1]
    p23 = ports_sw2[2]
    p24 = ports_sw2[3]

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

    step("Create L2 LAG in both switches")
    create_lag(sw1, l2_lag_id, 'off')
    create_lag(sw2, l2_lag_id, 'off')

    step("Associate interfaces [1, 2] to L2 LAG in both switches")
    associate_interface_to_lag(sw1, p11, l2_lag_id)
    associate_interface_to_lag(sw1, p12, l2_lag_id)
    associate_interface_to_lag(sw2, p21, l2_lag_id)
    associate_interface_to_lag(sw2, p22, l2_lag_id)

    step("Verify LAG configuration")
    verify_lag_config(sw1, l2_lag_id, [p11, p12])
    verify_lag_config(sw2, l2_lag_id, [p21, p22])

    step("Create L3 LAG in both switches")
    create_lag(sw1, l3_lag_id, 'off')
    create_lag(sw2, l3_lag_id, 'off')

    step("Associate interface 3 to L3 LAG in both switches")
    associate_interface_to_lag(sw1, p13, l3_lag_id)
    associate_interface_to_lag(sw2, p23, l3_lag_id)

    step("Verify LAG configuration")
    verify_lag_config(sw1, l3_lag_id, [p13])
    verify_lag_config(sw2, l3_lag_id, [p23])

    step("Configure LAGs and workstations interfaces with same VLAN")
    associate_vlan_to_lag(sw1, vlan_identifier, l2_lag_id)
    associate_vlan_to_lag(sw2, vlan_identifier, l2_lag_id)
    associate_vlan_to_l2_interface(sw1, vlan_identifier, p14)
    associate_vlan_to_l2_interface(sw2, vlan_identifier, p24)

    step("Assign IP on the same range to LAG in both switches")
    assign_ip_to_lag(sw1, l3_lag_id, sw1_l3_lag_ip_address, ip_address_mask)
    assign_ip_to_lag(sw2, l3_lag_id, sw2_l3_lag_ip_address, ip_address_mask)

    step("Test ping between clients work")
    verify_connectivity_between_hosts(hs1, hs1_ip_address,
                                      hs2, hs2_ip_address)
    verify_connectivity_between_switches(sw1, sw1_l3_lag_ip_address,
                                         sw2, sw2_l3_lag_ip_address)

    step("Associate interface 2 to L3 LAG in both switches")
    associate_interface_to_lag(sw1, p12, l3_lag_id)
    associate_interface_to_lag(sw2, p22, l3_lag_id)

    step("Test ping between clients work")
    verify_connectivity_between_hosts(hs1, hs1_ip_address,
                                      hs2, hs2_ip_address)
    verify_connectivity_between_switches(sw1, sw1_l3_lag_ip_address,
                                         sw2, sw2_l3_lag_ip_address)

    step("Associate interface 2 to L2 LAG in both switches")
    associate_interface_to_lag(sw1, p12, l2_lag_id)
    associate_interface_to_lag(sw2, p22, l2_lag_id)

    step("Test ping between clients work")
    verify_connectivity_between_hosts(hs1, hs1_ip_address,
                                      hs2, hs2_ip_address)
    verify_connectivity_between_switches(sw1, sw1_l3_lag_ip_address,
                                         sw2, sw2_l3_lag_ip_address)

    step("Remove interface 2 from L2 LAG in both switches")
    remove_interface_from_lag(sw1, p12, l2_lag_id)
    remove_interface_from_lag(sw2, p22, l2_lag_id)

    step("Associate interface 2 to L3 LAG in both switches")
    associate_interface_to_lag(sw1, p12, l3_lag_id)
    associate_interface_to_lag(sw2, p22, l3_lag_id)

    step("Test ping between clients work")
    verify_connectivity_between_hosts(hs1, hs1_ip_address,
                                      hs2, hs2_ip_address)
    verify_connectivity_between_switches(sw1, sw1_l3_lag_ip_address,
                                         sw2, sw2_l3_lag_ip_address)

    step("Remove interface 2 from L3 LAG in both switches")
    remove_interface_from_lag(sw1, p12, l3_lag_id)
    remove_interface_from_lag(sw2, p22, l3_lag_id)

    step("Associate interface 2 to L2 LAG in both switches")
    associate_interface_to_lag(sw1, p12, l2_lag_id)
    associate_interface_to_lag(sw2, p22, l2_lag_id)

    step("Test ping between clients work")
    verify_connectivity_between_hosts(hs1, hs1_ip_address,
                                      hs2, hs2_ip_address)
    verify_connectivity_between_switches(sw1, sw1_l3_lag_ip_address,
                                         sw2, sw2_l3_lag_ip_address)
