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

"""OpenSwitch Test for LACPD heartbeat configurations."""

from time import sleep
from pytest import mark, fixture
from lacp_lib import (
    associate_interface_to_lag,
    associate_vlan_to_l2_interface,
    associate_vlan_to_lag,
    config_lacp_rate,
    create_lag_active,
    create_lag_passive,
    create_vlan,
    get_counters_from_packet_capture,
    tcpdump_capture_interface_start,
    tcpdump_capture_interface_stop,
    turn_on_interface,
    verify_connectivity_between_hosts,
    verify_show_lacp_aggregates,
    verify_turn_on_interfaces
)


TOPOLOGY = """
# +-------+                                  +-------+
# |       |    +--------+  LAG  +--------+   |       |
# |  hs1  <---->   sw1  <------->   sw2  <--->  hs2  |
# |       |    |   A    <------->    P   |   |       |
# +-------+    |        <------->        |   +-------+
#              +--------+       +--------+

# Nodes
[type=openswitch name="OpenSwitch 1 LAG active"] sw1
[type=openswitch name="OpenSwitch 2 LAG passive"] sw2
[type=host name="Host 1"] hs1
[type=host name="Host 2"] hs2

# Links
sw1:3 -- hs1:1
sw1:1 -- sw2:1
sw1:2 -- sw2:2
hs2:1 -- sw2:3
"""

# Ports
port_labels = ['1', '2', '3']

# VID for testing
test_vlan = '2'

# LAG ID for testing
test_lag = '2'
test_lag_if = 'lag' + test_lag

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
    'slow': {
        'packets_per_second': (1/30),
        'min_percent': 0,
        'max_percent': 1,
        'wait_time': 90,
    },
    'fast': {
        'packets_per_second': 1,
        'min_percent': 0,
        'max_percent': 1,
        'wait_time': 10
    }
}


def get_average_lacpd_sent_pdus(sw, lag_id):
    """Get average lacpd sent pdus.

    Runs the diag-dump lacp basic command and gets the average of pdus sent
    through all the interfaces of the lag.
    """
    output = sw.libs.vtysh.diag_dump_lacp_basic()
    pdu_dict = output['Counters']
    sent_pdus_sum = 0
    count = 0
    for interface_number in pdu_dict[lag_id]:
        sent_pdus_sum += pdu_dict[lag_id][interface_number]['lacp_pdus_sent']
        count += 1
    return sent_pdus_sum/count


@fixture(scope='module')
def main_setup(request, topology):
    """Test Case common configuration."""
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')

    print('Verifying switches are not None')
    assert sw1 is not None, 'Topology failed getting object sw1'
    assert sw2 is not None, 'Topology failed getting object sw2'

    print('Verifying hosts are not None')
    assert hs1 is not None, 'Topology failed getting object hs1'
    assert hs2 is not None, 'Topology failed getting object hs2'

    ports_sw1 = list()
    ports_sw2 = list()

    print("Mapping interfaces")
    for port in port_labels:
        ports_sw1.append(sw1.ports[port])
        ports_sw2.append(sw2.ports[port])

    print("Turning on all interfaces used in this test")
    for port in ports_sw1:
        turn_on_interface(sw1, port)

    for port in ports_sw2:
        turn_on_interface(sw2, port)

    print("Validate interfaces are turn on")
    verify_turn_on_interfaces(sw1, ports_sw1)
    verify_turn_on_interfaces(sw2, ports_sw2)

    print('Creating VLAN (%s) on Switch 1' % test_vlan)
    create_vlan(sw1, test_vlan)

    print('Creating VLAN (%s) on Switch 2' % test_vlan)
    create_vlan(sw2, test_vlan)

    print('Creating LAG (%s) active on Switch 1' % test_lag)
    create_lag_active(sw1, test_lag)

    print('Executing "show lacp aggregates" on Switch 1')
    verify_show_lacp_aggregates(sw1, test_lag_if, 'active')

    print('Associating VLAN (%s) to LAG (%s)' % (test_vlan, test_lag))
    associate_vlan_to_lag(sw1, test_vlan, test_lag)

    print('Creating LAG (%s) passive on Switch 2' % test_lag)
    create_lag_passive(sw2, test_lag)

    print('Executing "show lacp aggregates" on Switch 2')
    verify_show_lacp_aggregates(sw2, test_lag_if, 'passive')

    print('Associating VLAN (%s) to LAG (%s)' % (test_vlan, test_lag))
    associate_vlan_to_lag(sw2, test_vlan, test_lag)

    print('Configuring IP address to Host 1')
    hs1.libs.ip.interface('1',
                          addr='%s/24' % hs1_addr,
                          up=True)

    print('Configuring IP address to Host 2')
    hs2.libs.ip.interface('1',
                          addr='%s/24' % hs2_addr,
                          up=True)

    print("Associate interfaces to LAG in both switches")
    for port in ports_sw1[0:2]:
        associate_interface_to_lag(sw1, port, test_lag)
    for port in ports_sw2[0:2]:
        associate_interface_to_lag(sw2, port, test_lag)

    for switch in [sw1, sw2]:
        # Interface 4 is connected to one host
        associate_vlan_to_l2_interface(switch, test_vlan, switch.ports['3'])

    # Adding small delay to compensate for framework delay
    sleep(10)
    print('Verify connectivity between hosts')
    verify_connectivity_between_hosts(hs1, hs1_addr, hs2, hs2_addr, True)


