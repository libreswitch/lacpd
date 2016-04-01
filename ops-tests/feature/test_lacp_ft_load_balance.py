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

from time import sleep
from random import randint
from pytest import mark

from lacp_lib import create_lag
from lacp_lib import create_vlan
from lacp_lib import associate_interface_to_lag
from lacp_lib import associate_vlan_to_lag
from lacp_lib import associate_vlan_to_l2_interface
from lacp_lib import set_lag_lb_hash
from lacp_lib import check_lag_lb_hash
from lacp_lib import check_connectivity_between_hosts
from lacp_lib import turn_on_interface
from lacp_lib import validate_turn_on_interfaces

TOPOLOGY = """
# +-------+                                   +-------+
# |       |    +--------+  LAG  +--------+    |       |
# |  hs1  <---->        <------->        <---->  hs2  |
# |       |    |  ops1  <------->  ops2  |    |       |
# +-------+    |        |       |        <-+  +-------+
#              +--------+       +--------+ |
#                                          |  +-------+
#                                          |  |       |
#                                          +-->  hs3  |
#                                             |       |
#                                             +-------+

# Nodes
[type=openswitch name="OpenSwitch 1"] ops1
[type=openswitch name="OpenSwitch 2"] ops2
[type=host name="Host src 1" image="gdanii/iperf:latest"] hs1
[type=host name="Host dst 2" image="gdanii/iperf:latest"] hs2
[type=host name="Host dst 3" image="gdanii/iperf:latest"] hs3

# Links
hs1:1 -- ops1:1
hs2:1 -- ops2:1
hs3:1 -- ops2:4
ops1:2 -- ops2:2
ops1:3 -- ops2:3
"""


def generate_mac_addresses(counter):
    m_list = []
    while len(m_list) <= counter:
        # using docker mac addresses to avoid
        # conflicts with real hardware
        new_mac = '02:42:AC:11:%02X:%02X' %\
            (randint(0, 255), randint(0, 255))
        m_list.append(new_mac)
    return m_list


def generate_ip_addresses(counter):
    ip_list = []
    while len(ip_list) <= counter:
        ip_digit = randint(15, 254)
        new_ip = '10.0.11.{ip_digit}'.format(**locals())
        ip_list.append(new_ip)
    return ip_list


def chg_mac_address(hs, interface, new_mac):
    hs('ip link set dev {interface} down'.format(**locals()),
       shell='bash')
    hs('ip link set dev {interface} address {new_mac}'
       .format(**locals()),
       shell='bash')
    hs('ip link set dev {interface} up'.format(**locals()),
       shell='bash')


def chg_ip_address(hs, interface, new_ip, old_ip):
    hs('ip link set dev {interface} down'.format(**locals()),
       shell='bash')
    hs('ip addr del {old_ip} dev {interface}'.format(**locals()),
       shell='bash')
    hs('ip addr add {new_ip} dev {interface}'.format(**locals()),
       shell='bash')
    hs('ip link set dev {interface} up'.format(**locals()),
       shell='bash')


