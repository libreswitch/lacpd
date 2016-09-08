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
# Name:        test_ft_static_lag_l3_ping.py
#
# Objective:   Verify a ping between 2 switches configured with L3 static
#              LAGs works properly.
#
# Topology:    2 switches (DUT running Halon) connected by 3 interfaces
#
##########################################################################

from lacp_lib import (
    assign_ip_to_lag,
    associate_interface_to_lag,
    check_connectivity_between_switches,
    create_lag,
    turn_on_interface,
    verify_turn_on_interfaces
)
import time
import pytest

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
sw1:3 -- sw2:3
"""


@pytest.mark.skipif(True, reason="Skipping due to instability")
def test_l3_static_lag_ping_case_1(topology, step):
    """
    Case 1:
        Verify a simple ping works properly between 2 switches configured
        with static L3 LAGs. Each LAG having 3 interfaces.
    """
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    sw1_lag_id = '100'
    sw2_lag_id = '200'
    sw1_lag_ip_address = '10.0.0.1'
    sw2_lag_ip_address = '10.0.0.2'
    ip_address_mask = '24'
    number_pings = 10

    assert sw1 is not None
    assert sw2 is not None

    port_labels = ['1', '2', '3']
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
    p21 = ports_sw2[0]
    p22 = ports_sw2[1]
    p23 = ports_sw2[2]

    step("Turning on all interfaces used in this test")
    ports_sw1 = [p11, p12, p13]
    for port in ports_sw1:
        turn_on_interface(sw1, port)

    ports_sw2 = [p21, p22, p23]
    for port in ports_sw2:
        turn_on_interface(sw2, port)

    step("Create LAG in both switches")
    create_lag(sw1, sw1_lag_id, 'off')
    create_lag(sw2, sw2_lag_id, 'off')

    step("Associate interfaces [1,2, 3] to lag in both switches")
    associate_interface_to_lag(sw1, p11, sw1_lag_id)
    associate_interface_to_lag(sw1, p12, sw1_lag_id)
    associate_interface_to_lag(sw1, p13, sw1_lag_id)
    associate_interface_to_lag(sw2, p21, sw2_lag_id)
    associate_interface_to_lag(sw2, p22, sw2_lag_id)
    associate_interface_to_lag(sw2, p23, sw2_lag_id)

    step("#### Validate interfaces are turn on ####")
    verify_turn_on_interfaces(sw1, ports_sw1)
    verify_turn_on_interfaces(sw2, ports_sw2)

    step("Assign IP to LAGs")
    assign_ip_to_lag(sw1, sw1_lag_id, sw1_lag_ip_address, ip_address_mask)
    assign_ip_to_lag(sw2, sw2_lag_id, sw2_lag_ip_address, ip_address_mask)

    step("Ping switch2 from switch1 and viceversa")
    check_connectivity_between_switches(sw1, sw1_lag_ip_address, sw2,
                                        sw2_lag_ip_address, number_pings, True)
