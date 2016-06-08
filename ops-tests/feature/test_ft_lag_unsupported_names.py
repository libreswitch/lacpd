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
# Name         test_ft_lag_unsupported_names
#
# Objective:    To verify a LAG(static or dynamic) cannot be created if the
#               name is too long or if it has unsupported characters.
#
# Description: This test passes if the LAG can be configured using
#              valid names and cannot be configured using names longer than
#              permitted or names containing invalid characters.
#
# Topology:    |Switch|
#
# Success Criteria:  PASS -> Test will pass if LAG (static or dynamic) can be
#                            configured using valid name and cannot be
#                            configured using name longer than permitted or
#                            containing invalid characters.
#
#                    FAILED -> This test fails if the LAG (static or dynamic)
#                              cannot be configured using valid names or can be
#                              configured using an invalid name.
#
###############################################################################

from lacp_lib import (
    create_lag,
    delete_lag
)
from pytest import raises

TOPOLOGY = """
#  +----------+
#  |  switch  |
#  +----------+

# Nodes
[type=openswitch name="OpenSwitch 1"] sw1
"""


def test_unsupported_names(topology, step):

    sw1 = topology.get('sw1')
    assert sw1 is not None

    invalid_id = ['-1', '2001', 'as&']
    valid_id = ['1', '2000']
    mode_active = "active"
    mode_passive = "passive"

    for lag_id in valid_id:
        step("## Create static LAG with id: " + lag_id + " ##")
        create_lag(sw1, lag_id, 'off')

    for lag_id in invalid_id:
        step("## Create invalid LAG: " + lag_id + " ##")
        with raises(AssertionError):
            create_lag(sw1, lag_id, 'off')
        # Negative test, sending "end" command to exit configuration context
        sw1('end')

    for lag_id in valid_id:
        step("## Delete Static LAG id" + lag_id + " ##")
        delete_lag(sw1, lag_id)

    for lag_id in valid_id:
        step("## Create dynamic (active) LAG with id: " + lag_id + " ##")
        create_lag(sw1, lag_id, mode_active)

    for lag_id in invalid_id:
        step("## Create invalid LAG: " + lag_id + " ##")
        with raises(AssertionError):
            create_lag(sw1, lag_id, mode_active)
        # Negative test, sending "end" command to exit configuration context
        sw1('end')

    for lag_id in valid_id:
        step("## Delete dynamic (active) LAG id" + lag_id + " ##")
        delete_lag(sw1, lag_id)

    for lag_id in valid_id:
        step("## Create dynamic (passive) LAG with id: " + lag_id + " ##")
        create_lag(sw1, lag_id, mode_passive)

    for lag_id in invalid_id:
        step("## Create invalid LAG: " + lag_id + " ##")
        with raises(AssertionError):
            create_lag(sw1, lag_id, mode_passive)
        # Negative test, sending "end" command to exit configuration context
        sw1('end')
