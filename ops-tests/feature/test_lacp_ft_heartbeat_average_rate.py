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
OpenSwitch Test for LACPD heartbeat configurations.
"""

from time import sleep

from lacp_lib import create_lag_active
from lacp_lib import create_lag_passive
from lacp_lib import associate_interface_to_lag
from lacp_lib import associate_vlan_to_lag
from lacp_lib import turn_on_interface
from lacp_lib import validate_turn_on_interfaces
from lacp_lib import create_vlan
from lacp_lib import config_lacp_rate
from lacp_lib import associate_vlan_to_l2_interface
from lacp_lib import check_connectivity_between_hosts
from lacp_lib import tcpdump_capture_interface_start
from lacp_lib import tcpdump_capture_interface_stop
from lacp_lib import get_counters_from_packet_capture

TOPOLOGY = """
# +-------+                                  +-------+
# |       |    +--------+  LAG  +--------+   |       |
# |  hs1  <---->  ops1  <------->  ops2  <--->  hs2  |
# |       |    |   A    <------->    P   |   |       |
# +-------+    |        <------->        |   +-------+
#              +--------+       +--------+

# Nodes
[type=openswitch name="OpenSwitch 1 LAG active"] ops1
[type=openswitch name="OpenSwitch 2 LAG passive"] ops2
[type=host name="Host 1"] hs1
[type=host name="Host 2"] hs2

