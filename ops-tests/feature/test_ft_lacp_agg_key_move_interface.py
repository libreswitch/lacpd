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
# Name:        test_ft_lacp_agg_key_move_interface.py
#
# Objective:   Verify aggregation key functionality when
#              interface is moved to another LAG in one of
#              the switches from the topology
#
# Topology:    2 switch (DUT running Halon)
#
##########################################################################


"""
OpenSwitch Test for LACP aggregation key functionality
"""

import time
from lacp_lib import create_lag_active
from lacp_lib import associate_interface_to_lag
from lacp_lib import turn_on_interface
from lacp_lib import validate_local_key
from lacp_lib import validate_remote_key
from lacp_lib import validate_lag_name
from lacp_lib import validate_lag_state_sync
from lacp_lib import validate_lag_state_out_of_sync
from lacp_lib import validate_lag_state_afn
from lacp_lib import validate_lag_state_default_neighbor
from lacp_lib import validate_diagdump_lag_state_sync
from lacp_lib import validate_diagdump_lag_state_out_sync
from lacp_lib import validate_diagdump_lag_state_afn
from lacp_lib import validate_diagdump_lacp_interfaces
from lacp_lib import LOCAL_STATE
from lacp_lib import REMOTE_STATE
from lacp_lib import DIAG_DUMP_LOCAL_STATE
from lacp_lib import DIAG_DUMP_REMOTE_STATE
from lacp_lib import set_lacp_rate_fast
from lacp_lib import validate_turn_on_interfaces
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
sw1:5 -- sw2:6
sw1:6 -- sw2:7
sw1:7 -- sw2:5
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
def test_lacp_agg_key_move_interface(topology):
    """
    Case 1:
        Verify aggregation key functionality when
        interface is moved to another LAG in one of
        the switches from the topology
        Initial Topology:
            SW1>
                LAG100 -> Interfaces: 1,2
            SW2>
                LAG100 -> Interfaces: 1,2
        Final Topology:
            SW1>
                LAG200 -> Interfaces: 1,3
                LAG100 -> Interfaces: 2,4
            SW2>
                LAG100 -> Interfaces: 1,2
        Expected behaviour:
            Interface 1 should not be in Collecting/Distributing
            state in neither switch.
    """
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    sw1_lag_id = '100'
    sw1_lag_id_2 = '200'
    sw2_lag_id = '100'

    assert sw1 is not None
    assert sw2 is not None

    p11 = sw1.ports['1']
    p12 = sw1.ports['2']
    p13 = sw1.ports['3']
    p14 = sw1.ports['4']
    p21 = sw2.ports['1']
    p22 = sw2.ports['2']
    p23 = sw2.ports['3']
    p24 = sw2.ports['4']

    print("Turning on all interfaces used in this test")
    ports_sw1 = [p11, p12, p13, p14]
    for port in ports_sw1:
        turn_on_interface(sw1, port)

    ports_sw2 = [p21, p22, p23, p24]
    for port in ports_sw2:
        turn_on_interface(sw2, port)

    print("Waiting for interfaces to turn on")
    time.sleep(60)

    print("Validate interfaces are turn on")
    validate_turn_on_interfaces(sw1, ports_sw1)
    validate_turn_on_interfaces(sw2, ports_sw2)

    print("Create LAG in both switches")
    create_lag_active(sw1, sw1_lag_id)
    create_lag_active(sw2, sw2_lag_id)

    set_lacp_rate_fast(sw1, sw1_lag_id)
    set_lacp_rate_fast(sw2, sw2_lag_id)

    print("Associate interfaces [1,2] to lag in both switches")
    associate_interface_to_lag(sw1, p11, sw1_lag_id)
    associate_interface_to_lag(sw1, p12, sw1_lag_id)
    associate_interface_to_lag(sw2, p21, sw2_lag_id)
    associate_interface_to_lag(sw2, p22, sw2_lag_id)

    # Without this sleep time, we are validating temporary
    # states in state machines
    print("Waiting for LAG negotations between switches")
    time.sleep(80)

    print("Get information for LAG in interface 1 with both switches")
    map_lacp_sw1 = sw1.libs.vtysh.show_lacp_interface(p11)
    map_lacp_sw2 = sw2.libs.vtysh.show_lacp_interface(p21)

    print("Get the state of LAGs using diag-dump lacp basic in both switches")
    sw1_lacp_state = get_diagdump_lacp_state(sw1)
    sw2_lacp_state = get_diagdump_lacp_state(sw2)
    map_diagdump_lacp_sw1_p11 = sw1_lacp_state[str(sw1_lag_id)][int(p11)]
    map_diagdump_lacp_sw2_p21 = sw2_lacp_state[str(sw2_lag_id)][int(p21)]

    print("Get the configured interfaces for each LAG using diag-dump " +
          "lacp basic in both switches")
    sw1_lacp_interfaces = get_diagdump_lacp_interfaces(sw1)
    sw2_lacp_interfaces = get_diagdump_lacp_interfaces(sw2)
    validate_diagdump_lacp_interfaces(sw1_lacp_interfaces, sw1_lag_id,
                                      [p11, p12], [p11, p12], [p11, p12])
    validate_diagdump_lacp_interfaces(sw2_lacp_interfaces, sw2_lag_id,
                                      [p21, p22], [p21, p22], [p21, p22])

    print("Validate the LAG was created in both switches")
    validate_lag_name(map_lacp_sw1, sw1_lag_id)
    validate_local_key(map_lacp_sw1, sw1_lag_id)
    validate_remote_key(map_lacp_sw1, sw2_lag_id)
    validate_lag_state_sync(map_lacp_sw1, LOCAL_STATE)
    validate_lag_state_sync(map_lacp_sw1, REMOTE_STATE)
    # Validate diag-dump results
    validate_diagdump_lag_state_sync(map_diagdump_lacp_sw1_p11,
                                     DIAG_DUMP_LOCAL_STATE)
    validate_diagdump_lag_state_sync(map_diagdump_lacp_sw1_p11,
                                     DIAG_DUMP_REMOTE_STATE)

    validate_lag_name(map_lacp_sw2, sw2_lag_id)
    validate_local_key(map_lacp_sw2, sw2_lag_id)
    validate_remote_key(map_lacp_sw2, sw1_lag_id)
    validate_lag_state_sync(map_lacp_sw2, LOCAL_STATE)
    validate_lag_state_sync(map_lacp_sw2, REMOTE_STATE)
    # Validate diag-dump results
    validate_diagdump_lag_state_sync(map_diagdump_lacp_sw2_p21,
                                     DIAG_DUMP_LOCAL_STATE)
    validate_diagdump_lag_state_sync(map_diagdump_lacp_sw2_p21,
                                     DIAG_DUMP_REMOTE_STATE)

    print("Changing interface 1 to lag 200 in switch 1")
    create_lag_active(sw1, sw1_lag_id_2)
    set_lacp_rate_fast(sw1, sw1_lag_id_2)
    associate_interface_to_lag(sw1, p11, sw1_lag_id_2)
    associate_interface_to_lag(sw1, p13, sw1_lag_id_2)
    associate_interface_to_lag(sw1, p14, sw1_lag_id)

    # Waiting for switch 1 to update LAG status with newest configuration
    print("Waiting for LAG update withe new configuration")
    time.sleep(30)

    print("Get information from LAG in interface 1 with both switches")
    map_lacp_sw1_p11 = sw1.libs.vtysh.show_lacp_interface(p11)
    map_lacp_sw1_p12 = sw1.libs.vtysh.show_lacp_interface(p12)
    map_lacp_sw1_p13 = sw1.libs.vtysh.show_lacp_interface(p13)
    map_lacp_sw1_p14 = sw1.libs.vtysh.show_lacp_interface(p14)
    map_lacp_sw2_p21 = sw2.libs.vtysh.show_lacp_interface(p21)
    map_lacp_sw2_p22 = sw2.libs.vtysh.show_lacp_interface(p22)

    print("Get the state of LAGs using diag-dump lacp basic in both switches")
    sw1_lacp_state = get_diagdump_lacp_state(sw1)
    sw2_lacp_state = get_diagdump_lacp_state(sw2)
    map_diagdump_lacp_sw1_p11 = sw1_lacp_state[str(sw1_lag_id_2)][int(p11)]
    map_diagdump_lacp_sw1_p12 = sw1_lacp_state[str(sw1_lag_id)][int(p12)]
    map_diagdump_lacp_sw2_p21 = sw2_lacp_state[str(sw2_lag_id)][int(p21)]
    map_diagdump_lacp_sw2_p22 = sw2_lacp_state[str(sw2_lag_id)][int(p22)]

    print("Get the configured interfaces for each LAG using diagdump " +
          "getlacpinterfaces in both switches")
    sw1_lacp_interfaces = get_diagdump_lacp_interfaces(sw1)
    sw2_lacp_interfaces = get_diagdump_lacp_interfaces(sw2)
    validate_diagdump_lacp_interfaces(sw1_lacp_interfaces, sw1_lag_id,
                                      [p12, p24], [p12, p24], [p12])
    validate_diagdump_lacp_interfaces(sw1_lacp_interfaces, sw1_lag_id_2,
                                      [p11, p13], [p11, p13], [])
    validate_diagdump_lacp_interfaces(sw2_lacp_interfaces, sw2_lag_id,
                                      [p21, p22], [p21, p22], [p22])

    print("Validate lag is out of sync in interface 1 in both switches")
    validate_lag_name(map_lacp_sw1_p11, sw1_lag_id_2)
    validate_local_key(map_lacp_sw1_p11, sw1_lag_id_2)
    validate_remote_key(map_lacp_sw1_p11, sw2_lag_id)
    validate_lag_state_afn(map_lacp_sw1_p11, LOCAL_STATE)
    validate_lag_state_sync(map_lacp_sw1_p12, LOCAL_STATE)
    validate_lag_state_default_neighbor(map_lacp_sw1_p13,
                                        LOCAL_STATE)
    validate_lag_state_default_neighbor(map_lacp_sw1_p14,
                                        LOCAL_STATE)
    # Validate diag-dump results
    validate_diagdump_lag_state_afn(map_diagdump_lacp_sw1_p11,
                                    DIAG_DUMP_LOCAL_STATE)
    validate_diagdump_lag_state_sync(map_diagdump_lacp_sw1_p12,
                                     DIAG_DUMP_LOCAL_STATE)

    validate_lag_name(map_lacp_sw2_p21, sw2_lag_id)
    validate_local_key(map_lacp_sw2_p21, sw2_lag_id)
    validate_remote_key(map_lacp_sw2_p21, sw1_lag_id_2)
    validate_lag_state_out_of_sync(map_lacp_sw2_p21,
                                   LOCAL_STATE)
    validate_lag_state_sync(map_lacp_sw2_p22,
                            LOCAL_STATE)
    # Validate diag-dump results
    validate_diagdump_lag_state_out_sync(map_diagdump_lacp_sw2_p21,
                                         DIAG_DUMP_LOCAL_STATE)
    validate_diagdump_lag_state_sync(map_diagdump_lacp_sw2_p22,
                                     DIAG_DUMP_LOCAL_STATE)
