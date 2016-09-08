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
# Name:        test_lacpd_vtysh_ct_split_interface.py
#
# Objective:   Verify functionality for LAG with split Interfaces
#
# Topology:    1 switch (DUT running OpenSwitch)
#
##########################################################################

from pytest import mark

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


@mark.gate
@mark.skipif(True, reason="Skipping due to instability")
def test_lacp_split_interface(topology, step):
    sw1 = topology.get('sw1')

    assert sw1 is not None

    step("Configure LAG in switch")
    with sw1.libs.vtysh.ConfigInterfaceLag("1") as ctx:
        ctx.lacp_mode_active()

    step("Associate interface 49 to lag1")
    with sw1.libs.vtysh.ConfigInterface("49") as ctx:
        ctx.lag("1")

    step("Validate interface 49 was associated to lag1")
    output = sw1.libs.vtysh.show_lacp_aggregates()
    print(output)
    assert '49' in output['lag1']['interfaces'],\
        'Interface 49 was not associated to lag1'

    # This test is using rapid fire because the Modular Framework
    # it's not allowing to change the prompt to something different
    # than "switch#". There is an enhancement open to fix this
    step("Split interface 49")
    sw1('configure terminal')
    sw1('interface 49')
    sw1._shells['vtysh']._prompt = ('.*Do you want to continue [y/n]?')
    sw1('split')
    sw1._shells['vtysh']._prompt = ('(^|\n)switch(\\([\\-a-zA-Z0-9]*\\))?#')
    sw1('y')
    sw1('exit')
    sw1('exit')

    step("Validate interface 49 was removed from lag1 because is now split")
    output = sw1.libs.vtysh.show_lacp_aggregates()
    print(output)
    assert '49' not in output['lag1']['interfaces'],\
        'Interface 49 still associated to lag1'

    # Using rapid fire again for problem with prompt in Modular Framework
    step("Try to associate interface 49 to lag1")
    sw1('configure terminal')
    sw1('interface 49')
    output = sw1('lag 1')

    step("Validate there is an error when associated split interface to LAG")
    assert output == 'Split interface can\'t be part of a LAG',\
        'Not error shown when associating parent split interface to LAG'
    sw1('exit')
    sw1('exit')

    step("Validate interface 49 was not associated with lag1")
    output = sw1.libs.vtysh.show_lacp_aggregates()
    assert '49' not in output['lag1']['interfaces'],\
        'Interface 49 is associated to LAG when it shouldn\'t'
