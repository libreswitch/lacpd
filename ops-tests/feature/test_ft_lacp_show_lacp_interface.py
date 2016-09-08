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
# Name:        test_ft_lacp_show_lacp_interface.py
#
# Objective:   Verify the command show lacp interface shows the correct
#              information
#
# Topology:    2 switches connected by 2 interfaces
#
##########################################################################
from lacp_lib import (
    associate_interface_to_lag,
    create_lag,
    lag_no_active,
    turn_on_interface,
    verify_turn_on_interfaces
)

TOPOLOGY = """
#   +-----+------+
#   |            |
#   |    sw1     |
#   |            |
#   +---+---+----+
#       |   |
#       |   |     LAG 1
#       |   |
#   +---+---+----+
#   |            |
#   |     sw2    |
#   |            |
#   +-----+------+

# Nodes
[type=openswitch name="OpenSwitch 1"] sw1
[type=openswitch name="OpenSwitch 2"] sw2

# Links
sw1:1 -- sw2:1
sw1:2 -- sw2:2
"""


def test_show_lacp_interface_case_1(topology, step):
    """
    Case 1:
        Verify the correct output when running the command show lacp
        interface when LAG is changed from dynamic to static
    """
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    lag_id = '1'
    agg_name = 'lag' + lag_id

    assert sw1 is not None
    assert sw2 is not None

    ports_sw1 = list()
    ports_sw2 = list()
    port_labels = ['1', '2']

    step("Mapping interfaces")
    for port in port_labels:
        ports_sw1.append(sw1.ports[port])
        ports_sw2.append(sw2.ports[port])

    step("Turning on all interfaces used in this test")
    for port in ports_sw1:
        turn_on_interface(sw1, port)

    for port in ports_sw2:
        turn_on_interface(sw2, port)

    step("Verify all interface are up")
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
    create_lag(sw1, lag_id, 'active')
    create_lag(sw2, lag_id, 'active')

    step("Associate interfaces [1, 2] to LAG in both switches")
    for intf in ports_sw1[0:2]:
        associate_interface_to_lag(sw1, intf, lag_id)

    for intf in ports_sw2[0:2]:
        associate_interface_to_lag(sw2, intf, lag_id)

    step("Verify show lacp interfaces")
    output = sw1.libs.vtysh.show_lacp_interface()
    assert output['actor']['1']['agg_name'] == agg_name,\
        "Interface 1 is not in LAG 10"
    assert output['actor']['2']['agg_name'] == agg_name,\
        "Interface 2 is not in LAG 10"
    assert output['partner']['1']['agg_name'] == agg_name,\
        "Interface 1 is not in LAG 20"
    assert output['partner']['2']['agg_name'] == agg_name,\
        "Interface 2 is not in LAG 20"

    step("Set lacp mode to 'off' on both switches")
    lag_no_active(sw1, lag_id)
    lag_no_active(sw2, lag_id)

    step("Verify show lacp interfaces")
    """
    Only interface and aggregation name should be present
    lacpd state should be erased (false)
    """
    output = sw1.libs.vtysh.show_lacp_interface()
    for actor_partner in output:
        for intf in output[actor_partner]:
            assert output[actor_partner][intf]['intf'] == intf,\
                "Key intf should be 1"
            assert output[actor_partner][intf]['agg_name'] == agg_name,\
                "Key agg_name should be %s" % agg_name
            assert output[actor_partner][intf]['port_id'] == '',\
                "Key port_id should be empty"
            assert output[actor_partner][intf]['port_priority'] == '',\
                "Key port_priority should be empty"
            assert output[actor_partner][intf]['key'] == '',\
                "Key key should be empty"
            assert output[actor_partner][intf]['system_id'] == '',\
                "Key system_id should be empty"
            assert output[actor_partner][intf]['system_priority'] == '',\
                "Key system_priority should be empty"
            for flag in output[actor_partner][intf]['state']:
                assert output[actor_partner][intf]['state'][flag] is False,\
                    "Actor state shoud be false"
