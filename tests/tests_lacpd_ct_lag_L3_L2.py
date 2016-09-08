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

# OVS_VSCTL = "/usr/bin/ovs-vsctl "
OVS_VSCTL_GET_IP_CMD = "/usr/bin/ovs-vsctl get port %s ip4_address"
OVS_VSCTL_GET_IPV6_CMD = "/usr/bin/ovs-vsctl get port %s ip6_address"

first_interface = 'lag 100'
second_interface = 'lag 200'
third_interface = 'lag 300'


class vrfCLITest(OpsVsiTest):

    def setupNet(self):
        host_opts = self.getHostOpts()
        switch_opts = self.getSwitchOpts()
        infra_topo = SingleSwitchTopo(k=0, hopts=host_opts, sopts=switch_opts)
        self.net = Mininet(infra_topo, switch=VsiOpenSwitch,
                           host=Host, link=OpsVsiLink,
                           controller=None, build=True)

    def test_vrf_add_delete(self):
        '''
            Test VRF add and delete validations
        '''

        info("\n########## VRF add/delete validations ##########\n")
        s1 = self.net.switches[0]

        # OPS_TODO: When multiple VRF support is added, change the script
        # to include required validations.

        s1.cmdCLI("configure terminal")

        # Checking VRF name more than 32 characters

        ret = s1.cmdCLI('vrf thisisavrfnamewhichismorethan32characters')
        assert 'Non-default VRFs not supported' in ret, \
               'VRF name validation failed'
        info('### VRF name validation passed ###\n')

        # Adding another VRF

        ret = s1.cmdCLI('vrf thisisavrfnamewhichisexactly32c')
        assert 'Non-default VRFs not supported' in ret, \
               'VRF add validation failed'
        info('### VRF add validation passed ###\n')

        # Adding default VRF

        ret = s1.cmdCLI('vrf vrf_default')
        assert 'Default VRF already exists.' in ret, \
               'Default VRF add validation failed'
        info('### Default VRF add validation passed ###\n')

        # Deleting default VRF

        ret = s1.cmdCLI('no vrf vrf_default')
        assert 'Cannot delete default VRF.' in ret, \
               'VRF delete validation failed'
        info('### VRF delete validation passed ###\n')

        # Deleting VRF which does not exist

        ret = s1.cmdCLI('no vrf abcd')
        assert 'Non-default VRFs not supported' in ret, \
               'VRF lookup validation failed'
        info('### VRF lookup validation passed ###\n')

        # Cleanup

        s1.cmdCLI('exit')

    def test_no_internal_vlan(self):
        '''
            Test LAG status for up/no internal vlan
        '''

        info("\n########## LAG status up/no_internal_vlan validations"
             " ##########\n")
        s1 = self.net.switches[0]

        # Configurig vlan range for a single vlan
        s1.cmdCLI("configure terminal")
        s1.cmdCLI("vlan internal range 1024 1026 ascending")
        s1.cmdCLI('interface %s'% first_interface)
        s1.cmdCLI('no routing')
        s1.cmdCLI('exit')
        s1.cmdCLI('interface %s'% second_interface)
        s1.cmdCLI('no routing')
        s1.cmdCLI('exit')
        s1.cmdCLI('interface %s'% third_interface)
        s1.cmdCLI('ip address 11.1.1.1/8')
        s1.cmdCLI('exit')

        # Checking to see if up and no_internal_vlan cases are handled
        ret = s1.cmdCLI('do show vlan internal')
        ret = ret.replace(' ', '')
        expected_output = '\t' + first_interface
        assert expected_output not in ret, \
            'Interface status no_internal_vlan failed'
        info('### Interface status no_internal_vlan passed ###\n')

    def test_lag_ip_verification(self):
        '''
            Test configuration of IP address for port
        '''

        info("\n########## Assign/remove IP address "
             "to/from interface ##########\n")
        s1 = self.net.switches[0]

        # Adding IP address to L2 interface

        s1.cmdCLI('configure terminal')
        s1.cmdCLI('interface %s'% first_interface)
        ret = s1.cmdCLI('ip address 10.0.20.2/24')
        expected_output = 'Interface ' + 'lag100' \
            + ' is not L3.'
        assert expected_output in ret, 'IP address validation failed'
        info('### IP address validation passed ###\n')

        # Deleting IP address on an L3 interface which does not
        # have any IP address

        s1.cmdCLI('routing')
        ret = s1.cmdCLI('no ip address 10.0.30.2/24')
        expected_output = 'No IPv4 address configured on interface'
        assert expected_output in ret, \
            'IP address presence validation failed'
        info('### IP address presence validation passed ###\n')

        # Configuring IP address on L3 interface

        s1.cmdCLI('ip address 10.0.20.2/24')
        # intf_cmd = OVS_VSCTL + 'get port ' + first_interface \
        #     + ' ip4_address'
        # ip = s1.ovscmd(intf_cmd).strip()
        ip = s1.ovscmd(OVS_VSCTL_GET_IP_CMD % first_interface)
        assert ip is '10.0.20.2/24', 'IP address configuration failed'
        info('### IP address configured successfully ###\n')

        # Updating IP address on L3 interface

        s1.cmdCLI('ip address 10.0.20.3/24')
        # intf_cmd = OVS_VSCTL+ 'get port ' + first_interface \
        #     + ' ip4_address'
        # ip = s1.ovscmd(intf_cmd).strip()
        ip = s1.ovscmd(OVS_VSCTL_GET_IP_CMD % first_interface)
        assert ip is '10.0.20.3/24', 'IP address update failed'
        info('### IP address updated successfully ###\n')

        # Remove IP address on L3 interface by giving an IP address
        # that is not present

        s1.cmdCLI("no ip address 10.0.20.4/24 secondary")
        ret = s1.cmdCLI("no ip address 10.0.30.2/24")
        assert "IP address 10.0.30.2/24 not found." in ret, \
               'IP address delete validation failed'
        info('### IP address delete validation passed ###\n')

        # Remove IP address from L3 interface by giving correct IP address

        ret = s1.cmdCLI("no ip address 10.0.20.3/24")
        # intf_cmd = OVS_VSCTL + "get port " + first_interface \
        #            + " ip4_address"
        # ip = s1.ovscmd(intf_cmd).strip()
        ip = s1.ovscmd(OVS_VSCTL_GET_IP_CMD % first_interface)
        assert ip is '[]', 'IP address remove failed'
        info('### IP address removed successfully ###\n')

        # Cleanup

        s1.cmdCLI('exit')

    def test_lag_ipv6_verification(self):
        '''
            Test configuration of IPv6 address for port
        '''

        info("\n########## Assign/remove IPv6 address "
             "to/from interface ##########\n")
        s1 = self.net.switches[0]

        # Adding IPv6 address to L2 interface

        s1.cmdCLI('configure terminal')
        s1.cmdCLI('interface %s'% first_interface)
        ret = s1.cmdCLI('ipv6 address 2002::1/128')
        expected_output = 'Interface ' + 'lag100' \
            + ' is not L3.'
        assert expected_output in ret, 'IPv6 address validation failed'
        info('### IPv6 address validation passed ###\n')

        # Deleting IPv6 address on an L3 interface which does
        # not have any IPv6 address

        s1.cmdCLI('routing')
        ret = s1.cmdCLI('no ipv6 address 2002::1/128')
        expected_output = \
            'No IPv6 address configured on interface ' \
            + first_interface
        assert expected_output in ret, \
            'IPv6 address presence validation failed'
        info('### IPv6 address presence validation passed ###\n')

        # Configuring IPv6 address on L3 interface

        s1.cmdCLI('ipv6 address 2002::1/128')
        # intf_cmd = OVS_VSCTL + 'get port ' + first_interface \
        #     + ' ip6_address'
        # ipv6 = s1.ovscmd(intf_cmd).strip()
        ipv6 = s1.ovscmd(OVS_VSCTL_GET_IPV6_CMD % first_interface)
        assert ipv6 is '2002::1/128', \
            'IPv6 address configuration failed'
        info('### IPv6 address configured successfully ###\n')

        # Updating IPv6 address on L3 interface

        s1.cmdCLI('ipv6 address 2001::1/128')
        # intf_cmd = OVS_VSCTL + 'get port ' + first_interface \
        #     + ' ip6_address'
        # ipv6 = s1.ovscmd(intf_cmd).strip()
        ipv6 = s1.ovscmd(OVS_VSCTL_GET_IPV6_CMD % first_interface)
        assert ipv6 is '2001::1/128', 'IPv6 address update failed'
        info('### IPv6 address updated successfully ###\n')

        # Remove IPv6 address from L3 interface by giving correct IP address

        ret = s1.cmdCLI("no ipv6 address 2001::1/128")
        # intf_cmd = OVS_VSCTL + "get port " + first_interface \
        #            + " ip4_address"
        # ip = s1.ovscmd(intf_cmd).strip()
        ip = s1.ovscmd(OVS_VSCTL_GET_IP_CMD % first_interface)
        assert ip is '[]', 'IP address remove failed'
        info('### IP address removed successfully ###\n')

        # Cleanup

        s1.cmdCLI('exit')

    def test_toggle_l2_l3(self):
        '''
            Test routing / no routing commands for port
        '''

        info("\n########## Testing routing/ no routing "
             "working ##########\n")
        s1 = self.net.switches[0]

        # Configuring IP, IPv6 addresses on L3 interface

        s1.cmdCLI('configure terminal')
        s1.cmdCLI('interface %s'% first_interface)
        s1.cmdCLI("routing")
        s1.cmdCLI("ipv6 address 2002::1/128")
        s1.cmdCLI("ip address 10.1.1.1/8")
        ret = s1.cmdCLI("do show vrf")
        expected_output = '\t' + 'lag100'
        assert expected_output in ret, \
            'Interface is not L3. "routing" failed'
        info('### Interface is L3. "routing" passed ###\n')

        # Making L3 interface as L2

        s1.cmdCLI('no routing')
        ret = s1.cmdCLI('do show vrf')
        expected_output = '\t' + 'lag100'
        assert expected_output not in ret, 'Show vrf validation failed'
        info('### Show vrf validation passed ###\n')

        # Checking if IP address removed

        # intf_cmd = OVS_VSCTL + 'get port ' + first_interface \
        #     + ' ip4_address'
        # ip = s1.ovscmd(intf_cmd).strip()
        ip = s1.ovscmd(OVS_VSCTL_GET_IP_CMD % first_interface)
        assert ip is '[]', 'IP address remove failed'
        info('### IP address removed successfully ###\n')

        # Checking if IPv6 address removed

        # intf_cmd = OVS_VSCTL + 'get port ' + first_interface \
        #     + ' ip6_address'
        # ipv6 = s1.ovscmd(intf_cmd).strip()
        ipv6 = s1.ovscmd(OVS_VSCTL_GET_IPV6_CMD % first_interface)
        assert ipv6 is '[]', 'IPv6 address remove failed'
        info('### IPv6 address removed successfully ###\n')

        # Checking if no routing worked

        ret = s1.cmdCLI('ip address 10.1.1.1/8')
        expected_output = 'Interface ' + 'lag100' \
            + ' is not L3.'
        assert expected_output in ret, 'Attach to bridge failed'
        info('### Attached to bridge successfully ###\n')

        # Cleanup

        s1.cmdCLI('exit')


@pytest.mark.skipif(True, reason="Skipping due to instability")
class Test_vtysh_vrf:

    def setup_class(cls):

        # Create a test topology

        Test_vtysh_vrf.test = vrfCLITest()

    def teardown_class(cls):

        # ops-lacpd is stopped so that it produces the gcov coverage data
        Test_vtysh_vrf.test.net.switches[0].cmd("/bin/systemctl stop ops-lacpd")

        # Stop the Docker containers, and
        # mininet topology

        Test_vtysh_vrf.test.net.stop()

    def test_vrf_add_delete(self):
        self.test.test_vrf_add_delete()

    def test_no_internal_vlan(self):
        self.test.test_no_internal_vlan()

    def test_lag_ip_verification(self):
        self.test.test_lag_ip_verification()

    def test_lag_ipv6_verification(self):
        self.test.test_lag_ipv6_verification()

    def test_toggle_l2_l3(self):
        self.test.test_toggle_l2_l3()

    def __del__(self):
        del self.test
