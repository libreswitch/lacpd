# Copyright (C) 2016 Hewlett-Packard Development Company, L.P.
# All Rights Reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License"); you may
#    not use this file except in compliance with the License. You may obtain
#    a copy of the License at
#
#         http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
#    License for the specific language governing permissions and limitations
#    under the License.

"""System Priority Test Suite.

Name:        test_lacpd_ct_system_priority.py

Objective:   Verify test cases for system priority functionality

Topology:    3 switch (DUT running OpenSwitch)
"""

from pytest import mark, fixture
from lib_test import (
    print_header,
    set_port_parameter,
    sw_clear_user_config,
    sw_create_bond,
    sw_set_intf_pm_info,
    sw_delete_lag,
    sw_set_intf_user_config,
    sw_set_system_lacp_config,
    sw_wait_until_all_sm_ready
)


TOPOLOGY = """
#
# +-----+    +-----+
# |     <---->     |
# |     |    | sw2 |
# |     <---->     |
# |     |    +-----+
# | sw1 |
# |     |    +-----+
# |     <---->     |
# |     |    | sw3 |
# |     <---->     |
# +-----+    +-----+

# Nodes
[type=openswitch name="Switch 1"] sw1
[type=openswitch name="Switch 2"] sw2
[type=openswitch name="Switch 3"] sw3

# Links
sw1:1 -- sw2:1
sw1:2 -- sw2:2
sw1:3 -- sw3:1
sw1:4 -- sw3:2
"""


sw1_intf_start = 1
sw1_intf_end = 4

sw2_intf_start = 1
sw2_intf_end = 2

sw3_intf_start = 1
sw3_intf_end = 2



sw1_port_labels = ['1', '2', '3', '4']
sw2_port_labels = ['1', '2']
sw3_port_labels = ['1', '2']

sw1_intfs = []
sw2_intfs = []
sw3_intfs = []

test_lag_id = 'lag1'

sm_col_and_dist = '"Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,Dist:1,Def:0,Exp:0"'
sm_out_sync = '"Activ:1,TmOut:1,Aggr:1,Sync:0,Col:0,Dist:0,Def:0,Exp:0"'
sm_in_sync = '"Activ:1,TmOut:1,Aggr:1,Sync:1,Col:0,Dist:0,Def:0,Exp:0"'

###############################################################################
#
#                       ACTOR STATE STATE MACHINES VARIABLES
#
###############################################################################
# Everything is working and 'Collecting and Distributing'
active_ready = '"Activ:1,TmOut:\d,Aggr:1,Sync:1,Col:1,Dist:1,Def:0,Exp:0"'
# Interfaces configured in different lag
active_different_lag_intf = \
    '"Activ:1,TmOut:\d,Aggr:1,Sync:\d,Col:0,Dist:0,Def:0,Exp:0"'

def lacpd_switch_pre_setup(sw, start, end):
    for intf in range(start, end):
        sw_set_intf_pm_info(sw, intf, ('connector="SFP_RJ45"',
                                       'connector_status=supported',
                                       'max_speed="1000"',
                                       'supported_speeds="1000"'))


@fixture(scope="module")
def main_setup(request, topology):
    """Test Suite Setup."""
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    sw3 = topology.get('sw3')

    assert sw1 is not None
    assert sw2 is not None
    assert sw3 is not None

    for port in sw1_port_labels:
        sw1_intfs.append(sw1.ports[port])

    for port in sw2_port_labels:
        sw2_intfs.append(sw2.ports[port])

    for port in sw3_port_labels:
        sw3_intfs.append(sw3.ports[port])


@fixture()
def setup(request, topology):
    """Test Case Setup."""
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    sw3 = topology.get('sw3')

    assert sw1 is not None
    assert sw2 is not None
    assert sw3 is not None

    mac_addr_sw1 = sw1.libs.vtysh.show_interface(1)['mac_address']
    mac_addr_sw2 = sw2.libs.vtysh.show_interface(1)['mac_address']
    mac_addr_sw3 = sw3.libs.vtysh.show_interface(1)['mac_address']
    assert (
        mac_addr_sw1 != mac_addr_sw2 and
        mac_addr_sw1 != mac_addr_sw3 and
        mac_addr_sw2 != mac_addr_sw3,
        'Mac address of interfaces in sw1 is equal to mac address of ' +
        'interfaces in sw2. This is a test framework problem. Dynamic ' +
        'LAGs cannot work properly under this condition. Refer to Taiga ' +
        'issue #1251.')

    def cleanup():
        print('Clear the user_config of all the Interfaces.\n'
              'Reset the pm_info to default values.')

        for intf in range(sw1_intf_start, sw1_intf_end):
            sw_clear_user_config(sw1, intf)

        for intf in range(sw2_intf_start, sw2_intf_end):
            sw_clear_user_config(sw2, intf)

        for intf in range(sw3_intf_start, sw3_intf_end):
            sw_clear_user_config(sw3, intf)

    request.addfinalizer(cleanup)


