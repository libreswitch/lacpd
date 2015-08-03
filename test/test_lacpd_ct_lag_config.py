#!/usr/bin/python
#
# Copyright (C) 2015 Hewlett-Packard Development Company, L.P.
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
#

import os
import sys
import time
import subprocess
import pytest

from halonvsi.docker import *
from halonvsi.halon import *

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


def short_sleep(tm=.5):
    time.sleep(tm)


# Set user_config for an Interface.
def sw_set_intf_user_config(sw, interface, config):
    c = OVS_VSCTL + "set interface " + str(interface)
    for s in config:
        c += " user_config:" + s
    debug(c)
    return sw.cmd(c)


# Clear user_config for an Interface.
def sw_clear_user_config(sw, interface):
    c = OVS_VSCTL + "clear interface " + str(interface) + " user_config"
    debug(c)
    return sw.cmd(c)


# Set pm_info for an Interface.
def sw_set_intf_pm_info(sw, interface, config):
    c = OVS_VSCTL + "set interface " + str(interface)
    for s in config:
        c += " pm_info:" + s
    debug(c)
    return sw.cmd(c)


# Simulate the link state on an Interface
def simulate_link_state(sw, interface, link_state="up"):
    info("Setting the link state of interface " + interface + " to " + link_state + "\n")
    c = OVS_VSCTL + "set interface " + str(interface) + " link_state=" + link_state
    debug(c)
    return sw.cmd(c)


# Get the values of a set of columns from Interface table.
# This function returns a list of values if 2 or more
# fields are requested, and returns a single value (no list)
# if only 1 field is requested.
def sw_get_intf_state(sw, interface, fields):
    c = OVS_VSCTL + "get interface " + str(interface)
    for f in fields:
        c += " " + f
    out = sw.ovscmd(c).splitlines()
    # If a single column value is requested,
    # then return a singleton value instead of list.
    if len(out) == 1:
        out = out[0]
    debug(out)
    return out


# Create a bond/lag/trunk in the OVS-DB.
def sw_create_bond(s1, bond_name, intf_list, lacp_mode="off"):
    info("Creating LAG " + bond_name + " with interfaces: " + str(intf_list) + "\n")
    c = OVS_VSCTL + "add-bond bridge_normal " + bond_name + " " + " ".join(map(str, intf_list))
    c += " -- set port " + bond_name + " lacp=" + lacp_mode
    debug(c)
    return s1.cmd(c)


# Delete a bond/lag/trunk from OVS-DB.
def sw_delete_bond(sw, bond_name):
    info("Deleting the bond " + bond_name + "\n")
    c = OVS_VSCTL + "del-port bridge_normal " + bond_name
    debug(c)
    return sw.cmd(c)


# Add a new Interface to the existing bond.
def add_intf_to_bond(sw, bond_name, intf_name):

    info("Adding interface " + intf_name + " to LAG " + bond_name + "\n")

    # Get the UUID of the interface that has to be added.
    c = OVS_VSCTL + "get interface " + str(intf_name) + " _uuid"
    debug(c)
    intf_uuid = sw.cmd(c).rstrip('\r\n')

    # Get the current list of Interfaces in the bond.
    c = OVS_VSCTL + "get port " + bond_name + " interfaces"
    debug(c)
    out = sw.cmd(c)
    intf_list = out.rstrip('\r\n').strip("[]").replace(" ", "").split(',')

    if intf_uuid in intf_list:
        print "Interface " + intf_name + " is already part of " + bond_name
        return

    # Add the given intf_name's UUID to existing Interfaces.
    intf_list.append(intf_uuid)

    # Set the new Interface list in the bond.
    new_intf_str = '[' + ",".join(intf_list) + ']'

    c = OVS_VSCTL + "set port " + bond_name + " interfaces=" + new_intf_str
    debug(c)
    return sw.cmd(c)


# Add a list of Interfaces to the bond.
def add_intf_list_from_bond(sw, bond_name, intf_list):
    for intf in intf_list:
        add_intf_to_bond(sw, bond_name, intf)


