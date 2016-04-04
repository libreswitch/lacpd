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

import time
from lacp_lib import create_lag_active
from lacp_lib import associate_interface_to_lag
from lacp_lib import turn_on_interface
from lacp_lib import validate_lag_state_sync
from lacp_lib import validate_lag_name
from lacp_lib import LOCAL_STATE
from lacp_lib import set_lacp_rate_fast

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


def test_lacp_agg_key_cross_links(topology):
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

    print("Turning on all interfaces used in this test")
    ports_sw1 = [p11, p12, p13, p15, p16, p17]
    for port in ports_sw1:
        turn_on_interface(sw1, port)

    ports_sw2 = [p21, p22, p23, p25, p26, p27]
    for port in ports_sw2:
        turn_on_interface(sw2, port)

    print("Create LAGs (150, 250 and 350) in both switches")
    for lag in sw_lag_id:
        create_lag_active(sw1, lag)
        create_lag_active(sw2, lag)
        set_lacp_rate_fast(sw1, lag)
        set_lacp_rate_fast(sw2, lag)

    print("Associate interfaces with LAG in switch1")
    associate_interface_to_lag(sw1, p11, lag_id_1)
    associate_interface_to_lag(sw1, p15, lag_id_1)
    associate_interface_to_lag(sw1, p12, lag_id_2)
    associate_interface_to_lag(sw1, p16, lag_id_2)
    associate_interface_to_lag(sw1, p13, lag_id_3)
    associate_interface_to_lag(sw1, p17, lag_id_3)

    print("Associat6 interfaces with LAG in switch2")
    associate_interface_to_lag(sw2, p21, lag_id_1)
    associate_interface_to_lag(sw2, p26, lag_id_1)
    associate_interface_to_lag(sw2, p22, lag_id_2)
    associate_interface_to_lag(sw2, p27, lag_id_2)
    associate_interface_to_lag(sw2, p23, lag_id_3)
    associate_interface_to_lag(sw2, p25, lag_id_3)

    # Without this sleep time, we are validating temporary
    # states in state machines
    print("Waiting for LAG negotations between switches")
    time.sleep(100)

    print("Get information for LAG")
    map_lacp_sw1_5 = sw1.libs.vtysh.show_lacp_interface(p15)
    map_lacp_sw1_6 = sw1.libs.vtysh.show_lacp_interface(p16)
    map_lacp_sw1_7 = sw1.libs.vtysh.show_lacp_interface(p17)
    map_lacp_sw2_5 = sw2.libs.vtysh.show_lacp_interface(p25)
    map_lacp_sw2_6 = sw2.libs.vtysh.show_lacp_interface(p26)
    map_lacp_sw2_7 = sw2.libs.vtysh.show_lacp_interface(p27)

    print("Validate correct lag name in switch1")
    validate_lag_name(map_lacp_sw1_5, lag_id_1)
    validate_lag_name(map_lacp_sw1_6, lag_id_2)
    validate_lag_name(map_lacp_sw1_7, lag_id_3)

    print("Validate correct state in switch1 for interfaces 5,6,7")
    validate_lag_state_sync(map_lacp_sw1_5, LOCAL_STATE)
    validate_lag_state_sync(map_lacp_sw1_6, LOCAL_STATE)
    validate_lag_state_sync(map_lacp_sw1_7, LOCAL_STATE)

    print("Validate correct lag name in switch2")
    validate_lag_name(map_lacp_sw2_5, lag_id_3)
    validate_lag_name(map_lacp_sw2_6, lag_id_1)
    validate_lag_name(map_lacp_sw2_7, lag_id_2)

    print("Validate correct state in switch2 for interfaces 5,6,7")
    validate_lag_state_sync(map_lacp_sw2_5, LOCAL_STATE)
    validate_lag_state_sync(map_lacp_sw2_6, LOCAL_STATE)
    validate_lag_state_sync(map_lacp_sw2_7, LOCAL_STATE)
