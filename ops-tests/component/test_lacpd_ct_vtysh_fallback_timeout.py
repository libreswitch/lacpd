# Copyright (C) 2016 Hewlett-Packard Development Company, L.P.
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

##########################################################################
# Name:        test_lacpd_ct_vtysh_fallback_timeout.py
#
# Objective:   Verify functionality for lacp fallback timeout CLI commands
#
# Topology:    1 switch (DUT running OpenSwitch)
#
##########################################################################
from topology_lib_vtysh import exceptions
from lib_test import (
    verify_port_fallback_key
)


TOPOLOGY = """
#
#
# +-------+
# |       |
# |  sw1  |
# |       |
# +-------+
#

# Nodes
[type=openswitch name="Switch 1"] sw1

# Links
"""


def verify_show_running_config(sw, lag_id, fallback_timeout):
    output = sw.libs.vtysh.show_running_config()
    lag_map = output['interface']['lag'][lag_id]
    if (fallback_timeout != 0):
        assert str(fallback_timeout) == lag_map['lacp_fallback_timeout'],\
            'show running-config fallback timeout expected to be ' +\
            str(fallback_timeout)
    else:
        assert 'lacp_fallback_timeout' not in lag_map,\
            'show running-config fallback_timeout expected to be empty'


def verify_show_lacp_aggregates(sw, lag_name, fallback_timeout):
    output = sw.libs.vtysh.show_lacp_aggregates(lag_name)
    assert str(fallback_timeout) == output[lag_name]['fallback_timeout'],\
        'show lacp aggregates fallback_timeout expected to be ' +\
        str(fallback_timeout)


def test_lacp_fallback_timeout_vtysh(topology, step):
    '''
    This tests verifies CLI commands 'lacp fallback timeout <timeout>'
    sets OVSDB Port other_config:lacp_fallback_timeout properly when the
    timeout specified is between 1 and 900 inclusive.

    It also verifies that 'no lacp fallback timeout <timeout>' sets OVSDB
    Port other_config:lacp_fallback_timeout to zero only when the already
    configured timeout is specified.

    This test also verifies lacp fallback timeout appears in the output of
    'show running-config' and 'show lacp aggregates'
    '''

    sw1 = topology.get('sw1')
    assert sw1 is not None

    lag_id = '1'
    lag_name = 'lag1'
    fallback_timeout_key = 'lacp_fallback_timeout'
    timeout_empty_state = 'ovs-vsctl: no key lacp_fallback_timeout in ' +\
                          'Port record ' + lag_name + ' column other_config'
    valid_values = [1, 500, 900]
    invalid_values = [0, 901, 'a', 'string']

    step("Configure LAG in switch")
    with sw1.libs.vtysh.ConfigInterfaceLag(lag_id) as ctx:
        ctx.lacp_mode_active()

    ###########################################################################
    #                      1. Initial Configuration                           #
    ###########################################################################

    step('Verify initial fallback_timeout is empty')
    verify_port_fallback_key(sw1, lag_name, fallback_timeout_key,
                             timeout_empty_state,
                             'lacp_fallback_timeout expected to be EMPTY')
    verify_show_lacp_aggregates(sw1, lag_name, 0)
    verify_show_running_config(sw1, lag_id, 0)

    ###########################################################################
    #                      2. Valid values                                    #
    ###########################################################################

    for value in valid_values:
        step('Configure lacp_fallback_timeout to ' + str(value))
        with sw1.libs.vtysh.ConfigInterfaceLag(lag_id) as ctx:
            ctx.lacp_fallback_timeout(value)

        step('Verify fallback_timeout is ' + str(value))
        verify_port_fallback_key(sw1, lag_name, fallback_timeout_key,
                                 str(value), 'lacp_fallback_timeout ' +
                                 'expected to be ' + str(value))
        verify_show_lacp_aggregates(sw1, lag_name, value)
        verify_show_running_config(sw1, lag_id, value)

    ###########################################################################
    #                      3. Invalid values                                  #
    ###########################################################################

    for value in invalid_values:
        step('Configure lacp_fallback_timeout to ' + str(value))
        with sw1.libs.vtysh.ConfigInterfaceLag(lag_id) as ctx:
            try:
                ctx.lacp_fallback_timeout(value)
            except exceptions.UnknownCommandException:
                success = True
        assert success,\
            ('no lacp_fallback_timeout(' + str(value) +
             ') expected to raise exception ' +
             str(exceptions.UnknownCommandException))

        step('Verify fallback_timeout is still ' + str(valid_values[-1]))
        verify_port_fallback_key(sw1, lag_name, fallback_timeout_key,
                                 str(valid_values[-1]),
                                 'lacp_fallback_timeout expected to be ' +
                                 str(valid_values[-1]))
        verify_show_lacp_aggregates(sw1, lag_name, valid_values[-1])
        verify_show_running_config(sw1, lag_id, valid_values[-1])

    ###########################################################################
    #                      4. Invalid values with no                          #
    ###########################################################################

    for value in invalid_values:
        step('Configure lacp_fallback_timeout to ' + str(value))
        with sw1.libs.vtysh.ConfigInterfaceLag(lag_id) as ctx:
            try:
                ctx.no_lacp_fallback_timeout(value)
            except exceptions.UnknownCommandException:
                success = True
        assert success,\
            ('no lacp_fallback_timeout(' + str(value) +
             ') expected to raise exception ' +
             str(exceptions.UnknownCommandException))

        step('Verify fallback_timeout is still ' + str(valid_values[-1]))
        verify_port_fallback_key(sw1, lag_name, fallback_timeout_key,
                                 str(valid_values[-1]),
                                 'lacp_fallback_timeout expected to be ' +
                                 str(valid_values[-1]))
        verify_show_lacp_aggregates(sw1, lag_name, valid_values[-1])
        verify_show_running_config(sw1, lag_id, valid_values[-1])

    ###########################################################################
    #                      5. No with wrong value                             #
    ###########################################################################

    step('Configure no lacp_fallback_timeout with invalid value 2')
    success = False
    with sw1.libs.vtysh.ConfigInterfaceLag(lag_id) as ctx:
        try:
            ctx.no_lacp_fallback_timeout(2)
        except exceptions.FailedCommandException:
            success = True
    assert success, ('no lacp_fallback_timeout(2) expected to raise ' +
                     'exception ' +
                     str(exceptions.FailedCommandException))

    step('Verify fallback_timeout is still ' + str(valid_values[-1]))
    verify_port_fallback_key(sw1, lag_name, fallback_timeout_key,
                             str(valid_values[-1]),
                             'lacp_fallback_timeout expected to be ' +
                             str(valid_values[-1]))
    verify_show_lacp_aggregates(sw1, lag_name, valid_values[-1])
    verify_show_running_config(sw1, lag_id, valid_values[-1])

    ###########################################################################
    #                      6. No with right value                             #
    ###########################################################################

    step('Configure no lacp_fallback_timeout with ' + str(valid_values[-1]))
    with sw1.libs.vtysh.ConfigInterfaceLag(lag_id) as ctx:
        ctx.no_lacp_fallback_timeout(valid_values[-1])

    step('Verify initial fallback_timeout is empty')
    verify_port_fallback_key(sw1, lag_name, fallback_timeout_key,
                             timeout_empty_state,
                             'lacp_fallback_timeout expected to be EMPTY')
    verify_show_lacp_aggregates(sw1, lag_name, 0)
    verify_show_running_config(sw1, lag_id, 0)