# Remove an Interface from a bond.
def remove_intf_from_bond(sw, bond_name, intf_name, fail=True):

    info("Removing interface " + intf_name + " from LAG " + bond_name + "\n")

    # Get the UUID of the Interface that has to be removed.
    c = OVS_VSCTL + "get interface " + str(intf_name) + " _uuid"
    debug(c)
    intf_uuid = sw.cmd(c).rstrip('\r\n')

    # Get the current list of Interfaces in the bond.
    c = OVS_VSCTL + "get port " + bond_name + " interfaces"
    debug(c)
    out = sw.cmd(c)
    intf_list = out.rstrip('\r\n').strip("[]").replace(" ", "").split(',')

    if intf_uuid not in intf_list:
        assert fail == True, "Unable to find the interface in the bond"
        return

    # Remove the given intf_name's UUID from the bond's Interfaces.
    new_intf_list = [i for i in intf_list if i != intf_uuid]

    # Set the new Interface list in the bond.
    new_intf_str = '[' + ",".join(new_intf_list) + ']'

    c = OVS_VSCTL + "set port " + bond_name + " interfaces=" + new_intf_str
    debug(c)
    return sw.cmd(c)


# Remove a list of Interfaces from the bond.
def remove_intf_list_from_bond(sw, bond_name, intf_list):
    for intf in intf_list:
        remove_intf_from_bond(sw, bond_name, intf)


# Verify that an Interface is part of a bond.
def verify_intf_in_bond(sw, intf, msg):
    rx_en, tx_en = sw_get_intf_state(sw, intf, ['hw_bond_config:rx_enabled', \
                                                'hw_bond_config:tx_enabled'])
    assert rx_en == 'true' and tx_en == 'true', msg


# Verify that an Interface is not part of any bond.
def verify_intf_not_in_bond(sw, intf, msg):
        rx_en, tx_en = sw_get_intf_state(sw, intf, ['hw_bond_config:rx_enabled', \
                                                    'hw_bond_config:tx_enabled'])
        assert rx_en != 'true' and tx_en != 'true', msg

# Verify Interface status
def verify_intf_status(sw, intf, column_name, value, msg=''):
        column_val = sw_get_intf_state(sw, intf, [column_name])
        assert column_val == value, msg


def lacpd_switch_pre_setup(sw):

    for intf in irange(sw_1G_intf_start, sw_1G_intf_end):
        sw_set_intf_pm_info(sw, intf, ('connector="SFP_RJ45"', 'connector_status=supported',
                                        'max_speed="1000"', 'supported_speeds="1000"'))

    for intf in irange(sw_10G_intf_start, sw_10G_intf_end):
        sw_set_intf_pm_info(sw, intf, ('connector=SFP_SR', 'connector_status=supported',
                                       'max_speed="10000"', 'supported_speeds="10000"'))

    for intf in irange(sw_40G_intf_start, sw_40G_intf_end):
        sw_set_intf_pm_info(sw, intf, ('connector=QSFP_SR4', 'connector_status=supported',
                                       'max_speed="40000"', 'supported_speeds="40000,10000"'))


# Create a topology with two switches, and 10 ports connected
# to each other.
class myDualSwitchTopo( Topo ):
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
            switch = self.addSwitch('s%s' %s)

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


