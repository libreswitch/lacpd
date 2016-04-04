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
# Name:        test_ft_static_lag_l2_ping.py
#
# Objective:   Verify a ping between 2 workstations connected by 2 switches
#              configured with L2 static LAGs works properly.
#
# Topology:    2 switches (DUT running Halon) connected by 2 interfaces
#              2 workstations connected by the 2 switches
#
##########################################################################

from time import sleep
from lacp_lib import create_lag
from lacp_lib import associate_interface_to_lag
from lacp_lib import turn_on_interface
from lacp_lib import validate_turn_on_interfaces
from lacp_lib import create_vlan
from lacp_lib import associate_vlan_to_lag
from lacp_lib import associate_vlan_to_l2_interface
from lacp_lib import check_connectivity_between_hosts

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


def test_l2_static_lag_ping_case_1(topology):
    """
    Case 1:
        Verify a ping between 2 workstations connected by 2 switches configured
        with L2 static LAGs works properly.
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

    p11 = sw1.ports['1']
    p12 = sw1.ports['2']
    p13 = sw1.ports['3']
    p21 = sw2.ports['1']
    p22 = sw2.ports['2']
    p23 = sw2.ports['3']

    print("Turning on all interfaces used in this test")
    ports_sw1 = [p11, p12, p13]
    for port in ports_sw1:
        turn_on_interface(sw1, port)

    ports_sw2 = [p21, p22, p23]
    for port in ports_sw2:
        turn_on_interface(sw2, port)

    print("Waiting some time for the interfaces to be up")
    sleep(60)

    print("Verify all interface are up")
    validate_turn_on_interfaces(sw1, ports_sw1)
    validate_turn_on_interfaces(sw2, ports_sw2)

    print("Assign an IP address on the same range to each workstation")
    hs1.libs.ip.interface('1', addr=hs1_ip_address_with_mask, up=True)
    hs2.libs.ip.interface('1', addr=hs2_ip_address_with_mask, up=True)

    print('Creating VLAN in both switches')
    create_vlan(sw1, vlan_identifier)
    create_vlan(sw2, vlan_identifier)

    print("Create LAG in both switches")
    create_lag(sw1, sw1_lag_id, 'off')
    create_lag(sw2, sw2_lag_id, 'off')

    print("Associate interfaces [2, 3] to LAG in both switches")
    associate_interface_to_lag(sw1, p12, sw1_lag_id)
    associate_interface_to_lag(sw1, p13, sw1_lag_id)
    associate_interface_to_lag(sw2, p22, sw2_lag_id)
    associate_interface_to_lag(sw2, p23, sw2_lag_id)

    print("Configure LAGs and workstations interfaces with same VLAN")
    associate_vlan_to_lag(sw1, vlan_identifier, sw1_lag_id)
    associate_vlan_to_lag(sw2, vlan_identifier, sw2_lag_id)
    associate_vlan_to_l2_interface(sw1, vlan_identifier, p11)
    associate_vlan_to_l2_interface(sw2, vlan_identifier, p21)

    print("Waiting some time for change to apply")
    sleep(30)

    print("Ping workstation 2 from workstation 1 and viceversa")
    check_connectivity_between_hosts(hs1, hs1_ip_address, hs2, hs2_ip_address,
                                     number_pings, True)
