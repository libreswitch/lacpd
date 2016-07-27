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
# Name:        test_lacpd_ct_defaulted_port_priority.py
#
# Objective:   Verify test cases for port priority functionality when the
#              interface is in defaulted state
#
# Topology:    2 switch (DUT running OpenSwitch)
#
##########################################################################
import pytest
from lib_test import (
    disable_intf_list,
    enable_intf_list,
    set_port_parameter,
    sw_create_bond,
    sw_set_intf_user_config,
    sw_wait_until_all_sm_ready
)


TOPOLOGY = """
#
# +-------+     +-------+
# |       <----->       |
# |       |     |       |
# |       <----->       |
# |  sw1  |     |  sw2  |
# |       <----->       |
# |       |     |       |
# |       <----->       |
# +-------+     +-------+

# Nodes
[type=openswitch name="Switch 1"] sw1
[type=openswitch name="Switch 2"] sw2

# Links
# 1 Gig ports
sw1:1 -- sw2:1
sw1:2 -- sw2:2
sw1:3 -- sw2:3
sw1:4 -- sw2:4
"""

sw_1g_intf_start = 1
sw_1g_intf_end = 4
sw1_intf_labels_1G = ['1', '2', '3', '4']
sw_1g_intf = []

###############################################################################
#
#                       ACTOR STATE STATE MACHINES VARIABLES
#
###############################################################################
# Everything is working and 'Collecting and Distributing'
active_ready = '"Activ:1,TmOut:\d,Aggr:1,Sync:1,Col:1,Dist:1,Def:0,Exp:0"'
# Interfaces in defaultes state
active_defaulted = \
    '"Activ:1,TmOut:\d,Aggr:1,Sync:0,Col:0,Dist:0,Def:1,Exp:1"'


# Set interface:other_config parameter(s)
def set_intf_other_config(sw, intf, config):
    c = "set interface " + str(intf)
    for s in config:
        c += ' other_config:%s' % s
    return sw(c, shell='vsctl')


# Delete a bond/lag/trunk from OVS-DB.
def sw_delete_bond(sw, bond_name):
    print("Deleting the bond " + bond_name + "\n")
    c = "del-port bridge_normal " + bond_name
    return sw(c, shell='vsctl')


@pytest.fixture(scope="module")
def main_setup(request, topology):
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')

    assert sw1 is not None
    assert sw2 is not None

    global sw_1g_intf, sw1_intf_labels_1G
    for lbl in sw1_intf_labels_1G:
        sw_1g_intf.append(sw1.ports[lbl])


# Simulate valid pluggable modules in all the modules.
@pytest.fixture()
def setup(request, topology):
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')

    assert sw1 is not None
    assert sw2 is not None

    mac_addr_sw1 = sw1.libs.vtysh.show_interface(1)['mac_address']
    mac_addr_sw2 = sw2.libs.vtysh.show_interface(1)['mac_address']
    assert mac_addr_sw1 != mac_addr_sw2, \
        'Mac address of interfaces in sw1 is equal to mac address of ' + \
        'interfaces in sw2. This is a test framework problem. Dynamic ' + \
        'LAGs cannot work properly under this condition. Refer to Taiga ' + \
        'issue #1251.'


