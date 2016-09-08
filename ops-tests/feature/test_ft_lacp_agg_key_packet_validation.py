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
# Name:        test_ft_lacp_agg_key_packet_validation.py
#
# Objective:   Capture LACPDUs packets and validate the aggregation
#              key is set correctly for both switches
#
# Topology:    2 switch (DUT running Halon)
#
##########################################################################


"""
OpenSwitch Test for LACP aggregation key functionality
"""

from lacp_lib import create_lag_active
from lacp_lib import create_lag_passive
from lacp_lib import associate_interface_to_lag
from lacp_lib import turn_on_interface
from lacp_lib import get_device_mac_address
from lacp_lib import tcpdump_capture_interface
from lacp_lib import get_info_from_packet_capture
from lacp_lib import ACTOR
from lacp_lib import PARTNER
from lacp_lib import set_lacp_rate_fast
from lacp_lib import validate_turn_on_interfaces
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
def lacp_aggregation_key_packet_validation(topology):
    """
    Aggregation Key packet validation:
        Capture LACPDUs packets and validate the aggregation
        key is set correctly for both switches
    """
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    sw1_lag_id = '100'
    sw2_lag_id = '200'

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

    print("Wait for interfaces to turn on")
    time.sleep(60)

    validate_turn_on_interfaces(sw1, ports_sw1)
    validate_turn_on_interfaces(sw2, ports_sw2)

    print("Create LAG in both switches")
    create_lag_active(sw1, sw1_lag_id)
    create_lag_passive(sw2, sw2_lag_id)

    set_lacp_rate_fast(sw1, sw1_lag_id)
    set_lacp_rate_fast(sw2, sw2_lag_id)

    print("Associate interfaces [1,2] to lag in both switches")
    associate_interface_to_lag(sw1, p11, sw1_lag_id)
    associate_interface_to_lag(sw1, p12, sw1_lag_id)
    associate_interface_to_lag(sw2, p21, sw2_lag_id)
    associate_interface_to_lag(sw2, p22, sw2_lag_id)

    sw1_mac = get_device_mac_address(sw1, p11)

    print("Take capture from interface 1 in switch 1")
    capture = tcpdump_capture_interface(sw1, p11, 80)
    tcpdump_capture_interface(sw1, p12, 80)

    print("Validate actor and partner key from sw1 packets")
    sw1_actor = get_info_from_packet_capture(capture,
                                             ACTOR,
                                             sw1_mac)

    assert sw1_actor['key'] == sw1_lag_id,\
        'Packet is not sending the correct actor key in sw1'

    sw1_partner = get_info_from_packet_capture(capture,
                                               PARTNER,
                                               sw1_mac)

    assert sw1_partner['key'] == sw2_lag_id,\
        'Packet is not sending the correct partner key in sw1'
