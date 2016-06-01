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

##########################################################################
# Name:        test_ft_lacp_shutdown_enabled.py
#
# Objective:   To verify that packets are no passing if LAG is in shutdown
#
# Topology:    2 switches connected by 2 interfaces and 2 hosts connected
#              by 1 interfacel
#
##########################################################################

from pytest import mark
from lacp_lib import (
    associate_interface_to_lag,
    associate_vlan_to_l2_interface,
    associate_vlan_to_lag,
    check_connectivity_between_hosts,
    create_lag_active,
    create_lag_passive,
    create_vlan,
    lag_no_shutdown,
    lag_shutdown,
    LOCAL_STATE,
    REMOTE_STATE,
    turn_on_interface,
    verify_lag_config,
    verify_state_sync_lag,
    verify_turn_off_interfaces,
    verify_turn_on_interfaces
)


TOPOLOGY = """
# +-------+                                 +-------+
# |       |     +-------+     +-------+     |       |
# |  hs1  <----->  sw1  <----->  sw2  <----->  hs2  |
# |       |     +-------+     +-------+     |       |
# +-------+                                 +-------+

# Nodes
[type=openswitch name="Switch 1"] sw1
[type=openswitch name="Switch 2"] sw2
[type=host name="Host 1"] hs1
[type=host name="Host 2"] hs2

# Links
hs1:1 -- sw1:1
sw1:2 -- sw2:2
sw1:3 -- sw2:3
sw2:1 -- hs2:1
"""


@mark.platform_incompatible(['docker'])
def test_lag_shutdown_enabled(topology, step):
    """Test LAG with shutdown enabled.

    When lag shutdown is enabled IPv4 ping must be unsuccessful, after
    configuring LAGs as no shutdown IPv4 ping must be successful
    """

    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')

    sw1_lag_id = '100'
    sw2_lag_id = '200'
    h1_ip_address = '10.0.0.1'
    h2_ip_address = '10.0.0.2'
    vlan = '100'
    mask = '/24'

    ports_sw1 = list()
    ports_sw2 = list()
    port_labels = ['1', '2', '3']

    assert sw1 is not None
    assert sw2 is not None
    assert hs1 is not None
    assert hs2 is not None

    step("Configure IP and bring UP in host 1")
    hs1.libs.ip.interface('1', addr=h1_ip_address + mask, up=True)

    step("Configure IP and bring UP in host 2")
    hs2.libs.ip.interface('1', addr=h2_ip_address + mask, up=True)

    step("Mapping interfaces")
    for port in port_labels:
        ports_sw1.append(sw1.ports[port])
        ports_sw2.append(sw2.ports[port])

    step("Turning on all interfaces used in this test")
    for port in ports_sw1:
        turn_on_interface(sw1, port)

    for port in ports_sw2:
        turn_on_interface(sw2, port)

    step("Validate interfaces are turn on")
    verify_turn_on_interfaces(sw1, ports_sw1)
    verify_turn_on_interfaces(sw2, ports_sw2)

    step("Create LAG in both switches")
    create_lag_passive(sw1, sw1_lag_id)
    create_lag_active(sw2, sw2_lag_id)

    step("Configure vlan and switch interfaces")
    create_vlan(sw1, vlan)
    create_vlan(sw2, vlan)

    step("Associate vlan with lag in both switches")
    associate_vlan_to_lag(sw1, vlan, sw1_lag_id)
    associate_vlan_to_lag(sw2, vlan, sw2_lag_id)

    step("Associate vlan with l2 interfaces in both switches")
    associate_vlan_to_l2_interface(sw1, vlan, ports_sw1[0])
    associate_vlan_to_l2_interface(sw2, vlan, ports_sw2[0])

    step("Associate interfaces [2,3] to lag in both switches")
    for port in ports_sw1[1:3]:
        associate_interface_to_lag(sw1, port, sw1_lag_id)
    for port in ports_sw2[1:3]:
        associate_interface_to_lag(sw2, port, sw2_lag_id)

    step("Verify LAG configuration")
    verify_lag_config(sw1, sw1_lag_id, ports_sw1[1:3], mode='passive')
    verify_lag_config(sw2, sw2_lag_id, ports_sw2[1:3], mode='active')

    step("Verify if LAG is synchronized")
    verify_state_sync_lag(sw1, ports_sw1[1:3], LOCAL_STATE, 'passive')
    verify_state_sync_lag(sw1, ports_sw1[1:3], REMOTE_STATE, 'active')

    step("Test connectivity between hosts")
    check_connectivity_between_hosts(hs1, h1_ip_address, hs2, h2_ip_address,
                                     10, True)

    step("Turn off LAG in SW1")
    lag_shutdown(sw1, sw1_lag_id)

    step("Negative test connectivity between hosts")
    check_connectivity_between_hosts(hs1, h1_ip_address, hs2, h2_ip_address,
                                     10, False)

    step("Turn off LAG in SW2")
    lag_shutdown(sw2, sw2_lag_id)

    step("Negative test connectivity between hosts")
    check_connectivity_between_hosts(hs1, h1_ip_address, hs2, h2_ip_address,
                                     10, False)

    step("Verify all interface are down")
    verify_turn_off_interfaces(sw1, ports_sw1[1:3])
    verify_turn_off_interfaces(sw2, ports_sw2[1:3])

    step("Turn on LAG")
    lag_no_shutdown(sw1, sw1_lag_id)
    lag_no_shutdown(sw2, sw2_lag_id)

    step("Verify all interface are up")
    verify_turn_on_interfaces(sw1, ports_sw1[1:3])
    verify_turn_on_interfaces(sw2, ports_sw2[1:3])

    step("Verify if LAG is synchronized")
    verify_state_sync_lag(sw1, ports_sw1[1:3], LOCAL_STATE, 'passive')
    verify_state_sync_lag(sw1, ports_sw1[1:3], REMOTE_STATE, 'active')

    step("Test connectivity between hosts")
    check_connectivity_between_hosts(hs1, h1_ip_address, hs2, h2_ip_address,
                                     10, True)
