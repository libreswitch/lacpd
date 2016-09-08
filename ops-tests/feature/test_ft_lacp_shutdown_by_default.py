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
# Name:        test_ft_lacp_shutdown_by_default.py
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
    create_lag_active,
    create_vlan,
    LOCAL_STATE,
    REMOTE_STATE,
    set_lacp_rate_fast,
    turn_on_interface,
    verify_connectivity_between_hosts,
    verify_state_sync_lag,
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
hs1:1 -- sw1:3
sw1:1 -- sw2:1
sw1:2 -- sw2:2
sw2:3 -- hs2:1
"""


@mark.platform_incompatible(['docker'])
def test_lag_shutdown_by_default(topology, step):
    """Test LAG with shutdown by default ('no shutdown').

    By default a new LAG will be configured with 'no shutdown'. IPv4 pings
    from both clients must be successful.
    """

    step('\n############################################')
    step('Test lag shutdown (By default)')
    step('############################################')

    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')

    step('### Verifying switches are not None ###')
    assert sw1 is not None, 'Topology failed getting object sw1'
    assert sw2 is not None, 'Topology failed getting object sw2'

    step('### Verifying hosts are not None ###')
    assert hs1 is not None, 'Topology failed getting object hs1'
    assert hs2 is not None, 'Topology failed getting object hs2'

    sw1_lag_id = '100'
    sw2_lag_id = '200'
    h1_ip_address = '10.0.0.1'
    h2_ip_address = '10.0.0.2'
    vlan = '100'
    mask = '/24'

    ports_sw1 = list()
    ports_sw2 = list()
    port_labels = ['1', '2', '3']

    step("### Mapping interfaces from Docker ###")
    for port in port_labels:
        ports_sw1.append(sw1.ports[port])
        ports_sw2.append(sw2.ports[port])

    step("Sorting the port list")
    ports_sw1.sort()
    ports_sw2.sort()
    step("### Turning on all interfaces used in this test ###")
    for port in ports_sw1:
        turn_on_interface(sw1, port)

    for port in ports_sw2:
        turn_on_interface(sw2, port)

    step("#### Validate interfaces are turned on ####")
    verify_turn_on_interfaces(sw1, ports_sw1)
    verify_turn_on_interfaces(sw2, ports_sw2)

    # Changing hardcoded interfaces for dynamic assignation
    mac_addr_sw1 = sw1.libs.vtysh.show_interface('1')['mac_address']
    mac_addr_sw2 = sw2.libs.vtysh.show_interface('1')['mac_address']
    assert mac_addr_sw1 != mac_addr_sw2,\
        'Mac address of interfaces in sw1 is equal to mac address of ' +\
        'interfaces in sw2. This is a test framework problem. Dynamic ' +\
        'LAGs cannot work properly under this condition. Refer to Taiga ' +\
        'issue #1251.'

    step("### Create LAG in both switches ###")
    create_lag_active(sw1, sw1_lag_id)
    create_lag_active(sw2, sw2_lag_id)

    step("### Set LACP rate to fast ###")
    set_lacp_rate_fast(sw1, sw1_lag_id)
    set_lacp_rate_fast(sw2, sw2_lag_id)

    step("### Configure vlan and switch interfaces ###")
    create_vlan(sw1, vlan)
    create_vlan(sw2, vlan)

    step("### Associate vlan with lag in both switches ###")
    associate_vlan_to_lag(sw1, vlan, sw1_lag_id)
    associate_vlan_to_lag(sw2, vlan, sw2_lag_id)

    step("### Associate vlan with l2 interfaces in both switches ###")
    associate_vlan_to_l2_interface(sw1, vlan, ports_sw1[2])
    associate_vlan_to_l2_interface(sw2, vlan, ports_sw2[2])

    step("### Associate interfaces [1,2] to lag in both switches ###")
    for intf in ports_sw1[0:2]:
        associate_interface_to_lag(sw1, intf, sw1_lag_id)

    for intf in ports_sw2[0:2]:
        associate_interface_to_lag(sw2, intf, sw2_lag_id)

    step("### Verify if LAG is synchronized ###")
    verify_state_sync_lag(sw1, ports_sw1[0:2], LOCAL_STATE, 'active')
    verify_state_sync_lag(sw1, ports_sw1[0:2], REMOTE_STATE, 'active')

    step("### Configure IP and bring UP in host 1 ###")
    hs1.libs.ip.interface('1', addr=(h1_ip_address + mask), up=True)

    step("### Configure IP and bring UP in host 2 ###")
    hs2.libs.ip.interface('1', addr=(h2_ip_address + mask), up=True)

    verify_connectivity_between_hosts(hs1, h1_ip_address, hs2, h2_ip_address,
                                      True)

    step('\n############################################')
    step('Test lag shutdown (By default) DONE')
    step('############################################')
