/*
 * (c) Copyright 2015-2016 Hewlett Packard Enterprise Development LP
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

#include <openvswitch/vlog.h>

#include "lacp_cmn.h"
#include "avl.h"

#include "lacp_stubs.h"

VLOG_DEFINE_THIS_MODULE(lacp_stubs);

//***************************************************************************
// *  FUNCTION:     L2_hexmac_to_strmac()
//***************************************************************************
// *  DESCRIPTION:  This function receives a MAC address in hex format and
// *                populates a user-provided buffer (20 bytes minimum) with
// *                the MAC address in string format (NULL terminated)
// *
// *                A flag is also passed:
// *                - L2_MAC_TWOxSIX bit set     -> 00DEAD:BEEF00
// *                - L2_MAC_TWOxSIX bit clear   -> 00:11:22:33:44:55
// *                - L2_MAC_VENDOR  bit set     -> Yago      33:44:55
// *                                             -> Cabletron 00:00:05
// *
// ***************************************************************************
char *
L2_hexmac_to_strmac(macaddr_6_t hexMac, char *buffer,
                    size_t buflen, u8_t flag)
{
    buffer[0] = '\0';
    if (buflen < L2_MIN_MAC_STR) {
        return NULL;
    }

    if (flag & L2_MAC_TWOxSIX) {
        sprintf(buffer, "%02X%02X%02X:%02X%02X%02X",
                hexMac[0], hexMac[1], hexMac[2],
                hexMac[3], hexMac[4], hexMac[5]);
    } else {
        sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X",
                hexMac[0], hexMac[1], hexMac[2],
                hexMac[3], hexMac[4], hexMac[5]);
    }

    return buffer;

} // L2_hexmac_to_strmac
