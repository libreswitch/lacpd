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
# Name:        test_ft_lacp_agg_key_hosts.py
#
# Objective:   Verify test cases for aggregation key functionality including
#              hosts connected to the switches
#
# Topology:    3 switch, 3 hosts (DUT running Halon)
#
##########################################################################

"""
OpenSwitch Tests for LACP Aggregation Key functionality using hosts
"""

from lacp_lib import (
    associate_interface_to_lag,
    associate_vlan_to_l2_interface,
    associate_vlan_to_lag,
    create_lag_active,
    create_vlan,
    LOCAL_STATE,
    set_lacp_rate_fast,
    turn_on_interface,
    validate_vlan_state,
    verify_connectivity_between_hosts,
    verify_state_out_of_sync_lag,
    verify_state_sync_lag,
    verify_turn_on_interfaces
)
from pytest import mark
import time

TOPOLOGY = """
# +-------+              +-------+
# |       |              |       |
# |  hs1  |              |  hs2  |
# |       |              |       |
# +-------+              +-------+
#     |                      |
#     |                      |
#     |                      |
# +-------+              +-------+
# |  sw1  |              |  sw2  |
# +-------+              +-------+
#     |                      |
# LAG |                      | LAG
#     |      +-------+       |
#     +------+  sw3  +-------+
#            +-------+
#                |
#                |
#                |
#            +-------+
#            |       |
#            |  hs3  |
#            |       |
#            +-------+

# Nodes
[type=openswitch name="Switch 1"] sw1
[type=openswitch name="Switch 2"] sw2
[type=openswitch name="Switch 3"] sw3
[type=host name="Host 1"] hs1
[type=host name="Host 2"] hs2
[type=host name="Host 3"] hs3

# Links
sw1:1 -- sw3:1
sw1:2 -- sw3:2
sw2:1 -- sw3:3
sw1:3 -- hs1:1
sw2:2 -- hs2:1
sw3:4 -- hs3:1
"""

# Ports
port_labels = ['1', '2', '3', '4']


