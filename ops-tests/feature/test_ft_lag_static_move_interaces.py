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
# Name         test_ft_lag_static_move_interfaces
#
# Objective:    Move an interface associated with an static LAG to another
#               static LAG
#
# Description: This test verifies that an interface can be moved from one
#              static LAG to another static LAG.
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

import pytest
from lacp_lib import (
    associate_interface_to_lag,
    create_lag,
    LOCAL_STATE,
    validate_interface_not_in_lag,
    validate_lag_state_static,
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


@pytest.mark.skipif(True, reason="Skipping due to instability")
def test_static_move_interfaces(topology, step):
    sw1 = topology.get('sw1')
    assert sw1 is not None

    lag_id1 = '1'
    lag_id2 = '2'
    interface_id = '1'

    # Create a LAG
    step("Create a LAG with id" + lag_id1)
    create_lag(sw1, lag_id1, 'off')

    # Add an interface to LAG 1
    step("Add and interface to LAG " + lag_id1)
    associate_interface_to_lag(sw1, interface_id, lag_id1)

    # Verify LAG configuration
    step("Verify LAG configuration")
    verify_lag_config(sw1, lag_id1, interface_id)
    map_lacp = sw1.libs.vtysh.show_lacp_interface(interface_id)
    validate_lag_state_static(map_lacp, LOCAL_STATE)

    # Verify if the interface was added to static LAG
    step("Verify if the interface was added to static LAG")
    verify_lag_interface_lag_id(map_lacp, lag_id1)

    # Create a LAG
    step("Create a LAG with id " + lag_id2)
    create_lag(sw1, lag_id2, 'off')

    # Add an interface to LAG 2
    step("Add and interface to LAG 2")
    associate_interface_to_lag(sw1, '1', lag_id2)

    # Verify LAG configuration
    step("Verify LAG configuration")
    verify_lag_config(sw1, lag_id2, interface_id)
    map_lacp = sw1.libs.vtysh.show_lacp_interface(interface_id)
    validate_lag_state_static(map_lacp, LOCAL_STATE)

    # Verify if the interface was added to static LAG
    step("Verify if the interface was added to static LAG")
    verify_lag_interface_lag_id(map_lacp, lag_id2)

    # Verify if the interface is not in LAG 1
    step("Verify if the interface is not in LAG 1")
    validate_interface_not_in_lag(sw1, interface_id, lag_id1)
