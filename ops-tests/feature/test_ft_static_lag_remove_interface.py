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
# Name:        test_ft_static_lag_remove_interface.py
#
# Objective:   Verify interfaces are removed correctly from a LAG
#
# Topology:    1 switch
#
##########################################################################

from lacp_lib import (
    associate_interface_to_lag,
    create_lag,
    remove_interface_from_lag,
    turn_on_interface,
    verify_lag_config,
    verify_turn_off_interfaces,
    verify_turn_on_interfaces
)

TOPOLOGY = """
#   +-----+------+
#   |            |
#   |    sw1     |
#   |            |
#   +---+---+----+

# Nodes
[type=openswitch name="OpenSwitch 1"] sw1

#Links
sw1:1
"""


def test_remove_interface_from_lag_case_1(topology, step):
    """
    Case 1:
        Verify that an interface is correclty removed from a LAG
    """
    sw1 = topology.get('sw1')
    sw1_lag_id = '10'

    assert sw1 is not None

    step("Mapping interfaces")
    port = sw1.ports['1']

    step("Turning on interface 1 used in this test")
    turn_on_interface(sw1, port)

    step("Verify interface 1 is up")
    verify_turn_on_interfaces(sw1, [port])

    step("Create LAG")
    create_lag(sw1, sw1_lag_id, 'off')

    step("Associate interface [1] to LAG")
    associate_interface_to_lag(sw1, port, sw1_lag_id)

    step("Verify interface 1 is in LAG 1")
    verify_lag_config(sw1, sw1_lag_id, [port])

    step("Remove interface from LAG 1")
    remove_interface_from_lag(sw1, port, sw1_lag_id)

    step("Verify interface is shutdown")
    verify_turn_off_interfaces(sw1, [port])