@mark.gate
@mark.skipif(True, reason="Skipping due to instability")
def test_lacp_aggregation_key_with_hosts(topology):
    """
    Case 1:
        A single switch should form LAGs to 2 different
        other switches as long as the aggregation key is
        the same through members of a single LAG, but
        ports should be blocked if 1 of the members is
        configured to be a member of the LAG going to a different switch
    """
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    sw3 = topology.get('sw3')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')
    hs3 = topology.get('hs3')

    assert sw1 is not None
    assert sw2 is not None

    sw1_lag_id = '10'
    sw2_lag_id = '20'
    sw3_lag_id = '310'  # With switch 1
    sw3_lag_id_2 = '320'  # With switch 2
    sw1_vlan = '100'
    sw2_vlan = '200'
    sw3_sw1_vlan = '100'
    sw3_sw2_vlan = '200'
    hs1_ip = '10.0.10.1'
    hs2_ip = '10.0.20.1'
    hs3_ip_1 = '10.0.10.2'
    hs3_ip_2 = '10.0.20.2'
    mask = '/24'

    ports_sw1 = list()
    ports_sw2 = list()
    ports_sw3 = list()

    print("Mapping interfaces")
    for port in port_labels[0:3]:
        ports_sw1.append(sw1.ports[port])
    for port in port_labels[0:2]:
        ports_sw2.append(sw2.ports[port])
    for port in port_labels:
        ports_sw3.append(sw3.ports[port])

    step("Sorting the port list")
    ports_sw1.sort()
    ports_sw2.sort()
    ports_sw3.sort()

    p13h = ports_sw1[2]
    p22h = ports_sw2[1]
    p32 = ports_sw3[1]
    p34h = ports_sw3[3]

    print("Turning on all interfaces used in this test")
    for port in ports_sw1:
        turn_on_interface(sw1, port)

    for port in ports_sw2:
        turn_on_interface(sw2, port)

    for port in ports_sw3:
        turn_on_interface(sw3, port)

    print("Verify all interface are up")
    verify_turn_on_interfaces(sw1, ports_sw1)
    verify_turn_on_interfaces(sw2, ports_sw2)
    verify_turn_on_interfaces(sw3, ports_sw3)

    step("Create LAG in all switches")
    create_lag_active(sw1, sw1_lag_id)
    create_lag_active(sw2, sw2_lag_id)
    create_lag_active(sw3, sw3_lag_id)
    create_lag_active(sw3, sw3_lag_id_2)

    set_lacp_rate_fast(sw1, sw1_lag_id)
    set_lacp_rate_fast(sw2, sw2_lag_id)
    set_lacp_rate_fast(sw3, sw3_lag_id)
    set_lacp_rate_fast(sw3, sw3_lag_id_2)

    step("Associate interfaces with LAG")
    for intf in ports_sw1[0:2]:
        associate_interface_to_lag(sw1, intf, sw1_lag_id)
    associate_interface_to_lag(sw2, ports_sw2[0], sw2_lag_id)
    for intf in ports_sw3[0:2]:
        associate_interface_to_lag(sw3, intf, sw3_lag_id)
    associate_interface_to_lag(sw3, ports_sw3[2], sw3_lag_id_2)

    step("Configure IP and bring UP in host 1")
    hs1.libs.ip.interface('1', addr=(hs1_ip + mask), up=True)

    step("Configure IP and bring UP in host 2")
    hs2.libs.ip.interface('1', addr=(hs2_ip + mask), up=True)

    step("Configure IP and bring UP in host 3")
    hs3.libs.ip.interface('1', addr=(hs3_ip_1 + mask), up=True)

    create_vlan(sw1, sw1_vlan)
    create_vlan(sw2, sw2_vlan)
    create_vlan(sw3, sw3_sw1_vlan)
    create_vlan(sw3, sw3_sw2_vlan)

    associate_vlan_to_lag(sw1, sw1_vlan, sw1_lag_id)
    associate_vlan_to_lag(sw2, sw2_vlan, sw2_lag_id)
    associate_vlan_to_lag(sw3, sw3_sw1_vlan, sw3_lag_id)
    associate_vlan_to_lag(sw3, sw3_sw2_vlan, sw3_lag_id_2)

    # First associate Host 3 with Vlan from Switch 1
    step("Configure connection between Host 1 and 3")
    associate_vlan_to_l2_interface(sw1, sw1_vlan, p13h)
    associate_vlan_to_l2_interface(sw3, sw3_sw1_vlan, p34h)

    validate_vlan_state(sw1, sw1_vlan, "up")
    validate_vlan_state(sw3, sw3_sw1_vlan, "up")

    step("#### Test ping between clients work ####")
    verify_connectivity_between_hosts(hs1, hs1_ip,
                                      hs3, hs3_ip_1)

    # Then associate Host 3 with Vlan from Switch 2
    step("Configure connection between Host 2 and 3")
    associate_vlan_to_l2_interface(sw2, sw2_vlan, p22h)
    associate_vlan_to_l2_interface(sw3, sw3_sw2_vlan, p34h)

    validate_vlan_state(sw2, sw2_vlan, "up")
    validate_vlan_state(sw3, sw3_sw2_vlan, "up")

    hs3.libs.ip.remove_ip('1', addr=(hs3_ip_1 + mask))
    hs3.libs.ip.interface('1', addr=(hs3_ip_2 + mask), up=True)

    step("Check connectivty between Host 2 and 3")
    verify_connectivity_between_hosts(hs2, hs2_ip,
                                      hs3, hs3_ip_2)

    step("Setting up priorities")
    with sw1.libs.vtysh.ConfigInterface(ports_sw1[0]) as ctx:
        ctx.lacp_port_priority(1)
    with sw1.libs.vtysh.ConfigInterface(ports_sw1[1]) as ctx:
        ctx.lacp_port_priority(10)
    with sw3.libs.vtysh.ConfigInterface(ports_sw3[0]) as ctx:
        ctx.lacp_port_priority(1)
    with sw3.libs.vtysh.ConfigInterface(ports_sw3[1]) as ctx:
        ctx.lacp_port_priority(10)

    step("Changing interface 2 from Switch 3 to LAG with Switch 2")
    associate_interface_to_lag(sw3, p32, sw3_lag_id_2)

    # This should get the interface Out of Sync because that
    # interface is linked with Switch 1

    # Only the link 2 should be get out of sync
    verify_state_out_of_sync_lag(sw1, [ports_sw1[1]], LOCAL_STATE)
    verify_state_out_of_sync_lag(sw3, [ports_sw3[1]], LOCAL_STATE)

    verify_state_sync_lag(sw1, [ports_sw1[0]], LOCAL_STATE, 'active')
    verify_state_sync_lag(sw2, [ports_sw2[0]], LOCAL_STATE, 'active')
    verify_state_sync_lag(sw3, [ports_sw3[0]], LOCAL_STATE, 'active')
    verify_state_sync_lag(sw3, [ports_sw3[2]], LOCAL_STATE, 'active')

    step("Check connectivity between Host 2 and 3 again")
    verify_connectivity_between_hosts(hs2, hs2_ip,
                                      hs3, hs3_ip_2)

    step("Change configuration to connect Host 1 and 3 again")
    associate_vlan_to_l2_interface(sw3, sw3_sw1_vlan, p34h)

    validate_vlan_state(sw3, sw3_sw1_vlan, "up")

    hs3.libs.ip.remove_ip('1', addr=(hs3_ip_2 + mask))
    hs3.libs.ip.interface('1', addr=(hs3_ip_1 + mask), up=True)

    step("Check connectivity between Host 1 and Host 3")
    verify_connectivity_between_hosts(hs1, hs1_ip,
                                      hs3, hs3_ip_1)
