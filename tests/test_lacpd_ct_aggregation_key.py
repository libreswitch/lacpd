#!/usr/bin/python
#
# (c) Copyright 2016 Hewlett Packard Enterprise Development LP
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

import pytest
import time

from opsvsi.docker import *
from opsvsi.opsvsitest import *

OVS_VSCTL = "/usr/bin/ovs-vsctl "

sw_intf_start = 1
sw_intf_end = 4
sw_intf = [i for i in irange(sw_intf_start, sw_intf_end)]
sw_intf_str = [str(i) for i in irange(sw_intf_start, sw_intf_end)]

s1_crossed_over_intf = [5, 6, 7]
s1_crossed_over_intf_str = ["5", "6", "7"]
s2_crossed_over_intf = [6, 7, 5]
s2_crossed_over_intf_str = ["6", "7", "5"]

sw_all_intf_start = 1
sw_all_intf_end = 7
sw_all_intf = [str(i) for i in irange(sw_all_intf_start, sw_all_intf_end)]

sw_intf_not_connected = [8, 9, 10]


def timed_compare(data_func, params, compare_func,
                  expected_results, retries=20):
    while retries != 0:
        actual_results = data_func(params)
        result = compare_func(actual_results, expected_results, retries == 1)
        if result is True:
            return True, actual_results
        time.sleep(0.5)
        retries -= 1
    return False, actual_results


def sw_get_intf_state(params):
    c = OVS_VSCTL + "get interface " + str(params[1])
    for f in params[2]:
        c += " " + f
    out = params[0].ovscmd(c).splitlines()
    debug(out)
    return out


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


# Set pm_info for an Interface.
def sw_set_intf_pm_info(sw, interface, config):
    c = OVS_VSCTL + "set interface " + str(interface)
    for s in config:
        c += " pm_info:" + s
    debug(c)
    return sw.ovscmd(c)


def lacpd_switch_pre_setup(sw):

    for intf in irange(sw_all_intf_start, sw_all_intf_end):
        sw_set_intf_pm_info(sw, intf, ('connector="SFP_RJ45"',
                                       'connector_status=supported',
                                       'max_speed="1000"',
                                       'supported_speeds="1000"'))


# Set user_config for an Interface.
def sw_set_intf_user_config(sw, interface, config):
    c = OVS_VSCTL + "set interface " + str(interface)
    for s in config:
        c += " user_config:" + s
    debug(c)
    return sw.ovscmd(c)


# Create a bond/lag/trunk in the OVS-DB.
def sw_create_bond(s1, bond_name, intf_list, lacp_mode="off"):
    info("Creating LAG " + bond_name + " with interfaces: " +
         str(intf_list) + "\n")
    c = OVS_VSCTL + "add-bond bridge_normal " + bond_name +\
        " " + " ".join(map(str, intf_list))
    c += " -- set port " + bond_name + " lacp=" + lacp_mode
    debug(c)
    return s1.ovscmd(c)


def sw_rate_bond(s1, bond_name):
    info("LAG with lacp rate fast\n")
    c = OVS_VSCTL + "set port " + bond_name + " lacp rate fast"
    debug(c)
    return s1.ovscmd(c)


# Set interface:other_config parameter(s)
def set_intf_other_config(sw, intf, config):
    c = OVS_VSCTL + "set interface " + str(intf)
    for s in config:
        c += ' other_config:%s' % s
    debug(c)
    return sw.ovscmd(c)


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


def time_for_lacp_done(sw, intf, verify_values):
    request = []
    attrs = []
    for attr in verify_values:
        request.append('lacp_status:' + attr)
        attrs.append(attr)
    result = timed_compare(sw_get_intf_state,
                           (sw, intf, request),
                           verify_compare_complex, verify_values)
    field_vals = result[1]
    isdone = 0
    for i in range(0, len(attrs)):
        if field_vals[i] != verify_values[attrs[i]]:
            isdone = 1
    if isdone == 1:
        return False
    return True


