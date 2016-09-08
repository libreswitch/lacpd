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
import re

ovs_vsctl = "/usr/bin/ovs-vsctl "


def print_header(msg):
    header_length = len(msg)
    print('\n%s\n%s\n%s\n' % ('=' * header_length,
                              msg,
                              '=' * header_length))



def sw_set_system_lacp_config(sw, config):
    cmd = 'set system .'

    for c in config:
       cmd += " lacp_config:" + c

    return sw(cmd, shell='vsctl')


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
    c = "set interface " + str(interface)
    for s in config:
        c += " user_config:" + s
    return sw(c, shell='vsctl')


# Clear user_config for an Interface.
def sw_clear_user_config(sw, interface):
    c = "clear interface " + str(interface) + " user_config"
    return sw(c, shell='vsctl')


# Set pm_info for an Interface.
def sw_set_intf_pm_info(sw, interface, config):
    c = "set interface " + str(interface)
    for s in config:
        c += " pm_info:" + s
    return sw(c, shell='vsctl')


# Set open_vsw_lacp_config parameter(s)
def set_port_parameter(sw, port, config):
    c = "set port " + str(port)
    for s in config:
        c += ' %s' % s
    return sw(c, shell='vsctl')


# Get the values of a set of columns from Interface table.
# This function returns a list of values if 2 or more
# fields are requested, and returns a single value (no list)
# if only 1 field is requested.
def sw_get_intf_state(params):
    c = "get interface " + str(params[1])
    for f in params[2]:
        c += " " + f
    out = params[0](c, shell='vsctl').replace('"', '').splitlines()
    return out


def clear_port_parameter(sw, port, config):
    """Clears parameters in 'config' from 'port' on 'sw'."""

    cmd = "clear port %s %s" % (str(port), ' '.join(map(str, config)))
    return sw(cmd, shell='vsctl')


def remove_port_parameter(sw, port, col, keys):
    """Removes 'keys' in 'col' section from 'port' on 'sw'."""

    cmd = "remove port %s %s %s" % (port, col, ' '.join(map(str, keys)))

    return sw(cmd, shell='vsctl')


def set_intf_parameter(sw, intf, config):
    """Configure parameters in 'config' to 'intf' in 'sw'."""

    cmd = "set interface %s %s" % (str(intf), ' '.join(config))
    output = sw(cmd, shell='vsctl')

    assert output == '', 'Error found: %s, aborting!' % output

    return output


def sw_get_port_state(params):
    c = "get port " + str(params[1])
    for f in params[2]:
        c += " " + f
    out = params[0](c, shell='vsctl').splitlines()
    if len(out) == 1:
        out = out[0]
    return out


def sw_get_port_state_bs(params):
    c = "get port " + str(params[1])
    for f in params[2]:
        c += " " + f
    out = params[0](c, shell='vsctl').replace('"', '').splitlines()
    return out


# Create a bond/lag/trunk in the OVS-DB.
def sw_create_bond(s1, bond_name, intf_list, lacp_mode="off"):
    print("Creating LAG " + bond_name + " with interfaces: " +
          str(intf_list) + "\n")
    c = "add-bond bridge_normal " + bond_name +\
        " " + " ".join(map(str, intf_list))
    c += " -- set port " + bond_name + " lacp=" + lacp_mode
    return s1(c, shell='vsctl')


def sw_delete_lag(sw, lag_name):
    """Deletes LAG from OVSDB."""
    cmd = 'del-port ' + lag_name
    return sw(cmd, shell='vsctl')

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
    c = "get interface " + str(intf_name) + " _uuid"
    intf_uuid = sw(c, shell='vsctl').rstrip('\r\n')

    # Get the current list of Interfaces in the bond.
    c = "get port " + bond_name + " interfaces"
    out = sw(c, shell='vsctl')
    intf_list = out.rstrip('\r\n').strip("[]").replace(" ", "").split(',')

    if intf_uuid not in intf_list:
        assert fail is True, "Unable to find the interface in the bond"
        return

    # Remove the given intf_name's UUID from the bond's Interfaces.
    new_intf_list = [i for i in intf_list if i != intf_uuid]

    # Set the new Interface list in the bond.
    new_intf_str = '[' + ",".join(new_intf_list) + ']'

    c = "set port " + bond_name + " interfaces=" + new_intf_str
    out = sw(c, shell='vsctl')

    return out