# Links
hs1:1 -- ops1:4
ops1:1 -- ops2:1
ops1:2 -- ops2:2
ops1:3 -- ops2:3
hs2:1 -- ops2:4
"""


# Runs the diag-dump lacp basic command and gets the average of pdus sent
# through all the interfaces of the lag.
def get_average_lacpd_sent_pdus(sw, lag_id):
    output = sw.libs.vtysh.diag_dump_lacp_basic()
    pdu_dict = output['Counters']
    sent_pdus_sum = 0
    count = 0
    for interface_number in pdu_dict[lag_id]:
        sent_pdus_sum += pdu_dict[lag_id][interface_number]['lacp_pdus_sent']
        count += 1
    return sent_pdus_sum/count


def test_lacpd_heartbeat(topology):
    """
    Tests LACP heartbeat average rate (slow/fast)

    If LACP configured with fast rate a lacpdu is sent every second,
    if configured with slow rate a lacpdu is sent every 30 seconds.
    """

    # VID for testing
    test_vlan = '2'
    # LAG ID for testing
    test_lag = '2'
    test_lag_if = 'lag' + test_lag
    # interfaces to be added to LAG
    lag_interfaces = ['1', '2', '3']
    # interface connected to host
    host_interface = '4'
    # hosts addresses
    hs1_addr = '10.0.11.10'
    hs2_addr = '10.0.11.11'
    # heart beat info according with rate (slow|fast)
    # packets_per_seconds: LACPDUs sent per second
    # min_percent: minimum percentage of LACPDUs allowed
    #              calculated later as f(wait_time, packets_per_second)
    # max_percent: maximum percentage of LACPDUs allowed
    # wait_time: time for tcpdump
    hb_info = {
        'slow':
        {
            'packets_per_second': (1/30),
            'min_percent': 0,
            'max_percent': 1,
            'wait_time': 90,
        },
        'fast':
        {
            'packets_per_second': 1,
            'min_percent': 0,
            'max_percent': 1,
            'wait_time': 10
        }
    }

    ops1 = topology.get('ops1')
    ops2 = topology.get('ops2')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')

    assert ops1 is not None, 'Topology failed getting object ops1'
    assert ops2 is not None, 'Topology failed getting object ops2'
    assert hs1 is not None, 'Topology failed getting object hs1'
    assert hs2 is not None, 'Topology failed getting object hs2'

    for curr_ops in [ops1, ops2]:
        for curr_p in lag_interfaces + [host_interface]:
            turn_on_interface(curr_ops, curr_p)

    print('Wait for interfaces become up')
    sleep(60)
    for curr_ops in [ops1, ops2]:
        create_vlan(curr_ops, test_vlan)
        validate_turn_on_interfaces(curr_ops,
                                    lag_interfaces + [host_interface])

    create_lag_active(ops1, test_lag)
    lacp_map = ops1.libs.vtysh.show_lacp_aggregates()
    assert lacp_map[test_lag_if]['mode'] == 'active',\
        'LACP mode is not active'

    associate_vlan_to_lag(ops1, test_vlan, test_lag)

    create_lag_passive(ops2, test_lag)
    lacp_map = ops2.libs.vtysh.show_lacp_aggregates()
    assert lacp_map[test_lag_if]['mode'] == 'passive',\
        'LACP mode is not passive'

    associate_vlan_to_lag(ops2, test_vlan, test_lag)

    for curr_ops in [ops1, ops2]:
        # Add interfaces to LAG
        for curr_if in lag_interfaces:
            associate_interface_to_lag(curr_ops, curr_if, test_lag)
        # Interface 4 is connected to one host
        associate_vlan_to_l2_interface(curr_ops, test_vlan, host_interface)

    # Configure host interfaces
    hs1.libs.ip.interface('1', addr='{hs1_addr}/24'.format(**locals()),
                          up=True)
    hs2.libs.ip.interface('1', addr='{hs2_addr}/24'.format(**locals()),
                          up=True)

    print('Sleep few seconds to wait everything is up')
    sleep(60)

    check_connectivity_between_hosts(hs1, hs1_addr, hs2, hs2_addr,
                                     5, True)

    # Test for rates slow and fast
    for lag_rate_mode in ['slow', 'fast']:
        # Number of expected LACP packets (heartbeats)
        heartbeats = (hb_info[lag_rate_mode]['wait_time'] *
                      hb_info[lag_rate_mode]['packets_per_second'])
        # Min percentage according heartbeats
        hb_info[lag_rate_mode]['min_percent'] = (heartbeats - 1) / heartbeats
        hb_info[lag_rate_mode]['max_percent'] += 3 /\
            hb_info[lag_rate_mode]['wait_time']
        # Setting values for slow|fast
        print('Test LAG with {lag_rate_mode} rate'.format(**locals()))
        for c_sw in [ops1, ops2]:
            config_lacp_rate(c_sw, test_lag, lag_rate_mode == 'fast')
        # Sleep to avoid the negotiation muddling up the results
        sleep(15)

        # Listen with tcpdump for LAG interface
        tcp_dump_ops1 = tcpdump_capture_interface_start(
            ops1, test_lag_if
        )

        tcp_dump_ops2 = tcpdump_capture_interface_start(
            ops2, test_lag_if
        )

        assert tcp_dump_ops1 > 0, 'Could not start tcpdump on ops1'
        assert tcp_dump_ops2 > 0, 'Could not start tcpdump on ops2'

        # Get the initial amount of pdus sent through the interfaces of the lag
        # using diag-dump lacp basic command in order to calculate the
        # difference when the sleep has finished.
        ops1_initial_diagdump_pdu = get_average_lacpd_sent_pdus(ops1, test_lag)
        ops2_initial_diagdump_pdu = get_average_lacpd_sent_pdus(ops2, test_lag)

        sleep(hb_info[lag_rate_mode]['wait_time'] + 2)

        # Get the final amount of pdus sent through the interfaces of the lag
        # using diag-dump lacp basic command.
        ops1_final_diagdump_pdu = get_average_lacpd_sent_pdus(ops1, test_lag)
        ops2_final_diagdump_pdu = get_average_lacpd_sent_pdus(ops2, test_lag)

        tcp_data_ops1 = tcpdump_capture_interface_stop(
            ops1, test_lag_if, tcp_dump_ops1
        )

        tcp_data_ops2 = tcpdump_capture_interface_stop(
            ops2, test_lag_if, tcp_dump_ops2
        )
        # Get received LACP packets
        for tcp_data in [tcp_data_ops1, tcp_data_ops2]:

            pac_info = get_counters_from_packet_capture(tcp_data)

            final_result = pac_info['received'] / len(lag_interfaces)

            packets_avg = (final_result / heartbeats)

            assert packets_avg >= hb_info[lag_rate_mode]['min_percent']\
                and packets_avg <= hb_info[lag_rate_mode]['max_percent'],\
                'Packet average for {lag_rate_mode} mode'\
                ' is out of bounds'.format(**locals())

        # Validate the average of pdus obtained using diag-dump lacp basic
        # command
        for diagdump_data in [ops1_final_diagdump_pdu -
                              ops1_initial_diagdump_pdu,
                              ops2_final_diagdump_pdu -
                              ops2_initial_diagdump_pdu]:
            diagdump_pkt_avg = (float(diagdump_data) / heartbeats)

            assert diagdump_pkt_avg >= hb_info[lag_rate_mode]['min_percent']\
                and diagdump_pkt_avg <= hb_info[lag_rate_mode]['max_percent'],\
                'Diag dump packet average is out of bounds'