# Creating lacp state values
# Default values are with a Lag forming correctly
def create_lacp_state(actor_sync="1", actor_col="1", actor_dist="1",
                      partner_sync="1", partner_col="1", partner_dist="1"):
    verify = {}

    a = "Activ:1,TmOut:0,Aggr:1,Sync:" + actor_sync + ",Col:" + actor_col +\
        ",Dist:" + actor_dist + ",Def:0,Exp:0"
    p = "Activ:1,TmOut:0,Aggr:1,Sync:" + partner_sync + ",Col:" +\
        partner_col + ",Dist:" + partner_dist + ",Def:0,Exp:0"

    verify["actor_state"] = a
    verify["partner_state"] = p
    return verify


# Create a topology with two switches, and 10 ports connected
# to each other.
class MyDualSwitchTopo(Topo):
    """Dual switch topology with ten ports connected to them
       S1[1-7]<--->[1-7]S2
    """

    def build(self, hsts=0, sws=2, **_opts):
        self.hsts = hsts
        self.sws = sws

        "Add the hosts to the topology."
        for h in irange(1, hsts):
            host = self.addHost('h%s' % h)
            assert host is not None

        "Add the switches to the topology."
        for s in irange(1, sws):
            switch = self.addSwitch('s%s' % s)
            assert switch is not None

        "Add the links between the switches."
        for intf in sw_intf:
            self.addLink('s1', 's2', port1=intf, port2=intf)

        for s1Intf, s2Intf in zip(s1_crossed_over_intf, s2_crossed_over_intf):
            self.addLink('s1', 's2', port1=s1Intf, port2=s2Intf)


