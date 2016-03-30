#(C) Copyright 2016 Hewlett Packard Enterprise Development LP
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

import time
from opstestfw import *
from opstestfw.switch.CLI import *
from opstestfw.host import *

OVS_VSCTL = "/usr/bin/ovs-vsctl "


# This method calls a function to retrieve data, then calls another function
# to compare the data to the expected value(s). If it fails, it sleeps for
# half a second, then retries, up to a specified retry limit (default 20 = 10
# seconds). It returns a tuple of the test status and the actual results.
def timed_compare(data_func, params, compare_func,
                  expected_results, retries=20):
    while retries != 0:
        actual_results = data_func(params)
        result = compare_func(actual_results, expected_results, retries == 1)
        if result is True:
            return True, actual_results
        time.sleep(0.5)
        retries -= 1
    return False, actual_results


# Set user_config for an Interface.
def sw_set_intf_user_config(sw, interface, config):
    c = OVS_VSCTL + "set interface " + str(interface)
    for s in config:
        c += " user_config:" + s
    debug(c)
    return sw.ovscmd(c)


# Clear user_config for an Interface.
def sw_clear_user_config(sw, interface):
    c = OVS_VSCTL + "clear interface " + str(interface) + " user_config"
    debug(c)
    return sw.ovscmd(c)


# Set pm_info for an Interface.
def sw_set_intf_pm_info(sw, interface, config):
    c = OVS_VSCTL + "set interface " + str(interface)
    for s in config:
        c += " pm_info:" + s
    debug(c)
    return sw.ovscmd(c)


# Set open_vsw_lacp_config parameter(s)
def set_port_parameter(sw, port, config):
    c = OVS_VSCTL + "set port " + str(port)
    for s in config:
        c += ' %s' % s
    debug(c)
    return sw.ovscmd(c)


# Get the values of a set of columns from Interface table.
# This function returns a list of values if 2 or more
# fields are requested, and returns a single value (no list)
# if only 1 field is requested.
def sw_get_intf_state(params):
    c = OVS_VSCTL + "get interface " + str(params[1])
    for f in params[2]:
        c += " " + f
    out = params[0].ovscmd(c).splitlines()
    debug(out)
    return out


def sw_get_port_state(params):
    c = OVS_VSCTL + "get port " + str(params[1])
    for f in params[2]:
        c += " " + f
    out = params[0].ovscmd(c).splitlines()
    if len(out) == 1:
        out = out[0]
    debug(out)
    return out


# Create a bond/lag/trunk in the OVS-DB.
def sw_create_bond(s1, bond_name, intf_list, lacp_mode="off"):
    info("Creating LAG " + bond_name + " with interfaces: " +
         str(intf_list) + "\n")
    c = OVS_VSCTL + "add-bond bridge_normal " + bond_name +\
        " " + " ".join(map(str, intf_list))
    c += " -- set port " + bond_name + " lacp=" + lacp_mode
    debug(c)
    return s1.ovscmd(c)


def verify_compare_value(actual, expected, final):
    if actual != expected:
        return False
    return True


def verify_compare_tuple(actual, expected, final):
    if len(actual) != len(expected):
        return False
    if actual != expected:
        return False
    return True


def verify_compare_tuple_negate(actual, expected, final):
    if len(actual) != len(expected):
        return False
    for i in range(0, len(expected)):
        if actual[i] == expected[i]:
            return False
    return True


def verify_compare_complex(actual, expected, final):
    attrs = []
    for attr in expected:
        attrs.append(attr)
    if len(actual) != len(expected):
        return False
    for i in range(0, len(attrs)):
        if actual[i] != expected[attrs[i]]:
            return False
    return True


# Verify that an Interface is part of a bond.
def verify_intf_in_bond(sw, intf, msg):
    result = timed_compare(sw_get_intf_state,
                           (sw, intf, ['hw_bond_config:rx_enabled',
                            'hw_bond_config:tx_enabled']),
                            verify_compare_tuple, ['true', 'true'])
    assert result == (True, ["true", "true"]), msg


# Verify that an Interface is not part of any bond.
def verify_intf_not_in_bond(sw, intf, msg):
    result = timed_compare(sw_get_intf_state,
                           (sw, intf, ['hw_bond_config:rx_enabled',
                            'hw_bond_config:tx_enabled']),
                            verify_compare_tuple_negate, ['true', 'true'])
    assert result[0] is True and\
        result[1][0] is not 'true' and\
        result[1][1] is not 'true', msg


# Verify Interface status
def verify_intf_status(sw, intf, column_name, value, msg=''):
    result = timed_compare(sw_get_intf_state,
                           (sw, intf, [column_name]),
                            verify_compare_tuple, [value])
    assert result == (True, [value]), msg


def verify_intf_field_absent(sw, intf, field, msg):
    retries = 20
    while retries != 0:
        result = sw_get_intf_state((sw, intf, [field]))
        if "no key" in result[0]:
            return
        time.sleep(0.5)
        retries -= 1
    assert "no key" in result, msg


# Remove an Interface from a bond.
def remove_intf_from_bond(sw, bond_name, intf_name, fail=True):

    info("Removing interface " + intf_name + " from LAG " + bond_name + "\n")

    # Get the UUID of the Interface that has to be removed.
    c = OVS_VSCTL + "get interface " + str(intf_name) + " _uuid"
    debug(c)
    intf_uuid = sw.ovscmd(c).rstrip('\r\n')

    # Get the current list of Interfaces in the bond.
    c = OVS_VSCTL + "get port " + bond_name + " interfaces"
    debug(c)
    out = sw.ovscmd(c)
    intf_list = out.rstrip('\r\n').strip("[]").replace(" ", "").split(',')

    if intf_uuid not in intf_list:
        assert fail is True, "Unable to find the interface in the bond"
        return

    # Remove the given intf_name's UUID from the bond's Interfaces.
    new_intf_list = [i for i in intf_list if i != intf_uuid]

    # Set the new Interface list in the bond.
    new_intf_str = '[' + ",".join(new_intf_list) + ']'

    c = OVS_VSCTL + "set port " + bond_name + " interfaces=" + new_intf_str
    debug(c)
    out = sw.ovscmd(c)

    return out


# Remove a list of Interfaces from the bond.
def remove_intf_list_from_bond(sw, bond_name, intf_list):
    for intf in intf_list:
        remove_intf_from_bond(sw, bond_name, intf)
