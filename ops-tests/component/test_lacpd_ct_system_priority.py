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

##########################################################################
# Name:        test_lacpd_ct_system_priority.py
#
# Objective:   Verify test cases for system priority functionality
#
# Topology:    3 switch (DUT running OpenSwitch)
#
##########################################################################

"""
OpenSwitch Tests for LACP System priority functionality
"""

import pytest
from lib_test import (
    set_port_parameter,
    sw_clear_user_config,
    sw_create_bond,
    sw_set_intf_pm_info,
    sw_set_intf_user_config,
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


ovs_vsctl = "/usr/bin/ovs-vsctl "

sw_1g_intf_start = 1
sw_1g_intf_end = 4
sw1_intf_labels_1G = ['1', '2', '3', '4']
sw2_intf_labels_1G = ['1', '2']
sw3_intf_labels_1G = ['1', '2']
sw_1g_intf = []
sw2_1g_intf = []
sw3_1g_intf = []

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


# Set open_vsw_lacp_config parameter(s)
def set_open_vsw_lacp_config(sw, config):
    c = ovs_vsctl + "set system ."
    for s in config:
        c += " lacp_config:" + s
    return sw(c, shell='bash')


# Set interface:other_config parameter(s)
def set_intf_other_config(sw, intf, config):
    c = "set interface " + str(intf)
    for s in config:
        c += ' other_config:%s' % s
    return sw(c, shell='vsctl')


# Delete a bond/lag/trunk from OVS-DB.
def sw_delete_bond(sw, bond_name):
    print("Deleting the bond " + bond_name + "\n")
    c = ovs_vsctl + "del-port bridge_normal " + bond_name
    return sw(c, shell='bash')


def lacpd_switch_pre_setup(sw, start, end):
    for intf in range(start, end):
        sw_set_intf_pm_info(sw, intf, ('connector="SFP_RJ45"',
                                       'connector_status=supported',
                                       'max_speed="1000"',
                                       'supported_speeds="1000"'))


@pytest.fixture(scope="module")
def main_setup(request, topology):
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    sw3 = topology.get('sw3')

    assert sw1 is not None
    assert sw2 is not None
    assert sw3 is not None

    global sw_1g_intf, sw1_intf_labels_1G, sw2_intf_labels_1G,\
        sw3_intf_labels_1G
    for lbl in sw1_intf_labels_1G:
        sw_1g_intf.append(sw1.ports[lbl])
    for lbl in sw2_intf_labels_1G:
        sw2_1g_intf.append(sw2.ports[lbl])
    for lbl in sw3_intf_labels_1G:
        sw3_1g_intf.append(sw3.ports[lbl])


# Simulate valid pluggable modules in all the modules.
@pytest.fixture()
def setup(request, topology):
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    sw3 = topology.get('sw3')

    assert sw1 is not None
    assert sw2 is not None
    assert sw3 is not None

    print('Simulate valid pluggable modules in all the modules.')
    lacpd_switch_pre_setup(sw1, sw_1g_intf_start, sw_1g_intf_end)
    lacpd_switch_pre_setup(sw2, 1, 2)
    lacpd_switch_pre_setup(sw3, 1, 2)

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
        for intf in range(1, 54):
            sw_clear_user_config(sw1, intf)
            sw_clear_user_config(sw2, intf)
            sw_clear_user_config(sw3, intf)
            sw_set_intf_pm_info(sw1, intf, ('connector=absent',
                                'connector_status=unsupported'))
            sw_set_intf_pm_info(sw2, intf, ('connector=absent',
                                'connector_status=unsupported'))
            sw_set_intf_pm_info(sw3, intf, ('connector=absent',
                                'connector_status=unsupported'))

    request.addfinalizer(cleanup)


@pytest.mark.skipif(True, reason="Skipping due to instability")
def test_lacpd_lag_dynamic_system_priority(topology, step, main_setup, setup):
    """
    Case 1:
        If two switches (sw2 and sw3) are connected to a switch (sw1) using
        the same dynamic LAG, the resolution of the LAG should be in favor of
        the switch with the higher system priority
    """
    step("\n============= Dynamic LAG system priority test==========\n")
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    sw3 = topology.get('sw3')

    assert sw1 is not None
    assert sw2 is not None
    assert sw3 is not None

    # Enable all the interfaces under test.
    step("Enabling interfaces [1, 2, 3, 4] in all switches\n")
    for intf in sw_1g_intf[0:4]:
        sw_set_intf_user_config(sw1, intf, ['admin=up'])
        sw_set_intf_user_config(sw2, intf, ['admin=up'])
        sw_set_intf_user_config(sw3, intf, ['admin=up'])

    step("Setting system priorities in switches\n")
    set_open_vsw_lacp_config(sw1, ['lacp-system-priority=1'])
    set_open_vsw_lacp_config(sw2, ['lacp-system-priority=100'])
    set_open_vsw_lacp_config(sw3, ['lacp-system-priority=50'])

    step("Creating active LAGs in switches\n")
    sw_create_bond(sw1, "lag1", sw_1g_intf[0:4], lacp_mode="active")
    sw_create_bond(sw2, "lag1", sw2_1g_intf[0:2], lacp_mode="active")
    sw_create_bond(sw3, "lag1", sw3_1g_intf[0:2], lacp_mode="active")

    step("Setting LAGs lacp rate as fast in switches\n")
    set_port_parameter(sw1, "lag1", ['other_config:lacp-time=fast'])
    set_port_parameter(sw2, "lag1", ['other_config:lacp-time=fast'])
    set_port_parameter(sw3, "lag1", ['other_config:lacp-time=fast'])

    step("Waiting for LACP to complete negotiation\n")

    step("Verify state machines in all switches\n")
    """
    The interface 3 and 4 of sw1 must be collecting and distributing
    """
    sw_wait_until_all_sm_ready(
        [sw1], sw1_intf_labels_1G[2:4], active_ready)
    """
    The interface 1 and 2 of sw3 must be collecting and distributing since
    this is the switch with higher system priority between sw2 and sw3
    """
    sw_wait_until_all_sm_ready(
        [sw3], sw3_intf_labels_1G[0:2], active_ready)
    """
    The interface 1 and 2 of sw1 must not be collecting and distributing
    The interface 1 and 2 of sw2 must not be collecting and distributing since
    this is the switch with lower system priority between sw2 and sw3
    """
    sw_wait_until_all_sm_ready(
        [sw1, sw2], sw1_intf_labels_1G[0:2], active_different_lag_intf)

    # finish testing
    sw1("ovs-vsctl del-port lag1", shell='bash')
    sw2("ovs-vsctl del-port lag1", shell='bash')
    sw3("ovs-vsctl del-port lag1", shell='bash')
