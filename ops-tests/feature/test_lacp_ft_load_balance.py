# -*- coding: utf-8 -*-
#
# Copyright (C) 2016 Hewlett Packard Enterprise Development LP
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

"""
OpenSwitch Test for LAG load Balancing.

Testing with l2-src-dst, l3-src-dst and l4-src-dst hash algorithms.
The purpose is to try with these algorithms, traffic must be transmitted
in both LAG interfaces.
For l2 and l3, source addresses change to simulate different hosts
"""

from random import randint
from time import sleep
from lacp_lib import (
    associate_interface_to_lag,
    associate_vlan_to_l2_interface,
    associate_vlan_to_lag,
    check_lag_lb_hash,
    create_lag,
    create_vlan,
    set_lag_lb_hash,
    turn_on_interface,
    verify_connectivity_between_hosts,
    verify_turn_on_interfaces
)

from pytest import mark


TOPOLOGY = """
# +-------+                                   +-------+
# |       |    +--------+  LAG  +--------+    |       |
# |  hs1  <---->        <------->        <---->  hs2  |
# |       |    |  sw1   <------->  sw2   |    |       |
# +-------+    |        |       |        <-+  +-------+
#              +--------+       +--------+ |
#                                          |  +-------+
#                                          |  |       |
#                                          +-->  hs3  |
#                                             |       |
#                                             +-------+

# Nodes
[type=openswitch name="OpenSwitch 1"] sw1
[type=openswitch name="OpenSwitch 2"] sw2
[type=host name="Host src 1" image="gdanii/iperf:latest"] hs1
[type=host name="Host dst 2" image="gdanii/iperf:latest"] hs2
[type=host name="Host dst 3" image="gdanii/iperf:latest"] hs3

# Links
hs1:1 -- sw1:1
hs2:1 -- sw2:1
hs3:1 -- sw2:4
sw1:2 -- sw2:2
sw1:3 -- sw2:3
"""


def generate_mac_addresses(counter):
    """Generate MAC address."""
    m_list = []
    while len(m_list) <= counter:
        # using docker mac addresses to avoid
        # conflicts with real hardware
        new_mac = '02:42:AC:11:%02X:%02X' %\
            (randint(0, 255), randint(0, 255))
        m_list.append(new_mac)
    return m_list


def generate_ip_addresses(counter):
    """Generate IP address."""
    ip_list = []
    while len(ip_list) <= counter:
        ip_digit = randint(15, 254)
        new_ip = '10.0.11.{ip_digit}'.format(**locals())
        ip_list.append(new_ip)
    return ip_list


def chg_mac_address(hs, interface, new_mac):
    """Change MAC address."""
    hs('ip link set dev {interface} down'.format(**locals()),
       shell='bash')
    hs('ip link set dev {interface} address {new_mac}'
       .format(**locals()),
       shell='bash')
    hs('ip link set dev {interface} up'.format(**locals()),
       shell='bash')


def chg_ip_address(hs, interface, new_ip, old_ip):
    """Change IP address."""
    hs('ip link set dev {interface} down'.format(**locals()),
       shell='bash')
    hs('ip addr del {old_ip} dev {interface}'.format(**locals()),
       shell='bash')
    hs('ip addr add {new_ip} dev {interface}'.format(**locals()),
       shell='bash')
    hs('ip link set dev {interface} up'.format(**locals()),
       shell='bash')