@mark.gate
@mark.skipif(True, reason="Skipping due to instability")
def test_lacpd_lag_dynamic_system_priority(topology, step, main_setup, setup):
    """Dynamic System Priority Test Case.

    If two switches (sw2 and sw3) are connected to a switch (sw1) using the
    same dynamic LAG, the resolution of the LAG should be in favor of the
    switch with the higher system priority
    """
    print_header('Dynamic LAG system priority test')

    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    sw3 = topology.get('sw3')

    assert sw1 is not None
    assert sw2 is not None
    assert sw3 is not None

    # Enable all the interfaces under test.
    step('Enabling interfaces on switch 1')
    for intf in sw1_intfs:
        sw_set_intf_user_config(sw1, intf, ['admin=up'])

    for intf in sw2_intfs:
        sw_set_intf_user_config(sw2, intf, ['admin=up'])

    for intf in sw3_intfs:
        sw_set_intf_user_config(sw3, intf, ['admin=up'])

    ###########################################################################
    #
    #                           Switch 3 has more priority
    #
    ###########################################################################
    step('Setting system priorities in switches')
    sw_set_system_lacp_config(sw1, ['lacp-system-priority=1'])
    sw_set_system_lacp_config(sw2, ['lacp-system-priority=100'])
    sw_set_system_lacp_config(sw3, ['lacp-system-priority=50'])

    step('Creating active LAGs in switches')
    sw_create_bond(sw1, test_lag_id, sw1_intfs, lacp_mode='active')
    sw_create_bond(sw2, test_lag_id, sw2_intfs, lacp_mode='active')
    sw_create_bond(sw3, test_lag_id, sw3_intfs, lacp_mode='active')

    step('Setting LAGs lacp rate as fast in switches')
    set_port_parameter(sw1, test_lag_id, ['other_config:lacp-time=fast'])
    set_port_parameter(sw2, test_lag_id, ['other_config:lacp-time=fast'])
    set_port_parameter(sw3, test_lag_id, ['other_config:lacp-time=fast'])

    step('Verify state machines in all switches')

    step('Verify state machines from interfaces on Switch 1')
    sw_wait_until_all_sm_ready([sw1], sw1_intfs[0:2], sm_out_sync)
    sw_wait_until_all_sm_ready([sw1], sw1_intfs[2:4], sm_col_and_dist)

    step('Verify state machines from interfaces on Switch 2')
    sw_wait_until_all_sm_ready([sw2], sw2_intfs, sm_in_sync)

    step('Verify state machines from interfaces on Switch 3')
    sw_wait_until_all_sm_ready([sw3], sw3_intfs, sm_col_and_dist)

    ###########################################################################
    #
    #                           Switch 2 has more priority
    #
    ###########################################################################
    step('Setting system priorities in switches')
    sw_set_system_lacp_config(sw2, ['lacp-system-priority=50'])
    sw_set_system_lacp_config(sw3, ['lacp-system-priority=100'])

    step('Verify state machines in all switches')

    step('Verify state machines from interfaces on Switch 1')
    sw_wait_until_all_sm_ready([sw1], sw1_intfs[0:2], sm_col_and_dist)
    sw_wait_until_all_sm_ready([sw1], sw1_intfs[2:4], sm_out_sync)

    step('Verify state machines from interfaces on Switch 2')
    sw_wait_until_all_sm_ready([sw2], sw2_intfs, sm_col_and_dist)

    step('Verify state machines from interfaces on Switch 3')
    sw_wait_until_all_sm_ready([sw3], sw3_intfs, sm_in_sync)

    ###########################################################################
    #
    #                                   Cleanup
    #
    ###########################################################################
    sw_delete_lag(sw1, test_lag_id)
    sw_delete_lag(sw2, test_lag_id)
    sw_delete_lag(sw3, test_lag_id)
