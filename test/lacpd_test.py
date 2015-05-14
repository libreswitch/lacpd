#!/usr/bin/python
# Copyright (C) 2014-2015 Hewlett-Packard Development Company, L.P.
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
# lacpd component/functionality tests
#
# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4

import os
import sys
import time
import re
import subprocess
import unittest
import pexpect

from dal import DalContext
from dal import DalException

# Add test configuration directory to Python module path.
sys.path.append("/tmp/hc_test/test_config")

# Import test_config module from the above path.
from test_utils import *

# DAL pathname definitions
HW_CFG = "%s/hw_config"
PORT_HW_CFG = "/port.%s/hw_config"
LAG_CFG = "/lag.%s/config"
LAG_STATUS = "/lag.%s/status"
LAG_HW_CFG = "/lag.%s/hw_config"

###################################################################
def short_pause():
        print "Short pause for config changes..."
        time.sleep(0.25)

###################################################################
def setup_dal_elements():
        # Create /port.*/hw_config elements which are normally
        # created by bcmsdk.
        # HALON_TODO: Use unit test configurarion to get port list.
        MAX_PORT_NUM=55
        FIRST_SPLIT_PORT=49
        LAST_SPLIT_PORT=53
        for port in range(1, MAX_PORT_NUM):
            path = PORT_HW_CFG % (port)
            cfg = {}
            cfg['enable'] = False
            set_dal_element(path, cfg)
        # Add QSFP subports.
        for port in range(FIRST_SPLIT_PORT, LAST_SPLIT_PORT):
            for subport in range(1, 5):
                tuid = str(port) + "-" + str(subport)
                path = PORT_HW_CFG % (tuid)
                cfg = {}
                cfg['enable'] = False
                set_dal_element(path, cfg)

###################################################################
def create_lag(tuid='unique', lag_mode='mode', member_ports=[]):
        path = LAG_CFG % (tuid)
        cfg = {}
        cfg['lag_mode'] = lag_mode
        cfg['member_ports'] = member_ports
        set_dal_element(path, cfg)
        print "Created " + path

###################################################################
def add_ports_to_lag(_lag, port_list=[]):
        path = LAG_CFG % (_lag)
        cfg = {}
        cfg['member_ports'] = port_list
        set_dal_element(path, cfg)
        print "Added ports" + " to " + _lag

###################################################################
def delete_lag(tuid='unique'):
        path = LAG_CFG % (tuid)
        delete_dal_element(path)
        print "Deleted " + path

###################################################################
def verify_lag(self, _name, _lag_mode):
        path = LAG_CFG % (_name)
        results = get_dal_element(path)
        self.assertEqual(results['lag_mode'], _lag_mode,
                         path + " lag_mode does not match")

###################################################################
def verify_lag_hw_cfg(self, _name, _lag_id):
        path = LAG_HW_CFG % (_name)
        results = get_dal_element(path)
        self.assertEqual(results['lag_id'], _lag_id,
                         path + "  lag_id does not match. " +
                         "Expecting %d, got %d " % (_lag_id, results['lag_id']))

###################################################################
def verify_no_lag_hw_cfg(self, _name):
        path = LAG_HW_CFG % (_name)
        results = get_dal_element(path)
        if results:
            self.assertNotEqual(0, 0,
                                path + " not expecting to exist")
        print path, "does not exist as expected"

###################################################################
def checkListsEqual(L1, L2):
    if sorted(L1) == sorted(L2):
        return True
    else:
        return False

###################################################################
def verify_lag_cfg_members(self, _name, _member_list):
        path = LAG_CFG % (_name)
        results = get_dal_element(path)
        match = checkListsEqual(results['member_ports'], _member_list)
        self.assertEqual(match, 1,
                         path + " member list does not match.")

###################################################################
def verify_lag_port_hw_cfg(self, _name, _attr, _value):
        path = HW_CFG % (_name)
        results = get_dal_element(path)
        self.assertEqual(results[_attr], _value,
                         path + " " + _attr + " does not match. " +
                         "Expecting %s, got %s " % (_value, results[_attr]))

###################################################################
def verify_no_lag_port_hw_cfg_attr(self, _name, _attr):
        path = HW_CFG % (_name)
        results = get_dal_element(path)
        try:
            temp = results[_attr]
            self.assertNotEqual(0, 0,
                         path + " " + _attr + " exists when it shouldn't. " +
                         "Got %s " % (results[_attr]))
        except KeyError:
            print path, "Attribute %s not found as expected" % (_attr)

