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

###############################################################################
# Name         test_ft_lacp_delete_invalid_lag.py
#
# Objective:   Tests that a previously configured dynamic Link Aggregation does
#              not stop forwarding traffic when attempting to delete several
#              non-existent Link Aggregations with different names that may not
#              be supported
#
# Topology:    |Host| ----- |Switch| ------------------ |Switch| ----- |Host|
#                                   (Dynamic LAG - 2 links)
# Success Criteria:  PASS -> Non-existent LAGs cannot be deleted and they
#                            don't affect the functioning LAG
#
#                    FAILED -> Functioning LAG configuration is changed or any
#                              of the non-existing LAGs don't produce errors
#                              when attempting to delete them
#
###############################################################################

from pytest import raises
from pytest import mark
from lacp_lib import (
    associate_interface_to_lag,
    associate_vlan_to_l2_interface,
    associate_vlan_to_lag,
    check_connectivity_between_hosts,
    create_lag,
    create_vlan,
    delete_lag,
    LOCAL_STATE,
    REMOTE_STATE,
    set_lacp_rate_fast,
    turn_on_interface,
    verify_connectivity_between_hosts,
    verify_lag_config,
    verify_state_sync_lag,
    verify_turn_on_interfaces
)
from topology_lib_vtysh.exceptions import UnknownCommandException
from topology_lib_vtysh.exceptions import UnknownVtyshException
import pytest

TOPOLOGY = """

# +-------+                            +-------+
# |       |                            |       |
# |  hs1  |                            |  hs2  |
# |       |                            |       |
# +---1---+                            +---1---+
#     |                                    |
#     |                                    |
#     |                                    |
#     |                                    |
# +---1---+                            +---1---+
# |       |                            |       |
# |       2----------------------------2       |
# |  sw1  3----------------------------3  sw2  |
# |       |                            |       |
# +-------+                            +-------+



# Nodes
[type=openswitch name="Switch 1"] sw1
[type=openswitch name="Switch 2"] sw2
[type=host name="Host 1"] hs1
[type=host name="Host 2"] hs2

# Links
sw1:1 -- hs1:1
sw1:2 -- sw2:2
sw1:3 -- sw2:3
sw2:1 -- hs2:1
"""


@pytest.fixture(scope='module')
def main_setup(request, topology):
    """Test Case common configuration."""
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')

    assert sw1 is not None, 'Topology failed getting object sw1'
    assert sw2 is not None, 'Topology failed getting object sw2'
    assert hs1 is not None, 'Topology failed getting object hs1'
    assert hs2 is not None, 'Topology failed getting object hs2'

    lag_id = '1'
    vlan_id = '900'
    mode_active = 'active'
    mode_passive = 'passive'

    p11h = sw1.ports['1']
    p12 = sw1.ports['2']
    p13 = sw1.ports['3']
    p21h = sw2.ports['1']
    p22 = sw2.ports['2']
    p23 = sw2.ports['3']

    ports_sw1 = [p11h, p12, p13]
    ports_sw2 = [p21h, p22, p23]

    print("#### Turning on interfaces in sw1 ###")
    for port in ports_sw1:
        turn_on_interface(sw1, port)

    print("#### Turning on interfaces in sw2 ###")
    for port in ports_sw2:
        turn_on_interface(sw2, port)

    print("#### Validate interfaces are turned on ####")
    verify_turn_on_interfaces(sw1, ports_sw1)
    verify_turn_on_interfaces(sw2, ports_sw2)

    mac_addr_sw1 = sw1.libs.vtysh.show_interface(1)['mac_address']
    mac_addr_sw2 = sw2.libs.vtysh.show_interface(1)['mac_address']
    assert mac_addr_sw1 != mac_addr_sw2,\
        'Mac address of interfaces in sw1 is equal to mac address of ' +\
        'interfaces in sw2. This is a test framework problem. Dynamic ' +\
        'LAGs cannot work properly under this condition. Refer to Taiga ' +\
        'issue #1251.'

    print("##### Create LAGs ####")
    create_lag(sw1, lag_id, mode_active)
    create_lag(sw2, lag_id, mode_passive)

    print("### Set LACP rate to fast ###")
    set_lacp_rate_fast(sw1, lag_id)
    set_lacp_rate_fast(sw2, lag_id)

    print("#### Associate Interfaces to LAG ####")
    for intf in ports_sw1[1:3]:
        associate_interface_to_lag(sw1, intf, lag_id)

    for intf in ports_sw2[1:3]:
        associate_interface_to_lag(sw2, intf, lag_id)

    print("#### Verify LAG configuration ####")
    verify_lag_config(sw1, lag_id, ports_sw1[1:3],
                      heartbeat_rate='fast', mode=mode_active)
    verify_lag_config(sw2, lag_id, ports_sw2[1:3],
                      heartbeat_rate='fast', mode=mode_passive)

    print("### Verify if LAG is synchronized")
    verify_state_sync_lag(sw1, ports_sw1[1:3], LOCAL_STATE, mode_active)
    verify_state_sync_lag(sw1, ports_sw1[1:3], REMOTE_STATE, mode_passive)
    verify_state_sync_lag(sw2, ports_sw2[1:3], LOCAL_STATE, mode_passive)
    verify_state_sync_lag(sw2, ports_sw2[1:3], REMOTE_STATE, mode_active)

    print("#### Configure VLANs on switches ####")
    create_vlan(sw1, vlan_id)
    create_vlan(sw2, vlan_id)

    associate_vlan_to_l2_interface(sw1, vlan_id, p11h)
    associate_vlan_to_lag(sw1, vlan_id, lag_id)

    associate_vlan_to_l2_interface(sw2, vlan_id, p21h)
    associate_vlan_to_lag(sw2, vlan_id, lag_id)

    print("#### Configure workstations ####")
    hs1.libs.ip.interface('1', addr='140.1.1.10/24', up=True)
    hs2.libs.ip.interface('1', addr='140.1.1.11/24', up=True)


