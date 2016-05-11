#!/usr/bin/python
#
# (c) Copyright 2015-2016 Hewlett Packard Enterprise Development LP
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
#

import os
import sys
import time
import subprocess
import pytest

from opsvsi.docker import *
from opsvsi.opsvsitest import *

from lib_test import sw_set_intf_user_config
from lib_test import sw_clear_user_config
from lib_test import sw_set_intf_pm_info
from lib_test import set_port_parameter
from lib_test import sw_get_intf_state
from lib_test import sw_get_port_state
from lib_test import sw_create_bond
from lib_test import verify_intf_in_bond
from lib_test import verify_intf_not_in_bond
from lib_test import verify_intf_status
from lib_test import timed_compare
from lib_test import remove_intf_from_bond
from lib_test import remove_intf_list_from_bond

OVS_VSCTL = "/usr/bin/ovs-vsctl "

# Test case configuration.
DFLT_BRIDGE = "bridge_normal"

# Interfaces from 1-10 are 1G ports.
# Interfaces from 11-20 are 10G ports.
# Interfaces from 49-54 are 40G ports.
sw_to_host1 = 21
sw_to_host2 = 22

sw_1G_intf_start = 1
sw_1G_intf_end = 10
n_1G_links = 10
sw_1G_intf = [str(i) for i in irange(sw_1G_intf_start, sw_1G_intf_end)]

sw_10G_intf_start = 11
sw_10G_intf_end = 20
n_10G_link2 = 10
sw_10G_intf = [str(i) for i in irange(sw_10G_intf_start, sw_10G_intf_end)]

sw_40G_intf_start = 49
sw_40G_intf_end = 54
n_40G_link2 = 6
sw_40G_intf = [str(i) for i in irange(sw_40G_intf_start, sw_40G_intf_end)]


# Parse the lacp_status:*_state string
def parse_lacp_state(state):
    return dict(map(lambda l: map(lambda j: j.strip(), l),
                map(lambda i: i.split(':'), state.split(','))))


# Set open_vsw_lacp_config parameter(s)
def set_open_vsw_lacp_config(sw, config):
    c = OVS_VSCTL + "set system ."
    for s in config:
        c += " lacp_config:" + s
    debug(c)
    return sw.ovscmd(c)


def sys_open_vsw_lacp_config_clear(sw):
    c = OVS_VSCTL + "remove system . lacp_config lacp-system-id " + \
        "lacp_config lacp-system-priority"
    debug(c)
    sw.ovscmd(c)


# Set interface:other_config parameter(s)
def set_intf_other_config(sw, intf, config):
    c = OVS_VSCTL + "set interface " + str(intf)
    for s in config:
        c += ' other_config:%s' % s
    debug(c)
    return sw.ovscmd(c)


# Get interface:other_config parameter(s)
def get_intf_other_config(sw, intf, params):
    c = OVS_VSCTL + "get interface " + str(intf)
    for f in params:
        c += ' other_config:%s' % f
    out = sw.ovscmd(c).splitlines()
    if len(out) == 1:
        out = out[0]
    debug(out)
    return out


# Delete interface other config parameter(s)
def del_intf_other_config(sw, intf, params):
    c = OVS_VSCTL + "remove interface " + str(intf)
    for f in params:
        c += ' other_config %s' % f
    debug(c)
    return sw.ovscmd(c)


# Simulate the link state on an Interface
def simulate_link_state(sw, interface, link_state="up"):
    info("Setting the link state of interface " + interface +
         " to " + link_state + "\n")
    c = OVS_VSCTL + "set interface " + str(interface) +\
        " link_state=" + link_state
    debug(c)
    return sw.ovscmd(c)


# Delete a bond/lag/trunk from OVS-DB.
def sw_delete_bond(sw, bond_name):
    info("Deleting the bond " + bond_name + "\n")
    c = OVS_VSCTL + "del-port bridge_normal " + bond_name
    debug(c)
    return sw.ovscmd(c)


