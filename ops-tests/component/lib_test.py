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

ovs_vsctl = "/usr/bin/ovs-vsctl "


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
        sleep(0.5)
        retries -= 1
    return False, actual_results


# Set user_config for an Interface.
def sw_set_intf_user_config(sw, interface, config):
    c = ovs_vsctl + "set interface " + str(interface)
    for s in config:
        c += " user_config:" + s
    return sw(c, shell='bash')


# Clear user_config for an Interface.
def sw_clear_user_config(sw, interface):
    c = ovs_vsctl + "clear interface " + str(interface) + " user_config"
    return sw(c, shell='bash')


# Set pm_info for an Interface.
def sw_set_intf_pm_info(sw, interface, config):
    c = ovs_vsctl + "set interface " + str(interface)
    for s in config:
        c += " pm_info:" + s
    return sw(c, shell='bash')


# Set open_vsw_lacp_config parameter(s)
def set_port_parameter(sw, port, config):
    c = ovs_vsctl + "set port " + str(port)
    for s in config:
        c += ' %s' % s
    return sw(c, shell='bash')


# Get the values of a set of columns from Interface table.
# This function returns a list of values if 2 or more
# fields are requested, and returns a single value (no list)
# if only 1 field is requested.
def sw_get_intf_state(params):
    c = ovs_vsctl + "get interface " + str(params[1])
    for f in params[2]:
        c += " " + f
    out = params[0](c, shell='bash').replace('"', '').splitlines()
    return out


def sw_get_port_state(params):
    c = ovs_vsctl + "get port " + str(params[1])
    for f in params[2]:
        c += " " + f
    out = params[0](c, shell='bash').splitlines()
    if len(out) == 1:
        out = out[0]
    return out


# Create a bond/lag/trunk in the OVS-DB.
def sw_create_bond(s1, bond_name, intf_list, lacp_mode="off"):
    print("Creating LAG " + bond_name + " with interfaces: " +
          str(intf_list) + "\n")
    c = ovs_vsctl + "add-bond bridge_normal " + bond_name +\
        " " + " ".join(map(str, intf_list))
    c += " -- set port " + bond_name + " lacp=" + lacp_mode
    return s1(c, shell='bash')


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
        sleep(0.5)
        retries -= 1
    assert "no key" in result, msg


# Remove an Interface from a bond.
def remove_intf_from_bond(sw, bond_name, intf_name, fail=True):
    print("Removing interface " + intf_name + " from LAG " + bond_name + "\n")

    # Get the UUID of the Interface that has to be removed.
    c = ovs_vsctl + "get interface " + str(intf_name) + " _uuid"
    intf_uuid = sw(c, shell='bash').rstrip('\r\n')

    # Get the current list of Interfaces in the bond.
    c = ovs_vsctl + "get port " + bond_name + " interfaces"
    out = sw(c, shell='bash')
    intf_list = out.rstrip('\r\n').strip("[]").replace(" ", "").split(',')

    if intf_uuid not in intf_list:
        assert fail is True, "Unable to find the interface in the bond"
        return

    # Remove the given intf_name's UUID from the bond's Interfaces.
    new_intf_list = [i for i in intf_list if i != intf_uuid]

    # Set the new Interface list in the bond.
    new_intf_str = '[' + ",".join(new_intf_list) + ']'

    c = ovs_vsctl + "set port " + bond_name + " interfaces=" + new_intf_str
    out = sw(c, shell='bash')

    return out


# Remove a list of Interfaces from the bond.
def remove_intf_list_from_bond(sw, bond_name, intf_list):
    for intf in intf_list:
        remove_intf_from_bond(sw, bond_name, intf)
