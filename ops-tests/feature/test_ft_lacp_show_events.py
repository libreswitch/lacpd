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

"""
OpenSwitch Test for LACP events.
"""

from time import sleep

from lacp_lib import create_lag
from lacp_lib import associate_interface_to_lag
from lacp_lib import associate_vlan_to_lag
from lacp_lib import remove_interface_from_lag
from lacp_lib import turn_on_interface
from lacp_lib import validate_turn_on_interfaces
from lacp_lib import create_vlan
from lacp_lib import config_lacp_rate
from lacp_lib import associate_vlan_to_l2_interface
from lacp_lib import check_connectivity_between_hosts

TOPOLOGY = """
# +-------+                                  +-------+
# |       |    +--------+  LAG  +--------+   |       |
# |  hs1  <---->  ops1  <------->  ops2  <--->  hs2  |
# |       |    |   A    <------->    P   |   |       |
# +-------+    |        <------->        |   +-------+
#              +--------+       +--------+

# Nodes
[type=openswitch name="OpenSwitch 1 LAG active"] ops1
[type=openswitch name="OpenSwitch 2 LAG passive"] ops2
[type=host name="Host 1"] hs1
[type=host name="Host 2"] hs2

# Links
hs1:1 -- ops1:4
ops1:1 -- ops2:1
ops1:2 -- ops2:2
ops1:3 -- ops2:3
hs2:1 -- ops2:4
"""


def test_show_lacp_events(topology):
    """
    Tests output for show events
    Main objective is to configure two switches with
    dynamic LAG (active/passive)
    Add and remove interfaces and turn off one of the LAGs
    Call show events to see results
    """

    # VID for testing
    test_vlan = '2'
    # LAG ID for testing
    test_lag = '2'
    test_lag_if = 'lag' + test_lag
    # interfaces to be added to LAG
    lag_interfaces = ['1', '3']
    # interface connected to host
    host_interface = '4'
    # hosts addresses
    hs1_addr = '10.0.11.10'
    hs2_addr = '10.0.11.11'

    show_event_lacp_cmd = 'show events category lacp'
    removed_if = '2'

    ops1 = topology.get('ops1')
    ops2 = topology.get('ops2')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')

    assert ops1 is not None, 'Topology failed getting object ops1'
    assert ops2 is not None, 'Topology failed getting object ops2'
    assert hs1 is not None, 'Topology failed getting object hs1'
    assert hs2 is not None, 'Topology failed getting object hs2'

    for curr_ops in [ops1, ops2]:
        for curr_p in lag_interfaces + [removed_if, host_interface]:
            turn_on_interface(curr_ops, curr_p)

    print('Wait for interfaces become up')
    sleep(60)
    for curr_ops in [ops1, ops2]:
        create_vlan(curr_ops, test_vlan)
        validate_turn_on_interfaces(curr_ops,
                                    lag_interfaces +
                                    [removed_if, host_interface])

    create_lag(ops1, test_lag, 'active')
    config_lacp_rate(ops1, test_lag, True)
    associate_vlan_to_lag(ops1, test_vlan, test_lag)

    create_lag(ops2, test_lag, 'passive')
    config_lacp_rate(ops2, test_lag, True)
    associate_vlan_to_lag(ops2, test_vlan, test_lag)

    for curr_ops in [ops1, ops2]:
        # Add interfaces to LAG
        for curr_if in lag_interfaces + [removed_if]:
            associate_interface_to_lag(curr_ops, curr_if, test_lag)
        # Interface 4 is connected to one host
        associate_vlan_to_l2_interface(curr_ops, test_vlan, host_interface)

    # Configure host interfaces
    hs1.libs.ip.interface('1', addr='{hs1_addr}/24'.format(**locals()),
                          up=True)
    hs2.libs.ip.interface('1', addr='{hs2_addr}/24'.format(**locals()),
                          up=True)

    print('Sleep few seconds to wait everything is up')
    sleep(60)

    check_connectivity_between_hosts(hs1, hs1_addr, hs2, hs2_addr,
                                     5, True)

    remove_interface_from_lag(ops1, removed_if, test_lag)
    remove_interface_from_lag(ops2, removed_if, test_lag)

    output = ops1(show_event_lacp_cmd, shell='vtysh')

    assert '|15007|LOG_INFO|LACP system ID set to'\
        and '|15006|LOG_INFO|LACP mode set to active for LAG {test_lag}'\
        .format(**locals())\
        and '|15001|LOG_INFO|Dynamic LAG {test_lag} created'\
        .format(**locals())\
        and '|15008|LOG_INFO|LACP rate set to fast for LAG {test_lag}'\
        .format(**locals())\
        and '|15004|LOG_INFO|Interface {removed_if} removed from '\
        'LAG {test_lag}'.format(**locals())\
        in output
    for curr_if in lag_interfaces + [removed_if]:
        assert '|15003|LOG_INFO|Interface {curr_if} added to LAG {test_lag}'\
            .format(**locals())\
            and '|15009|LOG_INFO|Partner is detected for interface {curr_if}'\
            ' LAG {test_lag}'.format(**locals())\
            in output

    output = ops2(show_event_lacp_cmd, shell='vtysh')

    assert '|15007|LOG_INFO|LACP system ID set to'\
        and '|15006|LOG_INFO|LACP mode set to passive for LAG {test_lag}'\
        .format(**locals())\
        and '|15001|LOG_INFO|Dynamic LAG {test_lag} created'\
        .format(**locals())\
        and '|15008|LOG_INFO|LACP rate set to fast for LAG {test_lag}'\
        .format(**locals())\
        and '|15004|LOG_INFO|Interface {removed_if} removed from '\
        'LAG {test_lag}'.format(**locals())\
        in output
    for curr_if in lag_interfaces + [removed_if]:
        assert '|15003|LOG_INFO|Interface {curr_if} added to LAG {test_lag}'\
            .format(**locals())\
            and '|15009|LOG_INFO|Partner is detected for interface {curr_if}'\
            ' LAG {test_lag}'.format(**locals())\
            in output

    with ops2.libs.vtysh.ConfigInterfaceLag(test_lag) as ctx:
        ctx.no_lacp_mode_passive()

    print('Waiting for switch2 is not seen')
    sleep(60)

    output = ops1(show_event_lacp_cmd, shell='vtysh')

    for curr_if in lag_interfaces:
        assert '|15011|LOG_WARN|Partner is lost (timed out) '\
            'for interface {curr_if} '.format(**locals())\
            in output

    output = ops2(show_event_lacp_cmd, shell='vtysh')
    assert '|15006|LOG_INFO|LACP mode set to off for '\
        'LAG {test_lag}'.format(**locals())\
        and '|ops-lacpd|15002|LOG_INFO|Dynamic LAG {test_lag} deleted'\
        .format(**locals())\
        in output