# Remove a list of Interfaces from the bond.
def remove_intf_list_from_bond(sw, bond_name, intf_list):
    for intf in intf_list:
        remove_intf_from_bond(sw, bond_name, intf)


# Remove all interfaces from a bond.
def remove_all_intf_from_bond(sw, bond_name):
    print("Removing all interfaces from LAG " + bond_name + "\n")
    c = "set port " + bond_name + " interfaces=[]"
    out = sw(c, shell='vsctl')
    return out


# Set system level LACP config
def set_system_lacp_config(sw, config):
    c = "set system ."
    for s in config:
        c += " lacp_config:" + s
    return sw(c, shell='vsctl')


# Verify interface bond status
def verify_intf_bond_status(sw, intf, state, msg):
    result = timed_compare(sw_get_intf_state,
                           (sw, intf, ['bond_status:' + state]),
                           verify_compare_value, ['true'])
    assert result == (True, ["true"]), msg


# Verify interface bond status is empty
def verify_intf_bond_status_empty(sw, intf, msg):
    for state in ['up', 'down', 'blocked']:
        result = timed_compare(sw_get_intf_state,
                               (sw, intf, ['bond_status:' + state]),
                               verify_compare_value,
                               ['ovs-vsctl: no key ' + state +
                                ' in Interface record ' + intf +
                                ' column bond_status'])
        assert result == (True, ['ovs-vsctl: no key ' + state +
                                 ' in Interface record ' + intf +
                                 ' column bond_status']), msg


# Verify port bond status
def verify_port_bond_status(sw, lag, state,  msg):
    result = timed_compare(sw_get_port_state_bs,
                           (sw, lag, ['bond_status:' + state]),
                           verify_compare_value, ['true'])
    assert result == (True, ["true"]), msg


# Verify LACP interface status
def verify_intf_lacp_status(sw, intf, verify_values, context=''):
    request = []
    attrs = []
    for attr in verify_values:
        request.append('lacp_status:' + attr)
        attrs.append(attr)
    result = timed_compare(sw_get_intf_state,
                           (sw, intf, request),
                           verify_compare_complex, verify_values)
    field_vals = result[1]
    for i in range(0, len(attrs)):
        verify_values[attrs[i]].replace('"', '')
        assert field_vals[i] == verify_values[attrs[i]], context +\
            ": invalid value for " + attrs[i] + ", expected " +\
            verify_values[attrs[i]] + ", got " + field_vals[i]


def sw_wait_until_all_sm_ready(sws, intfs, flags, max_retries=30):
    """Verify that all 'intfs' SMs have 'flags' enabled.

    We need to verify that all interfaces' State Machines have these 'flags'
    enabled within 'ovs-vsctl' command output.

    The main structure is an array of arrays with the format of:
    [ [<switch>, <interfaces number>, <SM ready>], ...]

    'not_ready' will be all arrays that 'SM ready' is still False and needs to
    verify again.
    """
    all_intfs = []
    sm = []
    retries = 0

    for sw in sws:
        all_intfs += [[sw, intf, False] for intf in intfs]

    # All arrays shall be True
    while not all(intf[2] for intf in all_intfs):
        # Retrieve all arrays that have False
        not_ready = filter(lambda intf: not intf[2], all_intfs)

        if retries is max_retries:
            # dump info
            cmd = 'list system'
            sm[0](cmd, shell='vsctl', silent=False)

            cmd = 'list interface'
            sm[0](cmd, shell='vsctl', silent=False)

            assert False, \
                'Exceeded max retries. SM never achieved status: %s' % flags

        for sm in not_ready:
            cmd = 'get interface %s lacp_status:actor_state' % sm[1]

            """
            If you want to print the output remove or set 'silent' to False

            It was removed because of the frequency of 'ovs-vsctl' calls to
            validate SM status and constant prints will make test output
            ilegible
            """
            out = sm[0](cmd, shell='vsctl', silent=False)
            sm[2] = bool(re.match(flags, out))

        retries += 1
        sleep(1)


