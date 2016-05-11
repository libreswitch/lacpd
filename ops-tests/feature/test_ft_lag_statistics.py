# (C) Copyright 2016 Hewlett Packard Enterprise Development LP
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
###############################################################################
# Name:        test_ft_lag_statistics.py
#
# Description: Tests that a configured static Link Aggregation can summarize
#              accurate individual interfaces statistics as they change
#
# Author:      Jose Hernandez
#
# Topology:  |Host| ----- |Switch| ---------------------- |Switch| ----- |Host|
#                                   (Static LAG - 3 links)
#
# Success Criteria:  PASS -> Statistics are accurate and represent the sum of
#                            each individual member
#
#                    FAILED -> Information from the statistics does not
#                              represent the sum of each individual member
#
###############################################################################

from time import sleep
from lacp_lib import create_lag
from lacp_lib import turn_on_interface
from lacp_lib import validate_turn_on_interfaces
from lacp_lib import associate_interface_to_lag
from lacp_lib import verify_lag_config
from lacp_lib import create_vlan
from lacp_lib import verify_vlan_full_state
from lacp_lib import check_connectivity_between_hosts
from lacp_lib import associate_vlan_to_l2_interface
from lacp_lib import associate_vlan_to_lag
from lacp_lib import verify_lag_interface_key
from lacp_lib import verify_lag_interface_priority
from lacp_lib import verify_lag_interface_id
from lacp_lib import verify_lag_interface_system_id
from lacp_lib import verify_lag_interface_system_priority
from lacp_lib import verify_lag_interface_lag_id
from lacp_lib import validate_lag_state_static
from lacp_lib import LOCAL_STATE
from lacp_lib import REMOTE_STATE
from lacp_lib import retry_wrapper
from lacp_lib import compare_lag_interface_basic_settings
import pytest

TOPOLOGY = """
#            +-----------------+
#            |                 |
#            |      Host 1     |
#            |                 |
#            +-----------------+
#                     |
#                     |
#     +-------------------------------+
#     |                               |
#     |                               |
#     |            Switch 1           |
#     |                               |
#     +-------------------------------+
#          |         |        |
#          |         |        |
#          |         |        |
#     +-------------------------------+
#     |                               |
#     |                               |
#     |            Switch 2           |
#     |                               |
#     +-------------------------------+
#                     |
#                     |
#            +-----------------+
#            |                 |
#            |     Host 2      |
#            |                 |
#            +-----------------+

# Nodes
[type=openswitch name="Switch 1"] sw1
[type=openswitch name="Switch 2"] sw2
[type=host name="Host 1" image="openswitch/ubuntutest:latest"] hs1
[type=host name="Host 2" image="openswitch/ubuntutest:latest"] hs2

# Links

sw1:1 -- hs1:1
sw2:1 -- hs2:1
sw1:2 -- sw2:2
sw1:3 -- sw2:3
sw1:4 -- sw2:4
"""

# Global variables
SW_LBL_PORTS = ['1', '2', '3', '4']
LAG_ID = '1'
LAG_VLAN = 900
NETWORK = '10.90.0.'
NETMASK = '24'
NUMBER_PINGS = 5
BASE_IPERF_PORT = 5000
IPERF_TIME = 30
IPERF_BW = '10m'
SW_COUNTERS_DELAY = 20


def compare_values(value1, value2, error=0.02):
    value2_max = value2 * (1 + error)
    value2_min = value2 * (1 - error)
    assert value1 <= value2_max, ' '.join(
        ['Value of {} is more than {}'.format(value1, error * 100),
         'percent higher than {}'.format(value2)]
    )
    assert value1 >= value2_min, ' '.join(
        ['Value of {} is more than {}'.format(value1, error * 100),
         'percent lower than {}'.format(value2)]
    )


def compare_switches_counters(sw_list, stats):
    # Verify switches counters match
    for port in SW_LBL_PORTS:
        print('Compare port {} between switches'.format(port))
        print('rx from sw1 vs tx from sw2')
        compare_values(
            int(stats[sw_list[0]][port]['{}_packets'.format('rx')]),
            int(stats[sw_list[1]][port]['{}_packets'.format('tx')])
        )
        print('tx from sw1 vs rx from sw2')
        compare_values(
            int(stats[sw_list[0]][port]['{}_packets'.format('tx')]),
            int(stats[sw_list[1]][port]['{}_packets'.format('rx')])
        )