class LacpdAggregationKeyTest(OpsVsiTest):

    def setupNet(self):

        # Create a topology with two VsiOpenSwitch switches,
        # and a host connected to each switch.
        host_opts = self.getHostOpts()
        switch_opts = self.getSwitchOpts()
        lacpd_topo = MyDualSwitchTopo(sws=2, hopts=host_opts,
                                      sopts=switch_opts)

        self.net = Mininet(lacpd_topo, switch=VsiOpenSwitch,
                           host=Host, link=OpsVsiLink,
                           controller=None, build=True)

    def pre_setup(self):
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]

        assert s1 is not None
        assert s2 is not None

        lacpd_switch_pre_setup(s1)
        lacpd_switch_pre_setup(s2)

    def enable_all_intf(self):
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]

        assert s1 is not None
        assert s2 is not None

        for intf in irange(sw_all_intf_start, sw_all_intf_end):
            sw_set_intf_user_config(s1, intf, ['admin=up'])
            sw_set_intf_user_config(s2, intf, ['admin=up'])

    def test_LAG_created_with_only_one_LAG(self):
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]

        assert s1 is not None
        assert s2 is not None

        # TOPOLOGY
        # -----------------------
        # Switch 1
        #   LAG 100:
        #       Interface 1
        #       Interface 2
        #
        # Switch 2
        #   LAG 100:
        #       Interface 1
        #       Interface 2

        self.pre_setup()

        self.enable_all_intf()

        sw_create_bond(s1, "lag100", sw_intf[0:2], lacp_mode="active")
        sw_rate_bond(s1, "lag100")
        sw_create_bond(s2, "lag100", sw_intf[0:2], lacp_mode="active")
        sw_rate_bond(s2, "lag100")

        for intf in sw_intf[0:2]:
            set_intf_other_config(s1, intf, ['lacp-aggregation-key=100'])
            set_intf_other_config(s2, intf, ['lacp-aggregation-key=100'])

        time.sleep(10)
        for i in range(0, 5):
            time.sleep(10)
            if time_for_lacp_done(s1,
                                  sw_intf[1],
                                  create_lacp_state()):
                break
        for i in range(0, 5):
            time.sleep(10)
            if time_for_lacp_done(s2,
                                  sw_intf[1],
                                  create_lacp_state()):
                break

        for intf in sw_intf_str[0:2]:
            verify_intf_lacp_status(s1,
                                    intf,
                                    create_lacp_state(),
                                    "s1:" + intf)

            verify_intf_lacp_status(s2,
                                    intf,
                                    create_lacp_state(),
                                    "s2:" + intf)

        s1.ovscmd("ovs-vsctl del-port lag100")

        # TOPOLOGY
        # -----------------------
        # Switch 1
        #   LAG 200:
        #       Interface 1
        #       Interface 8
        #   LAG 100:
        #       Interface 2
        #       Interface 9
        #
        # Switch 2
        #   LAG 100:
        #       Interface 1
        #       Interface 2
        sw_create_bond(s1, "lag200", [sw_intf[0], sw_intf_not_connected[0]],
                       lacp_mode="active")
        sw_rate_bond(s1, "lag200")
        set_intf_other_config(s1, sw_intf[0], ['lacp-aggregation-key=200'])
        set_intf_other_config(s1, sw_intf_not_connected[0],
                              ['lacp-aggregation-key=200'])

        sw_create_bond(s1, "lag100", [sw_intf[1], sw_intf_not_connected[1]],
                       lacp_mode="active")
        sw_rate_bond(s1, "lag100")
        set_intf_other_config(s1, sw_intf[1], ['lacp-aggregation-key=100'])
        set_intf_other_config(s1, sw_intf_not_connected[1],
                              ['lacp-aggregation-key=100'])

        time.sleep(10)
        for i in range(0, 5):
            time.sleep(10)
            if time_for_lacp_done(s1,
                                  sw_intf[0],
                                  create_lacp_state(actor_col="0",
                                                    actor_dist="0",
                                                    partner_sync="0",
                                                    partner_col="0",
                                                    partner_dist="0")):
                break
        for i in range(0, 5):
            time.sleep(10)
            if time_for_lacp_done(s2,
                                  sw_intf[1],
                                  create_lacp_state()):
                break


        verify_intf_lacp_status(s1,
                                sw_intf[0],
                                create_lacp_state(actor_col="0",
                                                  actor_dist="0",
                                                  partner_sync="0",
                                                  partner_col="0",
                                                  partner_dist="0"),
                                "s1:" + sw_intf_str[0])

        verify_intf_lacp_status(s2,
                                sw_intf[0],
                                create_lacp_state(actor_sync="0",
                                                  actor_col="0",
                                                  actor_dist="0",
                                                  partner_col="0",
                                                  partner_dist="0"),
                                "s2:" + sw_intf_str[0])

        # Cleaning configuration
        s1.ovscmd("ovs-vsctl del-port lag100")
        s1.ovscmd("ovs-vsctl del-port lag200")
        s2.ovscmd("ovs-vsctl del-port lag100")

    def test_LAG_with_cross_links(self):
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]

        assert s1 is not None
        assert s2 is not None

        self.pre_setup()
        self.enable_all_intf()

        # TOPOLOGY
        # -----------------------
        # Switch 1
        #   LAG 50:
        #       Interface 1
        #       Interface 2
        #       Interface 3
        #       Interface 4
        #
        # Switch 2
        #   LAG 50:
        #       Interface 1
        #       Interface 2
        #   LAG 60:
        #       Interface 3
        #       Interface 4

        sw_create_bond(s1, "lag50", sw_intf[0:4], lacp_mode="active")
        sw_rate_bond(s1, "lag50")
        sw_create_bond(s2, "lag50", sw_intf[0:2], lacp_mode="active")
        sw_rate_bond(s2, "lag50")
        sw_create_bond(s2, "lag60", sw_intf[2:4], lacp_mode="active")
        sw_rate_bond(s2, "lag60")

        for intf in sw_intf[0:4]:
            set_intf_other_config(s1, intf, ['lacp-aggregation-key=50'])

        for intf in sw_intf[0:2]:
            set_intf_other_config(s2, intf, ['lacp-aggregation-key=50'])

        for intf in sw_intf[2:4]:
            set_intf_other_config(s2, intf, ['lacp-aggregation-key=60'])

        time.sleep(10)
        for i in range(0, 5):
            time.sleep(10)
            if time_for_lacp_done(s1,
                                  sw_intf[1],
                                  create_lacp_state()):
                break
        for i in range(0, 5):
            time.sleep(10)
            if time_for_lacp_done(s2,
                                  sw_intf[1],
                                  create_lacp_state()):
                break

        for intf in sw_intf_str[0:2]:
            verify_intf_lacp_status(s1,
                                    intf,
                                    create_lacp_state(),
                                    "s1:" + intf)
            verify_intf_lacp_status(s2,
                                    intf,
                                    create_lacp_state(),
                                    "s2:" + intf)

        for intf in sw_intf_str[2:4]:
            verify_intf_lacp_status(s1,
                                    intf,
                                    create_lacp_state(actor_sync="0",
                                                      actor_col="0",
                                                      actor_dist="0",
                                                      partner_col="0",
                                                      partner_dist="0"),
                                    "s1:" + intf)
            verify_intf_lacp_status(s2,
                                    intf,
                                    create_lacp_state(actor_col="0",
                                                      actor_dist="0",
                                                      partner_sync="0",
                                                      partner_col="0",
                                                      partner_dist="0"),
                                    "s2:" + intf)

        # Cleaning the configuration
        s1.ovscmd("ovs-vsctl del-port lag50")
        s2.ovscmd("ovs-vsctl del-port lag50")
        s2.ovscmd("ovs-vsctl del-port lag60")

    def test_LAG_with_differet_keys(self):

        s1 = self.net.switches[0]
        s2 = self.net.switches[1]

        assert s1 is not None
        assert s2 is not None

        self.pre_setup()
        self.enable_all_intf()

        # TOPOLOGY
        # -----------------------
        # Switch 1
        #   LAG 150:
        #       Interface 5
        #       Interface 1
        #   LAG 250:
        #       Interface 6
        #       Interface 2
        #   LAG 350:
        #       Interface 7
        #       Interface 3
        #
        # Switch 2
        #   LAG 150:
        #       Interface 6
        #       Interface 1
        #   LAG 250:
        #       Interface 7
        #       Interface 2
        #   LAG 350:
        #       Interface 5
        #       Interface 3

        sw_create_bond(s1, "lag150", [5, 1], lacp_mode="active")
        sw_rate_bond(s1, "lag150")
        sw_create_bond(s1, "lag250", [6, 2], lacp_mode="active")
        sw_rate_bond(s1, "lag250")
        sw_create_bond(s1, "lag350", [7, 3], lacp_mode="active")
        sw_rate_bond(s1, "lag350")

        sw_create_bond(s2, "lag150", [6, 1], lacp_mode="active")
        sw_rate_bond(s2, "lag150")
        sw_create_bond(s2, "lag250", [7, 2], lacp_mode="active")
        sw_rate_bond(s2, "lag250")
        sw_create_bond(s2, "lag350", [5, 3], lacp_mode="active")
        sw_rate_bond(s2, "lag350")

        set_intf_other_config(s1, 5, ['lacp-aggregation-key=150'])
        set_intf_other_config(s1, 1, ['lacp-aggregation-key=150'])
        set_intf_other_config(s2, 6, ['lacp-aggregation-key=150'])
        set_intf_other_config(s2, 1, ['lacp-aggregation-key=150'])
        set_intf_other_config(s1, 6, ['lacp-aggregation-key=250'])
        set_intf_other_config(s1, 2, ['lacp-aggregation-key=250'])
        set_intf_other_config(s2, 7, ['lacp-aggregation-key=250'])
        set_intf_other_config(s2, 2, ['lacp-aggregation-key=250'])
        set_intf_other_config(s1, 7, ['lacp-aggregation-key=350'])
        set_intf_other_config(s1, 3, ['lacp-aggregation-key=350'])
        set_intf_other_config(s2, 5, ['lacp-aggregation-key=350'])
        set_intf_other_config(s2, 3, ['lacp-aggregation-key=350'])

        time.sleep(10)
        for i in range(0, 5):
            time.sleep(10)
            if time_for_lacp_done(s1,
                                  sw_intf[0],
                                  create_lacp_state()):
                break
        for i in range(0, 5):
            time.sleep(10)
            if time_for_lacp_done(s2,
                                  sw_intf[0],
                                  create_lacp_state()):
                break

        for intf in sw_all_intf[0:3]:
            verify_intf_lacp_status(s1,
                                    intf,
                                    create_lacp_state(),
                                    "s1:" + intf)

            verify_intf_lacp_status(s2,
                                    intf,
                                    create_lacp_state(),
                                    "s2:" + intf)

        for intf in s1_crossed_over_intf_str[0:3]:
            verify_intf_lacp_status(s1,
                                    intf,
                                    create_lacp_state(),
                                    "s1:" + intf)

            verify_intf_lacp_status(s2,
                                    intf,
                                    create_lacp_state(),
                                    "s2:" + intf)

        s1.ovscmd("ovs-vsctl del-port lag150")
        s1.ovscmd("ovs-vsctl del-port lag250")
        s1.ovscmd("ovs-vsctl del-port lag350")

        s2.ovscmd("ovs-vsctl del-port lag150")
        s2.ovscmd("ovs-vsctl del-port lag250")
        s2.ovscmd("ovs-vsctl del-port lag350")

    def test_LAG_with_system_priority_and_port_priority(self):
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]

        assert s1 is not None
        assert s2 is not None

        # TOPOLOGY
        # -----------------------
        # Switch 1
        #   LAG 100:
        #       Interface 1
        #       Interface 2
        #
        # Switch 2
        #   LAG 200:
        #       Interface 1
        #       Interface 2
        self.pre_setup()
        self.enable_all_intf()

        sw_create_bond(s1, "lag100", sw_intf[0:2], lacp_mode="active")
        sw_rate_bond(s1, "lag100")
        sw_create_bond(s2, "lag200", sw_intf[0:2], lacp_mode="active")
        sw_rate_bond(s2, "lag200")

        for intf in sw_intf[0:2]:
            set_intf_other_config(s1, intf, ['lacp-aggregation-key=100'])
            set_intf_other_config(s2, intf, ['lacp-aggregation-key=200'])
        time.sleep(10)
        for i in range(0, 5):
            time.sleep(10)
            if time_for_lacp_done(s1,
                                  sw_intf[0],
                                  create_lacp_state()):
                break
        for i in range(0, 5):
            time.sleep(10)
            if time_for_lacp_done(s2,
                                  sw_intf[0],
                                  create_lacp_state()):
                break

        for intf in sw_intf_str[0:2]:
            verify_intf_lacp_status(s1,
                                    intf,
                                    create_lacp_state(),
                                    "s1:" + intf)

            verify_intf_lacp_status(s2,
                                    intf,
                                    create_lacp_state(),
                                    "s2:" + intf)

        s1.ovscmd("ovs-vsctl del-port lag100")
        s2.ovscmd("ovs-vsctl del-port lag200")

    def test_case5(self):
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]

        assert s1 is not None
        assert s2 is not None

        self.pre_setup()
        self.enable_all_intf()

        # TOPOLOGY
        # -----------------------
        # Switch 1 (System Priority 200)
        #   LAG 50:
        #       Interface 1 (Port Priority = 100)
        #       Interface 2 (Port Priority = 100)
        #       Interface 3 (Port Priority = 100)
        #       Interface 4 (Port Priority = 100)
        #
        # Switch 2 (System Priority 1)
        #   LAG 50:
        #       Interface 1 (Port Priority = 100)
        #       Interface 2 (Port Priority = 100)
        #   LAG 60:
        #       Interface 3 (Port Priority = 1)
        #       Interface 4 (Port Priority = 1)

        s1.ovscmd("ovs-vsctl set system . "
                  " lacp_config:lacp-system-priority=200")

        s2.ovscmd("ovs-vsctl set system . "
                  " lacp_config:lacp-system-priority=1")

        sw_create_bond(s1, "lag50", sw_intf[0:4], lacp_mode="active")
        sw_create_bond(s2, "lag50", sw_intf[0:2], lacp_mode="active")
        sw_create_bond(s2, "lag60", sw_intf[2:4], lacp_mode="active")

        for intf in sw_intf[0:4]:
            set_intf_other_config(s1, intf, ['lacp-aggregation-key=50'])
            set_intf_other_config(s1, intf, ['lacp-port-priority=100'])

        for intf in sw_intf[0:2]:
            set_intf_other_config(s2, intf, ['lacp-aggregation-key=50'])
            set_intf_other_config(s2, intf, ['lacp-port-priority=100'])

        for intf in sw_intf[2:4]:
            set_intf_other_config(s2, intf, ['lacp-aggregation-key=60'])
            set_intf_other_config(s2, intf, ['lacp-port-priority=1'])

        time.sleep(10)
        for i in range(0, 5):
            time.sleep(10)
            if time_for_lacp_done(s1,
                                  sw_intf[0],
                                  create_lacp_state(actor_sync="0",
                                                    actor_col="0",
                                                    actor_dist="0",
                                                    partner_col="0",
                                                    partner_dist="0")):
                break
        for i in range(0, 5):
            time.sleep(10)
            if time_for_lacp_done(s2,
                                  sw_intf[0],
                                  create_lacp_state(actor_sync="0",
                                                    actor_col="0",
                                                    actor_dist="0",
                                                    partner_col="0",
                                                    partner_dist="0")):
                break

        for intf in sw_intf_str[0:2]:
            verify_intf_lacp_status(s1,
                                    intf,
                                    create_lacp_state(actor_sync="0",
                                                      actor_col="0",
                                                      actor_dist="0",
                                                      partner_col="0",
                                                      partner_dist="0"),
                                    "s1:" + intf)

            verify_intf_lacp_status(s2,
                                    intf,
                                    create_lacp_state(actor_col="0",
                                                      actor_dist="0",
                                                      partner_sync="0",
                                                      partner_col="0",
                                                      partner_dist="0"),
                                    "s2:" + intf)

        for intf in sw_intf_str[2:4]:
            verify_intf_lacp_status(s1,
                                    intf,
                                    create_lacp_state(),
                                    "s1:" + intf)

            verify_intf_lacp_status(s2,
                                    intf,
                                    create_lacp_state(),
                                    "s2:" + intf)

        s1.ovscmd("ovs-vsctl del-port lag50")
        s2.ovscmd("ovs-vsctl del-port lag50")
        s2.ovscmd("ovs-vsctl del-port lag60")


