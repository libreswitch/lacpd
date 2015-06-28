/*
 * Copyright (C) 2005-2015 Hewlett-Packard Development Company, L.P.
 * All Rights Reserved.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License"); you may
 *   not use this file except in compliance with the License. You may obtain
 *   a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *   WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *   License for the specific language governing permissions and limitations
 *   under the License.
 */

#ifndef __SVLAN_ERROR_H__
#define __SVLAN_ERROR_H__

/*****************************************************************************
   File               : svlan_error.h
   Description        : The file contains all  the error codes that will
                        be used in the slave vlan module.
*****************************************************************************/
/*****************************************************************************
 * Defines for error codes
 *****************************************************************************/
#define      R_SVLAN_GLOB_ERROR_BASE              (950000)

#define      R_SVLAN_LPORT_ERROR_BASE             (R_SVLAN_GLOB_ERROR_BASE + 0)
#define      SVLAN_LPORT_EXISTS                   (R_SVLAN_LPORT_ERROR_BASE + 1)
#define      SVLAN_LPORT_INSERT_FAILED            (R_SVLAN_LPORT_ERROR_BASE + 2)
#define      SVLAN_VLAN_NOT_HANDLED               (R_SVLAN_LPORT_ERROR_BASE + 3)
#define      SVLAN_LPORT_NO_MEM                   (R_SVLAN_LPORT_ERROR_BASE + 4)
#define      SVLAN_LPORT_NOT_FOUND                (R_SVLAN_LPORT_ERROR_BASE + 5)
#define      SVLAN_LPORT_ERROR_ALLOCATING_ID      (R_SVLAN_LPORT_ERROR_BASE + 6)
#define      SVLAN_LPORT_ADMIN_ALREADY_UP         (R_SVLAN_LPORT_ERROR_BASE + 7)
#define      SVLAN_LPORT_ADMIN_ALREADY_DOWN       (R_SVLAN_LPORT_ERROR_BASE + 8)

#define      R_SVLAN_SPORT_ERROR_BASE             (R_SVLAN_GLOB_ERROR_BASE + 1000)
#define      SVLAN_SPORT_NOT_HANDLED              (R_SVLAN_SPORT_ERROR_BASE + 1)
#define      SVLAN_SPORT_NO_MEM                   (R_SVLAN_SPORT_ERROR_BASE + 2)
#define      SVLAN_SPORT_INSERT_FAILED            (R_SVLAN_SPORT_ERROR_BASE + 3)
#define      SVLAN_SPORT_NOT_FOUND                (R_SVLAN_SPORT_ERROR_BASE + 4)

#define      R_SVLAN_VLAN_ERROR_BASE              (R_SVLAN_GLOB_ERROR_BASE + 2000)
#define      SVLAN_VLAN_NO_MEM                    (R_SVLAN_VLAN_ERROR_BASE + 1)
#define      SVLAN_VLAN_INSERT_FAILED             (R_SVLAN_VLAN_ERROR_BASE + 2)
#define      SVLAN_VLAN_NOT_FOUND                 (R_SVLAN_VLAN_ERROR_BASE + 3)

#ifdef STARSHIP

#define      R_SVLAN_FTHLP_ERROR_BASE             (R_SVLAN_GLOB_ERROR_BASE + 3000)
#define      SVLAN_FTHLP_PROGRAMMING_ERROR        (R_SVLAN_FTHLP_ERROR_BASE + 1)
#define      SVLAN_FTHLP_ERROR_PROG_PORT2ETC      (R_SVLAN_FTHLP_ERROR_BASE + 2)
#define      SVLAN_SET_PORT_CONFIG_ERROR          (R_SVLAN_FTHLP_ERROR_BASE + 3)
#define      SVLAN_ERROR_PROGRAMMING_APV2ETC      (R_SVLAN_FTHLP_ERROR_BASE + 4)

#define      R_SVLAN_VSTP_ERROR_BASE              (R_SVLAN_GLOB_ERROR_BASE + 4000)
#define      SVLAN_MVST_ALREADY_ENABLED           (R_SVLAN_VSTP_ERROR_BASE + 1)
#define      SVLAN_MVST_NOT_ENABLED               (R_SVLAN_VSTP_ERROR_BASE + 2)
#define      SVLAN_MVST_INST_EXISTS               (R_SVLAN_VSTP_ERROR_BASE + 3)
#define      SVLAN_MVST_INST_DOES_NOT_EXIST       (R_SVLAN_VSTP_ERROR_BASE + 4)
#define      SVLAN_VSTP_CANT_ASSOC_DEFAULT_VLAN   (R_SVLAN_VSTP_ERROR_BASE + 5)
#define      SVLAN_VSTP_VLAN_ALREADY_ASSOCIATED   (R_SVLAN_VSTP_ERROR_BASE + 6)
#define      SVLAN_VSTP_CANT_DISASSOC_DEFAULT_VLAN   (R_SVLAN_VSTP_ERROR_BASE + 7)
#define      SVLAN_VSTP_VLAN_NOT_ASSOCIATED       (R_SVLAN_VSTP_ERROR_BASE + 8)
#define      SVLAN_VSTP_VLANS_ASSOCIATED          (R_SVLAN_VSTP_ERROR_BASE + 9)

#define      R_SVLAN_MISC_ERROR_BASE              (R_SVLAN_GLOB_ERROR_BASE + 5000)
#define      SVLAN_OUT_OF_MEMORY                  (R_SVLAN_MISC_ERROR_BASE + 1)
#define      SVLAN_LPORT_MAC_INDEX_INVALID        (R_SVLAN_MISC_ERROR_BASE + 2)

#endif

#endif /* __SVLAN_ERROR_H__ */