@pytest.mark.skipif(True, reason="Skipping due to constant failures")
def test_lacpd_lag_defaulted_port_priority(topology, step, main_setup, setup):
    """
    Case 1:
        Test the port priority functionality of LACP when an interface is
        sent to defaulted state by disabling the partner interface connected
        to the interface with higher port priortiy. Also test when a function
        is sent to defaulted state and all interfaces have the same priority.
        Two switches are connected with 4 interfaces using 2 LAGs in active
        mode.
        In switch 1, LAG 1 is formed with interfaces 1, 2, 3 and 4
        In switch 2, LAG 1 is formed with interfaces 1, 2, 4 and 4
    """
    step("\n============ lacpd dynamic LAG port priority test ============")
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')

    assert sw1 is not None
    assert sw2 is not None

    # Enable all the interfaces under test.
    step("Enabling interfaces [1, 2, 3, 4] in all switches")
    for intf in sw_1g_intf[0:4]:
        sw_set_intf_user_config(sw1, intf, ['admin=up'])
        sw_set_intf_user_config(sw2, intf, ['admin=up'])

    step("Creating active LAGs in switches")
    sw_create_bond(sw1, "lag1", sw_1g_intf[0:4], lacp_mode="active")
    sw_create_bond(sw2, "lag1", sw_1g_intf[0:4], lacp_mode="active")

    step("Setting LAGs lacp rate as fast in switches")
    set_port_parameter(sw1, "lag1", ['other_config:lacp-time=fast'])
    set_port_parameter(sw2, "lag1", ['other_config:lacp-time=fast'])

    step("Setting port priorities")
    set_intf_other_config(sw1, '1', ['lacp-port-priority=200'])
    set_intf_other_config(sw1, '2', ['lacp-port-priority=100'])
    set_intf_other_config(sw1, '3', ['lacp-port-priority=200'])
    set_intf_other_config(sw1, '4', ['lacp-port-priority=200'])

    set_intf_other_config(sw2, '1', ['lacp-port-priority=200'])
    set_intf_other_config(sw2, '2', ['lacp-port-priority=100'])
    set_intf_other_config(sw2, '3', ['lacp-port-priority=200'])
    set_intf_other_config(sw2, '4', ['lacp-port-priority=200'])

    step("Verify state machines in all switches")
    sw_wait_until_all_sm_ready(
        [sw1, sw2], sw1_intf_labels_1G[0:4], active_ready)

    step("Shutdown interface with higher port priority [2] in switch 2")
    disable_intf_list(sw2, sw1_intf_labels_1G[1])

    step("Verify interfaces 1, 3 and 4 are still working in both switches")
    sw_wait_until_all_sm_ready(
        [sw1, sw2], sw1_intf_labels_1G[0], active_ready)
    sw_wait_until_all_sm_ready(
        [sw1, sw2], sw1_intf_labels_1G[2:4], active_ready)

    step("Verify interface 2 is in defaulted state in switch 1")
    sw_wait_until_all_sm_ready(
        [sw1], sw1_intf_labels_1G[1], active_defaulted)

    step("No shutdown interface with higher port priority [2] in switch 2")
    enable_intf_list(sw2, sw1_intf_labels_1G[1])

    step("Verify state machines in all switches")
    sw_wait_until_all_sm_ready(
        [sw1, sw2], sw1_intf_labels_1G[0:4], active_ready)

    step("Set same port priority to all interfaces")
    set_intf_other_config(sw1, '2', ['lacp-port-priority=200'])
    set_intf_other_config(sw2, '2', ['lacp-port-priority=200'])

    step("Verify state machines in all switches")
    sw_wait_until_all_sm_ready(
        [sw1, sw2], sw1_intf_labels_1G[0:4], active_ready)

    step("Shutdown interface 2 in switch 2")
    disable_intf_list(sw2, sw1_intf_labels_1G[1])

    step("Verify interfaces 1, 3 and 4 are still working in both switches")
    sw_wait_until_all_sm_ready(
        [sw1, sw2], sw1_intf_labels_1G[0], active_ready)
    sw_wait_until_all_sm_ready(
        [sw1, sw2], sw1_intf_labels_1G[2:4], active_ready)

    step("Verify interface 2 is in defaulted state in switch 1")
    sw_wait_until_all_sm_ready(
        [sw1], sw1_intf_labels_1G[1], active_defaulted)

    step("No shutdown interface 2 in switch 2")
    enable_intf_list(sw2, sw1_intf_labels_1G[1])

    step("Verify state machines in all switches")
    sw_wait_until_all_sm_ready(
        [sw1, sw2], sw1_intf_labels_1G[0:4], active_ready)

    step("Set same port priority to all interfaces")
    # finish testing
    for intf in sw_1g_intf[0:4]:
        sw1("ovs-vsctl remove interface " + intf +
            " other_config lacp-port-priority", shell='bash')
        sw2("ovs-vsctl remove interface " + intf +
            " other_config lacp-port-priority", shell='bash')
    sw1("ovs-vsctl del-port lag1", shell='bash')
    sw2("ovs-vsctl del-port lag1", shell='bash')
