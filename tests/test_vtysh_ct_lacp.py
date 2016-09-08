#!/usr/bin/python

# (c) Copyright 2015-2016 Hewlett Packard Enterprise Development LP
#
# GNU Zebra is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2, or (at your option) any
# later version.
#
# GNU Zebra is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Zebra; see the file COPYING.  If not, write to the Free
# Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.

from opsvsi.docker import *
from opsvsi.opsvsitest import *
import re
import pytest


class LACPCliTest(OpsVsiTest):

    def setupNet(self):

        # if you override this function, make sure to
        # either pass getNodeOpts() into hopts/sopts of the topology that
        # you build or into addHost/addSwitch calls

        host_opts = self.getHostOpts()
        switch_opts = self.getSwitchOpts()
        infra_topo = SingleSwitchTopo(k=0, hopts=host_opts, sopts=switch_opts)
        self.net = Mininet(infra_topo, switch=VsiOpenSwitch,
                           host=Host, link=OpsVsiLink,
                           controller=None, build=True)

    def createLagPort(self):
        info('''
########## Test to create LAG Port ##########
''')
        lag_port_found = False
        s1 = self.net.switches[0]
        s1.cmdCLI('conf t')
        s1.cmdCLI('interface lag 1')
        out = s1.cmd('ovs-vsctl list port')
        lines = out.split('\n')
        for line in lines:
            if '"lag1"' in line:
                lag_port_found = True
        assert (lag_port_found is True), \
            'Test to create LAG Port - FAILED!!'
        return True

    def showLacpAggregates(self):
        info('''
########## Test Show lacp aggregates command ##########
''')
        lag_port_found_in_cmd = False
        s1 = self.net.switches[0]
        s1.cmdCLI('conf t')
        s1.cmdCLI('interface lag 2')
        out = s1.cmdCLI('do show lacp aggregates')
        lines = out.split('\n')
        for line in lines:
            if 'lag2' in line:
                lag_port_found_in_cmd = True
        assert (lag_port_found_in_cmd is True), \
            'Test Show lacp aggregates command - FAILED!!'
        return True

    def deleteLagPort(self):
        info('''
########## Test to delete LAG port ##########
''')
        lag_port_found = True
        s1 = self.net.switches[0]
        s1.cmdCLI('conf t')
        s1.cmdCLI('interface lag 3')
        s1.cmdCLI('exit')
        s1.cmdCLI('interface 10')
        s1.cmdCLI('lag 3')
        s1.cmdCLI('exit')
        s1.cmdCLI('no interface lag 3')
        out = s1.cmd('ovs-vsctl list port')
        lines = out.split('\n')
        for line in lines:
            if '"lag3"' in line:
                lag_port_found = False
        assert (lag_port_found is True), \
            'Test to delete LAG port - FAILED!'
        out = s1.cmd('do show running')
        lag_port_found = True
        lines = out.split('\n')
        for line in lines:
            if '"lag3"' in line:
                lag_port_found = False
        assert (lag_port_found is True), \
            'Test to delete LAG port - FAILED!'
        return True

    def addInterfacesToLags(self):
        info('''
########## Test to add interfaces to LAG ports ##########
''')
        interface_found_in_lag = False
        s1 = self.net.switches[0]
        s1.cmdCLI('conf t')
        s1.cmdCLI('interface 1')
        s1.cmdCLI('lag 1')
        s1.cmdCLI('interface 2')
        s1.cmdCLI('lag 1')
        s1.cmdCLI('interface 3')
        s1.cmdCLI('lag 2')
        s1.cmdCLI('interface 4')
        s1.cmdCLI('lag 2')
        out = s1.cmdCLI('do show lacp aggregates')
        lines = out.split('\n')
        for line in lines:
            if 'Aggregated-interfaces' in line and '3' in line and '4' in line:
                interface_found_in_lag = True

        assert (interface_found_in_lag is True), \
            'Test to add interfaces to LAG ports - FAILED!'
        return True

    def globalLacpCommands(self):
        info('''
########## Test global LACP commands ##########
''')
        global_lacp_cmd_found = False
        s1 = self.net.switches[0]
        s1.cmdCLI('conf t')
        s1.cmdCLI('lacp system-priority 999')
        out = s1.cmd('ovs-vsctl list system')
        lines = out.split('\n')
        for line in lines:
            if 'lacp-system-priority="999"' in line:
                global_lacp_cmd_found = True
        assert (global_lacp_cmd_found is True), \
            'Test global LACP commands - FAILED!'
        return True

    def lag_hash_LoadBalancing(self):
        info('''
########## Test LAG Load balancing for L2, L3 and L4 ##########
''')
        s1 = self.net.switches[0]
        s1.cmdCLI('configure terminal')
        s1.cmdCLI('interface lag 1')
        s1.cmdCLI('hash l2-src-dst')
        s1.cmdCLI('end')
        success = False
        out = s1.cmd('ovs-vsctl list port')
        lines = out.split('\n')
        for line in lines:
            if 'bond_mode="l2-src-dst-hash"' in line:
                success = True
                break
        assert success, \
            'Test (ovs-ctl) LAG Load balancing for L2 - FAILED!'

        success = False
        out = s1.cmdCLI('show running-config')
        lines = out.split('\n')
        for line in lines:
            if 'hash l2-src-dst' in line:
                success = True
                break
        assert success, \
            'Test (show running-config) LAG Load balancing for L2 - FAILED!'

        success = False
        out = s1.cmdCLI('show running interface lag1')
        lines = out.split('\n')
        for line in lines:
            if 'hash l2-src-dst' in line:
                success = True
                break
        assert success, \
            'Test (show running interface) LAG Load balancing for L2'\
            ' - FAILED!'

        success = False
        out = s1.cmdCLI('show lacp aggregates')
        lines = out.split('\n')
        for line in lines:
            if 'Hash                  : l2-src-dst' in line:
                success = True
                break
        assert success, \
            'Test (show lacp aggregates) LAG Load balancing for L2 - FAILED!'

        success = True
        s1.cmdCLI('configure terminal')
        s1.cmdCLI('interface lag 1')
        s1.cmdCLI('hash l3-src-dst')
        s1.cmdCLI('end')
        out = s1.cmd('ovs-vsctl list port')
        lines = out.split('\n')
        for line in lines:
            if 'bond_mode=' in line:
                success = False
                break
        assert success, \
            'Test (ovs-ctl) LAG Load balancing for L3 - FAILED!'

        success = True
        out = s1.cmdCLI('show running-config')
        lines = out.split('\n')
        for line in lines:
            if 'hash l3-src-dst' in line:
                success = False
                break
        assert success, \
            'Test (show running-config) LAG Load balancing for L3 - FAILED!'

        success = True
        out = s1.cmdCLI('show running interface lag1')
        lines = out.split('\n')
        for line in lines:
            if 'hash l3-src-dst' in line:
                success = False
                break
        assert success, \
            'Test (show running interface) LAG Load balancing for L3'\
            ' - FAILED!'

        success = False
        out = s1.cmdCLI('show lacp aggregates')
        lines = out.split('\n')
        for line in lines:
            if 'Hash                  : l3-src-dst' in line:
                success = True
                break
        assert success, \
            'Test (show lacp aggregates) LAG Load balancing for L3 - FAILED!'

        success = False
        s1.cmdCLI('configure terminal')
        s1.cmdCLI('interface lag 1')
        s1.cmdCLI('hash l4-src-dst')
        s1.cmdCLI('end')
        out = s1.cmd('ovs-vsctl list port')
        lines = out.split('\n')
        for line in lines:
            if 'bond_mode="l4-src-dst-hash"' in line:
                success = True
                break
        assert success, \
            'Test (ovs-ctl) LAG Load balancing for L4 - FAILED!'

        success = False
        out = s1.cmdCLI('show running-config')
        lines = out.split('\n')
        for line in lines:
            if 'hash l4-src-dst' in line:
                success = True
                break
        assert success, \
            'Test (show running-config) LAG Load balancing for L4 - FAILED!'

        success = False
        out = s1.cmdCLI('show running interface lag1')
        lines = out.split('\n')
        for line in lines:
            if 'hash l4-src-dst' in line:
                success = True
                break
        assert success, \
            'Test (show running interface) LAG Load balancing for L4'\
            ' - FAILED!'

        success = False
        out = s1.cmdCLI('show lacp aggregates')
        lines = out.split('\n')
        for line in lines:
            if 'Hash                  : l4-src-dst' in line:
                success = True
                break
        assert success, \
            'Test (show lacp aggregates) LAG Load balancing for L4 - FAILED!'
        return True

    def lagContextCommands(self):
        info('''
########## Test LAG context commands ##########
''')
        lag_context_cmds_found = True
        s1 = self.net.switches[0]
        s1.cmdCLI('conf t')
        s1.cmdCLI('interface lag 1')
        s1.cmdCLI('lacp mode active')
        s1.cmdCLI('lacp fallback')
        s1.cmdCLI('hash l2-src-dst')
        s1.cmdCLI('lacp rate fast')
        success = 0
        out = s1.cmd('ovs-vsctl list port')
        lines = out.split('\n')
        for line in lines:
            if re.search('lacp * : active', line) is not None:
                success += 1
                break
        out = s1.cmd('ovs-vsctl list port')
        lines = out.split('\n')
        for line in lines:
            if 'lacp-fallback-ab="true"' in line:
                success += 1
                break
        out = s1.cmd('ovs-vsctl list port')
        lines = out.split('\n')
        for line in lines:
            if 'bond_mode="l2-src-dst-hash"' in line:
                success += 1
                break
        out = s1.cmd('ovs-vsctl list port')
        lines = out.split('\n')
        for line in lines:
            if 'lacp-time=fast' in line:
                success += 1
                break
        assert success == 4,\
            'Test LAG context commands - FAILED!'

        success = 0

        # Test "no" forms of commands

        s1.cmdCLI('no lacp mode active')
        out = s1.cmd('ovs-vsctl list port')
        lines = out.split('\n')
        for line in lines:
            if 'lacp=active' in line:
                success += 1
                break
        s1.cmdCLI('no lacp fallback')
        out = s1.cmd('ovs-vsctl list port')
        lines = out.split('\n')
        for line in lines:
            if 'lacp-fallback-ab' in line:
                success += 1
                break
        s1.cmdCLI('no lacp rate fast')
        out = s1.cmd('ovs-vsctl list port')
        lines = out.split('\n')
        for line in lines:
            if 'lacp-time' in line:
                success += 1
                break
        assert success == 0,\
            'Test LAG context commands - FAILED!'
        return True

    def lacpPortId(self):
        info('''
########## Test lacp port-id commands ##########
''')

        test_interface = 1
        test_port_id = 999

        test_commands = ["no lacp port-id",
                         "no lacp port-id %s" % test_port_id]

        # Configure port-id
        s1 = self.net.switches[0]
        s1.cmdCLI('configure terminal')
        s1.cmdCLI('interface %s' % test_interface)

        for command in test_commands:
            # info("########## Setting port-id ##########\n")
            s1.cmdCLI('lacp port-id %s' % test_port_id)

            # Verify if lacp port-id was modified within DB
            success = False
            out = s1.cmd('ovs-vsctl list interface %s' % test_interface)
            lines = out.split('\n')
            for line in lines:
                if 'other_config' in line \
                        and 'lacp-port-id="%s"' % test_port_id in line:
                    success = True
                    break

            assert success, \
                'Test interface set port-priority command - FAILED!'

            # info("########## Executing command: %s ##########\n" % command)
            s1.cmdCLI(command)

            # Validate if lacp port-id was removed from DB
            success = False
            out = s1.cmd('ovs-vsctl list interface %s' % test_interface)
            lines = out.split('\n')
            for line in lines:
                if 'other_config' in line \
                        and 'lacp-port-id="%s"' % test_port_id not in line:
                    success = True
                    break

            assert success, \
                'Test interface set port-priority command - FAILED!'

        # Check that command fails when port-id does not exist
        out = s1.cmdCLI(test_commands[0])
        assert "Command failed" not in out, \
            "Test interface remove port-priority command - FAILED!"

        out = s1.cmdCLI(test_commands[1])
        assert "Command failed" in out, \
            "Test interface remove port-priority command - FAILED!"

        return True

    def lacpPortPriority(self):
        info('''
########## Test interface set port-priority command ##########
''')

        test_interface = 1
        test_port_priority = 111
        test_commands = ["no lacp port-priority",
                         "no lacp port-priority %s" % test_port_priority]

        # Configure port-priority
        s1 = self.net.switches[0]
        s1.cmdCLI('configure terminal')
        s1.cmdCLI('interface 1')

        for command in test_commands:
            # info("########## Setting port-priority ##########\n")
            s1.cmdCLI('lacp port-priority %s' % test_port_priority)

            success = False
            out = s1.cmd('ovs-vsctl list interface %s' % test_interface)
            lines = out.split('\n')
            for line in lines:
                if 'other_config' in line \
                        and 'lacp-port-priority="%s"' % test_port_priority \
                        in line:
                    success = True
                    break

            assert success, \
                "Test interface set port-priority command - FAILED!"

            # info("########## Executing command: %s ##########\n" % command)
            s1.cmdCLI(command)

            success = False
            out = s1.cmd('ovs-vsctl list interface %s' % test_interface)
            lines = out.split('\n')
            for line in lines:
                if 'other_config' in line \
                        and 'lacp-port-priority="%s"' % test_port_priority \
                        not in line:
                    success = True
                    break

            assert success, \
                "Test interface remove port-priority command - FAILED!"

        # Check that command fails when port-id does not exist
        out = s1.cmdCLI(test_commands[0])
        assert "Command failed" not in out, \
            "Test interface remove port-priority command - FAILED!"

        out = s1.cmdCLI(test_commands[1])
        assert "Command failed" in out, \
            "Test interface remove port-priority command - FAILED!"

        return True

    def showInterfaceLagBrief(self):
        info('''
########## Test show interface lag brief command ##########
''')
        s1 = self.net.switches[0]
        s1.cmdCLI('conf t')

        # Configure lag with undefined mode
        s1.cmdCLI('interface lag 3')
        s1.cmdCLI('exit')

        # Configure lag in passive mode
        s1.cmdCLI('interface lag 4')
        s1.cmdCLI('lacp mode passive')
        s1.cmdCLI('exit')

        # Configure lag in active mode
        s1.cmdCLI('interface lag 5')
        s1.cmdCLI('lacp mode active')
        s1.cmdCLI('exit')
        s1.cmdCLI('exit')

        # Verify show interface brief shows the lags created before
        success = 0
        out = s1.cmdCLI('show interface brief')
        # info('''%s \n''', out)
        lines = out.split('\n')
        for line in lines:
            if 'lag3' in line and \
                    'auto' in line and \
                    line.count("--") is 6:
                success += 1
            if 'lag4' in line and \
                    'passive' in line and \
                    'auto' in line and \
                    line.count('--') is 5:
                success += 1
            if 'lag5' in line and \
                    'active' in line and \
                    'auto' in line and \
                    line.count('--') is 5:
                success += 1
        # assert success == 3,\
        #    'Test show interface brief command - FAILED!'

        # Verify show interface lag4 brief shows only lag 4
        success = 0
        out = s1.cmdCLI('show interface lag4 brief')
        lines = out.split('\n')
        for line in lines:
            if 'lag4' in line and \
                    'passive' in line and \
                    'auto' in line and \
                    line.count('--') is 5:
                success += 1
            if 'lag1' in line or \
                    'lag2' in line or \
                    'lag3' in line or \
                    'lag5' in line:
                success -= 1
        # assert success == 1,\
        #    'Test show interface lag4 brief command - FAILED!'

        info('''
########## Test show interface lag transceiver command ##########
''')
        success = 0
        out = s1.cmdCLI('show interface lag5 transceiver')
        if 'Invalid switch interface ID.' in out:
            success += 1
        assert(success != 0),\
            'transceiver in lag command failed'
        return True

    def showInterfaceLag(self):
        info('''
########## Test show interface lag command ##########
''')
        s1 = self.net.switches[0]

        # Verify 'show interface lag1' shows correct  information about lag1
        success = 0
        out = s1.cmdCLI('show interface lag1')
        lines = out.split('\n')
        for line in lines:
            if 'Aggregate-name lag1 ' in line:
                success += 1
            if 'Aggregated-interfaces' in line and '1' in line and '2' in line:
                success += 1
            if 'Aggregate mode' in line and 'off' in line:
                success += 1
            if 'Speed' in line and '0 Mb/s' in line:
                success += 1
            if 'Aggregation-key' in line and '1' in line:
                success += 1
        assert success == 5,\
            'Test show interface lag1 command - FAILED!'

        # Verify 'show interface lag4' shows correct  information about lag4
        success = 0
        out = s1.cmdCLI('show interface lag4')
        lines = out.split('\n')
        for line in lines:
            if 'Aggregate-name lag4 ' in line:
                success += 1
            if 'Aggregated-interfaces : ' in line:
                success += 1
            if 'Aggregate mode' in line and 'passive' in line:
                success += 1
            if 'Speed' in line and '0 Mb/s' in line:
                success += 1
        assert success == 4,\
            'Test show interface lag4 command - FAILED!'

        # Verify 'show interface lag5' shows correct  information about lag5
        success = 0
        out = s1.cmdCLI('show interface lag5')
        lines = out.split('\n')
        for line in lines:
            if 'Aggregate-name lag5 ' in line:
                success += 1
            if 'Aggregated-interfaces : ' in line:
                success += 1
            if 'Aggregate mode' in line and 'active' in line:
                success += 1
            if 'Speed' in line and '0 Mb/s' in line:
                success += 1
        assert success == 4,\
            'Test show interface lag5 command - FAILED!'

        return True

    def showLacpInterfaces(self):
        show_lacp_interface = True
        success = 0
        s1 = self.net.switches[0]
        out = s1.cmdCLI('show lacp interface')
        lines = out.split('\n')
        for line in lines:
            if 'Intf Aggregate Port    Port     State   '\
               'System-id         System   Aggr' in line:
                success += 1
            if 'name      id      Priority                           '\
               'Priority Key' in line:
                success += 1
            if 'Intf Aggregate Partner Port     State   '\
               'System-id         System   Aggr' in line:
                success += 1
            if 'name      Port-id Priority                           '\
               'Priority Key' in line:
                success += 1
            if '1    lag1' in line:
                success += 1
            if '2    lag1' in line:
                success += 1
            if '3    lag2' in line:
                success += 1
            if '4    lag2' in line:
                success += 1

        assert success == 12,\
            'Test show lacp interface command = FAILED!'
        out = s1.cmdCLI('show lacp interface lag1')
        success = 0
        lines = out.split('\n')
        for line in lines:
            if 'Aggregate-name :' in line:
                success += 1
        assert success == 0,\
            'Test show lacp interface lag# command = FAILED!'
        out = s1.cmdCLI('show lacp interface lag')
        success = 0
        lines = out.split('\n')
        for line in lines:
            if 'Aggregate-name :' in line:
                success += 1
        assert success == 0,\
            'Test show lacp interface lag command = FAILED!'
        out = s1.cmdCLI('show lacp interface lag 1')
        success = 0
        lines = out.split('\n')
        for line in lines:
            if 'Aggregate-name :' in line:
                success += 1
        assert success == 0,\
            'Test show lacp interface lag # command = FAILED!'
        out = s1.cmdCLI('show lacp interface X')
        success = 0
        lines = out.split('\n')
        for line in lines:
            if 'Aggregate-name :' in line:
                success += 1
        assert success == 0,\
            'Test show lacp interface X command = FAILED!'

        return True

    def test_lag_shutdown(self):

        n_intfs = 4
        s1 = self.net.switches[0]
        s1.cmdCLI('configure terminal')
        s1.cmdCLI('interface lag 1')
        s1.cmdCLI('exit')

        for intf_num in range(1, n_intfs):
            s1.cmdCLI('interface %d' % intf_num)
            s1.cmdCLI('lag 1')
            s1.cmdCLI('exit')

        s1.cmdCLI('interface lag 1')
        s1.cmdCLI('no shutdown')
        s1.cmdCLI('exit')
        s1.cmdCLI('exit')

        out = s1.cmdCLI('show running-config')

        total_lag = 0
        lines = out.split("\n")

        for line in lines:
            if 'no shutdown' in line:
                total_lag += 1

        # total_lag should be the number of intfs + lag + default vlan
        assert total_lag is 5, \
            "Failed test, all interfaces are not up!"

        s1.cmdCLI('configure terminal')
        s1.cmdCLI('interface lag 1')
        s1.cmdCLI('shutdown')
        s1.cmdCLI('exit')
        s1.cmdCLI('exit')

        out = s1.cmdCLI('show running-config')

        total_lag = 0
        lines = out.split("\n")

        for line in lines:
            if 'no shutdown' in line:
                total_lag += 1

        # total_lag should be the no shutdown of the default vlan
        assert total_lag is 1, \
            "Failed test, all interfaces are not down!"

        return True

    def test_lag_ip_running_config(self):
        s1 = self.net.switches[0]
        s1.cmdCLI('configure terminal')
        s1.cmdCLI('interface lag 1')
        s1.cmdCLI('ip address 10.1.1.1/24')
        s1.cmdCLI('ipv6 address 2001:db8:a0b:12f0::1/64')
        s1.cmdCLI('exit')
        s1.cmdCLI('exit')

        out = s1.cmdCLI('show running-config')
        lines = out.split("\n")

        total = 0
        for line in lines:
            if 'ip address 10.1.1.1/24' in line:
                total += 1
            if 'ipv6 address 2001:db8:a0b:12f0::1/64' in line:
                total += 1

        assert total == 2,\
            "Failed test, LAG has no IP assigned"

        return True

