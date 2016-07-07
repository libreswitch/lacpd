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

from time import sleep
from lib_test import sw_create_bond
from lib_test import sw_set_intf_user_config
from lib_test import set_system_lacp_config
from lib_test import verify_intf_lacp_status
from pytest import mark

TOPOLOGY = """
# +-------+
# |  sw1  |
# +-------+

# Nodes
[type=openswitch name="Switch 1"] sw1
"""

test_lag = 'lag100'
test_port_labels = ['if01', 'if02']
test_lag_ifs = ['1', '2']
system_mac = 'aa:bb:cc:dd:ee:99'


@mark.gate
@mark.skipif(True, reason="Skipping due to instability")
def test_lacpd_ct_system_priority(topology, step):
    sw1 = topology.get('sw1')
    assert sw1 is not None

# TODO: Is there a common way in LACP CT to make sure the system is ready?
    sleep(10)

    for intf in test_lag_ifs:
        sw_set_intf_user_config(sw1, intf, ['admin=up'])

    sw_create_bond(sw1, test_lag, test_lag_ifs, lacp_mode='active')

    step('Verify interfaces are working')
# TODO get code from Mario once it merges

# Set sys_id and sys_pri
    for priority in range(500, 600):
        step("Set LACP system priority")
        set_system_lacp_config(sw1, ['lacp-system-id=' +
                               system_mac, 'lacp-system-priority=' +
                               str(priority)])

        step("Verify system lacp-system-priority was applied to the"
             " interfaces.\n")

        for intf in test_lag_ifs:
            verify_intf_lacp_status(sw1,
                                    intf,
                                    {"actor_system_id": str(priority) +
                                     "," + system_mac},
                                    "s1:" + intf)
