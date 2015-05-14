#!/bin/sh
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

if [ "x" = "${TARGET_IP}x" ]; then
    echo "Test target IP address not specified"
    exit 1
fi

# Files to be copied to target.
HC_TEST_DIR=/tmp/hc_test
LACPD_TEST_DIR=${HC_TEST_DIR}/lacpd_tests
LACPD_TESTS=lacpd_test.py

# Copy test programs to target
/usr/bin/ssh root@${TARGET_IP} "/bin/mkdir -p ${LACPD_TEST_DIR}"

/usr/bin/scp ${LACPD_TESTS} root@${TARGET_IP}:${LACPD_TEST_DIR}
RC=$?
if [ 0 -ne ${RC} ]; then
    echo "Failed copy test programs to target ${TARGET_IP}"
    exit 2
fi

# Copy test_config files for the given test target.
TEST_CFG_DIR="${PROJFW_DIR}/tools/test_utils/test_config"

/usr/bin/scp -r ${TEST_CFG_DIR} root@${TARGET_IP}:${HC_TEST_DIR}
RC=$?
if [ 0 -ne ${RC} ]; then
    echo "Failed copy test config to target ${TARGET_IP}"
    exit 2
fi

exit 0