###################################################################
def verify_lag_status(self, _name, _member_list):
        path = LAG_STATUS % (_name)
        results = get_dal_element(path)
        match = checkListsEqual(results['member_ports'], _member_list)
        self.assertEqual(match, 1,
                         path + " member list does not match.")

###################################################################
def verify_no_lag_status(self, _name):
        path = LAG_STATUS % (_name)
        results = get_dal_element(path)
        if results:
            self.assertNotEqual(0, 0,
                                path + " not expecting to exist")
        print path, "does not exist as expected"

###################################################################
###########    Lacpd component/functionality tests  ###############
###################################################################
class LacpdConfigTests(unittest.TestCase):

    @classmethod
    def setUpClass(cls):

        stop_daemon("ALL")

        # Cleanup database & restart DAL
        os.system("/bin/rm -rf /run/dal/*.aof")
        start_daemon("dal")

        # Start sysd, switchd and lacpd only
        start_daemon("sysd")
        short_pause();
        setup_dal_elements()
        start_daemon("lacpd")

###################################################################
    @classmethod
    def tearDownClass(cls):

        # stop sysd, lacpd and switchd
        stop_daemon("lacpd")
        stop_daemon("sysd")
        stop_daemon("dal")

        # Cleanup database
        os.system("/bin/rm -rf /run/dal/*.aof")

        start_daemon("ALL")

###################################################################
    def test_lacpd_config(self):

        print "LACPD unit tests start."

        ##########################################
        # 1.0 Static LAG Tests.
        ##########################################

        ##########################################
        # 1.01 Static LAG with no member ports.
        lag_name="TRUNK1"
        create_lag(lag_name, lag_mode="static")

        verify_lag(self, lag_name, "static")

        # Verfiy /lag.TRUNK1/hw_config
        verify_lag_hw_cfg(self, lag_name, 1)

        ##########################################
        # 1.02 Add ports /port.10, /port.14 to the lag
        member_ports = ["/port.10", "/port.14"]

        add_ports_to_lag(lag_name, member_ports)

        verify_lag_cfg_members(self, lag_name, member_ports)

        # Allow lacpd to finish processing.
        short_pause()

        # Verify that /port 10,14 are configured in hw_config.
        for port_name in member_ports:
            verify_lag_port_hw_cfg(self, port_name, "egress_enable", True)
            verify_lag_port_hw_cfg(self, port_name, "lag", "/lag." + lag_name)
        # Verify /lag.*/status
        verify_lag_status(self, lag_name, member_ports)

        ##########################################
        # 1.03 Add another port in the middle
        # Add ports /port.10 /port.11 and /port.14 to TRUNK1 lag
        member_ports = ["/port.10", "/port.11", "/port.14"]

        add_ports_to_lag(lag_name, member_ports)

        verify_lag_cfg_members(self, lag_name, member_ports)

        # Allow lacpd to finish processing.
        short_pause()

        # Verify that /port 10,11,14 are configured in hw_config.
        for port_name in member_ports:
            verify_lag_port_hw_cfg(self, port_name, "egress_enable", True)
            verify_lag_port_hw_cfg(self, port_name, "lag", "/lag." + lag_name)
        # Verify /lag.*/status
        verify_lag_status(self, lag_name, member_ports)

        ##########################################
        # 1.04 Remove the first port.
        # Remove port /port.10 from TRUNK1 lag
        member_ports = ["/port.11", "/port.14"]

        add_ports_to_lag(lag_name, member_ports)

        verify_lag_cfg_members(self, lag_name, member_ports)

        # Allow lacpd to finish processing.
        short_pause()

        # Verify that /port 11,14 are configured in hw_config.
        for port_name in member_ports:
            verify_lag_port_hw_cfg(self, port_name, "egress_enable", True)
            verify_lag_port_hw_cfg(self, port_name, "lag", "/lag." + lag_name)

        verify_no_lag_port_hw_cfg_attr(self, "/port.10", "egress_enable")
        verify_no_lag_port_hw_cfg_attr(self, "/port.10", "lag")

        ##########################################
        # 1.05 Delete the static LAG.
        delete_lag(lag_name)
        short_pause()

        # Verfiy that the /lag.TRUNK1/hw_config is removed.
        verify_no_lag_hw_cfg(self, lag_name)
        # Verfiy that the /lag.TRUNK1/status is removed.
        verify_no_lag_status(self, lag_name)

        print "LACPD unit tests end."


if __name__ == '__main__':

    test_config = get_test_config()
    unittest.main()