@mark.platform_incompatible(['docker'])
def test_lacpd_load_balance(topology, step):
    """Test LAG load balance with l2, l3 and l4 hash algorithms."""
    # VID for testing
    test_vlan = '256'
    # LAG ID for testing
    test_lag = '2'
    # Ports for testing
    test_port_tcp = 778
    test_port_udp = 777
    times_to_send = 150
    # interfaces to be added to LAG
    lag_interfaces = ['2', '3']
    # traffic counters
    lag_intf_counter_b = {}
    lag_intf_counter_a = {}
    delta_tx = {'l2-src-dst': {}, 'l3-src-dst': {}, 'l4-src-dst': {}}

    num_addresses = 10

    mac_list = generate_mac_addresses(num_addresses)
    ip_list = generate_ip_addresses(num_addresses)

    lag_lb_algorithms = ['l2-src-dst', 'l3-src-dst', 'l4-src-dst']
    sw1_host_interfaces = ['1']
    sw2_host_interfaces = ['1', '4']
    host1_addr = '10.0.11.10'
    host2_addr = '10.0.11.11'
    host3_addr = '10.0.11.12'

    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')
    hs3 = topology.get('hs3')

    step('Verifying switches are not None')
    assert sw1 is not None, 'Topology failed getting object sw1'
    assert sw2 is not None, 'Topology failed getting object sw2'

    step('Verifying hosts are not None')
    assert hs1 is not None, 'Topology failed getting object hs1'
    assert hs2 is not None, 'Topology failed getting object hs2'
    assert hs3 is not None, 'Topology failed getting object hs3'

    step('Turning interfaces ON on Switch 1')
    for intf in lag_interfaces + sw1_host_interfaces:
        turn_on_interface(sw1, intf)

    step('Turning interfaces ON on Switch 1')
    for intf in lag_interfaces + sw2_host_interfaces:
        turn_on_interface(sw2, intf)

    step('Verifying interfaces are Up on Switch 1')
    verify_turn_on_interfaces(sw1, lag_interfaces + sw1_host_interfaces)

    step('Verifying interfaces are Up on Switch 1')
    verify_turn_on_interfaces(sw2, lag_interfaces + sw2_host_interfaces)

    # Configure switches
    for switch in [sw1, sw2]:
        step('Creating VLAN %s' % test_vlan)
        create_vlan(switch, test_vlan)

        step('Creating LAG %s' % test_lag)
        create_lag(switch, test_lag, 'off')

        step('Associating VLAN %s to LAG %s' % test_vlan, test_lag)
        associate_vlan_to_lag(switch, test_vlan, test_lag)

        for intf in lag_interfaces:
            step('Associating interface %s to LAG %s' % (intf, test_lag))
            associate_interface_to_lag(switch, intf, test_lag)

    for intf in sw1_host_interfaces:
        step('Associating VLAN %s to interface %s' % (test_vlan, intf))
        associate_vlan_to_l2_interface(sw1, test_vlan, intf)

    for intf in sw2_host_interfaces:
        step('Associating VLAN %s to interface %s' % (test_vlan, intf))
        associate_vlan_to_l2_interface(sw2, test_vlan, intf)

    # Configure host interfaces
    hs1.libs.ip.interface('1',
                          addr='{host1_addr}/24'.format(**locals()),
                          up=True)

    hs2.libs.ip.interface('1',
                          addr='{host2_addr}/24'.format(**locals()),
                          up=True)

    hs3.libs.ip.interface('1',
                          addr='{host3_addr}/24'.format(**locals()),
                          up=True)

    step('Verifying connectivity between Host 1 and Host 2')
    verify_connectivity_between_hosts(hs1, host1_addr, hs2, host2_addr)

    step('Verifying connectivity between Host 1 and Host 3')
    verify_connectivity_between_hosts(hs1, host1_addr, hs3, host3_addr)

    hs2.libs.iperf.server_start(test_port_tcp, 20, False)
    hs2.libs.iperf.server_start(test_port_udp, 20, True)
    hs3.libs.iperf.server_start(test_port_tcp, 20, False)

    old_ip = host1_addr
    for lb_algorithm in lag_lb_algorithms:
        print('========== Testing with {lb_algorithm} =========='
              .format(**locals()))
        for switch in [sw1, sw2]:
            set_lag_lb_hash(switch, test_lag, lb_algorithm)
            # Check that hash is properly set
            check_lag_lb_hash(switch, test_lag, lb_algorithm)

        sleep(10)
        for intf in lag_interfaces:
            intf_info = sw1.libs.vtysh.show_interface(intf)
            lag_intf_counter_b[intf] = intf_info['tx_packets']

        for x in range(0, times_to_send):
            if lb_algorithm == 'l3-src-dst':
                ip = ip_list[x % num_addresses]
                chg_ip_address(hs1,
                               '1',
                               '{ip}/24'.format(**locals()),
                               '{old_ip}/24'.format(**locals()))
                old_ip = ip
            elif lb_algorithm == 'l2-src-dst':
                mac = mac_list[x % num_addresses]
                chg_mac_address(hs1, '1', mac)

            hs1.libs.iperf.client_start(host2_addr, test_port_tcp, 1, 2, False)

            if lb_algorithm == 'l4-src-dst':
                hs1.libs.iperf.client_start(host2_addr,
                                            test_port_udp,
                                            1,
                                            2,
                                            True)
            else:
                hs1.libs.iperf.client_start(host3_addr,
                                            test_port_tcp,
                                            1,
                                            2,
                                            False)
        sleep(15)
        for intf in lag_interfaces:
            intf_info = sw1.libs.vtysh.show_interface(intf)
            lag_intf_counter_a[intf] = intf_info['tx_packets']
            delta_tx[lb_algorithm][intf] =\
                lag_intf_counter_a[intf] - lag_intf_counter_b[intf]

            assert delta_tx[lb_algorithm][intf] > 0,\
                'Just one interface was used with {lb_algorithm}'\
                .format(**locals())

        if lb_algorithm == 'l3-src-dst':
            chg_ip_address(hs1, '1',
                           '{host1_addr}/24'.format(**locals()),
                           '{old_ip}/24'.format(**locals()))

    # Print a summary of counters
    for lb_algorithm in lag_lb_algorithms:
        print('======== {lb_algorithm} ============'.format(**locals()))
        for intf in lag_interfaces:
            print('Interface: {intf}'.format(**locals()))
            dt = delta_tx[lb_algorithm][intf]
            print('TX packets: {dt}'.format(**locals()))
            print('--')
    print('=================================')