@pytest.mark.skipif(True, reason="Skipping due to instability")
class Test_lacp_cli:

    def setup(self):
        pass

    def teardown(self):
        pass

    def setup_class(cls):
        Test_lacp_cli.test = LACPCliTest()

    def test_createLagPort(self):
        if self.test.createLagPort():
            info('''
########## Test to create LAG Port - SUCCESS! ##########
''')

    def test_showLacpAggregates(self):
        if self.test.showLacpAggregates():
            info('''
########## Test Show lacp aggregates command - SUCCESS! ##########
''')

    def test_deleteLagPort(self):
        if self.test.deleteLagPort():
            info('''
########## Test to delete LAG port - SUCCESS! ##########
''')

    def test_addInterfacesToLags(self):
        if self.test.addInterfacesToLags():
            info('''
########## Test to add interfaces to LAG ports - SUCCESS! ##########
''')

    def test_globalLacpCommands(self):
        if self.test.globalLacpCommands():
            info('''
########## Test global LACP commands - SUCCESS! ##########
''')

    def test_lagL234LoadBalancing(self):
        if self.test.lag_hash_LoadBalancing():
            info('''
########## Test LAG Load balancing for L2, L3 and L4 - SUCCESS! #######
''')

    def test_lagContextCommands(self):
        if self.test.lagContextCommands():
            info('''
########## Test LAG context commands - SUCCESS! ##########
''')

    def test_lacpPortId(self):
        if self.test.lacpPortId():
            info('''
########## Test lacp port-id commands - SUCCESS! ##########
''')

    def test_lacpPortPriority(self):
        if self.test.lacpPortPriority():
            info('''
########## Test lacp port-priority commands - SUCCESS! ##########
''')

    def test_showInterfaceLagBrief(self):
        if self.test.showInterfaceLagBrief():
            info('''
########## Test show interface lag brief command - SUCCESS! ##########
''')

    def test_showInterfaceLag(self):
        if self.test.showInterfaceLag():
            info('''
########## Test show interface lag command - SUCCESS! ##########
''')

    def test_showLacpInterface(self):
        if self.test.showLacpInterfaces():
            info('''
########## Test show lacp interface command - SUCCESS! ##########
''')

    def test_lag_shutdown(self):
        if self.test.test_lag_shutdown():
            info('''
########## Test show lacp shutdown command - SUCCESS! ##########
''')

    def test_lag_ip_running_config(self):
        if self.test.test_lag_ip_running_config():
            info('''
########## Test show LACP IP running-config command  - SUCCESS! ##########
''')

    def teardown_class(cls):
        Test_lacp_cli.test.net.stop()

    def setup_method(self, method):
        pass

    def teardown_method(self, method):
        pass

    def __del__(self):
        del self.test


