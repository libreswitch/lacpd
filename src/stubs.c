/*
 * (c) Copyright 2015 Hewlett Packard Enterprise Development LP
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
    int index = 0;
    union vendor {
        u32_t oui;
        u16_t x[2];
        u8_t  octet[4];
    } macVendor;

    static struct lookup_table {
        u32_t  oui;
        char  *str;
    } lookup[] = {
        { 0x000001E6,  "HP"        },
        { 0x000001E7,  "HP"        },
        { 0x000004EA,  "HP"        },
        { 0x00000883,  "HP"        },
        { 0x00000A57,  "HP"        },
        { 0x00000D9D,  "HP"        },
        { 0x00000E7F,  "HP"        },
        { 0x00000F20,  "HP"        },
        { 0x00001083,  "HP"        },
        { 0x0000110A,  "HP"        },
        { 0x00001185,  "HP"        },
        { 0x00001279,  "HP"        },
        { 0x00001321,  "HP"        },
        { 0x0000306E,  "HP"        },
        { 0x000030C1,  "HP"        },
        { 0x000060B0,  "HP"        },
        { 0x000080A0,  "HP"        },
        { 0x00080009,  "HP"        },
        { 0x00E06300,  "Yago"      },
        { 0x01005E00,  "IP mcast"  },
        { 0x00001D00,  "Cabletron" },
        { 0x00000C00,  "Cisco"     },
        { 0x08002000,  "Sun"       },
        { 0x00608C00,  "3Com"      },
        { 0x00609700,  "3Com"      },
        { 0x00600800,  "3Com"      },
        { 0x00A0C900,  "Intel"     },
        { 0x00008100,  "Bay"       },
        { 0x00001B00,  "Novell"    },
        { 0x00608C00,  "SMC"       },
        { 0x00E02900,  "SMC"       },
        { 0x00C07B00,  "Ascend"    },
        { 0x0,         ""          }
    };

    buffer[0] = '\0';
    if (buflen < L2_MIN_MAC_STR) {
        return NULL;
    }

    if (flag & L2_MAC_VENDOR) {
        macVendor.x[0] = *(u16_t *)hexMac;
        macVendor.x[1] = *(u16_t *)(hexMac + 2);
        macVendor.octet[3] = 0;

        while ((lookup[index].oui != 0) &&
               (macVendor.oui != lookup[index].oui)) {
            index++;
        }

        if (lookup[index].oui != 0) {
            char spaces[] = "          ";
            // Populate the OUI.
            strncpy(buffer, lookup[index].str, L2_MAX_OUI_STR+1);
            strncat(buffer, spaces, (L2_MAX_OUI_STR - strlen(buffer)));

            // Now do the last 3 bytes.
            sprintf(&buffer[L2_MAX_OUI_STR], " %02X:%02X:%02X",
                    hexMac[3], hexMac[4], hexMac[5]);

            return buffer;
        }
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
