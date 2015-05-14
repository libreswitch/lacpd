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

cmake_minimum_required (VERSION $ENV{CMAKE_MIN_VERS})

project(lacpd_test C CXX)

set (LACPD_TEST_DIR "/tmp/hc_test/lacpd_tests")
set (TARGET_IP $ENV{TARGET_IP})

add_custom_target(lacpd_test_setup
		  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/lacpd_test_setup.sh
	  	  WORKING_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR})

# Enable unit testing.
enable_testing()

MACRO(ADD_REMOTE_TEST test_name test_class)
	add_test(NAME  ${test_name}
		 COMMAND /usr/bin/ssh root@${TARGET_IP}
		 /usr/bin/python ${LACPD_TEST_DIR}/lacpd_test.py ${test_class})
ENDMACRO(ADD_REMOTE_TEST)

ADD_REMOTE_TEST(lacpd_config_test LacpdConfigTests)
