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

#ifndef __LACP_STUBS_H
#define __LACP_STUBS_H


typedef unsigned char u8_t;
typedef unsigned short u16_t;
typedef unsigned int u32_t;

#define L2_MIN_MAC_STR  (20)
#define L2_MAX_OUI_STR  (9)
#define L2_MAC_VENDOR    2        /* display MACs in Yago  :ABCDEF format */
#define L2_MAC_TWOxSIX   1        /* display MACs in 111111:222222 format */

extern char * L2_hexmac_to_strmac(macaddr_6_t hexMac, char *buffer,
                                  size_t buflen, u8_t flag);
#endif  //__LACP_STUBS_H