class lacpdTest(HalonTest):

    def setupNet(self):

        # Create a topology with two Halon switches,
        # and a host connected to each switch.
        host_opts = self.getHostOpts()
        switch_opts = self.getSwitchOpts()
        lacpd_topo = myDualSwitchTopo(sws=2, hopts=host_opts, sopts=switch_opts)

        self.net = Mininet(lacpd_topo, switch=HalonSwitch,
                           host=Host, link=HalonLink,
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

        for intf in irange(sw_1G_intf_start, sw_1G_intf_end) + \
                    irange(sw_10G_intf_start, sw_10G_intf_end) + \
                    irange(sw_40G_intf_start, sw_40G_intf_end):

            sw_set_intf_user_config(s1, intf, ['admin=up'])
            sw_set_intf_user_config(s2, intf, ['admin=up'])


    def static_lag_config(self):

        info("\n============= lacpd user config (static LAG) tests =============\n")
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]


        # Setup valid pluggable modules.
        info("Setting up valid pluggable modules in all the Interfaces.\n")
        self.test_pre_setup()

        # Create a Staic lag of 'eight' ports of each speed type
        sw_create_bond(s1, "lag0", sw_1G_intf[0:8])
        sw_create_bond(s1, "lag1", sw_10G_intf[0:8])
        sw_create_bond(s1, "lag2", sw_40G_intf[0:8])

        # When Interfaces are not enabled, they shouldn't be added to LAG.
        info("Verify that interfaces are not added to LAG when they are disabled.\n")
        for intf in sw_1G_intf[0:8] + sw_10G_intf[0:8] + sw_40G_intf[0:8]:
            verify_intf_not_in_bond(s1, intf, "Interfaces shsould not be part of LAG "
                                              "when they are disabled.")

        info("Enbling all the interfaces.\n")
        self.enable_all_intf()
        short_sleep()

        # Verify that hw_bond_config:{rx_enabled="true", tx_enabled="true"}
        # In static LAG, Interfaces should be added to LAG,
        # even though ports are not added to LAG on S2.
        info("Verify that all the interfaces are added to LAG.\n")
        for intf in sw_1G_intf[0:8]:
            verify_intf_in_bond(s1, intf, "Expected the 1G interfaces to be added to static lag")

        for intf in sw_10G_intf[0:8]:
            verify_intf_in_bond(s1, intf, "Expected the 10G interfaces to be added to static lag")

        for intf in sw_40G_intf[0:8]:
            verify_intf_in_bond(s1, intf, "Expected the 40G interfaces to be added to static lag")

        # HALON_TODO: Verify LAG status for static LAGs.
        #             Verify the LAG operation speed.

        # Remove an Interface from bond.
        # Verify that hw_bond_config:{rx_enabled="false", tx_enabled="false"}
        remove_intf_from_bond(s1, "lag0", sw_1G_intf[0])
        short_sleep()

        info("Verify that RX/TX is set to false when it is removed from LAG.\n")
        verify_intf_not_in_bond(s1, sw_1G_intf[0], \
                                "Expected the interfaces to be removed from static lag")

        # Add the Interface back into the LAG/Trunk.
        add_intf_to_bond(s1, "lag0", sw_1G_intf[0])
        short_sleep()

        # Verify that Interface is added back to LAG/Trunk
        info("Verify that RX/TX is set to true when it is added to LAG.\n")
        verify_intf_in_bond(s1, sw_1G_intf[0], "Interfaces is not added back to the trunk.")

        # In case of static LAGs we need a minimum of two Interfaces.
        # Remove all Interfaces except two.
        remove_intf_list_from_bond(s1, "lag0", sw_1G_intf[2:8])
        short_sleep()

        info("Verify that a LAG can exist with two interfaces.\n")
        for intf in sw_1G_intf[0:2]:
            verify_intf_in_bond(s1, intf, "Expected a static trunk of two interfaces.")

        for intf in sw_1G_intf[2:8]:
            verify_intf_not_in_bond(s1, intf, "Expected interfaces to be removed from the LAG.")

        # HALON_TODO: If we remove one more Interface,
        #             how will i know if the LAG has suddenly become PORT

        # Disable one of the Interfaces, then it should be removed from the LAG.
        info("Verify that a interface is removed from LAG when it is disabled.\n")
        sw_set_intf_user_config(s1, sw_10G_intf[0], ['admin=down'])
        short_sleep()
        verify_intf_not_in_bond(s1, sw_10G_intf[0], \
                                "Disabled interface is not removed from the LAG.")

        # Enable the Interface back, then it should be added back
        info("Verify that a interface is added back to LAG when it is re-enabled.\n")
        sw_set_intf_user_config(s1, sw_10G_intf[0], ['admin=up'])
        short_sleep()
        verify_intf_in_bond(s1, sw_10G_intf[0], \
                            "Re-enabled interface is not added back to the trunk.")

       # HALON_TODO: Enhance VSI to simulate link up/down.
       # Looks like we need ovs-appctl mechanism to simulate link down,
       # otherwise switchd is always re-setting the link.
       #  simulate_link_state(s1, sw_10G_intf[0], 'down')
       #  short_sleep()
       #  verify_intf_not_in_bond(s1, sw_10G_intf[0], \
       #                          "Link down interface is not removed from the trunk.")

       #  simulate_link_state(s1, sw_10G_intf[0], 'up')
       #  short_sleep()
       #  verify_intf_in_bond(s1, sw_10G_intf[0], \
       #                          "Interface is not added back when it is linked up")

        sw_delete_bond(s1, "lag0")
        sw_delete_bond(s1, "lag1")
        sw_delete_bond(s1, "lag2")

        self.test_post_cleanup()


    def static_lag_negative_tests(self):

        info("\n============= lacpd user config (static LAG negative) tests =============\n")
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]

        # Setup valid pluggable modules.
        self.test_pre_setup()

        # Create a static LAG with Interfaces of multiple speeds.
        sw_create_bond(s1, "lag0", sw_1G_intf[0:2])
        add_intf_list_from_bond(s1, "lag0", sw_10G_intf[0:2])

        # When Interfaces with different speeds are added,
        # then the first interface is choosen as base, and
        # then only those interfaces of the same speed are added to LAG

        info("Verify that interfaces with non-matching speeds are disabled in LAG.\n")
        for intf in sw_10G_intf[0:2]:
            verify_intf_not_in_bond(s1, intf, "Expected the 10G interfaces not added to LAG "
                                              "when there is speed mismatch")

        # HALON_TODO: When both the 1G interfaces are disabled/down,
        # then we should add the 10G interfaces to LAG.
        # Currently it is not working as expected.
        # Add the tests when it is fixed.

        sw_delete_bond(s1, "lag0")

        self.test_post_cleanup()


    def dynamic_lag_config(self):

        info("\n============= lacpd user config (dynamic LAG) tests =============\n")
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]


        # Enable all the interfaces under test.
        self.test_pre_setup()

        # Change the LACP system ID on one of the switches.
        # In VSI environment both the switches start with the same system ID.
        s1.cmd("ovs-vsctl set open_vswitch . lacp_config:lacp-system-id='70:72:aa:aa:aa:d4'")

        # HALON_TODO: There are bugs in LACP code.
        # Due to which it is not picking up the system_id change.
        # Until it is fixed, restart lacpd.
        s1.cmd("systemctl restart lacpd")
        s2.cmd("systemctl restart lacpd")

        # Create two dynamic LAG with two ports each.
        # the current schema doesn't allow creating a bond
        # with less than two ports. Once that is changed
        # we should modify the test case.
        sw_create_bond(s1, "lag0", sw_1G_intf[0:2], lacp_mode="active")
        sw_create_bond(s2, "lag0", sw_1G_intf[0:2], lacp_mode="active")
        short_sleep()

        # Enable both the interfaces.
        for intf in sw_1G_intf[0:2]:
            sw_set_intf_user_config(s1, intf, ['admin=up'])
            sw_set_intf_user_config(s2, intf, ['admin=up'])
        short_sleep()

        # Verify that all the interfaces are linked up
        for intf in sw_1G_intf[0:2]:
            verify_intf_status(s1, intf, "link_state", "up")
            verify_intf_status(s2, intf, "link_state", "up")
            verify_intf_status(s1, intf, "link_speed", "1000000000")
            verify_intf_status(s2, intf, "link_speed", "1000000000")

        short_sleep(8)

        for intf in sw_1G_intf[0:2]:
            verify_intf_in_bond(s1, intf, "Interfaces are expected to be part of dynamic LAG when "
                                          "both the switches are in active mode on switch1")
            verify_intf_in_bond(s2, intf, "Interfaces are expected to be part of dynamic LAG when "
                                          "both the switches are in active mode on switch1")


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

    # HALON_TODO:  Dynamic LAG functionality is not working
    # as expected to write tests.
    # def test_lacpd_dynamic_lag_config(self):
    #    self.test.dynamic_lag_config()