# Add a new Interface to the existing bond.
def add_intf_to_bond(sw, bond_name, intf_name):

    info("Adding interface " + intf_name + " to LAG " + bond_name + "\n")

    # Get the UUID of the interface that has to be added.
    c = OVS_VSCTL + "get interface " + str(intf_name) + " _uuid"
    debug(c)
    intf_uuid = sw.ovscmd(c).rstrip('\r\n')

    # Get the current list of Interfaces in the bond.
    c = OVS_VSCTL + "get port " + bond_name + " interfaces"
    debug(c)
    out = sw.ovscmd(c)
    intf_list = out.rstrip('\r\n').strip("[]").replace(" ", "").split(',')

    if intf_uuid in intf_list:
        info("Interface " + intf_name + " is already part of " +
             bond_name + "\n")
        return

    # Add the given intf_name's UUID to existing Interfaces.
    intf_list.append(intf_uuid)

    # Set the new Interface list in the bond.
    new_intf_str = '[' + ",".join(intf_list) + ']'

    c = OVS_VSCTL + "set port " + bond_name + " interfaces=" + new_intf_str
    debug(c)
    return sw.ovscmd(c)


# Add a list of Interfaces to the bond.
def add_intf_list_from_bond(sw, bond_name, intf_list):
    for intf in intf_list:
        add_intf_to_bond(sw, bond_name, intf)


def verify_compare_value(actual, expected, final):
    if actual != expected:
        return False
    return True


def verify_compare_tuple(actual, expected, final):
    if len(actual) != len(expected):
        return False
    if actual != expected:
        return False
    return True


def verify_compare_tuple_negate(actual, expected, final):
    if len(actual) != len(expected):
        return False
    for i in range(0, len(expected)):
        if actual[i] == expected[i]:
            return False
    return True


def verify_compare_complex(actual, expected, final):
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
        assert field_vals[i] == verify_values[attrs[i]], context +\
            ": invalid value for " + attrs[i] + ", expected " +\
            verify_values[attrs[i]] + ", got " + field_vals[i]


def verify_port_lacp_status(sw, lag, value, msg=''):
    result = timed_compare(sw_get_port_state,
                           (sw, lag, ["lacp_status"]),
                           verify_compare_value, value)
    assert result == (True, value), msg


def lacpd_switch_pre_setup(sw):

    for intf in irange(sw_1G_intf_start, sw_1G_intf_end):
        sw_set_intf_pm_info(sw, intf, ('connector="SFP_RJ45"',
                                       'connector_status=supported',
                                       'max_speed="1000"',
                                       'supported_speeds="1000"'))

    for intf in irange(sw_10G_intf_start, sw_10G_intf_end):
        sw_set_intf_pm_info(sw, intf, ('connector=SFP_SR',
                                       'connector_status=supported',
                                       'max_speed="10000"',
                                       'supported_speeds="10000"'))

    for intf in irange(sw_40G_intf_start, sw_40G_intf_end):
        sw_set_intf_pm_info(sw, intf, ('connector=QSFP_SR4',
                                       'connector_status=supported',
                                       'max_speed="40000"',
                                       'supported_speeds="40000,10000"'))


# Create a topology with two switches, and 10 ports connected
# to each other.
class myDualSwitchTopo(Topo):
    """Dual switch topology with ten ports connected to them
       H1[h1-eth0]<--->[1]S1[2-10]<--->[2-10]S2[1]<--->[h2-eth0]H2
    """

    def build(self, hsts=2, sws=2, n_links=10, **_opts):
        self.hsts = hsts
        self.sws = sws

        "Add the hosts to the topology."
        for h in irange(1, hsts):
            host = self.addHost('h%s' % h)

        "Add the switches to the topology."
        for s in irange(1, sws):
            switch = self.addSwitch('s%s' % s)

        "Add the links between the hosts and switches."
        self.addLink('h1', 's1', port1=1, port2=sw_to_host1)
        self.addLink('h2', 's2', port1=1, port2=sw_to_host2)

        "Add the links between the switches."
        for one_g_intf in irange(sw_1G_intf_start, sw_1G_intf_end):
            self.addLink('s1', 's2', port1=one_g_intf, port2=one_g_intf)

        for ten_g_intf in irange(sw_10G_intf_start, sw_10G_intf_end):
            self.addLink('s1', 's2', port1=ten_g_intf, port2=ten_g_intf)

        for forty_g_intf in irange(sw_40G_intf_start, sw_40G_intf_end):
            self.addLink('s1', 's2', port1=forty_g_intf, port2=forty_g_intf)