def sw_wait_until_one_sm_ready(sws, intfs, flags, max_retries=30):
    """Verify that one 'intfs' SM has 'flags' enabled.

    We need to verify that at least one interface in 'intf' has 'flags'
    enabled. The interface number will be returned
    """
    all_intfs = []
    sm = []
    retries = 0
    intf_fallback_enabled = 0

    for sw in sws:
        all_intfs += [[sw, intf, False] for intf in intfs]

    # One interface shall be True
    while not any(intf[2] for intf in all_intfs):
        # Retrieve all arrays that have False
        not_ready = filter(lambda intf: not intf[2], all_intfs)

        if retries is max_retries:
            # dump info
            cmd = 'list system'
            sm[0](cmd, shell='vsctl', silent=False)

            cmd = 'list interface'
            out = sm[0](cmd, shell='vsctl', silent=False)

            assert False, \
                'Exceeded max retries. SM never achieved status: %s' % flags

        for sm in not_ready:
            cmd = 'get interface %s lacp_status:actor_state' % sm[1]

            """
            If you want to print the output remove or set 'silent' to False

            It was removed because of the frequency of 'ovs-vsctl' calls to
            validate SM status and constant prints will make test output
            ilegible
            """
            out = sm[0](cmd, shell='vsctl', silent=False)
            sm[2] = bool(re.match(flags, out))

            if sm[2]:
                intf_fallback_enabled = sm[1]

        retries += 1
        sleep(1)

    return intf_fallback_enabled


# Add a new Interface to the existing bond.
def add_intf_to_bond(sw, bond_name, intf_name):

    print("Adding interface %s to LAG %s \n" %
          (intf_name, bond_name))
    # Get the UUID of the interface that has to be added.
    c = ("get interface %s _uuid" % (str(intf_name)))

    intf_uuid = sw(c.format(**locals()), shell='vsctl').rstrip('\r\n')

    # Get the current list of Interfaces in the bond.
    c = ("get port %s interfaces" % (bond_name))
    out = sw(c.format(**locals()), shell='vsctl')
    intf_list = out.rstrip('\r\n').strip("[]").replace(" ", "").split(',')

    assert intf_uuid not in intf_list, "Interface %s is already part of %s"\
                                       % (intf_name, bond_name)

    # Add the given intf_name's UUID to existing Interfaces.
    intf_list.append(intf_uuid)

    # Set the new Interface list in the bond.
    new_intf_str = "[%s]" % (",".join(intf_list))

    c = ("set port %s interfaces=%s" % (bond_name, new_intf_str))
    sw(c.format(**locals()), shell='vsctl')


def enable_intf_list(sw, intf_list):
    for intf in intf_list:
        sw_set_intf_user_config(sw, intf, ['admin=up'])


def disable_intf_list(sw, intf_list):
    for intf in intf_list:
        sw_set_intf_user_config(sw, intf, ['admin=down'])


def sw_wait_until_ready(sws, intfs, max_retries=30):
    all_intfs = []
    retries = 0

    for sw in sws:
        all_intfs += [[sw, intf, False] for intf in intfs]

    # All arrays shall be True
    while not all(intf[2] for intf in all_intfs):
        # Retrieve all arrays that have False
        not_ready = filter(lambda intf: not intf[2], all_intfs)

        assert retries is not max_retries, \
            "All interfaces are not up, test cannot continue from here!"

        for sm in not_ready:
            cmd = 'get interface %s link_state' % sm[1]

            out = sm[0](cmd, shell='vsctl', silent=False)

            sm[2] = 'up' in out

        retries += 1
        sleep(1)


def verify_interfaces_mac_uniqueness(sws, intfs):
    """Verify that all interfaces are unique among all switches used."""

    macs = []

    for sw in sws:
        for intf in intfs:
            cmd = 'get interface %s hw_intf_info:mac_addr' % intf
            mac = sw(cmd, shell='vsctl')

            macs.append(mac)

    assert len(macs) is len(set(macs)), \
        'MAC Addresses are not unique! Aborting...'


# Verify lag port fallback key
def verify_port_fallback_key(sw, lag, key, expected,  msg):
    result = timed_compare(sw_get_port_state_bs,
                           (sw, lag, ['other_config:' + key]),
                           verify_compare_value, [expected])
    assert result == (True, [expected]), msg
