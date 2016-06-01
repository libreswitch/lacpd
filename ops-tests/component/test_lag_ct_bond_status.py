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
# Name:        test_lag_ct_bond_status.py
#
# Objective:   Verify static lag bond status is properly updated according
#              to the status of the added interfaces.
#
# Topology:    2 switches (DUT running Halon) connected by 4 interfaces
#
#
##########################################################################
import pytest
from lib_test import (
    add_intf_to_bond,
    disable_intf_list,
    enable_intf_list,
    remove_all_intf_from_bond,
    remove_intf_from_bond,
    sw_create_bond,
    verify_intf_bond_status,
    verify_intf_bond_status_empty,
    verify_intf_in_bond,
    verify_intf_not_in_bond,
    verify_intf_status,
    verify_port_bond_status
)


TOPOLOGY = """
#   +-----+------+
#   |            |
#   |    sw1     |
#   |            |
#   +--+-+--+-+--+
#      | |  | |
#      | |  | | LAG 1
#      | |  | |
#   +--+-+--+-+--+
#   |            |
#   |     sw2    |
#   |            |
#   +-----+------+

# Nodes
[type=openswitch name="OpenSwitch 1"] sw1
[type=openswitch name="OpenSwitch 2"] sw2

# Links
sw1:1 -- sw2:1
sw1:2 -- sw2:2
sw1:3 -- sw2:3
sw1:4 -- sw2:4
"""


