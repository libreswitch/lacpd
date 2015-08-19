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

# Parse the lacp_status:*_state string
def parse_lacp_state(state):
    return dict(map(lambda l:map(lambda j:j.strip(),l), map(lambda i: i.split(':'), state.split(','))))

# Set pm_info for an Interface.
def sw_set_intf_pm_info(sw, interface, config):
    c = OVS_VSCTL + "set interface " + str(interface)
    for s in config:
        c += " pm_info:" + s
    debug(c)
    return sw.cmd(c)

# Set open_vsw_lacp_config parameter(s)
def set_open_vsw_lacp_config(sw, config):
    c = OVS_VSCTL + "set open_vswitch ."
    for s in config:
        c += " lacp_config:" + s
    debug(c)
    return sw.cmd(c)

# Set open_vsw_lacp_config parameter(s)
def set_port_parameter(sw, port, config):
    c = OVS_VSCTL + "set port " + str(port)
    for s in config:
        c += ' %s' % s
    debug(c)
    return sw.cmd(c)

# Set interface:other_config parameter(s)
def set_intf_other_config(sw, intf, config):
    c = OVS_VSCTL + "set interface " + str(intf)
    for s in config:
        c += ' other_config:%s' % s
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

def sw_get_port_state(sw, port, fields):
    c = OVS_VSCTL + "get port " + str(port)
    for f in fields:
        c += " " + f
    out = sw.ovscmd(c).splitlines()
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

def verify_intf_lacp_status(sw, intf, verify_values, context=''):
    request = []
    attrs = []
    for attr in verify_values:
        request.append('lacp_status:' + attr)
        attrs.append(attr)
    field_vals = sw_get_intf_state(sw, intf, request)
    if len(request) == 1:
        field_vals = [field_vals]
    for i in range(0, len(attrs)):
        assert field_vals[i] == verify_values[attrs[i]], context + ": invalid value for " + attrs[i] + ", expected " + verify_values[attrs[i]] + ", got " + field_vals[i]

