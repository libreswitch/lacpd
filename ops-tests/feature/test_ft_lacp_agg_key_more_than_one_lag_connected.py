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
# Name:        test_ft_lacp_agg_key_more_than_one_lag_connected.py
#
# Objective:   Verify only interfaces associated with the same
#              aggregation key get to Collecting/Distributing state
#
# Topology:    2 switch (DUT running Halon)
#
##########################################################################


"""
OpenSwitch Test for LACP aggregation key functionality
"""
import time
from lacp_lib import (
    associate_interface_to_lag,
    create_lag_active,
    DIAG_DUMP_LOCAL_STATE,
    LOCAL_STATE,
    set_lacp_rate_fast,
    turn_on_interface,
    validate_diagdump_lacp_interfaces,
    validate_diagdump_lag_state_afn,
    validate_diagdump_lag_state_out_sync,
    validate_diagdump_lag_state_sync,
    validate_lag_state_afn,
    validate_lag_state_out_of_sync,
    validate_lag_state_sync,
    verify_lag_config,
    verify_turn_on_interfaces
)
import pytest

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
"""


# Executes diag-dump lacp basic command and gets the LACP state
def get_diagdump_lacp_state(sw):
    output = sw.libs.vtysh.diag_dump_lacp_basic()
    return output['State']


# Executes diag-dump lacp basic command and gets the LAG interfaces
def get_diagdump_lacp_interfaces(sw):
    output = sw.libs.vtysh.diag_dump_lacp_basic()
    return output['Interfaces']


@pytest.mark.skipif(True, reason="Skipping due to instability")
def test_lacp_agg_key_more_than_one_lag_connected(topology):
    """
    Case 2:
        Verify only interfaces associated with the same
        aggregation key get to Collecting/Distributing state
        Initial Topology:
            SW1>
                LAG150 -> Interfaces: 1,2,3,4
            SW2>
                LAG150 -> Interfaces: 1,2
                LAG400 -> Interfaces: 3,4
        Expected behaviour:
            Interfaces 1 and 2 in both switches get state Active, InSync,
            Collecting and Distributing. Interfaces 3 and 4 should get state
            Active, OutOfSync, Collecting and Distributing
    """
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    sw1_lag_id = '150'
    sw2_lag_id = '150'
    sw2_lag_id_2 = '400'

    assert sw1 is not None
    assert sw2 is not None

    ports_sw1 = list()
    ports_sw2 = list()
    port_labels = ['1', '2', '3', '4']

    step("Mapping interfaces")
    for port in port_labels:
        ports_sw1.append(sw1.ports[port])
        ports_sw2.append(sw2.ports[port])

    step("Sorting the port list")
    ports_sw1.sort()
    ports_sw2.sort()

    step("Turning on all interfaces used in this test")
    for port in ports_sw1:
        turn_on_interface(sw1, port)

    for port in ports_sw2:
        turn_on_interface(sw2, port)

    step("Verify interfaces to turn on")
    verify_turn_on_interfaces(sw1, ports_sw1)
    verify_turn_on_interfaces(sw2, ports_sw2)

    mac_addr_sw1 = sw1.libs.vtysh.show_interface('1')['mac_address']
    mac_addr_sw2 = sw2.libs.vtysh.show_interface('1')['mac_address']
    assert mac_addr_sw1 != mac_addr_sw2, \
        'Mac address of interfaces in sw1 is equal to mac address of ' + \
        'interfaces in sw2. This is a test framework problem. Dynamic ' + \
        'LAGs cannot work properly under this condition. Refer to Taiga ' + \
        'issue #1251.'

    step("Create LAG in both switches")
    create_lag_active(sw1, sw1_lag_id)
    create_lag_active(sw2, sw2_lag_id)
    create_lag_active(sw2, sw2_lag_id_2)
    set_lacp_rate_fast(sw1, sw1_lag_id)
    set_lacp_rate_fast(sw2, sw2_lag_id)
    set_lacp_rate_fast(sw2, sw2_lag_id_2)

    step("Associate interfaces to lag in both switches")
    for interface in ports_sw1:
        associate_interface_to_lag(sw1, interface, sw1_lag_id)

    for interface in ports_sw2[0:2]:
        associate_interface_to_lag(sw2, interface, sw2_lag_id)

    for interface in ports_sw2[2:4]:
        associate_interface_to_lag(sw2, interface, sw2_lag_id_2)

    step("#### Verify LAG configuration ####")
    verify_lag_config(sw1, sw1_lag_id, ports_sw1, 'fast', mode='active')
    verify_lag_config(sw2, sw2_lag_id, ports_sw2[0:2], 'fast', mode='active')
    verify_lag_config(sw2, sw2_lag_id_2, ports_sw2[2:4], 'fast', mode='active')

    step("Get the configured interfaces for each LAG using diag-dump " +
         "lacp basic in both switches")
    time.sleep(60)
    sw1_lacp_interfaces = get_diagdump_lacp_interfaces(sw1)
    sw2_lacp_interfaces = get_diagdump_lacp_interfaces(sw2)
    validate_diagdump_lacp_interfaces(sw1_lacp_interfaces,
                                      sw1_lag_id,
                                      ports_sw1,
                                      ports_sw1,
                                      [ports_sw1[0], ports_sw1[1]])
    validate_diagdump_lacp_interfaces(sw2_lacp_interfaces, sw2_lag_id,
                                      [ports_sw2[0], ports_sw2[1]],
                                      [ports_sw2[0], ports_sw2[1]],
                                      [ports_sw2[0], ports_sw2[1]])
    validate_diagdump_lacp_interfaces(sw2_lacp_interfaces, sw2_lag_id_2,
                                      [ports_sw2[2], ports_sw2[3]],
                                      [ports_sw2[2], ports_sw2[3]], [])

    step("Get information for LAG in interface 1 with both switches")
    map_lacp_sw1_p11 = sw1.libs.vtysh.show_lacp_interface(ports_sw1[0])
    map_lacp_sw1_p12 = sw1.libs.vtysh.show_lacp_interface(ports_sw1[1])
    map_lacp_sw1_p13 = sw1.libs.vtysh.show_lacp_interface(ports_sw1[2])
    map_lacp_sw1_p14 = sw1.libs.vtysh.show_lacp_interface(ports_sw1[3])
    map_lacp_sw2_p21 = sw2.libs.vtysh.show_lacp_interface(ports_sw2[0])
    map_lacp_sw2_p22 = sw2.libs.vtysh.show_lacp_interface(ports_sw2[1])
    map_lacp_sw2_p23 = sw2.libs.vtysh.show_lacp_interface(ports_sw2[2])
    map_lacp_sw2_p24 = sw2.libs.vtysh.show_lacp_interface(ports_sw2[3])

    step("Get the state of LAGs using diag-dump lacp basic in both switches")
    sw1_lacp_state = get_diagdump_lacp_state(sw1)
    sw2_lacp_state = get_diagdump_lacp_state(sw2)

    # Recast labels to work with the library output
    ports_sw1[:] = [int(x) if x.isdigit() else x for x in ports_sw1]
    ports_sw2[:] = [int(x) if x.isdigit() else x for x in ports_sw2]

    map_diagdump_lacp_sw1_p11 = sw1_lacp_state[str(sw1_lag_id)][
        ports_sw1[0]]
    map_diagdump_lacp_sw1_p12 = sw1_lacp_state[str(sw1_lag_id)][
        ports_sw1[1]]
    map_diagdump_lacp_sw1_p13 = sw1_lacp_state[str(sw1_lag_id)][
        ports_sw1[2]]
    map_diagdump_lacp_sw1_p14 = sw1_lacp_state[str(sw1_lag_id)][
        ports_sw1[3]]
    map_diagdump_lacp_sw2_p21 = sw2_lacp_state[str(sw2_lag_id)][
        ports_sw2[0]]
    map_diagdump_lacp_sw2_p22 = sw2_lacp_state[str(sw2_lag_id)][
        ports_sw2[1]]
    map_diagdump_lacp_sw2_p23 = sw2_lacp_state[str(sw2_lag_id_2)][
        ports_sw2[2]]
    map_diagdump_lacp_sw2_p24 = sw2_lacp_state[str(sw2_lag_id_2)][
        ports_sw2[3]]

    step("Validate the LAG was created in both switches")
    validate_lag_state_sync(map_lacp_sw1_p11, LOCAL_STATE)
    validate_lag_state_sync(map_lacp_sw1_p12, LOCAL_STATE)
    validate_lag_state_out_of_sync(map_lacp_sw1_p13,
                                   LOCAL_STATE)
    validate_lag_state_out_of_sync(map_lacp_sw1_p14,
                                   LOCAL_STATE)

    validate_lag_state_sync(map_lacp_sw2_p21, LOCAL_STATE)
    validate_lag_state_sync(map_lacp_sw2_p22, LOCAL_STATE)
    validate_lag_state_afn(map_lacp_sw2_p23, LOCAL_STATE)
    validate_lag_state_afn(map_lacp_sw2_p24, LOCAL_STATE)

    # Validate diag-dump results
    validate_diagdump_lag_state_sync(map_diagdump_lacp_sw1_p11,
                                     DIAG_DUMP_LOCAL_STATE)
    validate_diagdump_lag_state_sync(map_diagdump_lacp_sw1_p12,
                                     DIAG_DUMP_LOCAL_STATE)
    validate_diagdump_lag_state_out_sync(map_diagdump_lacp_sw1_p13,
                                         DIAG_DUMP_LOCAL_STATE)
    validate_diagdump_lag_state_out_sync(map_diagdump_lacp_sw1_p14,
                                         DIAG_DUMP_LOCAL_STATE)

    validate_diagdump_lag_state_sync(map_diagdump_lacp_sw2_p21,
                                     DIAG_DUMP_LOCAL_STATE)
    validate_diagdump_lag_state_sync(map_diagdump_lacp_sw2_p22,
                                     DIAG_DUMP_LOCAL_STATE)
    validate_diagdump_lag_state_afn(map_diagdump_lacp_sw2_p23,
                                    DIAG_DUMP_LOCAL_STATE)
    validate_diagdump_lag_state_afn(map_diagdump_lacp_sw2_p24,
                                    DIAG_DUMP_LOCAL_STATE)