class lacpdTest(OpsVsiTest):

    def setupNet(self):

        # Create a topology with two VsiOpenSwitch switches,
        # and a host connected to each switch.
        host_opts = self.getHostOpts()
        switch_opts = self.getSwitchOpts()
        lacpd_topo = myDualSwitchTopo(sws=2, hopts=host_opts,
                                      sopts=switch_opts)

        self.net = Mininet(lacpd_topo, switch=VsiOpenSwitch,
                           host=Host, link=OpsVsiLink,
                           controller=None, build=True)

    # Simulate valid pluggable modules in all the modules.
    def test_pre_setup(self):
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]

        lacpd_switch_pre_setup(s1)
        lacpd_switch_pre_setup(s2)

    # Clear the user_config of all the Interfaces.
    # Reset the pm_info to default values.
    def test_post_cleanup(self):
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]

        for intf in irange(sw_1G_intf_start, sw_1G_intf_end) + \
                irange(sw_10G_intf_start, sw_10G_intf_end) + \
                irange(sw_40G_intf_start, sw_40G_intf_end):

            sw_clear_user_config(s1, intf)
            sw_clear_user_config(s2, intf)
            sw_set_intf_pm_info(s1, intf, ('connector=absent',
                                           'connector_status=unsupported'))
            sw_set_intf_pm_info(s2, intf, ('connector=absent',
                                           'connector_status=unsupported'))

    # Enable all the Interfaces used in the test.
    def enable_all_intf(self):
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]

        for intf in irange(sw_1G_intf_start, sw_1G_intf_end) +\
                irange(sw_10G_intf_start, sw_10G_intf_end) +\
                irange(sw_40G_intf_start, sw_40G_intf_end):

            sw_set_intf_user_config(s1, intf, ['admin=up'])
            sw_set_intf_user_config(s2, intf, ['admin=up'])

    def static_lag_config(self):

        info("\n============= lacpd user config "
             "(static LAG) tests =============\n")
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]

        # Setup valid pluggable modules.
        info("Setting up valid pluggable modules in all the Interfaces.\n")
        self.test_pre_setup()

        # Create a Static lag of 'eight' ports of each speed type
        sw_create_bond(s1, "lag0", sw_1G_intf[0:8])
        sw_create_bond(s1, "lag1", sw_10G_intf[0:8])
        sw_create_bond(s1, "lag2", sw_40G_intf[0:8])

        # When Interfaces are not enabled, they shouldn't be added to LAG.
        info("Verify that interfaces are not added to LAG "
             "when they are disabled.\n")
        for intf in sw_1G_intf[0:8] + sw_10G_intf[0:8] + sw_40G_intf[0:8]:
            verify_intf_not_in_bond(s1, intf, "Interfaces should not be part "
                                              "of LAG when they are disabled.")

        info("Enabling all the interfaces.\n")
        self.enable_all_intf()

        # Verify that hw_bond_config:{rx_enabled="true", tx_enabled="true"}
        # In static LAG, Interfaces should be added to LAG,
        # even though ports are not added to LAG on S2.
        info("Verify that all the interfaces are added to LAG.\n")
        for intf in sw_1G_intf[0:8]:
            verify_intf_in_bond(s1, intf, "Expected the 1G interfaces "
                                "to be added to static lag")

        for intf in sw_10G_intf[0:8]:
            verify_intf_in_bond(s1, intf, "Expected the 10G interfaces "
                                "to be added to static lag")

        for intf in sw_40G_intf[0:8]:
            verify_intf_in_bond(s1, intf, "Expected the 40G interfaces "
                                "to be added to static lag")

        # Remove an Interface from bond.
        # Verify that hw_bond_config:{rx_enabled="false", tx_enabled="false"}
        remove_intf_from_bond(s1, "lag0", sw_1G_intf[0])

        info("Verify that RX/TX is set to false when it is "
             "removed from LAG.\n")
        verify_intf_not_in_bond(s1, sw_1G_intf[0],
                                "Expected the interfaces to be removed "
                                "from static lag")

        # Add the Interface back into the LAG/Trunk.
        add_intf_to_bond(s1, "lag0", sw_1G_intf[0])

        # Verify that Interface is added back to LAG/Trunk
        info("Verify that RX/TX is set to true when it is added to LAG.\n")
        verify_intf_in_bond(s1, sw_1G_intf[0], "Interfaces is not "
                            "added back to the trunk.")

        # In case of static LAGs we need a minimum of two Interfaces.
        # Remove all Interfaces except two.
        remove_intf_list_from_bond(s1, "lag0", sw_1G_intf[2:8])

        info("Verify that a LAG can exist with two interfaces.\n")
        for intf in sw_1G_intf[0:2]:
            verify_intf_in_bond(s1, intf, "Expected a static trunk "
                                "of two interfaces.")

        for intf in sw_1G_intf[2:8]:
            verify_intf_not_in_bond(s1, intf, "Expected interfaces "
                                    "to be removed from the LAG.")

        # OPS_TODO: If we remove one more Interface,
        # how will we know if the LAG has suddenly become PORT

        # Disable one of the Interfaces, then it should be
        # removed from the LAG.
        info("Verify that a interface is removed "
             "from LAG when it is disabled.\n")
        sw_set_intf_user_config(s1, sw_10G_intf[0], ['admin=down'])
        verify_intf_not_in_bond(s1, sw_10G_intf[0],
                                "Disabled interface is not removed "
                                "from the LAG.")

        # Enable the Interface back, then it should be added back
        info("Verify that a interface is added back to LAG "
             "when it is re-enabled.\n")
        sw_set_intf_user_config(s1, sw_10G_intf[0], ['admin=up'])
        verify_intf_in_bond(s1, sw_10G_intf[0],
                            "Re-enabled interface is not added "
                            "back to the trunk.")

        # OPS_TODO: Enhance VSI to simulate link up/down.
        # Looks like we need ovs-appctl mechanism to simulate link down,
        # otherwise switchd is always re-setting the link.
        # simulate_link_state(s1, sw_10G_intf[0], 'down')
        # verify_intf_not_in_bond(s1, sw_10G_intf[0], \
        #                         "Link down interface is not removed "
        #                         "from the trunk.")

        # simulate_link_state(s1, sw_10G_intf[0], 'up')
        # verify_intf_in_bond(s1, sw_10G_intf[0], \
        #                     "Interface is not added back when it is "
        #                     "linked up")

        sw_delete_bond(s1, "lag0")
        sw_delete_bond(s1, "lag1")
        sw_delete_bond(s1, "lag2")

        self.test_post_cleanup()

    def static_lag_negative_tests(self):

        info("\n============= lacpd user config "
             "(static LAG negative) tests =============\n")
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]

        # Setup valid pluggable modules.
        self.test_pre_setup()

        sw_set_intf_user_config(s1, sw_1G_intf[0], ['admin=up'])
        sw_set_intf_user_config(s1, sw_1G_intf[1], ['admin=up'])

        sw_set_intf_user_config(s1, sw_10G_intf[0], ['admin=up'])
        sw_set_intf_user_config(s1, sw_10G_intf[1], ['admin=up'])

        # Create a static LAG with Interfaces of multiple speeds.
        sw_create_bond(s1, "lag0", sw_1G_intf[0:2])
        add_intf_list_from_bond(s1, "lag0", sw_10G_intf[0:2])

        # When Interfaces with different speeds are added,
        # then the first interface is choosen as base, and
        # then only those interfaces of the same speed are added to LAG

        info("Verify that interfaces with matching speeds "
             "are enabled in LAG.\n")
        for intf in sw_1G_intf[0:2]:
            verify_intf_in_bond(s1, intf, "Expected the 1G "
                                "interfaces to be added to LAG ")

        info("Verify that interfaces with "
             "non-matching speeds are disabled in LAG.\n")
        for intf in sw_10G_intf[0:2]:
            verify_intf_not_in_bond(s1, intf, "Expected the 10G interfaces "
                                    "not added to LAG "
                                    "when there is speed mismatch")

        # When both the 1G interfaces are disabled/down,
        # then we should add the 10G interfaces to LAG.
        info("Verify interfaces join LAG when speed "
             "matching block is removed.\n")

        remove_intf_list_from_bond(s1, "lag0", sw_1G_intf[0:2])

        for intf in sw_10G_intf[0:2]:
            verify_intf_in_bond(s1, intf,
                                "Interface should be added "
                                "to bond after others are deleted.")

        sw_delete_bond(s1, "lag0")

        self.test_post_cleanup()

    def dynamic_lag_config(self):

        info("\n============= lacpd user config "
             "(dynamic LAG) tests =============\n")
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]

        # These lines are helpful to debug errors with the daemon
        # during the tests execution
        # s1.ovscmd("ovs-appctl -t ops-lacpd vlog/set dbg")
        # s2.ovscmd("ovs-appctl -t ops-lacpd vlog/set dbg")

        system_mac = {}
        system_mac[1] = s1.ovscmd("ovs-vsctl get system . "
                                  "system_mac").rstrip('\r\n')
        system_mac[2] = s2.ovscmd("ovs-vsctl get system . "
                                  "system_mac").rstrip('\r\n')
        system_prio = {}
        system_prio[1] = "65534"
        system_prio[2] = "65534"

        # Enable all the interfaces under test.
        self.test_pre_setup()

        info("Test base mac address\n")
        for intf in sw_1G_intf[0:2]:
            sw_set_intf_user_config(s1, intf, ['admin=up'])
            sw_set_intf_user_config(s2, intf, ['admin=up'])

        sw_create_bond(s1, "lag0", sw_1G_intf[0:2], lacp_mode="active")
        sw_create_bond(s2, "lag0", sw_1G_intf[0:2], lacp_mode="active")

        for intf in sw_1G_intf[0:2]:
            set_intf_other_config(s1, intf, ['lacp-aggregation-key=1'])
            set_intf_other_config(s2, intf, ['lacp-aggregation-key=1'])
        time.sleep(30)
        for intf in sw_1G_intf[0:2]:
            verify_intf_lacp_status(s1,
                                    intf,
                                    {"actor_system_id": system_prio[1] +
                                     "," + system_mac[1],
                                     "partner_state":
                                     "Activ:1,TmOut:0,Aggr:1,Sync:1,Col:1,"
                                     "Dist:1,Def:0,Exp:0",
                                     "partner_system_id": system_prio[2] +
                                     "," + system_mac[2]},
                                    "s1:" + intf)

            verify_intf_lacp_status(s2,
                                    intf,
                                    {"actor_system_id": system_prio[2] +
                                     "," + system_mac[2],
                                     "partner_state":
                                     "Activ:1,TmOut:0,Aggr:1,Sync:1,Col:1,"
                                     "Dist:1,Def:0,Exp:0",
                                     "partner_system_id":
                                     system_prio[1] + "," + system_mac[1]},
                                    "s1:" + intf)

        # Test lacp-time

        info("Verify port:other_config:lacp-time.\n")

        # Verify default lacp-time is "slow"

        verify_intf_lacp_status(s1,
                                1,
                                {"actor_state":
                                 "Activ:1,TmOut:0,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0",
                                 "actor_system_id": system_prio[1] +
                                 "," + system_mac[1],
                                 "partner_state":
                                 "Activ:1,TmOut:0,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0"},
                                "s1:1")

        succes = False
        out = s1.cmdCLI('show lacp aggregates')
        lines = out.split('\n')
        for line in lines:
            if 'Heartbeat rate' and 'slow' in line:
                succes = True
        assert succes is True,\
            "Failed show lacp aggregates, Heartbeat rate is not slow"

        # Set lacp-time back to "fast"
        set_port_parameter(s1, "lag0", ['other_config:lacp-time=fast'])
        set_port_parameter(s1, "lag1", ['other_config:lacp-time=fast'])
        set_port_parameter(s1, "lag2", ['other_config:lacp-time=fast'])
        set_port_parameter(s2, "lag0", ['other_config:lacp-time=fast'])
        set_port_parameter(s2, "lag1", ['other_config:lacp-time=fast'])
        set_port_parameter(s2, "lag2", ['other_config:lacp-time=fast'])

        # Verify "timeout" is now "fast"
        verify_intf_lacp_status(s1,
                                intf,
                                {"actor_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0",
                                 "actor_system_id": system_prio[1] +
                                 "," + system_mac[1],
                                 "partner_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0"},
                                "s1:" + intf)

        succes = False
        out = s1.cmdCLI('show lacp aggregates')
        lines = out.split('\n')
        for line in lines:
            if 'Heartbeat rate' and 'fast' in line:
                succes = True
        assert succes is True,\
            "Failed show lacp aggregates, Heartbeat rate is not fast"

        info("Delete and recreate lag\n")
        # delete and recreate lag
        s1.ovscmd("ovs-vsctl del-port lag0")
        s2.ovscmd("ovs-vsctl del-port lag0")

        for intf in sw_1G_intf[0:2]:
            sw_set_intf_user_config(s1, intf, ['admin=up'])
            sw_set_intf_user_config(s2, intf, ['admin=up'])

        # Create two dynamic LAG with two interfaces each.
        # the current schema doesn't allow creating a bond
        # with less than two interfaces. Once that is changed
        # we should modify the test case.
        info("Creating dynamic lag with two interfaces\n")
        sw_create_bond(s1, "lag0", sw_1G_intf[0:2], lacp_mode="active")
        sw_create_bond(s2, "lag0", sw_1G_intf[0:2], lacp_mode="active")

        set_port_parameter(s1, "lag0", ['other_config:lacp-time=fast'])
        set_port_parameter(s2, "lag0", ['other_config:lacp-time=fast'])

        # Enable both interfaces.
        for intf in sw_1G_intf[0:2]:
            sw_set_intf_user_config(s1, intf, ['admin=up'])
            sw_set_intf_user_config(s2, intf, ['admin=up'])

        # Verify that all the interfaces are linked up
        for intf in sw_1G_intf[0:2]:
            verify_intf_status(s1, intf, "link_state", "up")
            verify_intf_status(s2, intf, "link_state", "up")
            verify_intf_status(s1, intf, "link_speed", "1000000000")
            verify_intf_status(s2, intf, "link_speed", "1000000000")

        for intf in sw_1G_intf[0:2]:
            verify_intf_in_bond(s1, intf, "Interfaces are expected to be "
                                "part of dynamic LAG when "
                                "both the switches are in "
                                "active mode on switch1")
            verify_intf_in_bond(s2, intf, "Interfaces are expected to be "
                                "part of dynamic LAG when "
                                "both the switches are in "
                                "active mode on switch1")

        # Test system:lacp_config:{lacp-system-id,lacp-system-priority}

        intf = sw_1G_intf[0]

        # Test port:lacp

        # Set lacp to "passive"
        set_port_parameter(s1, "lag0", ['lacp=passive'])

        verify_intf_lacp_status(s1,
                                intf,
                                {"actor_state":
                                 "Activ:0,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0",
                                 "actor_system_id": system_prio[1] +
                                 "," + system_mac[1],
                                 "partner_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0",
                                 "partner_system_id": system_prio[2] +
                                 "," + system_mac[2]},
                                "s1:" + intf)

        # Set lacp to "active"
        set_port_parameter(s1, "lag0", ['lacp=active'])

        verify_intf_lacp_status(s1,
                                intf,
                                {"actor_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0",
                                 "actor_system_id": system_prio[1] +
                                 "," + system_mac[1],
                                 "partner_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0",
                                 "partner_system_id": system_prio[2] +
                                 "," + system_mac[2]},
                                "s1:" + intf)

        # Test lacp-time

        info("Verify port:other_config:lacp-time.\n")

        # Set lacp-time to "slow"
        set_port_parameter(s1, "lag0", ['other_config:lacp-time=slow'])

        # Verify "timeout" is now "slow"
        verify_intf_lacp_status(s1,
                                1,
                                {"actor_state":
                                 "Activ:1,TmOut:0,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0",
                                 "actor_system_id": system_prio[1] +
                                 "," + system_mac[1],
                                 "partner_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0",
                                 "partner_system_id": system_prio[2] +
                                 "," + system_mac[2]},
                                "s1:1")

        # Set lacp-time back to "fast"
        set_port_parameter(s1, "lag0", ['other_config:lacp-time=fast'])

        # Verify "timeout" is now "fast"
        verify_intf_lacp_status(s1,
                                intf,
                                {"actor_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0",
                                 "actor_system_id": system_prio[1] +
                                 "," + system_mac[1],
                                 "partner_state":
                                 "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                 "Dist:1,Def:0,Exp:0",
                                 "partner_system_id": system_prio[2] +
                                 "," + system_mac[2]},
                                "s1:" + intf)

        # Test interface:other_config:{lacp-port-id,lacp-port-priority}

        # Changing aggregation key on interface for s1
        # This will take out the interface from the lag0
        info("Validate aggregation key functionality\n")
        set_intf_other_config(s1, intf, ['lacp-aggregation-key=2'])

        # First validate if the interface change the aggregation key correctly
        verify_intf_lacp_status(s1,
                                intf,
                                {"actor_key": "2"},
                                "s1:" + intf)

        # Verifying the interface is not part of any LAG anymore
        verify_intf_not_in_bond(s1, intf, "Interfaces should not be part of "
                                "the dynamic LAG when aggregation key "
                                "is changed")

        # Get the interface back to the LAG
        set_intf_other_config(s1, intf, ['lacp-aggregation-key=1'])
        verify_intf_in_bond(s1, intf, "Interface should get part of "
                            "the dynamic LAG when aggregation key is changed")

        # Test interface:other_config:{lacp-port-id,lacp-port-priority}
        info("Test interface other_config values for lacp-port-id and "
             "lacp-port-priority\n")
        # save original values
        original_pri_info = sw_get_intf_state((s1, intf,
                                              ['lacp_status:'
                                               'actor_port_id']))[0]
        # Set port_id, port_priority, and aggregation-key
        set_intf_other_config(s1, intf,
                              ['lacp-port-id=222',
                               'lacp-port-priority=123'])

        # Get the new values
        verify_intf_lacp_status(s1,
                                intf,
                                {"actor_port_id": "123,222"},
                                "s1:" + intf)

        # Set invalid port_id and port_priority
        set_intf_other_config(s1, intf,
                              ['lacp-port-id=-1',
                               'lacp-port-priority=-1'])

        # Get the new values
        verify_intf_lacp_status(s1,
                                intf,
                                {"actor_port_id": original_pri_info},
                                "s1:" + intf)

        set_intf_other_config(s1, intf,
                              ['lacp-port-id=65536',
                               'lacp-port-priority=65536'])

        # Get the new values
        pri_info = sw_get_intf_state((s1, intf,
                                     ['lacp_status:'
                                      'actor_port_id']))[0].split(',')

        verify_intf_lacp_status(s1,
                                intf,
                                {"actor_port_id": original_pri_info},
                                "s1:" + intf)

        info("Clear lacp-port-id and lacp-port-priority\n")
        s1.ovscmd("ovs-vsctl remove interface " +
                  intf + " other_config lacp-port-id")
        s1.ovscmd("ovs-vsctl remove interface " +
                  intf + " other_config lacp-port-priority")

        verify_intf_lacp_status(s1,
                                intf,
                                {"actor_port_id": original_pri_info},
                                "s1:" + intf)

        info("Verify port lacp_status\n")
        # verify lag status
        verify_port_lacp_status(s1,
                                "lag0",
                                '{bond_speed=1000, bond_status=ok}',
                                'Port lacp_status is expected to be '
                                'bond_speed=1000, '
                                'bond_status=ok')
        verify_port_lacp_status(s2,
                                "lag0",
                                '{bond_speed=1000, bond_status=ok}',
                                'Port lacp_status is expected to be '
                                'bond_speed=1000, '
                                'bond_status=ok')

        info("Verify interface lacp_status\n")
        for intf in sw_1G_intf[0:2]:
            verify_intf_lacp_status(s1,
                                    intf,
                                    {"actor_state":
                                        "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                        "Dist:1,Def:0,Exp:0",
                                        "actor_system_id": system_prio[1] +
                                        "," + system_mac[1],
                                        "partner_state":
                                        "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                        "Dist:1,Def:0,Exp:0",
                                        "partner_system_id": system_prio[2] +
                                        "," + system_mac[2]},
                                    "s1:" + intf)
            verify_intf_lacp_status(s2,
                                    intf,
                                    {"actor_state":
                                        "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                        "Dist:1,Def:0,Exp:0",
                                        "actor_system_id": system_prio[2] +
                                        "," + system_mac[2],
                                        "partner_state":
                                        "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                        "Dist:1,Def:0,Exp:0",
                                        "partner_system_id": system_prio[1] +
                                        "," + system_mac[1]},
                                    "s2:" + intf)

        info("Verify dynamic update of port-level override\n")
        sw_create_bond(s2, "lag1", sw_1G_intf[2:4], lacp_mode="active")

        for intf in sw_1G_intf[2:4]:
            sw_set_intf_user_config(s1, intf, ['admin=up'])
            sw_set_intf_user_config(s2, intf, ['admin=up'])

        for intf in sw_1G_intf[2:4]:
            set_intf_other_config(s1, intf, ['lacp-aggregation-key=2'])
            set_intf_other_config(s2, intf, ['lacp-aggregation-key=2'])

        for intf in sw_1G_intf[2:4]:
            verify_intf_lacp_status(s2,
                                    intf,
                                    {"actor_system_id": system_prio[2] +
                                        "," + system_mac[2]},
                                    "s2:" + intf)