def verify_port_lacp_status(sw, lag, value, msg=''):
    lacp_status = sw_get_port_state(sw, lag, ["lacp_status"])
    assert lacp_status == value, msg

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
        #             how will we know if the LAG has suddenly become PORT

        # Disable one of the Interfaces, then it should be removed from the LAG.
        info("Verify that a interface is removed from LAG when it is disabled.\n")
        sw_set_intf_user_config(s1, sw_10G_intf[0], ['admin=down'])
        short_sleep(2)
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

        base_mac = {}
        base_mac[1] = "70:72:11:11:11:d4"
        base_mac[2] = "70:72:22:22:22:d4"

        change_mac = "aa:bb:cc:dd:ee:ff"
        change_prio = "555"
        invalid_mac = "aa:bb:cc:dd:ee:fg"
        invalid_prio = "55a"

        alt_mac = "70:72:33:33:33:d4"
        alt_prio = "99"

        base_prio = "100"
        port_mac = "70:72:44:44:44:d4"
        port_prio = "88"

        # Enable all the interfaces under test.
        self.test_pre_setup()

        # Change the LACP system ID on the switches.
        # In VSI environment both the switches start with the same system ID.
        s1.cmd("ovs-vsctl set open_vswitch . lacp_config:lacp-system-id='" + base_mac[1] + "' lacp_config:lacp-system-priority=" + base_prio)
        s2.cmd("ovs-vsctl set open_vswitch . lacp_config:lacp-system-id='" + base_mac[2] + "' lacp_config:lacp-system-priority=" + base_prio)

        # Create two dynamic LAG with two ports each.
        # the current schema doesn't allow creating a bond
        # with less than two ports. Once that is changed
        # we should modify the test case.
        s1.cmd("ovs-appctl -t lacpd vlog/set DBG")
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

        # Test open_vswitch:lacp_config:{lacp-system-id,lacp-system-priority}

        intf = sw_1G_intf[0]

        # Set sys_id and sys_pri
        set_open_vsw_lacp_config(s1, ['lacp-system-id=' + change_mac, 'lacp-system-priority=' + change_prio])

        short_sleep(2)

        # Get sys_id and sys_pri
        sys = sw_get_intf_state(s1, intf, ['lacp_status:actor_system_id']).split(',')

        info("Verify open_vswitch:lacp_config:lacp-system-id.\n")
        assert sys[1] == change_mac, \
                         "actor_system_id should be %s, is %s".format(change_mac, sys[1])

        info("Verify open_vswitch:lacp_config:lacp-system-priority.\n")
        assert sys[0] == change_prio, "actor_system_priority should be %s, is %s".format(change_prio, sys[0])

        # Attempt to set invalid sys_id and invalid sys_pri
        set_open_vsw_lacp_config(s1, ['lacp-system-id=' + invalid_mac])
        # HALON_TODO: smap_get_int returns 55 for "55a", so this test fails. Commenting out for now.
        #set_open_vsw_lacp_config(s1, ['lacp-system-priority=55a'])
        short_sleep(2)
        sys = sw_get_intf_state(s1, intf, ['lacp_status:actor_system_id']).split(',')
        assert sys[1] == change_mac, \
                         "actor_system_id should be %s, is %s".format(change_mac, sys[1])
        #assert sys[0] == change_prio, "actor_system_priority should be %s, is %s".format(change_prio, sys[0])


        # Test port:lacp
        state_string = sw_get_intf_state(s1, intf, ['lacp_status:actor_state'])
        states = parse_lacp_state(state_string)

        info("Verify port:lacp.\n")
        assert states['Activ'] == '1', "Actor mode should be Active(1), is %s".format(states['Activ'])

        # Set lacp to "passive"
        set_port_parameter(s1, "lag0" , [ 'lacp=passive'])

        short_sleep(2)

        state_string = sw_get_intf_state(s1, intf, ['lacp_status:actor_state'])
        states = parse_lacp_state(state_string)
        assert states['Activ'] == '0', "Actor mode should be Passive(0), is %s".format(states['Activ'])

        # Set lacp to "off"
        set_port_parameter(s1, "lag0" , [ 'lacp=off'])

        short_sleep(6)

        state_string = sw_get_intf_state(s1, intf, ['lacp_status:actor_state'])
        if "no key actor_state" not in state_string:
            assert 1 == 0, "lacp status should be empty, but state is [%s]" % state_string

        # Set lacp to "active"
        set_port_parameter(s1, "lag0" , [ 'lacp=active'])

        short_sleep(6)

        state_string = sw_get_intf_state(s1, intf, ['lacp_status:actor_state'])
        states = parse_lacp_state(state_string)
        assert states['Activ'] == '1', "Actor mode should be Active(1), is %s".format(states['Activ'])

        # Test port:other_config:{lacp-system-id,lacp-system-priority,lacp-time}
        # HALON_TODO: Add tests for sys_id and sys_pri when code is added.

        # Test lacp-time

        info("Verify port:other_config:lacp-time.\n")

        # Verify default is "timeout", which is "fast"
        state_string = sw_get_intf_state(s1, intf, ['lacp_status:actor_state'])
        states = parse_lacp_state(state_string)
        assert states['TmOut'] == '1', "Timeout should be set(1), is %s".format(states['TmOut'])

        # Set lacp-time to "slow"
        set_port_parameter(s1, "lag0" , [ 'other_config:lacp-time=slow'])

        short_sleep(2)

        # Verify "timeout" is now "slow"
        state_string = sw_get_intf_state(s1, intf, ['lacp_status:actor_state'])
        states = parse_lacp_state(state_string)
        assert states['TmOut'] == '0', "Timeout should be not set(0), is %s".format(states['TmOut'])

        # Set lacp-time back to "fast"
        set_port_parameter(s1, "lag0" , [ 'other_config:lacp-time=fast'])

        short_sleep(2)

        # Verify "timeout" is now "fast"
        state_string = sw_get_intf_state(s1, intf, ['lacp_status:actor_state'])
        states = parse_lacp_state(state_string)
        assert states['TmOut'] == '1', "Timeout should be set(1), is %s".format(states['TmOut'])

        # OPS_TODO: lacp-aggregation-key is nonsensical in this context. The
        # implementation has been removed from lacpd.
        # Test interface:other_config:{lacp-port-id,lacp-port-priority}

        # Set port_id, port_priority, and aggregation-key
        set_intf_other_config(s1, intf, ['lacp-port-id=222', 'lacp-port-priority=123'])
        short_sleep()

        # Get the new values
        pri_info = sw_get_intf_state(s1, intf, ['lacp_status:actor_port_id']).split(',')

        info("Verify port-priority.\n")
        assert pri_info[0] == '123', "Port priority should be 123, is %s" % pri_info[0]

        info("Verify port-id.\n")
        assert pri_info[1] == '222', "Port id should be 222, is %s" % pri_info[1]

        # Set invalid port_id and port_priority
        set_intf_other_config(s1, intf, ['lacp-port-id=-1', 'lacp-port-priority=-1'])
        short_sleep()

        # Get the new values
        pri_info = sw_get_intf_state(s1, intf, ['lacp_status:actor_port_id']).split(',')

        info("Verify port-priority.\n")
        assert pri_info[0] == '123', "Port priority should be 123, is %s" % pri_info[0]

        info("Verify port-id.\n")
        assert pri_info[1] == '222', "Port id should be 222, is %s" % pri_info[1]

        set_intf_other_config(s1, intf, ['lacp-port-id=65536', 'lacp-port-priority=65536'])
        short_sleep(2)

        # Get the new values
        pri_info = sw_get_intf_state(s1, intf, ['lacp_status:actor_port_id']).split(',')

        info("Verify port-priority.\n")
        assert pri_info[0] == '123', "Port priority should be 123, is %s" % pri_info[0]

        info("Verify port-id.\n")
        assert pri_info[1] == '222', "Port id should be 222, is %s" % pri_info[1]


        info("Verify port lacp_status\n")
        # verify lag status
        verify_port_lacp_status(s1,
                "lag0",
                '{bond_speed=1000, bond_status=ok}',
                'Port lacp_status is expected to be bond_speed=1000, '
                'bond_status=ok')
        verify_port_lacp_status(s2,
                "lag0",
                '{bond_speed=1000, bond_status=ok}',
                'Port lacp_status is expected to be bond_speed=1000, '
                'bond_status=ok')

        info("Verify interface lacp_status\n")
        for intf in sw_1G_intf[0:2]:
            verify_intf_lacp_status(s1,
                    intf,
                    { "actor_state" : "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                      "Dist:1,Def:0,Exp:0",
                      "actor_system_id" : change_prio + "," + change_mac,
                      "partner_state" : "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                        "Dist:1,Def:0,Exp:0",
                      "partner_system_id" : base_prio + "," + base_mac[2]
                      },
                    "s1:" + intf)
            verify_intf_lacp_status(s2,
                    intf,
                    { "actor_state" : "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                      "Dist:1,Def:0,Exp:0",
                      "actor_system_id" : base_prio + "," + base_mac[2],
                      "partner_state" : "Activ:1,TmOut:1,Aggr:1,Sync:1,Col:1,"
                                        "Dist:1,Def:0,Exp:0",
                      "partner_system_id" : change_prio + "," + change_mac
                      },
                    "s2:" + intf)

        info("Verify dynamic update of system-level override\n")
        s1.cmd("ovs-vsctl set open_vswitch . lacp_config:lacp-system-id='" + alt_mac + "' lacp_config:lacp-system-priority=" + alt_prio)

        short_sleep(1)

        for intf in sw_1G_intf[0:2]:
            verify_intf_lacp_status(s1,
                    intf,
                    { "actor_state" : "Activ:1,TmOut:1,Aggr:1,Sync:0,Col:0,"
                                      "Dist:0,Def:0,Exp:0",
                      "actor_system_id" : alt_prio + "," + alt_mac,
                      "partner_state" : "Activ:1,TmOut:1,Aggr:1,Sync:0,Col:0,"
                                        "Dist:0,Def:0,Exp:0",
                      "partner_system_id" : base_prio + "," + base_mac[2]
                    },
                    "s1:" + intf)
            verify_intf_lacp_status(s2,
                    intf,
                    { "actor_state" : "Activ:1,TmOut:1,Aggr:1,Sync:0,Col:0,"
                                      "Dist:0,Def:0,Exp:0",
                      "actor_system_id" : base_prio + "," + base_mac[2],
                      "partner_state" : "Activ:1,TmOut:1,Aggr:1,Sync:0,Col:0,"
                                        "Dist:0,Def:0,Exp:0",
                      "partner_system_id" : alt_prio + "," + alt_mac
                    },
                    "s2:" + intf)

        info("Verify dynamic update of port-level override\n")
        sw_create_bond(s2, "lag1", sw_1G_intf[4:6], lacp_mode="active")

        short_sleep()

        for intf in sw_1G_intf[4:6]:
            sw_set_intf_user_config(s1, intf, ['admin=up'])
            sw_set_intf_user_config(s2, intf, ['admin=up'])

        short_sleep(2)

        for intf in sw_1G_intf[4:6]:
            verify_intf_lacp_status(s2,
                    intf,
                    { "actor_system_id" : base_prio + "," + base_mac[2] },
                    "s2:" + intf)

        info("Verify isolation of port-level override\n")
        # change just lag0
        s2.cmd("ovs-vsctl set port lag0 other_config:lacp-system-id='" + port_mac + "' other_config:lacp-system-priority=" + port_prio)

        short_sleep(1)

        # verify that lag0 changed
        for intf in sw_1G_intf[0:2]:
            verify_intf_lacp_status(s2,
                    intf,
                    { "actor_system_id" : port_prio + "," + port_mac },
                    "s2:" + intf)

        # verify that lag1 did not change
        for intf in sw_1G_intf[4:6]:
            verify_intf_lacp_status(s2,
                    intf,
                    { "actor_system_id" : base_prio + "," + base_mac[2] },
                    "s2:" + intf)

        info("Verify port-level override applied to newly added interfaces\n")
        # add an interface to lag0
        add_intf_to_bond(s2, "lag0", sw_1G_intf[2])
        sw_set_intf_user_config(s2, sw_1G_intf[2], ['admin=up'])

        short_sleep()

        # verify that new interface has picked up correct information
        verify_intf_lacp_status(s2,
                sw_1G_intf[2],
                { "actor_system_id" : port_prio + "," + port_mac },
                "s2:" + sw_1G_intf[2])

        info("Verify clearing port-level override\n")
        # clear port-level settings
        s2.cmd("ovs-vsctl remove port lag0 other_config lacp-system-id "
               "other_config lacp-system-priority")

        short_sleep()

        # verify that lag0 changed back to system values
        for intf in sw_1G_intf[0:3]:
            verify_intf_lacp_status(s2,
                    intf,
                    { "actor_system_id" : base_prio + "," + base_mac[2] },
                    "s2:" + intf)

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

    def test_lacpd_dynamic_lag_config(self):
        self.test.dynamic_lag_config()
