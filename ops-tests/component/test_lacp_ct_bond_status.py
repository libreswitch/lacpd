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
# Name:        test_lacp_ct_bond_status.py
#
# Objective:   Verify lacp bond status is properly updated according to the
#              status of the added interfaces.
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
    set_port_parameter,
    sw_create_bond,
    sw_wait_until_all_sm_ready,
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


###############################################################################
#
#                       ACTOR STATE STATE MACHINES VARIABLES
#
###############################################################################
sm_col_and_dist = '"Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,Dist:1,Def:0,Exp:0"'
active_no_fallback = \
    '"Activ:1,TmOut:\d,Aggr:1,Sync:0,Col:0,Dist:0,Def:1,Exp:1"'
active_fallback = '"Activ:1,TmOut:\d,Aggr:1,Sync:0,Col:0,Dist:0,Def:1,Exp:0"'

###############################################################################
#
#                           TEST TOGGLE VARIABLES
#
###############################################################################
disable_lag = ['lacp=off']
active_lag = ['lacp=active']

fallback_key = 'lacp-fallback-ab'
other_config_key = 'other_config'
enable_fallback = ['%s:%s="true"' % (other_config_key, fallback_key)]
disable_fallback = ['%s:%s="false"' % (other_config_key, fallback_key)]


@pytest.mark.skipif(True, reason="Skipping due to instability")
def test_lacp_bond_status(topology, step):
    """
        Verify correct LACP bond_status according to the status of member
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

    step("Creating dynamic lag with 4 interfaces")
    sw_create_bond(sw1, lag_name, ports_sw1, lacp_mode='active')
    sw_create_bond(sw2, lag_name, ports_sw2, lacp_mode='active')

    step("Turning on all the interfaces used in this test")
    for intf in ports_sw1:
        verify_intf_status(sw1, intf, "link_state", "up")
    for intf in ports_sw2:
        verify_intf_status(sw2, intf, "link_state", "up")

    step('Setting LAGs lacp rate as fast in switches')
    set_port_parameter(sw1, lag_name, ['other_config:lacp-time=fast'])
    set_port_parameter(sw2, lag_name, ['other_config:lacp-time=fast'])

    step('Verify state machines from interfaces on Switch 1')
    sw_wait_until_all_sm_ready([sw1], ports_sw1, sm_col_and_dist)

    step('Verify state machines from interfaces on Switch 2')
    sw_wait_until_all_sm_ready([sw2], ports_sw2, sm_col_and_dist)

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

    step("Verify interface is not part of LAG.")
    verify_intf_not_in_bond(sw1, ports_sw1[0],
                            "Expected the interfaces to be removed "
                            "from static lag")

    step("Verify that interface bond_status is empty.")
    verify_intf_bond_status_empty(sw1, ports_sw1[0], "Interface expected"
                                  " to have bond status EMPTY")

    step("Verify LAG bond status is UP")
    verify_port_bond_status(sw1, lag_name, "up", "Expected the LAG "
                            "to have bond status UP")

    step("Add interface back to LAG")
    add_intf_to_bond(sw1, lag_name, ports_sw1[0])

    step("Verify interface is part of LAG.")
    verify_intf_in_bond(sw1, ports_sw1[0], "Interfaces is not "
                        "added back to the LAG.")

    step("Verify that interface bond_status is up.")
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

    step("Remove partner interfaces 2")
    remove_intf_from_bond(sw2, lag_name, p22)

    step("Verify that interface bond_status is blocked.")
    verify_intf_bond_status(sw1, p12, "blocked", "Expected "
                            "interfaces to have bond status BLOCKED")

    step("Verify LAG bond status is UP")
    verify_port_bond_status(sw1, lag_name, "up", "Expected the LAG "
                            "to have bond status UP")

    step("Add partner interface back to LAG")
    add_intf_to_bond(sw2, lag_name, p22)

    '''
        Falbback functionality is still not merge in rel/dill, so we need to
        disable cases 6 and 7.
    '''
    ###########################################################################
    # 6. When lacp_status = defaulted and fallback = false.
    ###########################################################################

    # step('Disabling Fallback on both switches')
    # set_port_parameter(sw1, lag_name, disable_fallback)
    # set_port_parameter(sw2, lag_name, disable_fallback)

    # step('Shutting down LAG1 on sw2')
    # set_port_parameter(sw2, lag_name, disable_lag)
    # step('Verify that all sw1 SMs are in "Defaulted and Expired"')
    # sw_wait_until_all_sm_ready([sw1], ports_sw1, active_no_fallback)

    # step("Verify LAG bond status is BLOCKED")
    # verify_port_bond_status(sw1, lag_name, "blocked", "Expected the LAG "
                            # "to have bond status BLOCKED")

    # step('Turning on LAG1 on sw2')
    # set_port_parameter(sw2, lag_name, active_lag)

    # step('Verify state machines from interfaces on Switch 1')
    # sw_wait_until_all_sm_ready([sw1], ports_sw1, sm_col_and_dist)

    # step('Verify state machines from interfaces on Switch 2')
    # sw_wait_until_all_sm_ready([sw2], ports_sw2, sm_col_and_dist)

    ###########################################################################
    # 7. When lacp_status = defaulted and fallback = true.
    ###########################################################################

    # step('Enabling Fallback on both switches')
    # set_port_parameter(sw1, lag_name, enable_fallback)
    # set_port_parameter(sw2, lag_name, enable_fallback)

    # step('Shutting down LAG1 on sw2')
    # set_port_parameter(sw2, lag_name, disable_lag)
    # step('Verify that all sw1 SMs are in "Defaulted and Expired"')
    # sw_wait_until_all_sm_ready([sw1], ports_sw1, active_fallback)

    # step("Verify LAG bond status is UP")
    # verify_port_bond_status(sw1, lag_name, "up", "Expected the LAG "
                            # "to have bond status UP")

    # step('Turning on LAG1 on sw2')
    # set_port_parameter(sw2, lag_name, active_lag)

    # step('Disabling Fallback on both switches')
    # set_port_parameter(sw1, lag_name, disable_fallback)
    # set_port_parameter(sw2, lag_name, disable_fallback)

    # step('Verify state machines from interfaces on Switch 1')
    # sw_wait_until_all_sm_ready([sw1], ports_sw1, sm_col_and_dist)

    # step('Verify state machines from interfaces on Switch 2')
    # sw_wait_until_all_sm_ready([sw2], ports_sw2, sm_col_and_dist)

    ###########################################################################
    # 8. When LAG has no member interfaces.
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
