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
# Name:        test_ft_static_lag_ip_config.py
#
# Objective:   Verify a static LAG can configure IP correctly
#
# Topology:    1 switch
#
##########################################################################

from lacp_lib import (
    assign_ip_to_lag,
    assign_secondary_ip_to_lag,
    create_lag,
    delete_ip_from_lag,
    delete_secondary_ip_from_lag
)

TOPOLOGY = """
#   +-----+------+
#   |            |
#   |    sw1     |
#   |            |
#   +---+---+----+

# Nodes
[type=openswitch name="OpenSwitch 1"] sw1
"""


def test_ip_config_case_1(topology, step):
    """
    Case 1:
        Verify the correct configuration of ip address
    """
    sw1 = topology.get('sw1')
    ip_address_mask = '24'
    ip_address = '10.0.0.1'
    ip_address_complete = '10.0.0.1/24'
    ip_address_secondary = '20.0.0.1'
    ip_address_secondary_complete = '20.0.0.1/24'
    sw1_lag_id = '10'

    assert sw1 is not None

    step("Create LAG in switch 1")
    create_lag(sw1, sw1_lag_id, 'off')

    step("Assign IP to LAG 1")
    assign_ip_to_lag(sw1, sw1_lag_id, ip_address, ip_address_mask)

    step("Assign IP secondaries to LAG 1")
    assign_secondary_ip_to_lag(sw1, sw1_lag_id,
                               ip_address_secondary,
                               ip_address_mask)

    step("Verify IP configuration")
    lag_name = 'lag{sw1_lag_id}'.format(**locals())
    lag_config = sw1.libs.vtysh.show_interface(lag_name)
    assert lag_config['ipv4'] == ip_address_complete,\
        "LAG IP address is not properly configured - Failed"
    assert lag_config['ipv4_secondary'] == ip_address_secondary_complete,\
        "LAG IP secondary address is not properly configured - Failed"

    step("Delete primary IP address")
    not_deleted = False
    try:
        delete_ip_from_lag(sw1, sw1_lag_id, ip_address, ip_address_mask)
    except:
        not_deleted = True

    step("Verify primary IP address is not deleted")
    lag_config = sw1.libs.vtysh.show_interface(lag_name)
    assert not_deleted and lag_config['ipv4'] == ip_address_complete,\
        "LAG IP address is not properly configured - Failed"

    step("Delete secondary IP address")
    delete_secondary_ip_from_lag(sw1, sw1_lag_id, ip_address_secondary,
                                 ip_address_mask)

    step("Verify secondary IP address was deleted")
    lag_config = sw1.libs.vtysh.show_interface(lag_name)
    assert lag_config['ipv4_secondary'] != ip_address_secondary_complete,\
        "LAG IP secondary address was not deleted - Failed"

    step("Delete primary IP address")
    delete_ip_from_lag(sw1, sw1_lag_id, ip_address, ip_address_mask)

    step("Verify primary IP address is deleted")
    lag_config = sw1.libs.vtysh.show_interface(lag_name)
    assert lag_config['ipv4'] != ip_address_complete,\
        "LAG IP primary address was not deleted - Failed"
