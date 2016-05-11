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
# Name:        test_ft_lacp_agg_key_lacp_different_agg_keys.py
#
# Objective:   Verify LAGs with different names from switches can
#              get connected as long as all interfaces connected have
#              same aggregation key
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
from lacp_lib import LOCAL_STATE
from lacp_lib import REMOTE_STATE
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


@pytest.mark.skipif(True, reason="Skipping due to instability")
def test_lacp_different_aggregation_keys(topology):
    """
    Case 4:
        Verify LAGs with different names from switches can
        get connected as long as all interfaces connected have
        same aggregation key
        Initial Topology:
            SW1>
                LAG10 -> Interfaces: 1,2
            SW2>
                LAG20 -> Interfaces: 1,2
        Expected behaviour:
        All interfaces in all LAGs should be InSync, Collecting
        and Distributing state
    """
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')

    sw1_lag_id = '10'
    sw2_lag_id = '20'

    assert sw1 is not None
    assert sw2 is not None

    p11 = sw1.ports['1']
    p12 = sw1.ports['2']
    p21 = sw2.ports['1']
    p22 = sw2.ports['2']

    print("Turning on all interfaces used in this test")
    ports_sw1 = [p11, p12]
    for port in ports_sw1:
        turn_on_interface(sw1, port)

    ports_sw2 = [p21, p22]
    for port in ports_sw2:
        turn_on_interface(sw2, port)

    print("Wait to validate interface are turn on")
    time.sleep(60)

    print("Validate interfaces are turn on")
    validate_turn_on_interfaces(sw1, ports_sw1)
    validate_turn_on_interfaces(sw2, ports_sw2)

    print("Create LAG in both switches")
    create_lag_active(sw1, sw1_lag_id)
    create_lag_active(sw2, sw2_lag_id)

    set_lacp_rate_fast(sw1, sw1_lag_id)
    set_lacp_rate_fast(sw2, sw2_lag_id)

    print("Associate interfaces 1,2 to the lag in both switches")
    associate_interface_to_lag(sw1, p11, sw1_lag_id)
    associate_interface_to_lag(sw1, p12, sw1_lag_id)
    associate_interface_to_lag(sw2, p21, sw2_lag_id)
    associate_interface_to_lag(sw2, p22, sw2_lag_id)

    # Without this sleep time, we are validating temporary
    # states in state machines
    print("Waiting for LAG negotations between switches")
    time.sleep(100)

    print("Get information for LAG in interface 1 with both switches")
    map_lacp_sw1_1 = sw1.libs.vtysh.show_lacp_interface(p11)
    map_lacp_sw1_2 = sw1.libs.vtysh.show_lacp_interface(p12)
    map_lacp_sw2_1 = sw2.libs.vtysh.show_lacp_interface(p21)
    map_lacp_sw2_2 = sw2.libs.vtysh.show_lacp_interface(p21)

    print("Validate the LAG was created in switch1")
    validate_lag_state_sync(map_lacp_sw1_1, LOCAL_STATE)
    validate_lag_state_sync(map_lacp_sw1_1, REMOTE_STATE)
    validate_lag_state_sync(map_lacp_sw1_2, LOCAL_STATE)
    validate_lag_state_sync(map_lacp_sw1_2, REMOTE_STATE)

    print("Validate the LAG was created in switch2")
    validate_lag_state_sync(map_lacp_sw2_1, LOCAL_STATE)
    validate_lag_state_sync(map_lacp_sw2_1, REMOTE_STATE)
    validate_lag_state_sync(map_lacp_sw2_2, LOCAL_STATE)
    validate_lag_state_sync(map_lacp_sw2_2, REMOTE_STATE)
