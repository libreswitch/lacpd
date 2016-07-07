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
# Name         test_ft_lag_dynamic_move_interfaces
#
# Objective:    Move an interface associated with an dynamic LAG to another
#               dynamic LAG
#
# Description: This test verifies that an interface can be moved from one
#              dynamic LAG to another dynamic LAG
#
# Topology:    |Switch|
#
# Success Criteria:  PASS -> The interface is on LAG 1 after step 2 and
#                           it is moved to LAG 2 after step 4
#
#                    FAILED -> The interface is not on LAG 1 after step 2,
#                               nor is it on LAG 2 after step 4
#
###############################################################################

from pytest import mark
from lacp_lib import (
    associate_interface_to_lag,
    create_lag,
    validate_interface_not_in_lag,
    verify_lag_config,
    verify_lag_interface_lag_id
)

TOPOLOGY = """
#  +----------+
#  |  switch  |
#  +----------+

# Nodes
[type=openswitch name="OpenSwitch 1"] sw1
"""


@mark.gate
@mark.skipif(True, reason="Skipping due to instability")
def test_dynamic_move_interfaces(topology, step):
    sw1 = topology.get('sw1')
    assert sw1 is not None

    lag_id1 = '1'
    lag_id2 = '2'
    lag_id10 = '10'
    lag_id20 = '20'
    lag_mode_active = 'active'
    lag_mode_passive = 'passive'
    interface_id1 = '1'
    interface_id2 = '2'

    # Create a LAG passive
    step("Create a dynamic LAG passive with id " + lag_id1)
    create_lag(sw1, lag_id1, lag_mode_passive)

    # Add an interface to LAG 1
    step("Add and interface to LAG " + lag_id1)
    associate_interface_to_lag(sw1, interface_id1, lag_id1)

    # Verify LAG configuration
    step("Verify LAG configuration")
    verify_lag_config(sw1, lag_id1, interface_id1, mode=lag_mode_passive)

    # Verify if the interface was added to LAG
    step("Verify if the interface was added to LAG")
    map_lacp = sw1.libs.vtysh.show_lacp_interface(interface_id1)
    verify_lag_interface_lag_id(map_lacp, lag_id1)

    # Create a LAG passive
    step("Create a dynamic LAG passive with id " + lag_id2)
    create_lag(sw1, lag_id2, lag_mode_passive)

    # Add an interface to LAG 2
    step("Add and interface to LAG 2")
    associate_interface_to_lag(sw1, interface_id1, lag_id2)

    # Verify LAG configuration
    step("Verify LAG configuration")
    verify_lag_config(sw1, lag_id2, interface_id1, mode=lag_mode_passive)

    # Verify if the interface was added to LAG
    step("Verify if the interface was added to LAG")
    map_lacp = sw1.libs.vtysh.show_lacp_interface(interface_id1)
    verify_lag_interface_lag_id(map_lacp, lag_id2)

    # Verify if the interface is not in LAG 1
    step("Verify if the interface is not in LAG 1")
    validate_interface_not_in_lag(sw1, interface_id1, lag_id1)

    # Create a LAG active
    step("Create a dynamic LAG active with id " + lag_id10)
    create_lag(sw1, lag_id10, lag_mode_active)

    # Add an interface to LAG 10
    step("Add and interface to LAG " + lag_id10)
    associate_interface_to_lag(sw1, interface_id2, lag_id10)

    # Verify LAG configuration
    step("Verify LAG configuration")
    verify_lag_config(sw1, lag_id10, interface_id2, mode=lag_mode_active)

    # Verify if the interface was added to LAG
    step("Verify if the interface was added to LAG")
    map_lacp = sw1.libs.vtysh.show_lacp_interface(interface_id2)
    verify_lag_interface_lag_id(map_lacp, lag_id10)

    # Create a LAG active
    step("Create a dynamic LAG active with id " + lag_id20)
    create_lag(sw1, lag_id20, lag_mode_active)

    # Add an interface to LAG 2
    step("Add and interface to LAG 20")
    associate_interface_to_lag(sw1, interface_id2, lag_id20)

    # Verify LAG configuration
    step("Verify LAG configuration")
    verify_lag_config(sw1, lag_id20, interface_id2, mode=lag_mode_active)

    # Verify if the interface was added to LAG
    step("Verify if the interface was added to LAG")
    map_lacp = sw1.libs.vtysh.show_lacp_interface(interface_id2)
    verify_lag_interface_lag_id(map_lacp, lag_id20)

    # Verify if the interface is not in LAG 1
    step("Verify if the interface is not in LAG 10")
    validate_interface_not_in_lag(sw1, interface_id2, lag_id10)
