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

from time import sleep
from pytest import raises
from lacp_lib import turn_on_interface
from lacp_lib import validate_turn_on_interfaces
from lacp_lib import create_lag_active
from lacp_lib import create_lag_passive
from lacp_lib import associate_interface_to_lag
from lacp_lib import associate_vlan_to_l2_interface
from lacp_lib import associate_vlan_to_lag
from lacp_lib import create_vlan
from lacp_lib import check_connectivity_between_hosts
from lacp_lib import delete_lag
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


@pytest.mark.skipif(True, reason="Skipping due to instability")
def test_delete_invalid_lag(topology):

    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    hs1 = topology.get('hs1')
    hs2 = topology.get('hs2')

    assert sw1 is not None
    assert sw2 is not None
    assert hs1 is not None
    assert hs2 is not None

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

    print("#### Wait for interfaces to turn on ####")
    sleep(60)

    print("#### Validate interfaces are turn on ####")
    validate_turn_on_interfaces(sw1, ports_sw1)
    validate_turn_on_interfaces(sw2, ports_sw2)

    print("##### Create LAGs ####")
    create_lag_active(sw1, '1')
    create_lag_passive(sw2, '1')

    print("#### Associate Interfaces to LAG ####")
    for intf in ports_sw1[1:3]:
        associate_interface_to_lag(sw1, intf, '1')

    for intf in ports_sw2[1:3]:
        associate_interface_to_lag(sw2, intf, '1')

    print("#### Wait for LAG negotiation ####")
    sleep(40)

    print("#### Configure VLANs on switches ####")
    create_vlan(sw1, '900')
    create_vlan(sw2, '900')

    associate_vlan_to_l2_interface(sw1, '900', p11h)
    associate_vlan_to_lag(sw1, '900', '1')

    associate_vlan_to_l2_interface(sw2, '900', p21h)
    associate_vlan_to_lag(sw2, '900', '1')

    print("#### Configure workstations ####")
    hs1.libs.ip.interface('1', addr='140.1.1.10/24', up=True)
    hs2.libs.ip.interface('1', addr='140.1.1.11/24', up=True)

    print("#### Waiting for vlan and interface configuration ####")
    sleep(100)
    print("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    print("#### Delete non-existent LAGs on both switches #### ")

    print("### Attempt to delete LAGs on switch1 ###")
    print("## With ID XX ##")
    with raises(UnknownCommandException):
        delete_lag(sw1, 'XX')

    print("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    print("## With ID 0 ##")
    with raises(UnknownCommandException):
        delete_lag(sw1, '0')

    print("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    print("## With ID -1 ##")
    with raises(UnknownCommandException):
        delete_lag(sw1, '-1')

    print("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    print("## With ID 2000 ##")
    with raises(UnknownVtyshException):
        delete_lag(sw1, '2000')

    print("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    print("## With ID 2001 ##")
    with raises(UnknownCommandException):
        delete_lag(sw1, '2001')

    print("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    print("## With ID @%&$#() ##")
    with raises(UnknownCommandException):
        delete_lag(sw1, '@%&$#()')

    print("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    print("## With ID 60000 ##")
    with raises(UnknownCommandException):
        delete_lag(sw1, '60000')

    print("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    print("## With ID 600 ##")
    with raises(UnknownVtyshException):
        delete_lag(sw1, '600')

    print("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    print("## With ID 2 ##")
    with raises(UnknownVtyshException):
        delete_lag(sw1, '2')

    print("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    print("### Attempt to delete LAGs on switch2 ###")
    print("## With ID XX ##")
    with raises(UnknownCommandException):
        delete_lag(sw2, 'XX')

    print("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    print("## With ID 0 ##")
    with raises(UnknownCommandException):
        delete_lag(sw2, '0')

    print("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    print("## With ID -1 ##")
    with raises(UnknownCommandException):
        delete_lag(sw2, '-1')

    print("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    print("## With ID 2000 ##")
    with raises(UnknownVtyshException):
        delete_lag(sw2, '2000')

    print("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    print("## With ID 2001 ##")
    with raises(UnknownCommandException):
        delete_lag(sw2, '2001')

    print("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    print("## With ID @%&$#() ##")
    with raises(UnknownCommandException):
        delete_lag(sw2, '@%&$#()')

    print("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    print("## With ID 60000 ##")
    with raises(UnknownCommandException):
        delete_lag(sw2, '60000')

    print("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    print("## With ID 600 ##")
    with raises(UnknownVtyshException):
        delete_lag(sw2, '600')

    print("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)

    print("## With ID 2 ##")
    with raises(UnknownVtyshException):
        delete_lag(sw2, '2')

    print("#### Test ping between clients work ####")
    check_connectivity_between_hosts(hs1, '140.1.1.10', hs2, '140.1.1.11',
                                     5, True)