@pytest.mark.skipif(True, reason="Skipping due to instability")
class Test_lacpd:

    def setup(self):
        pass

    def teardown(self):
        pass

    def setup_class(cls):
        # Create the Mininet topology based on Mininet.
        Test_lacpd.test = lacpdTest()

        # Stop PMD. This tests manually sets lot of DB elements
        # that 'pmd' is responsible for. To avoid any cross
        # interaction disable 'pmd'
        Test_lacpd.test.net.switches[0].cmd("/bin/systemctl stop pmd")
        Test_lacpd.test.net.switches[1].cmd("/bin/systemctl stop pmd")

    def teardown_class(cls):
        Test_lacpd.test.net.switches[0].cmd("/bin/systemctl start pmd")
        Test_lacpd.test.net.switches[1].cmd("/bin/systemctl start pmd")

        # ops-lacpd is stopped so that it produces the gcov coverage data
        #
        # Daemons from both switches will dump the coverage data to the
        # same file but the data write is done on daemon exit only.
        # The systemctl command waits until the process exits to return the
        # prompt and the object.cmd() function waits for the command to return,
        # therefore it is safe to stop the ops-lacpd daemons sequentially
        # This ensures that data from both processes is captured.
        Test_lacpd.test.net.switches[0].cmd("/bin/systemctl stop ops-lacpd")
        Test_lacpd.test.net.switches[1].cmd("/bin/systemctl stop ops-lacpd")

        # Stop the Docker containers, and
        # mininet topology
        Test_lacpd.test.net.stop()

    def setup_method(self, method):
        pass

    def teardown_method(self, method):
        pass

    def __del__(self):
        del self.test

    # Link aggregation(lacp) daemon tests.
    def test_lacpd_static_lag_config(self):
        self.test.static_lag_config()

    def test_lacpd_static_lag_negative_tests(self):
        self.test.static_lag_negative_tests()

    def test_lacpd_dynamic_lag_config(self):
        self.test.dynamic_lag_config()
