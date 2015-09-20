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

/*****************************************************************************
 *  File               : nemo_types.h
 *  Description        : The file contains all ctypes needed by the system.
 *                       Derived from RS code base file: mls_types.h
 *
 *****************************************************************************/

#ifndef _NEMO_TYPES_H_
#define _NEMO_TYPES_H_

#ifndef NULL
#define NULL (0L)
#endif

#ifndef bool
typedef int  bool;     /* boolean: NOTE: will conflict with C99 builtin bool, dont' use THIS */
#define bool int
#endif
typedef int boolean;

/* MAC Addresses */
/* length in bytes */
#define MAC_BYTEADDR_SIZE  (6)
/* length in short int */
#define MAC_SHORTADDR_SIZE (3)

/* MAC address in 3 elements (ushort) */
typedef unsigned short        macaddr_3_t[MAC_SHORTADDR_SIZE];
/* MAC address in 6 elements (bytes)  */
typedef unsigned char         macaddr_6_t[MAC_BYTEADDR_SIZE];

typedef union macaddr_u {
    unsigned char  c_mac[MAC_BYTEADDR_SIZE];
    unsigned short s_mac[MAC_SHORTADDR_SIZE];
    struct {
        unsigned long  uu_mac;
        unsigned short ss_mac;
    } aa_mac;
#define u_mac aa_mac.uu_mac
} macaddr_t;

/* IPv4 and v6 address in bytes */
#define IPV4_BYTEADDR_SIZE (4)
#define IPV6_BYTEADDR_SIZE (16)
#ifdef STARSHIP
#define IS_IPV4_MASK_INVALID(x) ((((x)>>1) ^ (x)) & ((x) & 0x7fffffff))
#endif

#ifndef FALSE
#define FALSE         (0)
#endif

#ifndef TRUE
#define TRUE          (1)
#endif

#define ERROR_VAL     (-1)
#define R_SUCCESS     (0)
#define R_ERROR       (-1)
#define R_ERROR_NOMEM (-2)

typedef char                INT8;
typedef unsigned char       UINT8;
typedef short               INT16;
typedef unsigned short      UINT16;
typedef long                INT32;
typedef unsigned long       UINT32;
/* LONGLONG */
typedef long long           INT64;
/* LONGLONG */
typedef unsigned long long  UINT64;

#ifndef __uint_t
typedef unsigned int        uint_t;
#define __uint_t
#endif

#endif /* _NEMO_TYPES_H_ */