@mark.platform_incompatible(['docker'])
def test_lacpd_load_balance(topology):
    """
    Tests LAG load balance with l2, l3 and l4 hash algorithms
    """

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

    ops1 = topology.get('ops1')
    ops2 = topology.get('ops2')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')
    hs3 = topology.get('hs3')

    assert ops1 is not None, 'Topology failed getting object ops1'
    assert ops2 is not None, 'Topology failed getting object ops2'
    assert hs1 is not None, 'Topology failed getting object hs1'
    assert hs2 is not None, 'Topology failed getting object hs2'
    assert hs3 is not None, 'Topology failed getting object hs3'

    # Turn on interfaces
    for curr_if in lag_interfaces + sw1_host_interfaces:
        turn_on_interface(ops1, curr_if)

    for curr_if in lag_interfaces + sw2_host_interfaces:
        turn_on_interface(ops2, curr_if)

    # Wait for interface become up
    sleep(5)
    validate_turn_on_interfaces(ops1, lag_interfaces + sw1_host_interfaces)
    validate_turn_on_interfaces(ops2, lag_interfaces + sw2_host_interfaces)

    # Configure switches
    for curr_ops in [ops1, ops2]:
        create_vlan(curr_ops, test_vlan)
        create_lag(curr_ops, test_lag, 'off')
        associate_vlan_to_lag(curr_ops, test_vlan, test_lag)
        for curr_if in lag_interfaces:
            associate_interface_to_lag(curr_ops, curr_if, test_lag)

    for curr_p in sw1_host_interfaces:
        associate_vlan_to_l2_interface(ops1, test_vlan, curr_p)

    for curr_p in sw2_host_interfaces:
        associate_vlan_to_l2_interface(ops2, test_vlan, curr_p)

    # Configure host interfaces
    hs1.libs.ip.interface('1', addr='{host1_addr}/24'.format(**locals()),
                          up=True)

    hs2.libs.ip.interface('1', addr='{host2_addr}/24'.format(**locals()),
                          up=True)

    hs3.libs.ip.interface('1', addr='{host3_addr}/24'.format(**locals()),
                          up=True)

    print('Sleep few seconds to wait everything is up')
    sleep(30)

    # Pinging to verify connections are OK
    check_connectivity_between_hosts(hs1, host1_addr,
                                     hs2, host2_addr)
    check_connectivity_between_hosts(hs1, host1_addr,
                                     hs3, host3_addr)

    hs2.libs.iperf.server_start(test_port_tcp, 20, False)
    hs2.libs.iperf.server_start(test_port_udp, 20, True)
    hs3.libs.iperf.server_start(test_port_tcp, 20, False)

    old_ip = host1_addr
    for lb_algorithm in lag_lb_algorithms:
        print('========== Testing with {lb_algorithm} =========='
              .format(**locals()))
        for curr_ops in [ops1, ops2]:
            set_lag_lb_hash(curr_ops, test_lag, lb_algorithm)
            # Check that hash is properly set
            check_lag_lb_hash(curr_ops, test_lag, lb_algorithm)
        sleep(10)
        for curr_p in lag_interfaces:
            intf_info = ops1.libs.vtysh.show_interface(curr_p)
            lag_intf_counter_b[curr_p] = intf_info['tx_packets']

        for x in range(0, times_to_send):
            if lb_algorithm == 'l3-src-dst':
                ip = ip_list[x % num_addresses]
                chg_ip_address(hs1, '1',
                               '{ip}/24'.format(**locals()),
                               '{old_ip}/24'.format(**locals()))
                old_ip = ip
            elif lb_algorithm == 'l2-src-dst':
                mac = mac_list[x % num_addresses]
                chg_mac_address(hs1, '1', mac)

            hs1.libs.iperf.client_start(host2_addr,
                                        test_port_tcp,
                                        1, 2, False)

            if lb_algorithm == 'l4-src-dst':
                hs1.libs.iperf.client_start(host2_addr,
                                            test_port_udp,
                                            1, 2, True)
            else:
                hs1.libs.iperf.client_start(host3_addr,
                                            test_port_tcp,
                                            1, 2, False)
        sleep(15)
        for curr_p in lag_interfaces:
            intf_info = ops1.libs.vtysh.show_interface(curr_p)
            lag_intf_counter_a[curr_p] = intf_info['tx_packets']
            delta_tx[lb_algorithm][curr_p] =\
                lag_intf_counter_a[curr_p] - lag_intf_counter_b[curr_p]

            assert delta_tx[lb_algorithm][curr_p] > 0,\
                'Just one interface was used with {lb_algorithm}'\
                .format(**locals())

        if lb_algorithm == 'l3-src-dst':
            chg_ip_address(hs1, '1',
                           '{host1_addr}/24'.format(**locals()),
                           '{old_ip}/24'.format(**locals()))

    # Print a summary of counters
    for lb_algorithm in lag_lb_algorithms:
        print('======== {lb_algorithm} ============'.format(**locals()))
        for curr_p in lag_interfaces:
            print('Interface: {curr_p}'.format(**locals()))
            dt = delta_tx[lb_algorithm][curr_p]
            print('TX packets: {dt}'.format(**locals()))
            print('--')
    print('=================================')
