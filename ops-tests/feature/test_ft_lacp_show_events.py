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

"""OpenSwitch Test Suite for LACP events."""

from pytest import mark
from lacp_lib import (
    config_lacp_rate,
    associate_interface_to_lag,
    associate_vlan_to_l2_interface,
    associate_vlan_to_lag,
    create_lag,
    create_vlan,
    remove_interface_from_lag,
    turn_on_interface,
    verify_connectivity_between_hosts,
    verify_turn_on_interfaces,
)


TOPOLOGY = """
# +-------+                                  +-------+
# |       |    +--------+  LAG  +--------+   |       |
# |  hs1  <---->  sw1   <------->  sw2  <--->  hs2   |
# |       |    |   A    <------->    P   |   |       |
# +-------+    |        <------->        |   +-------+
#              +--------+       +--------+

# Nodes
[type=openswitch name="OpenSwitch 1 LAG active"] sw1
[type=openswitch name="OpenSwitch 2 LAG passive"] sw2
[type=host name="Host 1"] hs1
[type=host name="Host 2"] hs2

# Links
hs1:1 -- sw1:4
sw1:1 -- sw2:1
sw1:2 -- sw2:2
sw1:3 -- sw2:3
hs2:1 -- sw2:4
"""


@mark.platform_incompatible(['docker'])
def test_show_lacp_events(topology, step):
    """Test output for show events.

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
    lag_intfs = ['1', '3']

    # interface connected to host
    host_intf = '4'

    # hosts addresses
    hs1_addr = '10.0.11.10'
    hs2_addr = '10.0.11.11'

    show_event_lacp_cmd = 'show events category lacp'
    removed_intf = '2'

    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')

    step('Verifying switches are not None')
    assert sw1 is not None, 'Topology failed getting object sw1'
    assert sw2 is not None, 'Topology failed getting object sw2'

    step('Verifying hosts are not None')
    assert hs1 is not None, 'Topology failed getting object hs1'
    assert hs2 is not None, 'Topology failed getting object hs2'

    step('Turning on interfaces')
    for switch in [sw1, sw2]:
        for intf in lag_intfs + [removed_intf, host_intf]:
            turn_on_interface(switch, intf)

    step('Verifying interfaces from Switch 1 are Up')
    verify_turn_on_interfaces(sw1, lag_intfs + [removed_intf, host_intf])

    step('Verifying interfaces from Switch 2 are Up')
    verify_turn_on_interfaces(sw1, lag_intfs + [removed_intf, host_intf])

    step('Creating VLAN (%s) on Switch 1' % test_vlan)
    create_vlan(sw1, test_vlan)

    step('Creating VLAN (%s) on Switch 2' % test_vlan)
    create_vlan(sw2, test_vlan)

    step('Creating LAG (%s) on Switch 1' % test_lag)
    create_lag(sw1, test_lag, 'active')
    config_lacp_rate(sw1, test_lag, True)
    associate_vlan_to_lag(sw1, test_vlan, test_lag)

    step('Creating LAG (%s) on Switch 2' % test_lag)
    create_lag(sw2, test_lag, 'passive')
    config_lacp_rate(sw2, test_lag, True)
    associate_vlan_to_lag(sw2, test_vlan, test_lag)

    for switch in [sw1, sw2]:
        for intf in lag_intfs + [removed_intf]:
            step('Assigning interface %s to LAG %s' % (intf, test_lag))
            associate_interface_to_lag(switch, intf, test_lag)

        step('Associating VLAN %s to host interface %s' % (test_vlan,
                                                           host_intf))
        associate_vlan_to_l2_interface(switch, test_vlan, host_intf)

    step('Configuring interface on Host 1')
    hs1.libs.ip.interface('1',
                          addr='{hs1_addr}/24'.format(**locals()),
                          up=True)

    step('Configuring interface on Host 2')
    hs2.libs.ip.interface('1',
                          addr='{hs2_addr}/24'.format(**locals()),
                          up=True)

    step('Verifying connectivity between hosts (Successful)')
    verify_connectivity_between_hosts(hs1, hs1_addr, hs2, hs2_addr, True)

    step('Removing interface (%s) from LAG (%s)' % (removed_intf, test_lag))
    remove_interface_from_lag(sw1, removed_intf, test_lag)

    step('Removing interface (%s) from LAG (%s)' % (removed_intf, test_lag))
    remove_interface_from_lag(sw2, removed_intf, test_lag)

    output = sw1(show_event_lacp_cmd, shell='vtysh')

    assert '|15007|LOG_INFO|LACP system ID set to'\
        and '|15006|LOG_INFO|LACP mode set to active for LAG {test_lag}'\
        .format(**locals())\
        and '|15001|LOG_INFO|Dynamic LAG {test_lag} created'\
        .format(**locals())\
        and '|15008|LOG_INFO|LACP rate set to fast for LAG {test_lag}'\
        .format(**locals())\
        and '|15004|LOG_INFO|Interface {removed_intf} removed from '\
        'LAG {test_lag}'.format(**locals())\
        in output

    for intf in lag_intfs + [removed_intf]:
        assert '|15003|LOG_INFO|Interface {intf} added to LAG {test_lag}'\
            .format(**locals())\
            and '|15009|LOG_INFO|Partner is detected for interface {intf}'\
            ' LAG {test_lag}'.format(**locals())\
            in output

    output = sw2(show_event_lacp_cmd, shell='vtysh')

    assert '|15007|LOG_INFO|LACP system ID set to'\
        and '|15006|LOG_INFO|LACP mode set to passive for LAG {test_lag}'\
        .format(**locals())\
        and '|15001|LOG_INFO|Dynamic LAG {test_lag} created'\
        .format(**locals())\
        and '|15008|LOG_INFO|LACP rate set to fast for LAG {test_lag}'\
        .format(**locals())\
        and '|15004|LOG_INFO|Interface {removed_intf} removed from '\
        'LAG {test_lag}'.format(**locals())\
        in output

    for intf in lag_intfs + [removed_intf]:
        assert '|15003|LOG_INFO|Interface {intf} added to LAG {test_lag}'\
            .format(**locals())\
            and '|15009|LOG_INFO|Partner is detected for interface {intf}'\
            ' LAG {test_lag}'.format(**locals())\
            in output

    with sw2.libs.vtysh.ConfigInterfaceLag(test_lag) as ctx:
        ctx.no_lacp_mode_passive()

    step('Verifying connectivity between hosts (Unsuccessful)')
    verify_connectivity_between_hosts(hs1, hs1_addr, hs2, hs2_addr, False)

    output = sw1(show_event_lacp_cmd, shell='vtysh')

    for intf in lag_intfs:
        assert '|15011|LOG_WARN|Partner is lost (timed out) '\
            'for interface {intf} '.format(**locals())\
            in output

    output = sw2(show_event_lacp_cmd, shell='vtysh')
    assert '|15006|LOG_INFO|LACP mode set to off for '\
        'LAG {test_lag}'.format(**locals())\
        and '|ops-lacpd|15002|LOG_INFO|Dynamic LAG {test_lag} deleted'\
        .format(**locals())\
        in output
