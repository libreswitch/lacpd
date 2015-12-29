#!/usr/bin/python
#
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
from lib_test import verify_intf_field_absent
from lib_test import remove_intf_list_from_bond


# Interfaces from 1-10 are 1G ports.
sw_to_host1 = 21
sw_to_host2 = 22

sw_1G_intf_start = 1
sw_1G_intf_end = 2
n_1G_links = 2
sw_1G_intf = [str(i) for i in irange(sw_1G_intf_start, sw_1G_intf_end)]


def lacpd_switch_pre_setup(sw):

    for intf in irange(sw_1G_intf_start, sw_1G_intf_end):
        sw_set_intf_pm_info(sw, intf, ('connector="SFP_RJ45"',
                                       'connector_status=supported',
                                       'max_speed="1000"',
                                       'supported_speeds="1000"'))


# Create a topology with two switches, and 2 ports connected
# to each other.
class myDualSwitchTopo(Topo):
    """Dual switch topology with ten ports connected to them
       H1[h1-eth0]<--->S1[1-2]<--->[1-2]S2<--->[h2-eth0]H2
    """

    def build(self, hsts=2, sws=2, n_links=2, **_opts):
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


class lacp_fallbackTest(OpsVsiTest):

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

        for intf in irange(sw_1G_intf_start, sw_1G_intf_end):
            sw_clear_user_config(s1, intf)
            sw_clear_user_config(s2, intf)
            sw_set_intf_pm_info(s1, intf, ('connector=absent',
                                           'connector_status=unsupported'))
            sw_set_intf_pm_info(s2, intf, ('connector=absent',
                                           'connector_status=unsupported'))

    def dynamic_lag_config(self):

        info("\n============= lacpd user config"
             " (dynamic LAG) tests =============\n")
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]

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

        # Test lacp down

        info("\n############### Test Case 1 - Verify lacp negotiation"
             " fails ###############\n")
        info("#######################################################"
             "####################\n")

        # Create two dynamic LAG with two ports each.
        sw_create_bond(s1, "lag0", sw_1G_intf[0:2], lacp_mode="active")
        sw_create_bond(s2, "lag0", sw_1G_intf[0:2], lacp_mode="active")

        set_port_parameter(s1, "lag0", ['other_config:lacp-time=fast'])
        set_port_parameter(s2, "lag0", ['other_config:lacp-time=fast'])

        # Enable both the interfaces.
        for intf in sw_1G_intf[0:2]:
            sw_set_intf_user_config(s1, intf, ['admin=up'])
            sw_set_intf_user_config(s2, intf, ['admin=up'])

        # Verify that all the interfaces are linked up
        info("\n### Verify that all the interfaces are linked up ###\n")
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
                                          "active mode on switch2")

        # Set lacp fail
        info("lacp fallback true \n")
        set_port_parameter(s1, "lag0", ['other_config:lacp-fallback-ab=true'])
        set_port_parameter(s2, "lag0", ['other_config:lacp-fallback-ab=true'])

        # Disable Interfaces from bond in switch 2.
        # Verify that hw_bond_config:{rx_enabled="false", tx_enabled="false"}
        # Set lacp to "off"
        set_port_parameter(s2, "lag0", ['lacp=off'])

        info("Verify that interfaces were disabled from LAG"
             " when RX/TX are set to false.\n")

        # interfaces must be down due to partner missing
        info("\n### interfaces must be down due to partner missing ###\n")
        for intf in sw_1G_intf[0:2]:
            if intf == "1":
                info("Verifying interface 1 is up \n")
                verify_intf_in_bond(s1, intf, "One Interface must be up")
            if intf != "1":
                info("Verifying rest of interfaces are down \n")
                verify_intf_not_in_bond(s1, intf,
                                        "Rest of interfaces must be down")

        # Add the Interface back into the LAG/Trunk.
        # Set lacp to "active"
        info("\n############# LACP back to active \n")
        set_port_parameter(s2, "lag0", ['lacp=active'])

        info("Verify interface lacp_status\n")
        # interfaces must be up when lag is back to active
        for intf in sw_1G_intf[0:2]:
            info("Verifying all interfaces are up \n")
            verify_intf_in_bond(s1, intf, "All Interfaces must be up")

        info("lacp fallback false \n\n")
        set_port_parameter(s1, "lag0", ['other_config:lacp-fallback-ab=false'])
        set_port_parameter(s2, "lag0", ['other_config:lacp-fallback-ab=false'])

        #interfaces back to out of bond to lag failure

        # Set lacp to "off"
        set_port_parameter(s2, "lag0", ['lacp=off'])

        time.sleep(4)

        info("############# Verify interface lacp_status ############# \n")
        info("##########  LACP failed and fallback = false ########### \n")
        # interfaces must be down due to partner missing.
        for intf in sw_1G_intf[0:2]:
            info("Verifying all interfaces are down \n")
            verify_intf_not_in_bond(s1, intf,
                                    "All interfaces must be down")


class Test_lacpd:

    def setup(self):
        pass

    def teardown(self):
        pass

    def setup_class(cls):
        # Create the Mininet topology based on Mininet.
        Test_lacpd.test = lacp_fallbackTest()

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

    # Set fallback(lacp) daemon tests.

    def test_lacpd_dynamic_lag_config(self):
        self.test.dynamic_lag_config()
      # CLI(self.test.net)
