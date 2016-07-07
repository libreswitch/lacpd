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
# Name:        test_lag_ct_appctl_getlacpinterfaces.py
#
# Objective:   Verify correct format output of ovs-appctl lag command
#              getlacpinterfaces <lag_name>..
#
# Topology:    2 switches (DUT running Halon) connected by 4 interfaces
#
#
##########################################################################

from lib_test import sw_create_bond
from lib_test import sw_set_intf_user_config
from pytest import mark


TOPOLOGY = """
#   +-----+------+
#   |            |
#   |    sw1     |
#   |            |
#   +--+-+--+-+--+
#      | |  | |
# LAG1 | |  | | LAG2
#      | |  | |
#   +--+-+--+-+--+
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
sw1:3 -- sw2:3
sw1:4 -- sw2:4
"""


def enable_intf_list(sw, intf_list):
    for intf in intf_list:
        sw_set_intf_user_config(sw, intf, ['admin=up'])


@mark.gate
@mark.skipif(True, reason="Skipping due to instability")
def test_ovs_appctl_getlacpinterfaces(topology):
    """
        Verify the correct format output of the ovs-appctl command
        getlacpinterfaces <lag_name>.
    """
    sw1 = topology.get('sw1')
    sw2 = topology.get('sw2')
    lag_name_1 = 'lag1'
    lag_name_2 = 'lag2'

    """
    The expected output has this format:
    Port lag_name: lag_name
        configured_members   :
        eligible_members     :
        participant_members  :
    Port lag_name:
        configured_members   :
        eligible_members     :
        participant_members  :
    """
    expected_output = [lag_name_1, lag_name_2, "configured_members",
                       "eligible_members", "participant_members"]

    assert sw1 is not None
    assert sw2 is not None

    p11 = sw1.ports['1']
    p12 = sw1.ports['2']
    p13 = sw1.ports['3']
    p14 = sw1.ports['4']
    p21 = sw2.ports['1']
    p22 = sw2.ports['2']
    p23 = sw2.ports['3']
    p24 = sw2.ports['4']

    print("Turning on all interfaces used in this test")
    ports_sw1 = [p11, p12, p13, p14]
    enable_intf_list(sw1, ports_sw1)
    ports_sw2 = [p21, p22, p23, p24]
    enable_intf_list(sw2, ports_sw2)

    print("Create LAGs in both switches")
    output = sw_create_bond(sw1, lag_name_1, ports_sw1[0:2],
                            lacp_mode="active")
    assert output == "", ("Error creating LAG %s returned %s"
                          % (lag_name_1, output))
    output = sw_create_bond(sw2, lag_name_1, ports_sw2[0:2],
                            lacp_mode="active")
    assert output == "", ("Error creating LAG %s returned %s"
                          % (lag_name_1, output))
    output = sw_create_bond(sw1, lag_name_2, ports_sw1[2:4], lacp_mode="off")
    assert output == "", ("Error creating LAG %s returned %s"
                          % (lag_name_2, output))
    output = sw_create_bond(sw2, lag_name_2, ports_sw2[2:4], lacp_mode="off")
    assert output == "", ("Error creating LAG %s returned %s"
                          % (lag_name_2, output))

    print("Execute getlacpinterfaces command")
    c = "ovs-appctl -t ops-lacpd lacpd/getlacpinterfaces"
    output = sw1(c, shell='bash')
    for expected_output_element in expected_output[0:2]:
        assert expected_output_element in output,\
            "Element: %s is not in output" % (expected_output_element)
    for expected_output_element in expected_output[2:5]:
        assert output.count(expected_output_element) == 2,\
            "Element: %s is not twice in output" % (expected_output_element)

    print("Execute getlacpinterfaces command for lag2")
    c = "ovs-appctl -t ops-lacpd lacpd/getlacpinterfaces lag2"
    output = sw1(c, shell='bash')
    for expected_output_element in expected_output[1:5]:
        assert expected_output_element in output,\
            "Element: %s is not in output" % (expected_output_element)

    print("Execute getlacpinterfaces command with non existent lag")
    c = "ovs-appctl -t ops-lacpd lacpd/getlacpinterfaces lag3"
    output = sw1(c, shell='bash')
    assert output == "", ("Error: getlacpinterfaces command with non " +
                          "existent LAG returned %s" % (output))
