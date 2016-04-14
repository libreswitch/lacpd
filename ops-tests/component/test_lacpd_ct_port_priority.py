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
# Name:        test_lacpd_ct_port_priority.py
#
# Objective:   Verify test cases for port priority functionality
#
# Topology:    2 switch (DUT running OpenSwitch)
#
##########################################################################

from time import sleep
import pytest
from lib_test import sw_set_intf_user_config
from lib_test import sw_clear_user_config
from lib_test import sw_set_intf_pm_info
from lib_test import set_port_parameter
from lib_test import sw_get_intf_state
from lib_test import sw_create_bond
from lib_test import timed_compare


TOPOLOGY = """
#
# +-------+     +-------+
# |       <----->       |
# |       |     |       |
# |       <----->       |
# |       |     |       |
# | sw1   <----->  sw2  |
# |       |     |       |
# |       <----->       |
# |       |     |       |
# |       <----->       |
# +-------+     +-------+

# Nodes
[type=openswitch name="Switch 1"] sw1
[type=openswitch name="Switch 2"] sw2

# Links
# 1 Gig ports
sw1:if01 -- sw2:if01
sw1:if02 -- sw2:if02
sw1:if03 -- sw2:if03
sw1:if04 -- sw2:if04
sw1:if05 -- sw2:if05
"""


ovs_vsctl = "/usr/bin/ovs-vsctl "

sw_1g_intf_start = 1
sw_1g_intf_end = 5
port_labels_1G = ['if01', 'if02', 'if03', 'if04', 'if05']
sw_1g_intf = []

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


def verify_compare_complex(actual, expected, unused):
    attrs = []
    for attr in expected:
        attrs.append(attr)
    if len(actual) != len(expected):
        return False
    for i in range(0, len(attrs)):
        if actual[i] != expected[attrs[i]]:
            return False
    return True


def verify_intf_lacp_status(sw, intf, verify_values, context=''):
    request = []
    attrs = []
    for attr in verify_values:
        request.append('lacp_status:' + attr)
        attrs.append(attr)
    result = timed_compare(sw_get_intf_state,
                           (sw, intf, request),
                           verify_compare_complex, verify_values)
    field_vals = result[1]
    for i in range(0, len(attrs)):
        verify_values[attrs[i]].replace('"', '')
        assert field_vals[i] == verify_values[attrs[i]], context +\
            ": invalid value for " + attrs[i] + ", expected " +\
            verify_values[attrs[i]] + ", got " + field_vals[i]


def lacpd_switch_pre_setup(sw):
    for intf in range(sw_1g_intf_start, sw_1g_intf_end):
        if intf > 9:
            port = "if"
        else:
            port = "if0"
        # port = intf > 9 ? "if" : "if0"
        port = "{}{}".format(port, intf)
        sw_set_intf_pm_info(sw, sw.ports[port], ('connector="SFP_RJ45"',
                                                 'connector_status=supported',
                                                 'max_speed="1000"',
                                                 'supported_speeds="1000"'))


@pytest.fixture(scope="module")
def main_setup(request, topology):
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')

    assert sw1 is not None
    assert sw2 is not None

    global sw_1g_intf, port_labels_1G
    for lbl in port_labels_1G:
        sw_1g_intf.append(sw1.ports[lbl])


# Simulate valid pluggable modules in all the modules.
@pytest.fixture()
def setup(request, topology):
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')

    assert sw1 is not None
    assert sw2 is not None

    print('Simulate valid pluggable modules in all the modules.')
    lacpd_switch_pre_setup(sw1)
    lacpd_switch_pre_setup(sw2)

    def cleanup():
        print('Clear the user_config of all the Interfaces.\n'
              'Reset the pm_info to default values.')
        for intf in range(1, 54):
            sw_clear_user_config(sw1, intf)
            sw_clear_user_config(sw2, intf)
            sw_set_intf_pm_info(sw1, intf, ('connector=absent',
                                'connector_status=unsupported'))
            sw_set_intf_pm_info(sw2, intf, ('connector=absent',
                                'connector_status=unsupported'))

    request.addfinalizer(cleanup)