###############################################################################
#
#   Validates max number of lags added
#
#   Allowed MAX number 256
#
###############################################################################
@pytest.mark.skipif(True, reason="Skipping due to instability")
class LACPMaxNumberOfLags(OpsVsiTest):
    def setupNet(self):
        host_opts = self.getHostOpts()
        switch_opts = self.getSwitchOpts()
        infra_topo = SingleSwitchTopo(k=0, hopts=host_opts, sopts=switch_opts)
        self.net = Mininet(infra_topo,
                           switch=VsiOpenSwitch,
                           host=Host,
                           link=OpsVsiLink,
                           controller=None,
                           build=True)

    def test_max_number_of_lags(self):
        info("########## "
             "Test max number of LAGs allowed "
             "########## ")

        max_lag = 256

        s1 = self.net.switches[0]
        s1.cmdCLI('configure terminal')

        # Create allowed LAGs
        for lag_num in range(1, max_lag + 1):
            s1.cmdCLI('interface lag %d' % lag_num)
            s1.cmdCLI('exit')

        # exit configure terminal
        s1.cmdCLI('exit')

        out = s1.cmdCLI("show running-config")
        lines = out.split('\n')

        # Check if all LAGs were created
        total_lag = 0
        for line in lines:
            if 'interface lag ' in line:
                total_lag += 1

        assert total_lag is max_lag, \
            "Failed test, all LAGs not created!"

        # Crate LAG 257
        s1.cmdCLI('configure terminal')
        out = s1.cmdCLI('interface lag %d' % (max_lag + 1))

        assert "Cannot create LAG interface." in out, \
            "Failed test, new LAG created!"

        info("DONE\n")

    def test_lag_qos_config(self):
        s1 = self.net.switches[0]

        # Set up.
        s1.cmdCLI('end')
        s1.cmdCLI('configure terminal')
        s1.cmdCLI('no interface lag 100')

        # Configure qos on a lag.
        s1.cmdCLI('interface lag 100')
        s1.cmdCLI('qos trust none')
        s1.cmdCLI('qos dscp 1')
        s1.cmdCLI('apply qos schedule-profile default')

        # Check the running config.
        out = s1.cmdCLI('do show running-config')
        assert 'interface lag 100' in out
        assert 'qos trust none' in out
        assert 'qos dscp 1' in out
        assert 'apply qos schedule-profile default' in out

        # Tear down.
        s1.cmdCLI('end')
        s1.cmdCLI('configure terminal')
        s1.cmdCLI('no interface lag 100')
        s1.cmdCLI('end')


@pytest.mark.skipif(True, reason="Skipping due to instability")
class Test_lacp_max_lags:

    def setup(self):
        pass

    def teardown(self):
        pass

    def setup_class(cls):
        Test_lacp_max_lags.test = LACPMaxNumberOfLags()

    def teardown_class(cls):
        Test_lacp_max_lags.test.net.stop()

    def test_max_number_of_lags(self):
        self.test.test_max_number_of_lags()

    def test_lag_qos_config(self):
        self.test.test_lag_qos_config()

    def setup_method(self, method):
        pass

    def teardown_method(self, method):
        pass

    def __del__(self):
        del self.test