@pytest.mark.skipif(True, reason="Skipping due to instability")
class TestLacpAggrKey:

    def setup(self):
        pass

    def teardown(self):
        pass

    def setup_class(cls):
        TestLacpAggrKey.test = LacpdAggregationKeyTest()

        TestLacpAggrKey.test.net.switches[0].cmd("/bin/systemctl stop pmd")
        TestLacpAggrKey.test.net.switches[1].cmd("/bin/systemctl stop pmd")

    def test_LAG_created_with_only_one_LAG(self):
        self.test.test_LAG_created_with_only_one_LAG()

    @pytest.mark.skipif(True, reason="Skipping due to instability")
    def test_LAG_with_cross_links(self):
        self.test.test_LAG_with_cross_links()

    def test_LAG_with_differet_keys(self):
        self.test.test_LAG_with_differet_keys()

    def test_LAG_with_system_priority_and_port_priority(self):
        self.test.test_LAG_with_system_priority_and_port_priority()

    # This test case is commented because it not working
    # with the system priority and the port priority.
    # There is defect in Taiga to track this
    # def test_case5(self):
    #    self.test.test_case5()
    def teardown_class(cls):
        TestLacpAggrKey.test.net.switches[0].cmd("/bin/systemctl start pmd")
        TestLacpAggrKey.test.net.switches[1].cmd("/bin/systemctl start pmd")

        # ops-lacpd is stopped so that it produces the gcov coverage data
        #
        # Daemons from both switches will dump the coverage data to the
        # same file but the data write is done on daemon exit only.
        # The systemctl command waits until the process exits to return the
        # prompt and the object.cmd() function waits for the command to return,
        # therefore it is safe to stop the ops-lacpd daemons sequentially
        # This ensures that data from both processes is captured.
        TestLacpAggrKey.test.net.switches[0].cmd("/bin/systemctl stop ops-lacpd")
        TestLacpAggrKey.test.net.switches[1].cmd("/bin/systemctl stop ops-lacpd")

        TestLacpAggrKey.test.net.stop()

    def setup_method(self, method):
        pass

    def teardown_method(self, method):
        pass

    def __del__(self):
        del self.test
