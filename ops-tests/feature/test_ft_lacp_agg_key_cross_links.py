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

##########################################################################
# Name:        test_ft_lacp_agg_key_cross_links.py
#
# Objective:   Verify LAGs should be formed independent of port ids as long
#              as aggregation key is the same
#
# Topology:    2 switch (DUT running Halon)
#
##########################################################################


"""
OpenSwitch Test for LACP aggregation key functionality
"""

from pytest import mark
from lacp_lib import(
    LOCAL_STATE,
    associate_interface_to_lag,
    create_lag_active,
    set_lacp_rate_fast,
    turn_on_interface,
    verify_lag_config,
    verify_state_sync_lag,
    verify_turn_on_interfaces
)
import time

TOPOLOGY = """
# +-------+     +-------+
# |  sw1  |-----|  sw2  |
# +-------+     +-------+

# Nodes
[type=openswitch name="Switch 1"] sw1
[type=openswitch name="Switch 2"] sw2

# Links
sw1:1 -- sw2:1
sw1:2 -- sw2:2
sw1:3 -- sw2:3
sw1:4 -- sw2:4
sw1:5 -- sw2:6
sw1:6 -- sw2:7
sw1:7 -- sw2:5
"""

@mark.skipif(True, reason="Skipping due to instability")
@mark.gate
@mark.platform_incompatible(['ostl'])
def test_lacp_agg_key_cross_links(topology, step):
    """
    Case 3:
        Verify LAGs should be formed independent of port ids as long
        as aggregation key is the same
        Initial Topology:
            SW1>
                LAG150 -> Interfaces: 1,5
                LAG250 -> Interfaces: 2,6
                LAG350 -> Interfaces: 3,7
            SW2>
                LAG150 -> Interfaces: 1,6
                LAG250 -> Interfaces: 2,7
                LAG350 -> Interfaces: 3,5
        Expected behaviour:
        All interfaces in all LAGs should be InSync, Collecting
        and Distributing state
    """
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')

    lag_id_1 = '150'
    lag_id_2 = '250'
    lag_id_3 = '350'
    sw_lag_id = [lag_id_1, lag_id_2, lag_id_3]

    assert sw1 is not None
    assert sw2 is not None

    ports_sw1 = list()
    ports_sw2 = list()
    port_labels = ['1', '2', '3', '4', '5', '6', '7']

    step("### Mapping interfaces from Docker ###")
    for port in port_labels:
        ports_sw1.append(sw1.ports[port])
        ports_sw2.append(sw2.ports[port])

    step("#### Turning on interfaces in sw1 ###")
    for port in ports_sw1:
        turn_on_interface(sw1, port)

    step("#### Turning on interfaces in sw2 ###")
    for port in ports_sw2:
        turn_on_interface(sw2, port)

    step("#### Validate interfaces are turn on ####")
    verify_turn_on_interfaces(sw1, ports_sw1)
    verify_turn_on_interfaces(sw2, ports_sw2)

    mac_addr_sw1 = sw1.libs.vtysh.show_interface(1)['mac_address']
    mac_addr_sw2 = sw2.libs.vtysh.show_interface(1)['mac_address']
    assert mac_addr_sw1 != mac_addr_sw2, \
        'Mac address of interfaces in sw1 is equal to mac address of ' + \
        'interfaces in sw2. This is a test framework problem. Dynamic ' + \
        'LAGs cannot work properly under this condition. Refer to Taiga ' + \
        'issue #1251.'

    step("Create LAGs (150, 250 and 350) in both switches")
    for lag in sw_lag_id:
        create_lag_active(sw1, lag)
        create_lag_active(sw2, lag)
        set_lacp_rate_fast(sw1, lag)
        set_lacp_rate_fast(sw2, lag)

    p11 = sw1.ports['1']
    p12 = sw1.ports['2']
    p13 = sw1.ports['3']
    p15 = sw1.ports['5']
    p16 = sw1.ports['6']
    p17 = sw1.ports['7']
    p21 = sw2.ports['1']
    p22 = sw2.ports['2']
    p23 = sw2.ports['3']
    p25 = sw2.ports['5']
    p26 = sw2.ports['6']
    p27 = sw2.ports['7']

    step("#### Associate Interfaces to LAG ####")
    step("Associate interfaces with LAG in switch1")
    associate_interface_to_lag(sw1, p11, lag_id_1)
    associate_interface_to_lag(sw1, p15, lag_id_1)
    associate_interface_to_lag(sw1, p12, lag_id_2)
    associate_interface_to_lag(sw1, p16, lag_id_2)
    associate_interface_to_lag(sw1, p13, lag_id_3)
    associate_interface_to_lag(sw1, p17, lag_id_3)

    step("Associate interfaces with LAG in switch2")
    associate_interface_to_lag(sw2, p21, lag_id_1)
    associate_interface_to_lag(sw2, p26, lag_id_1)
    associate_interface_to_lag(sw2, p22, lag_id_2)
    associate_interface_to_lag(sw2, p27, lag_id_2)
    associate_interface_to_lag(sw2, p23, lag_id_3)
    associate_interface_to_lag(sw2, p25, lag_id_3)

    step("#### Verify LAG configuration ####")
    verify_lag_config(
        sw1, lag_id_1, [p11, p15], mode='active', heartbeat_rate='fast')
    verify_lag_config(
        sw1, lag_id_2, [p12, p16], mode='active', heartbeat_rate='fast')
    verify_lag_config(
        sw1, lag_id_3, [p13, p17], mode='active', heartbeat_rate='fast')
    verify_lag_config(
        sw2, lag_id_1, [p21, p26], mode='active', heartbeat_rate='fast')
    verify_lag_config(
        sw2, lag_id_2, [p22, p27], mode='active', heartbeat_rate='fast')
    verify_lag_config(
        sw2, lag_id_3, [p23, p25], mode='active', heartbeat_rate='fast')

    ports_sw1.remove('4')
    ports_sw2.remove('4')
    step("Validate correct state in switch1 for interfaces")
    verify_state_sync_lag(sw1, ports_sw1, LOCAL_STATE, 'active')
    step("Validate correct state in switch2 for interfaces")
    verify_state_sync_lag(sw2, ports_sw2, LOCAL_STATE, 'active')