def compare_lag_to_switches_counters(sw_stats, lag_stats, ports):
    for param in [
        'rx_bytes',
        'rx_packets',
        'rx_crc_fcs',
        'rx_dropped',
        'rx_error',
        'tx_packets',
        'tx_bytes',
        'tx_collisions',
        'tx_dropped',
        'tx_errors',
        'speed'
    ]:
        total = 0
        for port in ports:
            total += int(sw_stats[port][param])
        print('Verifying LAG interface value for {}'.format(param))
        compare_values(int(lag_stats[param]), total)
    for port in ports:
        assert lag_stats['speed_unit'] == sw_stats[port]['speed_unit'],\
            'Unexpected change in speed unit {}, Expected {}'.format(
            lag_stats['speed_unit'],
            sw_stats[port]['speed_unit']
        )


def verify_lacp_state(
    sw1,
    sw2
):
    lacp_def_value = ''
    lacp_def_priority = ''
    sys_id = ''
    print('Verify LACP state on LAG members')
    for port in SW_LBL_PORTS[1:]:
        sw1_lacp_state = sw1.libs.vtysh.show_lacp_interface(port)
        sw2_lacp_state = sw2.libs.vtysh.show_lacp_interface(port)
        sw_lacp_states = [sw1_lacp_state, sw2_lacp_state]
        for (
            sw_lacp_state,
            rev_sw_lacp_state
        ) in zip(
            sw_lacp_states,
            reversed(sw_lacp_states)
        ):
            verify_lag_interface_lag_id(sw_lacp_state)
            verify_lag_interface_key(
                sw_lacp_state,
                rev_sw_lacp_state,
                key=lacp_def_value,
                value_check=True,
                cross_check=True
            )
            verify_lag_interface_priority(
                sw_lacp_state,
                rev_sw_lacp_state,
                priority=lacp_def_value,
                value_check=True,
                cross_check=True
            )
            verify_lag_interface_system_priority(
                sw_lacp_state,
                sw2_int_map_lacp=rev_sw_lacp_state,
                system_priority=lacp_def_priority,
                value_check=True,
                cross_check=True
            )
            verify_lag_interface_system_id(
                sw_lacp_state,
                sw2_int_map_lacp=rev_sw_lacp_state,
                system_id=sys_id,
                value_check=True,
                cross_check=True
            )
            verify_lag_interface_id(
                sw_lacp_state,
                rev_sw_lacp_state,
                id='',
                value_check=True,
                cross_check=True
            )
            validate_lag_state_static(sw_lacp_state, LOCAL_STATE)
            validate_lag_state_static(sw_lacp_state, REMOTE_STATE)


def enable_switches_interfaces(sw_list, step):
    step('Enable switches interfaces')
    for sw in sw_list:
        for port in SW_LBL_PORTS:
            turn_on_interface(sw, port)
    # Defining internal method to use decorator

    @retry_wrapper(
        'Ensure interfaces are turned on',
        'Interfaces not yet ready',
        5,
        60)
    def internal_check_interfaces(sw_list):
        for sw in sw_list:
            validate_turn_on_interfaces(sw, SW_LBL_PORTS)
    internal_check_interfaces(sw_list)


def configure_lags(sw_list, sw_real_ports, step):
    step('Create LAGs')
    for sw in sw_list:
        create_lag(sw, LAG_ID, 'off')
        for port in sw_real_ports[sw][1:]:
            associate_interface_to_lag(sw, port, LAG_ID)
        verify_lag_config(
            sw,
            LAG_ID,
            sw_real_ports[sw][1:]
        )
    check_func = retry_wrapper(
        'Verify LACP status on both devices',
        'Configuration not yet applied',
        2,
        4
    )(verify_lacp_state)
    check_func(
        sw_list[0],
        sw_list[1]
    )


def configure_vlans(sw_list, sw_real_ports, step):
    step('Configure VLANs on devices')
    for sw in sw_list:
        # Create VLAN
        create_vlan(sw, LAG_VLAN)
        # Associate VLAN to LAG

        associate_vlan_to_lag(sw, str(LAG_VLAN), LAG_ID)
        # Associate VLAN to host interface
        associate_vlan_to_l2_interface(
            sw,
            str(LAG_VLAN),
            sw_real_ports[sw][0]
        )
        # Verify VLAN configuration was successfully applied
        verify_vlan_full_state(
            sw,
            LAG_VLAN, interfaces=[
                sw_real_ports[sw][0],
                'lag{}'.format(LAG_ID)
            ]
        )


def configure_workstations(hs_list, step):
    step('Configure workstations')
    for hs_num, hs in enumerate(hs_list):
        hs.libs.ip.interface(
            SW_LBL_PORTS[0],
            addr='{}{}/{}'.format(NETWORK, hs_num + 1, NETMASK),
            up=True
        )