@pytest.mark.skipif(True, reason="Skipping due to instability")
def test_lag_bond_status(topology, step):
    """
        Verify correct LAG bond_status according to the status of member
        interfaces status.
    """
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    lag_name = 'lag1'

    assert sw1 is not None
    assert sw2 is not None

    p11 = sw1.ports['1']
    p12 = sw1.ports['2']
    p13 = sw1.ports['3']
    p14 = sw1.ports['4']
    p21 = sw2.ports['1']
    p22 = sw2.ports['2']
    p23 = sw2.ports['3']
    p24 = sw2.ports['4']

    ports_sw1 = [p11, p12, p13, p14]
    ports_sw2 = [p21, p22, p23, p24]

    step("Turning on all interfaces used in this test")
    enable_intf_list(sw1, ports_sw1)
    enable_intf_list(sw2, ports_sw2)

    step("Creating static lag with 4 interfaces")
    sw_create_bond(sw1, lag_name, ports_sw1)
    sw_create_bond(sw2, lag_name, ports_sw2)

    for intf in ports_sw1:
        verify_intf_status(sw1, intf, "link_state", "up")
    for intf in ports_sw2:
        verify_intf_status(sw2, intf, "link_state", "up")

    ###########################################################################
    # 1.  When all member interfaces have bond_status up.
    ###########################################################################

    step("Verify that all the interfaces are added to LAG and that "
         "bond_status is up.")
    for intf in ports_sw1:
        verify_intf_in_bond(sw1, intf, "Expected interfaces "
                            "to be added to static lag")

        verify_intf_bond_status(sw1, intf, "up", "Expected interfaces "
                                "to have bond status UP")

    step("Verify LAG bond status is UP when all the member  interfaces "
         "have bond_status equal to UP")
    verify_port_bond_status(sw1, lag_name, "up", "Expected the LAG "
                            "to have bond status UP")

    ###########################################################################
    # 2.  When at least 1 member interface has bond_status up.
    ###########################################################################

    step("Turning off all interfaces of LAG but one")
    disable_intf_list(sw1, ports_sw1[1:4])

    step("Verify that turned off interfaces bond_status is down")
    for intf in ports_sw1[1:4]:
        verify_intf_bond_status(sw1, intf, "down", "Expected turned off "
                                "interfaces to have bond status DOWN")

    step("Verify that on interface bond_status is up")
    verify_intf_bond_status(sw1, ports_sw1[0], "up", "Expected turned on "
                            "interface to have bond status UP")

    step("Verify LAG bond status is UP when at least one member interface "
         "have bond_status equal to UP")
    verify_port_bond_status(sw1, lag_name, "up", "Expected the LAG "
                            "to have bond status UP")

    step("Turning back on interfaces")
    enable_intf_list(sw1, ports_sw1[1:4])

    ###########################################################################
    # 3.  Interfaces not member of LAG don't have bond_status.
    ###########################################################################

    step("Remove interface from LAG")
    remove_intf_from_bond(sw1, lag_name, ports_sw1[0])

    step("Verify interface is not part of LAG and bond_status is empty.")
    verify_intf_not_in_bond(sw1, ports_sw1[0],
                            "Expected the interfaces to be removed "
                            "from static lag")
    verify_intf_bond_status_empty(sw1, ports_sw1[0], "Interface expected"
                                  " to have bond status EMPTY")

    step("Verify LAG bond status is UP")
    verify_port_bond_status(sw1, lag_name, "up", "Expected the LAG "
                            "to have bond status UP")

    step("Add interface back to LAG")
    add_intf_to_bond(sw1, lag_name, ports_sw1[0])

    step("Verify interface is part of LAG and bond_status is up.")
    verify_intf_in_bond(sw1, ports_sw1[0], "Interfaces is not "
                        "added back to the LAG.")

    verify_intf_bond_status(sw1, ports_sw1[0], "up", "Expected interfaces "
                            "to have bond status UP")

    step("Verify LAG bond status is UP")
    verify_port_bond_status(sw1, lag_name, "up", "Expected the LAG "
                            "to have bond status UP")

    ###########################################################################
    # 4.  When all member interfaces have bond_status down.
    ###########################################################################

    step("Turning off all interfaces used in this test")
    disable_intf_list(sw1, ports_sw1)

    step("Verify that interfaces are not added to LAG when they are disabled"
         "and interface bond status is set to down")
    for intf in ports_sw1:
        verify_intf_not_in_bond(sw1, intf, "Interfaces should not be part "
                                "of LAG when they are disabled.")

        verify_intf_bond_status(sw1, intf, "down", "Expected interfaces "
                                "to have bond status DOWN")

    step("Verify that when all interfaces have bond_status equal to down "
         "LAG bond_status is down")
    verify_port_bond_status(sw1, lag_name, "down", "Expected the LAG "
                            "to have bond status DOWN")

    step("Turning back on all interfaces used in this test")
    enable_intf_list(sw1, ports_sw1)

    ###########################################################################
    # 5. Not eligible interface have bond_status equal to blocked.
    ###########################################################################

    step("Remove all interfaces from LAG but one")
    for intf in ports_sw1[0:3]:
        remove_intf_from_bond(sw1, lag_name, intf)

    step("Change speed of interface 2 in switch 1")
    c = ('set interface ' + str(p12) + ' hw_intf_config:speeds="10000,1000" ' +
         'hw_intf_info:max_speed="10000" hw_intf_info:speeds="10000,1000"')
    sw1(c, shell='vsctl')
    c = ('set interface ' + str(p12) + ' user_config:speeds="10000"')
    sw1(c, shell='vsctl')

    step("Add interface 2 to LAG")
    add_intf_to_bond(sw1, lag_name, p12)

    # Now we have one LAG with two interfaces with different speed each.
    # One interface is eligible and the other is not, so only one should have
    # bond_status equal to blocked.
    number_no_blocked = 0
    try:
        verify_intf_bond_status(sw1, p12, "blocked", "Expected "
                                "interfaces to have bond status BLOCKED")
    except AssertionError:
        number_no_blocked += 1
    try:
        verify_intf_bond_status(sw1, p14, "blocked", "Expected "
                                "interfaces to have bond status BLOCKED")
    except AssertionError:
        number_no_blocked += 1

    assert number_no_blocked == 1, "Expected only one interface to have " +\
        "bond_status BLOCKED"

    step("Verify LAG bond status is UP")
    verify_port_bond_status(sw1, lag_name, "up", "Expected the LAG "
                            "to have bond status UP")

    ###########################################################################
    # 6. When LAG has no member interfaces.
    ###########################################################################

    step("Remove all interfaces from LAG")
    remove_all_intf_from_bond(sw1, lag_name)

    step("Verify interface is not part of LAG and bond_status is empty.")
    for intf in ports_sw1:
        verify_intf_not_in_bond(sw1, intf,
                                "Expected the interfaces to be removed "
                                "from static lag")
        verify_intf_bond_status_empty(sw1, intf, "Interfaces expected"
                                      " to have bond status EMPTY")

    step("Verify LAG bond status is DOWN when LAG has no interfaces")
    verify_port_bond_status(sw1, lag_name, "down", "Expected the LAG "
                            "to have bond status DOWN")
