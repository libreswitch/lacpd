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

from lacp_lib import (
    associate_interface_to_lag,
    create_lag_active,
    LOCAL_STATE,
    REMOTE_STATE,
    set_lacp_rate_fast,
    turn_on_interface,
    verify_lag_config,
    verify_state_sync_lag,
    verify_turn_on_interfaces
)
import time
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

    ports_sw1 = list()
    ports_sw2 = list()
    port_labels = ['1', '2']

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

    step("Validate interfaces are turn on")
    verify_turn_on_interfaces(sw1, ports_sw1)
    verify_turn_on_interfaces(sw2, ports_sw2)

    step("Create LAG in both switches")
    create_lag_active(sw1, sw1_lag_id)
    create_lag_active(sw2, sw2_lag_id)

    set_lacp_rate_fast(sw1, sw1_lag_id)
    set_lacp_rate_fast(sw2, sw2_lag_id)

    step("Associate interfaces 1,2 to the lag in both switches")
    for port in ports_sw1:
        associate_interface_to_lag(sw1, port, sw1_lag_id)

    for port in ports_sw2:
        associate_interface_to_lag(sw2, port, sw2_lag_id)

    step("#### Verify LAG configuration ####")
    verify_lag_config(sw1, sw1_lag_id, ports_sw1, 'fast', mode='active')
    verify_lag_config(sw2, sw2_lag_id, ports_sw2, 'fast', mode='active')

    step("Verify if LAG is synchronized")
    verify_state_sync_lag(sw1, ports_sw1, LOCAL_STATE, 'active')
    verify_state_sync_lag(sw1, ports_sw1, REMOTE_STATE, 'active')