def test_lacpd_lag_dynamic_port_priority(topology, step, main_setup, setup):
    """
    Case 1:
        Test the port priority functionality of LACP by setting a lower
        priority to an interface and allowing other interfaces with higher
        priority to join the corresponding LAGs.
        Two switches are connected with five interfaces using 2 LAGs in active
        mode.
        In switch 1, LAG 1 is formed with interfaces 1 and 2, LAG 2 is
        formed with interfaces 3, 4 and 5
        In switch 2, LAG 1 is formed with interfaces 1, 2 and 3, LAG 2 is
        formed with interfaces 4 and 5
    """
    step("\n============= lacpd dynamic LAG port priority test =============\n")
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')

    assert sw1 is not None
    assert sw2 is not None

    # Enable all the interfaces under test.
    step("Enabling interfaces [1, 2, 3, 4, 5] in all switches\n")
    for intf in sw_1g_intf[0:5]:
        sw_set_intf_user_config(sw1, intf, ['admin=up'])
        sw_set_intf_user_config(sw2, intf, ['admin=up'])

    step("Creating active LAGs in switches\n")
    sw_create_bond(sw1, "lag1", sw_1g_intf[0:2], lacp_mode="active")
    sw_create_bond(sw1, "lag2", sw_1g_intf[2:5], lacp_mode="active")
    sw_create_bond(sw2, "lag1", sw_1g_intf[0:3], lacp_mode="active")
    sw_create_bond(sw2, "lag2", sw_1g_intf[3:5], lacp_mode="active")

    step("Setting LAGs lacp rate as fast in switches\n")
    set_port_parameter(sw1, "lag1", ['other_config:lacp-time=fast'])
    set_port_parameter(sw1, "lag2", ['other_config:lacp-time=fast'])
    set_port_parameter(sw2, "lag1", ['other_config:lacp-time=fast'])
    set_port_parameter(sw2, "lag2", ['other_config:lacp-time=fast'])

    step("Waiting for LACP to complete negotiation\n")
    sleep(50)

    """
    Every interface have the default port priority
    In switch 1:
        Interface 1 must be collecting and distributing
        Interface 2 must be collecting and distributing
        Interface 3 must not be collecting and distributing
        Interface 4 must not be collecting and distributing
        Interface 5 must not be collecting and distributing
    In switch 2:
        Interface 1 must be collecting and distributing
        Interface 2 must be collecting and distributing
        Interface 3 must not be collecting and distributing
        Interface 4 must not be collecting and distributing
        Interface 5 must not be collecting and distributing
    """
    step("Verify state machines in all switches\n")
    for intf in sw_1g_intf[0:2]:
        verify_intf_lacp_status(sw1,
                                intf,
                                {"partner_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0",
                                 "actor_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0"},
                                "s1:" + intf)

        verify_intf_lacp_status(sw2,
                                intf,
                                {"partner_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0",
                                 "actor_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0"},
                                "s2:" + intf)

    verify_intf_lacp_status(sw1,
                            3,
                            {"partner_state":
                             "Activ:1,TmOut:1,Aggr:1,Sync:0,Col:0,"
                             "Dist:0,Def:0,Exp:0",
                             "actor_state":
                             "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:0,"
                             "Dist:0,Def:0,Exp:0"},
                            "s1:" + "3")

    verify_intf_lacp_status(sw2,
                            3,
                            {"partner_state":
                             "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:0,"
                             "Dist:0,Def:0,Exp:0",
                             "actor_state":
                             "Activ:1,TmOut:1,Aggr:1,Sync:0,Col:0,"
                             "Dist:0,Def:0,Exp:0"},
                            "s2:" + "3")

    for intf in sw_1g_intf[3:5]:
        verify_intf_lacp_status(sw1,
                                intf,
                                {"partner_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:0,"
                                 "Dist:0,Def:0,Exp:0",
                                 "actor_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:0,Col:0,"
                                 "Dist:0,Def:0,Exp:0"},
                                "s1:" + intf)

        verify_intf_lacp_status(sw2,
                                intf,
                                {"partner_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:0,Col:0,"
                                 "Dist:0,Def:0,Exp:0",
                                 "actor_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:0,"
                                 "Dist:0,Def:0,Exp:0"},
                                "s2:" + intf)

    step("Setting port priorities\n")
    set_intf_other_config(sw1, '1', ['lacp-port-priority=100'])
    set_intf_other_config(sw1, '2', ['lacp-port-priority=100'])
    set_intf_other_config(sw1, '3', ['lacp-port-priority=200'])
    set_intf_other_config(sw1, '4', ['lacp-port-priority=100'])
    set_intf_other_config(sw1, '5', ['lacp-port-priority=100'])

    set_intf_other_config(sw2, '1', ['lacp-port-priority=100'])
    set_intf_other_config(sw2, '2', ['lacp-port-priority=100'])
    set_intf_other_config(sw2, '3', ['lacp-port-priority=200'])
    set_intf_other_config(sw2, '4', ['lacp-port-priority=100'])
    set_intf_other_config(sw2, '5', ['lacp-port-priority=100'])

    step("Waiting for LACP to complete negotiation\n")
    sleep(20)
    """
    Interface 3 has the lower priority, this allows other interfaces to
    agreggate to the LAGs
    In switch 1:
        Interface 1 must be collecting and distributing
        Interface 2 must be collecting and distributing
        Interface 3 must not be collecting and distributing
        Interface 4 must be collecting and distributing
        Interface 5 must be collecting and distributing
    In switch 2:
        Interface 1 must be collecting and distributing
        Interface 2 must be collecting and distributing
        Interface 3 must not be collecting and distributing
        Interface 4 must be collecting and distributing
        Interface 5 must be collecting and distributing
    """
    step("Verify state machines in all switches\n")
    for intf in sw_1g_intf[0:2]:
        verify_intf_lacp_status(sw1,
                                intf,
                                {"partner_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0",
                                 "actor_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0"},
                                "s1:" + intf)

        verify_intf_lacp_status(sw2,
                                intf,
                                {"partner_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0",
                                 "actor_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0"},
                                "s2:" + intf)

    for intf in sw_1g_intf[3:5]:
        verify_intf_lacp_status(sw1,
                                intf,
                                {"partner_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0",
                                 "actor_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0"},
                                "s1:" + intf)

        verify_intf_lacp_status(sw2,
                                intf,
                                {"partner_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0",
                                 "actor_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0"},
                                "s2:" + intf)

    verify_intf_lacp_status(sw1,
                            3,
                            {"partner_state":
                             "Activ:1,TmOut:1,Aggr:1,Sync:0,Col:0,"
                             "Dist:0,Def:0,Exp:0",
                             "actor_state":
                             "Activ:1,TmOut:1,Aggr:1,Sync:0,Col:0,"
                             "Dist:0,Def:0,Exp:0"},
                            "s1:" + "3")

    verify_intf_lacp_status(sw2,
                            3,
                            {"partner_state":
                             "Activ:1,TmOut:1,Aggr:1,Sync:0,Col:0,"
                             "Dist:0,Def:0,Exp:0",
                             "actor_state":
                             "Activ:1,TmOut:1,Aggr:1,Sync:0,Col:0,"
                             "Dist:0,Def:0,Exp:0"},
                            "s2:" + "3")

    # finish testing
    for intf in sw_1g_intf[0:5]:
        sw1("ovs-vsctl remove interface " + intf +
            " other_config lacp-port-priority", shell='bash')
        sw2("ovs-vsctl remove interface " + intf +
            " other_config lacp-port-priority", shell='bash')
    sw1("ovs-vsctl del-port lag1", shell='bash')
    sw1("ovs-vsctl del-port lag2", shell='bash')
    sw2("ovs-vsctl del-port lag1", shell='bash')
    sw2("ovs-vsctl del-port lag2", shell='bash')


