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
# Name:        test_lacpd_ct_vtysh_fallback_mode.py
#
# Objective:   Verify functionality for lacp fallback mode CLI commands
#
# Topology:    1 switch (DUT running OpenSwitch)
#
##########################################################################
from lib_test import (
    verify_port_fallback_key
)
import pytest


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


def verify_show_running_config(sw, lag_id, fallback_mode):
    output = sw.libs.vtysh.show_running_config()
    lag_map = output['interface']['lag'][lag_id]
    if (fallback_mode == 'all_active'):
        assert fallback_mode == lag_map['lacp_fallback_mode'],\
            'show running-config fallback_mode expected to be ' + fallback_mode
    else:
        assert 'lacp_fallback_mode' not in lag_map,\
            'show running-config fallback_mode expected to be empty'


def verify_show_lacp_aggregates(sw, lag_name, fallback_mode):
    output = sw.libs.vtysh.show_lacp_aggregates(lag_name)
    assert fallback_mode == output[lag_name]['fallback_mode'],\
        'show lacp aggregates fallback_mode expected to be ' + fallback_mode


@pytest.mark.skipif(True, reason="Skipping due to missing code")
def test_lacp_fallback_mode_vtysh(topology, step):
    '''
    This tests verifies CLI commands 'lacp fallback mode (priority/all_active)'
    sets OVSDB Port other_config:lacp_fallback_mode properly.
    1. When all_active mode is configured, other_config:lacp_fallback_mode
    should be set to 'all_active'.
    2. When priority mode is configured, other_config:lacp_fallback_mode
    should be empty.

    This test also verifies lacp fallback mode appears in the output of
    'show running-config' and 'show lacp aggregates'
    '''

    sw1 = topology.get('sw1')
    assert sw1 is not None

    lag_id = '1'
    lag_name = 'lag1'
    fallback_mode_key = 'lacp_fallback_mode'
    mode_empty_state = 'ovs-vsctl: no key lacp_fallback_mode in Port ' +\
                       'record ' + lag_name + ' column other_config'

    step("Configure LAG in switch")
    with sw1.libs.vtysh.ConfigInterfaceLag(lag_id) as ctx:
        ctx.lacp_mode_active()

    ###########################################################################
    #                       1. Initial State                                  #
    ###########################################################################

    step('Verify initial fallback mode is empty')
    verify_port_fallback_key(sw1, lag_name, fallback_mode_key,
                             mode_empty_state,
                             'lacp_fallback_mode expected to be EMPTY')
    verify_show_lacp_aggregates(sw1, lag_name, 'priority')
    verify_show_running_config(sw1, lag_id, 'priority')

    ###########################################################################
    #                       2. Set to all_active                              #
    ###########################################################################

    step('Configure lacp_fallback_mode as all_active')
    with sw1.libs.vtysh.ConfigInterfaceLag(lag_id) as ctx:
        ctx.lacp_fallback_mode_all_active()

    step('Verify fallback mode is all_active')
    verify_port_fallback_key(sw1, lag_name, fallback_mode_key, 'all_active',
                             'lacp_fallback_mode expected to be ALL_ACTIVE')
    verify_show_lacp_aggregates(sw1, lag_name, 'all_active')
    verify_show_running_config(sw1, lag_id, 'all_active')

    ###########################################################################
    #                       3. Set to priority                                #
    ###########################################################################

    step('Configure lacp_fallback_mode as priority')
    with sw1.libs.vtysh.ConfigInterfaceLag(lag_id) as ctx:
        ctx.lacp_fallback_mode_priority()

    step('Verify fallback mode is empty')
    verify_port_fallback_key(sw1, lag_name, fallback_mode_key,
                             mode_empty_state,
                             'lacp_fallback_mode expected to be EMPTY')
    verify_show_lacp_aggregates(sw1, lag_name, 'priority')
    verify_show_running_config(sw1, lag_id, 'priority')

    ###########################################################################
    #                   4. Set to all_active and remove it                    #
    ###########################################################################

    step('Configure lacp_fallback_mode as all_active')
    with sw1.libs.vtysh.ConfigInterfaceLag(lag_id) as ctx:
        ctx.lacp_fallback_mode_all_active()

    step('Verify fallback mode is all_active')
    verify_port_fallback_key(sw1, lag_name, fallback_mode_key, 'all_active',
                             'lacp_fallback_mode expected to be ALL_ACTIVE')
    verify_show_lacp_aggregates(sw1, lag_name, 'all_active')
    verify_show_running_config(sw1, lag_id, 'all_active')

    step('Configure lacp_fallback_mode as no all_active')
    with sw1.libs.vtysh.ConfigInterfaceLag(lag_id) as ctx:
        ctx.no_lacp_fallback_mode_all_active()
    step('Verify fallback mode is empty')
    verify_port_fallback_key(sw1, lag_name, fallback_mode_key,
                             mode_empty_state,
                             'lacp_fallback_mode expected to be EMPTY')
    verify_show_lacp_aggregates(sw1, lag_name, 'priority')
    verify_show_running_config(sw1, lag_id, 'priority')

    ###########################################################################
    #     5. Transition from priority to all_active and back to priority      #
    ###########################################################################

    step('Configure lacp_fallback_mode as all_active')
    with sw1.libs.vtysh.ConfigInterfaceLag(lag_id) as ctx:
        ctx.lacp_fallback_mode_all_active()

    step('Verify fallback mode is all_active')
    verify_port_fallback_key(sw1, lag_name, fallback_mode_key, 'all_active',
                             'lacp_fallback_mode expected to be ALL_ACTIVE')
    verify_show_lacp_aggregates(sw1, lag_name, 'all_active')
    verify_show_running_config(sw1, lag_id, 'all_active')

    step('Configure lacp_fallback_mode as priority')
    with sw1.libs.vtysh.ConfigInterfaceLag(lag_id) as ctx:
        ctx.lacp_fallback_mode_priority()

    step('Verify fallback mode is empty')
    verify_port_fallback_key(sw1, lag_name, fallback_mode_key,
                             mode_empty_state,
                             'lacp_fallback_mode expected to be EMPTY')
    verify_show_lacp_aggregates(sw1, lag_name, 'priority')
    verify_show_running_config(sw1, lag_id, 'priority')