@mark.gate
@mark.platform_incompatible(['docker'])
def test_lacpd_heartbeat(topology, main_setup, step):
    """Test LACP heartbeat average rate (slow/fast).

    If LACP configured with fast rate a lacpdu is sent every second,
    if configured with slow rate a lacpdu is sent every 30 seconds.
    """
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')

    step('Verifying switches are not None')
    assert sw1 is not None, 'Topology failed getting object sw1'
    assert sw2 is not None, 'Topology failed getting object sw2'

    step('Verifying hosts are not None')
    assert hs1 is not None, 'Topology failed getting object hs1'
    assert hs2 is not None, 'Topology failed getting object hs2'

    # Test for rates slow and fast
    for lag_rate_mode in ['slow', 'fast']:
        # Number of expected LACP packets (heartbeats)
        heartbeats = (hb_info[lag_rate_mode]['wait_time'] *
                      hb_info[lag_rate_mode]['packets_per_second'])

        # Min percentage according heartbeats
        hb_info[lag_rate_mode]['min_percent'] = (heartbeats - 1) / heartbeats
        hb_info[lag_rate_mode]['max_percent'] += 5 /\
            hb_info[lag_rate_mode]['wait_time']

        # Setting values for slow|fast
        print('Test LAG with {lag_rate_mode} rate'.format(**locals()))
        for switch in [sw1, sw2]:
            config_lacp_rate(switch, test_lag, lag_rate_mode == 'fast')

        step('Sleep to avoid the negotiation muddling up the results')
        sleep(15)

        # Listen with tcpdump for LAG interface
        step('Starting TCP dump capture on Switch 1')
        tcp_dump_sw1 = tcpdump_capture_interface_start(sw1, test_lag_if)
        assert tcp_dump_sw1 > 0, 'Could not start tcpdump on sw1'

        step('Starting TCP dump capture on Switch 2')
        tcp_dump_sw2 = tcpdump_capture_interface_start(sw2, test_lag_if)
        assert tcp_dump_sw2 > 0, 'Could not start tcpdump on ops2'

        # Get the initial amount of pdus sent through the interfaces of the lag
        # using diag-dump lacp basic command in order to calculate the
        # difference when the sleep has finished.
        sw1_initial_diagdump_pdu = get_average_lacpd_sent_pdus(sw1, test_lag)
        sw2_initial_diagdump_pdu = get_average_lacpd_sent_pdus(sw2, test_lag)

        step('Waiting for some pdus to be sent')
        sleep(hb_info[lag_rate_mode]['wait_time'])

        # Get the final amount of pdus sent through the interfaces of the lag
        # using diag-dump lacp basic command.
        sw1_final_diagdump_pdu = get_average_lacpd_sent_pdus(sw1, test_lag)
        sw2_final_diagdump_pdu = get_average_lacpd_sent_pdus(sw2, test_lag)

        step('Stopping TCP dump capture on Switch 1')
        tcp_data_sw1 = tcpdump_capture_interface_stop(sw1,
                                                      test_lag_if,
                                                      tcp_dump_sw1)

        step('Stopping TCP dump capture on Switch 2')
        tcp_data_sw2 = tcpdump_capture_interface_stop(sw2,
                                                      test_lag_if,
                                                      tcp_dump_sw2)

        # Get received LACP packets
        for tcp_data in [tcp_data_sw1, tcp_data_sw2]:

            pac_info = get_counters_from_packet_capture(tcp_data)

            final_result = pac_info['received'] / len(port_labels[0:2])

            packets_avg = (final_result / heartbeats)

            assert packets_avg >= hb_info[lag_rate_mode]['min_percent']\
                and packets_avg <= hb_info[lag_rate_mode]['max_percent'],\
                'Packet average for {lag_rate_mode} mode'\
                ' is out of bounds'.format(**locals())

        # Validate the average of pdus obtained using diag-dump lacp basic
        # command
        for diagdump_data in [sw1_final_diagdump_pdu -
                              sw1_initial_diagdump_pdu,
                              sw2_final_diagdump_pdu -
                              sw2_initial_diagdump_pdu]:
            diagdump_pkt_avg = (float(diagdump_data) / heartbeats)

            assert diagdump_pkt_avg >= hb_info[lag_rate_mode]['min_percent']\
                and diagdump_pkt_avg <= hb_info[lag_rate_mode]['max_percent'],\
                'Diag dump packet average is out of bounds'