def validate_connectivity(hs_list, wait, step):
    step('Check workstations connectivity')
    if wait is False:
        check_connectivity_between_hosts(
            hs_list[0],
            '{}{}'.format(NETWORK, 1),
            hs_list[1],
            '{}{}'.format(NETWORK, 2),
            NUMBER_PINGS,
            True
        )
    else:
        check_func = retry_wrapper(
            'Verifying workstations connectivity',
            'Configuration not yet applied',
            5,
            15
        )(check_connectivity_between_hosts)
        check_func(
            hs_list[0],
            '{}{}'.format(NETWORK, 1),
            hs_list[1],
            '{}{}'.format(NETWORK, 2),
            NUMBER_PINGS,
            True
        )


def verify_lag_statistics(sw_list, hs_list, sw_real_ports, step):
    sw_stats_before = {}
    sw_stats_after = {}
    step('Verify LAG statistics')
    print('Collect all switch ports statistics')
    for sw in sw_list:
        sw_stats_before[sw] = {}
        for port in SW_LBL_PORTS:
            sw_stats_before[sw][port] = sw.libs.vtysh.show_interface(port)
        sw_stats_before[sw]['lag{}'.format(LAG_ID)] =\
            sw.libs.vtysh.show_interface('lag{}'.format(LAG_ID))
        print('Verify LAG statistic interface basic settings')
        compare_lag_interface_basic_settings(
            sw_stats_before[sw]['lag{}'.format(LAG_ID)],
            LAG_ID,
            sw_real_ports[sw][1:]
        )
        print('Compare LAG counters vs switches interfaces')
        compare_lag_to_switches_counters(
            sw_stats_before[sw],
            sw_stats_before[sw]['lag{}'.format(LAG_ID)],
            SW_LBL_PORTS[1:]
        )
    print('Start iperf servers')
    for i, hs in enumerate(hs_list):
        hs.libs.iperf.server_start(BASE_IPERF_PORT + i, udp=True)
    print('Start traffic transmission')
    for hs, other_base in zip(hs_list, [2, 1]):
        hs.libs.iperf.client_start(
            '{}{}'.format(NETWORK, other_base),
            BASE_IPERF_PORT + other_base - 1,
            time=IPERF_TIME,
            udp=True,
            bandwidth=IPERF_BW
        )
    print('Wait for traffic to finish in {} seconds'.format(IPERF_TIME))
    sleep(IPERF_TIME)
    print('Stop iperf')
    for hs in hs_list:
        hs.libs.iperf.server_stop()
        hs.libs.iperf.client_stop()

    @retry_wrapper(
        'Obtain interfaces information and verify their consistency',
        'Information provided by counters is not yet reliable',
        5,
        SW_COUNTERS_DELAY)
    def internal_check():
        print('Get statistics from switches')
        for sw in sw_list:
            sw_stats_after[sw] = {}
            for port in SW_LBL_PORTS:
                sw_stats_after[sw][port] = sw.libs.vtysh.show_interface(port)
            sw_stats_after[sw]['lag{}'.format(LAG_ID)] =\
                sw.libs.vtysh.show_interface('lag{}'.format(LAG_ID))
        print('Verify obtained information is consistent')
        print('Compare counters between siwtches')
        compare_switches_counters(sw_list, sw_stats_after)
    internal_check()
    print('Compare switch counters individually')
    for sw in sw_list:
        print('Verify LAG statistic interface basic settings')
        compare_lag_interface_basic_settings(
            sw_stats_after[sw]['lag{}'.format(LAG_ID)],
            LAG_ID,
            sw_real_ports[sw][1:]
        )
        print('Compare LAG counters vs switches interfaces')
        compare_lag_to_switches_counters(
            sw_stats_after[sw],
            sw_stats_after[sw]['lag{}'.format(LAG_ID)],
            SW_LBL_PORTS[1:]
        )


@pytest.mark.skipif(True, reason="Skipping due to instability")
def test_ft_lag_statistics(topology, step):
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')

    assert hs1 is not None, 'hs1 was not initialized'
    assert hs2 is not None, 'hs2 was not initialized'
    assert sw1 is not None, 'sw1 was not initialized'
    assert sw2 is not None, 'sw2 was not initialized'

    sw_real_ports = {
        sw1: [sw1.ports[port] for port in SW_LBL_PORTS],
        sw2: [sw2.ports[port] for port in SW_LBL_PORTS]
    }

    # Enable switches interfaces
    enable_switches_interfaces([sw1, sw2], step)

    # Configure static LAGs with members
    configure_lags([sw1, sw2], sw_real_ports, step)

    # Add VLAN configuration to LAGs and workstation interfaces
    configure_vlans([sw1, sw2], sw_real_ports, step)

    # Configure workstations
    configure_workstations([hs1, hs2], step)

    # Validate workstations can communicate
    validate_connectivity([hs1, hs2], True, step)

    # Obtain and validate LAG statistics
    verify_lag_statistics([sw1, sw2], [hs1, hs2], sw_real_ports, step)