@mark.platform_incompatible(['docker'])
def test_delete_invalid_lag(topology, main_setup, step):
    """
       Test connectivity between hosts persists after trying to delete
       non-existent LAGs on both switches.

    """
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')

    assert sw1 is not None, 'Topology failed getting object sw1'
    assert sw2 is not None, 'Topology failed getting object sw2'
    assert hs1 is not None, 'Topology failed getting object hs1'
    assert hs2 is not None, 'Topology failed getting object hs2'

    step("#### Test ping between clients work ####")
    verify_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                      True)

    step("#### Delete non-existent LAGs on both switches #### ")

    step("### Attempt to delete LAGs on switch1 ###")
    step("## With ID XX ##")
    with raises(UnknownCommandException):
        delete_lag(sw1, 'XX')

    step("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    step("## With ID 0 ##")
    with raises(UnknownCommandException):
        delete_lag(sw1, '0')

    step("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    step("## With ID -1 ##")
    with raises(UnknownCommandException):
        delete_lag(sw1, '-1')

    step("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    step("## With ID 2000 ##")
    with raises(UnknownVtyshException):
        delete_lag(sw1, '2000')

    step("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    step("## With ID 2001 ##")
    with raises(UnknownCommandException):
        delete_lag(sw1, '2001')

    step("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    step("## With ID @%&$#() ##")
    with raises(UnknownCommandException):
        delete_lag(sw1, '@%&$#()')

    step("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    step("## With ID 60000 ##")
    with raises(UnknownCommandException):
        delete_lag(sw1, '60000')

    step("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    step("## With ID 600 ##")
    with raises(UnknownVtyshException):
        delete_lag(sw1, '600')

    step("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    step("## With ID 2 ##")
    with raises(UnknownVtyshException):
        delete_lag(sw1, '2')

    step("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    step("### Attempt to delete LAGs on switch2 ###")
    step("## With ID XX ##")
    with raises(UnknownCommandException):
        delete_lag(sw2, 'XX')

    step("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    step("## With ID 0 ##")
    with raises(UnknownCommandException):
        delete_lag(sw2, '0')

    step("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    step("## With ID -1 ##")
    with raises(UnknownCommandException):
        delete_lag(sw2, '-1')

    step("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    step("## With ID 2000 ##")
    with raises(UnknownVtyshException):
        delete_lag(sw2, '2000')

    step("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    step("## With ID 2001 ##")
    with raises(UnknownCommandException):
        delete_lag(sw2, '2001')

    step("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    step("## With ID @%&$#() ##")
    with raises(UnknownCommandException):
        delete_lag(sw2, '@%&$#()')

    step("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    step("## With ID 60000 ##")
    with raises(UnknownCommandException):
        delete_lag(sw2, '60000')

    step("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    step("## With ID 600 ##")
    with raises(UnknownVtyshException):
        delete_lag(sw2, '600')

    step("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    step("## With ID 2 ##")
    with raises(UnknownVtyshException):
        delete_lag(sw2, '2')

    step("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)
