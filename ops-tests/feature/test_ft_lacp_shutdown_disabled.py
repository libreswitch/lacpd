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
# Name:        test_ft_lacp_shutdown_disabled.py
#
# Objective:   To verify that packets are passing if LAG is no shutdown
#
# Topology:    2 switches connected by 2 interfaces and 2 hosts connected
#              by 1 interfacel
#
##########################################################################
import time
from lacp_lib import create_lag_active
from lacp_lib import associate_interface_to_lag
from lacp_lib import turn_on_interface
from lacp_lib import is_interface_up
from lacp_lib import create_vlan
from lacp_lib import associate_vlan_to_lag
from lacp_lib import associate_vlan_to_l2_interface
from lacp_lib import check_connectivity_between_hosts
from lacp_lib import lag_no_shutdown
from lacp_lib import validate_turn_on_interfaces

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


def test_lag_shutdown_disabled(topology):
    """Test LAG with shutdown enabled.

    By default a new LAG is configured as 'no shutdown', so this case will
    test to execute 'no shutdown' on both switches and connectivity must
    remain working and IPv4 pings between both clients must be successful.
    """

    print('\n############################################')
    print('Test lag no shutdown')
    print('############################################')

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

    assert sw1 is not None
    assert sw2 is not None
    assert hs1 is not None
    assert hs2 is not None

    print("Configure IP and bring UP in host 1")
    hs1.libs.ip.interface('1', addr=(h1_ip_address + mask), up=True)

    print("Configure IP and bring UP in host 2")
    hs2.libs.ip.interface('1', addr=(h2_ip_address + mask), up=True)

    p11 = sw1.ports['1']
    p12 = sw1.ports['2']
    p13 = sw1.ports['3']
    p21 = sw2.ports['1']
    p22 = sw2.ports['2']
    p23 = sw2.ports['3']

    print("Turning on all interfaces used in this test")
    ports_sw1 = [p11, p12, p13]
    for port in ports_sw1:
        turn_on_interface(sw1, port)

    ports_sw2 = [p21, p22, p23]
    for port in ports_sw2:
        turn_on_interface(sw2, port)

    print("Wait for interfaces to turn on")
    time.sleep(60)

    print("Validating interfaces are turn on")
    validate_turn_on_interfaces(sw1, ports_sw1)
    validate_turn_on_interfaces(sw2, ports_sw2)

    print("Create LAG in both switches")
    create_lag_active(sw1, sw1_lag_id)
    create_lag_active(sw2, sw2_lag_id)

    print("Configure vlan and switch interfaces")
    create_vlan(sw1, vlan)
    create_vlan(sw2, vlan)

    print("Associate vlan with lag in both switches")
    associate_vlan_to_lag(sw1, vlan, sw1_lag_id)
    associate_vlan_to_lag(sw2, vlan, sw2_lag_id)

    print("Associate vlan with l2 interfaces in both switches")
    associate_vlan_to_l2_interface(sw1, vlan, p13)
    associate_vlan_to_l2_interface(sw2, vlan, p23)

    print("Associate interfaces [1,2] to lag in both switches")
    associate_interface_to_lag(sw1, p11, sw1_lag_id)
    associate_interface_to_lag(sw1, p12, sw1_lag_id)
    associate_interface_to_lag(sw2, p21, sw2_lag_id)
    associate_interface_to_lag(sw2, p22, sw2_lag_id)

    print("Waiting for vlan configuration and LAG negotiation")
    time.sleep(60)

    print("Verify all interface are up")
    for port in ports_sw1:
        assert is_interface_up(sw1, port),\
            "Interface " + port + " should be up"
    for port in ports_sw2:
        assert is_interface_up(sw2, port),\
            "Interface " + port + " should be up"

    # 'no shutdown' on switch 1
    check_connectivity_between_hosts(hs1, h1_ip_address, hs2, h2_ip_address,
                                     10, True)
    lag_no_shutdown(sw1, sw1_lag_id)
    check_connectivity_between_hosts(hs1, h1_ip_address, hs2, h2_ip_address,
                                     10, True)

    # 'no shutdown' on switch 2
    check_connectivity_between_hosts(hs1, h1_ip_address, hs2, h2_ip_address,
                                     10, True)
    lag_no_shutdown(sw2, sw2_lag_id)
    check_connectivity_between_hosts(hs1, h1_ip_address, hs2, h2_ip_address,
                                     10, True)

    print("Verify all interface are up")
    for port in ports_sw1:
        assert is_interface_up(sw1, port),\
            "Interface " + port + " should be up"
    for port in ports_sw2:
        assert is_interface_up(sw2, port),\
            "Interface " + port + " should be up"

    print('\n############################################')
    print('Test lag no shutdown DONE')
    print('############################################')