def test_lacpd_lag_dynamic_partner_priority(topology, step, main_setup, setup):
    """
    Case 2:
        Test the port priority functionality of LACP by testing the partner
        port priority. If two interfaces have the same port priority, the
        decision is made using the partner port priority
        Two switches are connected with 4 interfaces
        In switch 1, LAG 1 is formed with interfaces 1 to 4.
        In switch 2, LAG 1 is formed with interfaces 1 and 2, LAG 2 is
        formed with interfaces 3 and 4
    """
    step("\n============= lacpd dynamic LAG partner priority test =============\n")
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')

    assert sw1 is not None
    assert sw2 is not None

    # Enable all the interfaces under test.
    step("Enabling interfaces [1, 2, 3, 4] in all switches\n")
    for intf in sw_1g_intf[0:4]:
        sw_set_intf_user_config(sw1, intf, ['admin=up'])
        sw_set_intf_user_config(sw2, intf, ['admin=up'])

    step("Creating active LAGs in switches\n")
    sw_create_bond(sw1, "lag1", sw_1g_intf[0:4], lacp_mode="active")
    sw_create_bond(sw2, "lag1", sw_1g_intf[0:2], lacp_mode="active")
    sw_create_bond(sw2, "lag2", sw_1g_intf[2:4], lacp_mode="active")

    step("Setting LAGs lacp rate as fast in switches\n")
    set_port_parameter(sw1, "lag1", ['other_config:lacp-time=fast'])
    set_port_parameter(sw2, "lag1", ['other_config:lacp-time=fast'])
    set_port_parameter(sw2, "lag2", ['other_config:lacp-time=fast'])

    step("Setting port priorities\n")
    set_intf_other_config(sw1, '1', ['lacp-port-priority=100'])
    set_intf_other_config(sw1, '2', ['lacp-port-priority=100'])
    set_intf_other_config(sw1, '3', ['lacp-port-priority=100'])
    set_intf_other_config(sw1, '4', ['lacp-port-priority=100'])

    set_intf_other_config(sw2, '1', ['lacp-port-priority=100'])
    set_intf_other_config(sw2, '2', ['lacp-port-priority=100'])
    set_intf_other_config(sw2, '3', ['lacp-port-priority=1'])
    set_intf_other_config(sw2, '4', ['lacp-port-priority=1'])

    step("Waiting for LACP to complete negotiation\n")
    sleep(30)
    """
    Interface 3 and 4 in switch 2 have the higher priority, LAG 1 in switch 1
    will connect to the LAG 2 because the partner has higher priority

    In switch 1:
        Interface 1 must not be collecting and distributing
        Interface 2 must not be collecting and distributing
        Interface 3 must be collecting and distributing
        Interface 4 must be collecting and distributing
    In switch 2:
        Interface 1 must not be collecting and distributing
        Interface 2 must not be collecting and distributing
        Interface 3 must be collecting and distributing
        Interface 4 must be collecting and distributing
    """
    step("Verify state machines in all switches\n")

    for intf in sw_1g_intf[0:2]:
        verify_intf_lacp_status(sw1,
                                intf,
                                {"partner_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:0,"
                                 "Dist:0,Def:0,Exp:0",
                                 "actor_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:0,Col:0,"
                                 "Dist:0,Def:0,Exp:0"},
                                "s1:" + intf)

        verify_intf_lacp_status(sw2,
                                intf,
                                {"partner_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:0,Col:0,"
                                 "Dist:0,Def:0,Exp:0",
                                 "actor_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:0,"
                                 "Dist:0,Def:0,Exp:0"},
                                "s2:" + intf)

    for intf in sw_1g_intf[2:4]:
        verify_intf_lacp_status(sw1,
                                intf,
                                {"partner_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0",
                                 "actor_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0"},
                                "s1:" + intf)

        verify_intf_lacp_status(sw2,
                                intf,
                                {"partner_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0",
                                 "actor_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0"},
                                "s2:" + intf)


    # finish testing
    for intf in sw_1g_intf[0:4]:
        sw1("ovs-vsctl remove interface " + intf +
            " other_config lacp-port-priority", shell='bash')
        sw2("ovs-vsctl remove interface " + intf +
            " other_config lacp-port-priority", shell='bash')
    sw1("ovs-vsctl del-port lag1", shell='bash')
    sw2("ovs-vsctl del-port lag1", shell='bash')
    sw2("ovs-vsctl del-port lag2", shell='bash')
