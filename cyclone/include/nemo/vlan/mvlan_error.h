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

#ifndef _MVLAN_ERROR_H
#define _MVLAN_ERROR_H
/*****************************************************************************
   File               : mvlan_error.h                                          
   Description        : The file contains all  the error codes that will
                        be used in the vlan module.
                                                                        
*****************************************************************************/
/*****************************************************************************
 * Defines for error codes
 *****************************************************************************/ 
#include     <nemo/glob_error_base.h>

#define      R_MVLAN_LINE_ERROR_BASE              (R_MVLAN_GLOB_ERROR_BASE + 0)
#define      MVLAN_LINE_INVLID_LINE_NUM           (R_MVLAN_LINE_ERROR_BASE +1)
#define      MVLAN_LINE_LC_EXISTS                 (R_MVLAN_LINE_ERROR_BASE +2)
#define      MVLAN_LINE_NOMEM                     (R_MVLAN_LINE_ERROR_BASE +3)
#define      MVLAN_LINE_NO_LC_INFO                (R_MVLAN_LINE_ERROR_BASE +4)
#define      MVLAN_LINE_LC_DOWN                   (R_MVLAN_LINE_ERROR_BASE +5)
#define      MVLAN_LINE_SUB_SLOT_ALREADY_INITED   (R_MVLAN_LINE_ERROR_BASE +6)
#define      MVLAN_LINE_INVALID_OPERATION         (R_MVLAN_LINE_ERROR_BASE +7)
#define      MVLAN_LINE_PM_DEAD                   (R_MVLAN_LINE_ERROR_BASE +8)
#define      MVPM_OUT_OF_MEMORY                   (R_MVLAN_LINE_ERROR_BASE +9)



#define      R_MVLAN_VLAN_ERROR_BASE              (R_MVLAN_GLOB_ERROR_BASE + 1000)
#define      MVLAN_VLAN_NOT_HANDLED               (R_MVLAN_VLAN_ERROR_BASE + 1)
#define      MVLAN_VLAN_ALREADY_INITED            (R_MVLAN_VLAN_ERROR_BASE + 2)
#define      MVLAN_VLAN_INSERT_FAILED             (R_MVLAN_VLAN_ERROR_BASE + 3)
#define      MVLAN_VLAN_EXISTS                    (R_MVLAN_VLAN_ERROR_BASE + 4)
#define      MVLAN_INVALID_VLAN_ID                (R_MVLAN_VLAN_ERROR_BASE + 5)
#define      MVLAN_NAME_TOO_LONG                  (R_MVLAN_VLAN_ERROR_BASE + 7)
#define      MVLAN_NAME_EXISTS                    (R_MVLAN_VLAN_ERROR_BASE + 8)
#define      MVLAN_NO_FREE_VID                    (R_MVLAN_VLAN_ERROR_BASE + 9)
#define      MVLAN_VID_ALREADY_ALOCATED           (R_MVLAN_VLAN_ERROR_BASE + 10)
#define      MVLAN_VID_ALREADY_FREE               (R_MVLAN_VLAN_ERROR_BASE + 11)
#define      MVLAN_UNSUPPORTED_TYPE               (R_MVLAN_VLAN_ERROR_BASE + 12)
#define      MVLAN_VLAN_HANDLE_NOT_FOUND          (R_MVLAN_VLAN_ERROR_BASE + 13)
#define      MVLAN_VLAN_NAME_NOT_FOUND            (R_MVLAN_VLAN_ERROR_BASE + 14)
#define      MVLAN_VLAN_NON_ZERO_REF_COUNT        (R_MVLAN_VLAN_ERROR_BASE + 15)
#define      MVLAN_VID_NOT_FOUND                  (R_MVLAN_VLAN_ERROR_BASE + 16)
#define      MVLAN_VID_EOT                        (R_MVLAN_VLAN_ERROR_BASE + 17)
#define      MVLAN_NAME_SIZE_TOO_SMALL            (R_MVLAN_VLAN_ERROR_BASE + 18)
#define      MVLAN_VLAN_NO_MEM                    (R_MVLAN_VLAN_ERROR_BASE + 19)
#define      MVLAN_NAME_NOT_FOUND                 (R_MVLAN_VLAN_ERROR_BASE + 20)
#define      MVLAN_NAME_EOT                       (R_MVLAN_VLAN_ERROR_BASE + 21)
#define      MVLAN_NAME_NO_NAME                   (R_MVLAN_VLAN_ERROR_BASE + 22)
#define      MVLAN_VLAN_SPORTS_ATTACHED           (R_MVLAN_VLAN_ERROR_BASE + 23)
#define      MVLAN_SPORT_ALREADY_IN_VLAN          (R_MVLAN_VLAN_ERROR_BASE + 24)
#define      MVLAN_SPORT_NOT_IN_VLAN              (R_MVLAN_VLAN_ERROR_BASE + 25)
#define      MVLAN_SPORT_ALREADY_ATTACHED         (R_MVLAN_VLAN_ERROR_BASE + 26)
#define      MVLAN_PORT_IN_SPORT                  (R_MVLAN_VLAN_ERROR_BASE + 27)
#define      MVLAN_PORT_ALREADY_IN_VLAN           (R_MVLAN_VLAN_ERROR_BASE + 28)
#define      MVLAN_PORT_NOT_TRUNK                 (R_MVLAN_VLAN_ERROR_BASE + 29)
#define      MVLAN_TOO_MANY_PORTS                 (R_MVLAN_VLAN_ERROR_BASE + 30)
#define      MVLAN_PORT_HAS_CONFIGURATION         (R_MVLAN_VLAN_ERROR_BASE + 31)
#define      MVLAN_PORT_HAS_STP                   (R_MVLAN_VLAN_ERROR_BASE + 32)
#define      MVLAN_PORT_NOT_IN_VLAN               (R_MVLAN_VLAN_ERROR_BASE + 33)
#define      MVLAN_STP_ENABLED                    (R_MVLAN_VLAN_ERROR_BASE + 34)
#define      MVLAN_SPORT_NOT_ATTACHED             (R_MVLAN_VLAN_ERROR_BASE + 35)
#define      MVLAN_ERROR_IMPLICIT_VLAN            (R_MVLAN_VLAN_ERROR_BASE + 36)
#define      MVLAN_HANDLE_EOT                     (R_MVLAN_VLAN_ERROR_BASE + 37)
#define      MVLAN_HANDLE_EOT                     (R_MVLAN_VLAN_ERROR_BASE + 37)
#define      MVLAN_IF_ID_ALREADY_SET              (R_MVLAN_VLAN_ERROR_BASE + 38)
#define      MVLAN_IF_ID_NOT_SET                  (R_MVLAN_VLAN_ERROR_BASE + 39)
#define      MVLAN_IF_ID_PRESENT                  (R_MVLAN_VLAN_ERROR_BASE + 40)
#define      MVLAN_EPTI_ADD_FAILED                (R_MVLAN_VLAN_ERROR_BASE + 41)
#define      MVLAN_HANDLED_BY_FSM                 (R_MVLAN_VLAN_ERROR_BASE + 42)
#define      MVLAN_CFG_EXISTS                     (R_MVLAN_VLAN_ERROR_BASE + 43)
#define      MVLAN_PORT_HAS_LACP                  (R_MVLAN_VLAN_ERROR_BASE + 44)
#define      MVLAN_PORT_HAS_TRUNK_VLANS           (R_MVLAN_VLAN_ERROR_BASE + 45)
#define      MVLAN_PORT_HAS_ACCESS_VLAN           (R_MVLAN_VLAN_ERROR_BASE + 46)
#define      MVLAN_PORT_HAS_MVST                  (R_MVLAN_VLAN_ERROR_BASE + 47)
#define      MVLAN_SPORT_HAS_MVST                 (R_MVLAN_VLAN_ERROR_BASE + 48)
#define      MVLAN_SET_MAC_SUPPORTED_ONLY_FOR_PHY_PORTS\
                                                  (R_MVLAN_VLAN_ERROR_BASE + 49)
#define      MVLAN_SET_MAC_SUPPORTED_FOR_IMPLICIT_CASE_ONLY\
                                                  (R_MVLAN_VLAN_ERROR_BASE + 50)
#define      MVLAN_VLAN_ALREADY_SET_AS_NATIVE     (R_MVLAN_VLAN_ERROR_BASE + 51)
#define      MVLAN_ANOTHER_VLAN_SET_AS_NATIVE     (R_MVLAN_VLAN_ERROR_BASE + 52)
#define      MVLAN_CANT_ADD_PORT_TO_DEFAULT       (R_MVLAN_VLAN_ERROR_BASE + 53)
#define      MVLAN_NO_PORT_SPECIFIED_FOR_IMPLICIT (R_MVLAN_VLAN_ERROR_BASE + 54)
#define      MVLAN_SVLAN_ID_INVALID               (R_MVLAN_VLAN_ERROR_BASE + 55)
#define      MVLAN_SVLAN_PARENT_PORT_ALREADY_IN_VLAN\
                                                  (R_MVLAN_VLAN_ERROR_BASE + 56)
#define      MVLAN_SVLAN_SIBLING_PORT_ALREADY_IN_VLAN\
                                                  (R_MVLAN_VLAN_ERROR_BASE + 57)
#define      MVLAN_CHILD_SVLAN_PORT_ALREADY_IN_VLAN\
                                                  (R_MVLAN_VLAN_ERROR_BASE + 58)
#define      MVLAN_SVLAN_PORT_ALREADY_WITH_ANOTHER_CUSTOMER\
                                                  (R_MVLAN_VLAN_ERROR_BASE + 59)
#define      MVLAN_SPORT_ALREADY_HAS_VID          (R_MVLAN_VLAN_ERROR_BASE + 60)
#define      MVLAN_PORT_HAS_NATIVE_VLAN           (R_MVLAN_VLAN_ERROR_BASE + 61)
#define      MVLAN_SPORT_EXCEEDED_MAX_NUM_LPORT   (R_MVLAN_VLAN_ERROR_BASE + 62)
#define      MVLAN_SPORT_PORT_TYPE_MISMATCH       (R_MVLAN_VLAN_ERROR_BASE + 63)
#define      MVLAN_SET_MAC_EXCEEDED_NUM_MAC_LIMIT (R_MVLAN_VLAN_ERROR_BASE + 64)
#define      MVLAN_SET_MAC_CONFLICT               (R_MVLAN_VLAN_ERROR_BASE + 65)
#define      MVLAN_PORT_IN_NATIVE_VLAN            (R_MVLAN_VLAN_ERROR_BASE + 66)
#define      MVLAN_PORT_IN_TRUNK_VLAN             (R_MVLAN_VLAN_ERROR_BASE + 67)

#define      R_MVLAN_LPORT_ERROR_BASE              (R_MVLAN_GLOB_ERROR_BASE  + 2000)
#define      MVLAN_LPORT_EXISTS                    (R_MVLAN_LPORT_ERROR_BASE + 1)
#define      MVLAN_LPORT_NO_MEM                    (R_MVLAN_LPORT_ERROR_BASE + 2)
#define      MVLAN_LPORT_NOT_FOUND                 (R_MVLAN_LPORT_ERROR_BASE + 3)
#define      MVLAN_LPORT_NON_ZERO_REF_COUNT        (R_MVLAN_LPORT_ERROR_BASE + 4)
#define      MVLAN_LPORT_INSERT_FAILED             (R_MVLAN_LPORT_ERROR_BASE + 5)
#define      MVLAN_LPORT_SPORT_ATTACHED            (R_MVLAN_LPORT_ERROR_BASE + 6)
#define      MVLAN_LPORT_NO_SPORT                  (R_MVLAN_LPORT_ERROR_BASE + 7)
#define      MVLAN_LPORT_NO_DEF_VLAN               (R_MVLAN_LPORT_ERROR_BASE + 8)
#define      MVLAN_LPORT_LACP_PARAM_ALREADY_SET    (R_MVLAN_LPORT_ERROR_BASE + 9)
#define      MVLAN_LPORT_MPLS_VCID_ALREADY_SET     (R_MVLAN_LPORT_ERROR_BASE + 10)
#define      MVLAN_LPORT_MPLS_TYPE_ALREADY_SET     (R_MVLAN_LPORT_ERROR_BASE + 11)
#define      MVLAN_LPORT_MPLS_INVALID_ADDRESS      (R_MVLAN_LPORT_ERROR_BASE + 12)
#define      MVLAN_LPORT_MPLS_VCTYPE_NOT_KNOWN     (R_MVLAN_LPORT_ERROR_BASE + 13)
#define      MVLAN_LPORT_MPLS_VCTYPE_ALREADY_SET   (R_MVLAN_LPORT_ERROR_BASE + 14)
#define      MVLAN_LPORT_MPLS_GROUPID_ALREADY_SET  (R_MVLAN_LPORT_ERROR_BASE + 15)
#define      MVLAN_LPORT_ADMIN_ALREADY_UP          (R_MVLAN_LPORT_ERROR_BASE + 16)
#define      MVLAN_LPORT_ADMIN_ALREADY_DOWN        (R_MVLAN_LPORT_ERROR_BASE + 17)
#define      MVLAN_LPORT_PRE_CONFIG_NOT_FOUND      (R_MVLAN_LPORT_ERROR_BASE + 18)
#define      MVLAN_LPORT_OPERATION_NOT_SUPPORTED   (R_MVLAN_LPORT_ERROR_BASE + 19)
#define      MVLAN_LPORT_PORT_IN_SLAVE_MODE        (R_MVLAN_LPORT_ERROR_BASE + 20)
#define      MVLAN_LPORT_AUTONEG_NOT_DISABLED      (R_MVLAN_LPORT_ERROR_BASE + 21)
#define      MVLAN_LPORT_SPEED_SET_NOT_SUPPORTED   (R_MVLAN_LPORT_ERROR_BASE + 22)
#define      MVLAN_LPORT_NO_CFG_FOUND              (R_MVLAN_LPORT_ERROR_BASE + 23)
#define      MVLAN_LPORT_FORBID_SET_LAG_PORT_SPEED (R_MVLAN_LPORT_ERROR_BASE + 24)

#define      R_MVLAN_SPORT_ERROR_BASE              (R_MVLAN_GLOB_ERROR_BASE  + 3000)
#define      MVLAN_SPORT_NOT_HANDLED               (R_MVLAN_SPORT_ERROR_BASE + 1)
#define      MMVLAN_SPORT_NAME_TOO_LONG            (R_MVLAN_SPORT_ERROR_BASE + 2)
#define      MVLAN_SPORT_EXISTS                    (R_MVLAN_SPORT_ERROR_BASE + 3)
#define      MVLAN_SPORT_ID_ZERO                   (R_MVLAN_SPORT_ERROR_BASE + 4)
#define      MVLAN_SPORT_MAX_NAME_SIZE             (R_MVLAN_SPORT_ERROR_BASE + 5)
#define      MVLAN_SPORT_INSERT_FAILED             (R_MVLAN_SPORT_ERROR_BASE + 6)
#define      MVLAN_SPORT_NO_MEM                    (R_MVLAN_SPORT_ERROR_BASE + 7)
#define      MVLAN_SPORT_NON_ZERO_REFCOUNT         (R_MVLAN_SPORT_ERROR_BASE + 8)
#define      MVLAN_SPORT_NOT_FOUND                 (R_MVLAN_SPORT_ERROR_BASE + 9)
#define      MVLAN_SPORT_EOT                       (R_MVLAN_SPORT_ERROR_BASE + 10)
#define      MVLAN_SPORT_LPORT_ATTACHED            (R_MVLAN_SPORT_ERROR_BASE + 11)
#define      MVLAN_SPORT_VLAN_ATTACHED             (R_MVLAN_SPORT_ERROR_BASE + 12)
#define      MVLAN_SPORT_PORT_IN_SPORT             (R_MVLAN_SPORT_ERROR_BASE + 13)
#define      MVLAN_SPORT_PORT_IS_TRUNK             (R_MVLAN_SPORT_ERROR_BASE + 14)
#define      MVLAN_SPORT_PORT_IN_VLAN              (R_MVLAN_SPORT_ERROR_BASE + 15)
#define      MVLAN_SPORT_PORT_HAS_STP              (R_MVLAN_SPORT_ERROR_BASE + 16)
#define      MVLAN_SPORT_PORT_NO_CFG               (R_MVLAN_SPORT_ERROR_BASE + 17)
#define      MVLAN_SPORT_PORT_NOT_IN_SPORT         (R_MVLAN_SPORT_ERROR_BASE + 18)
#define      MVLAN_SPORT_ALREADY_TRUNK             (R_MVLAN_SPORT_ERROR_BASE + 19)
#define      MVLAN_SPORT_NOT_TRUNK                 (R_MVLAN_SPORT_ERROR_BASE + 20)
#define      MVLAN_SPORT_NO_MORE_VLANS             (R_MVLAN_SPORT_ERROR_BASE + 21)
#define      MVLAN_SPORT_INVALID_TYPE              (R_MVLAN_SPORT_ERROR_BASE + 22)
#define      MVLAN_SPORT_IS_TRUNK                  (R_MVLAN_SPORT_ERROR_BASE + 23)
#define      MVLAN_SPORT_PORT_HAS_LACP             (R_MVLAN_SPORT_ERROR_BASE + 24)
#define      MVLAN_SPORT_ON_MULTIPLE_VLANS         (R_MVLAN_SPORT_ERROR_BASE + 25)
#define      MVLAN_SPORT_AGGR_PARAMS_NOT_SET       (R_MVLAN_SPORT_ERROR_BASE + 26)
#define      MVLAN_SPORT_IS_NOT_TRUNK              (R_MVLAN_SPORT_ERROR_BASE + 27)
#define      MVLAN_SPORT_HAS_TRUNK_VLANS           (R_MVLAN_SPORT_ERROR_BASE + 28)
#define      MVLAN_SPORT_HAS_ACCESS_VLAN           (R_MVLAN_SPORT_ERROR_BASE + 29)
#define      MVLAN_SPORT_NO_TRUNK_NATIVE_VLAN_SET  (R_MVLAN_SPORT_ERROR_BASE + 30)
#define      MVLAN_SPORT_NATIVE_VLAN_ALAREADY_SET  (R_MVLAN_SPORT_ERROR_BASE + 31)
#define      MVLAN_IMPLICIT_NOT_ALLOWED_NOT_TRUNK  (R_MVLAN_SPORT_ERROR_BASE + 32)
#define      MVLAN_IP_ALREAY_CONFIGURED            (R_MVLAN_SPORT_ERROR_BASE + 33)
#define      MVLAN_IMPLICIT_NOT_ALLOWED_ON_STP_ENABLED_PORTS \
                                                   (R_MVLAN_SPORT_ERROR_BASE + 34)
#define      MVLAN_SPORT_PORT_HAS_MTU_CONFIGURED   (R_MVLAN_SPORT_ERROR_BASE + 35)
#define      MVLAN_IMPLICIT_NOT_ALLOWED_ON_LAG_MEMBERS \
                                                   (R_MVLAN_SPORT_ERROR_BASE + 36)
#define      MVLAN_IMPLICIT_NOT_ALLOWED_ON_LACP_ENABLED_PORTS \
                                                   (R_MVLAN_SPORT_ERROR_BASE + 37)
#define      MVLAN_SPORT_NO_VLAN                   (R_MVLAN_SPORT_ERROR_BASE + 38)
#define      MVLAN_CANT_MAKE_MEMBER_VLAN_AS_NATIVE (R_MVLAN_SPORT_ERROR_BASE + 39) 

#define      R_MVLAN_SEND_ERROR_BASE               (R_MVLAN_GLOB_ERROR_BASE  + 4000)
#define      MVLAN_SEND_LINECARD_ABSENT            (R_MVLAN_SEND_ERROR_BASE + 1)
#define      MVLAN_SEND_BAD_SUBSCRIPTION           (R_MVLAN_SEND_ERROR_BASE + 2)
#define      MVLAN_SEND_PM_DEAD                    (R_MVLAN_SEND_ERROR_BASE + 3)
#define      MVLAN_SEND_NO_MORE_EPTI               (R_MVLAN_SEND_ERROR_BASE + 4)



#define      R_MVLAN_CFG_ERROR_BASE                (R_MVLAN_GLOB_ERROR_BASE  + 5000)
#define      MVLAN_CFG_ALREADY_INITED              (R_MVLAN_CFG_ERROR_BASE + 1)
#define      MVLAN_CFG_NO_MEM                      (R_MVLAN_CFG_ERROR_BASE + 2)
#define      MVLAN_CFG_INSERT_FAILED               (R_MVLAN_CFG_ERROR_BASE + 3)
#define      MVLAN_CFG_PORT_NOT_FOUND              (R_MVLAN_CFG_ERROR_BASE + 4)
#define      MVLAN_CFG_PORT_IN_USE                 (R_MVLAN_CFG_ERROR_BASE + 5)
#define      MVLAN_CFG_NO_MORE_VLANS               (R_MVLAN_CFG_ERROR_BASE + 6)
#define      MVLAN_CFG_NO_VLAN_MATCH               (R_MVLAN_CFG_ERROR_BASE + 7)
#define      MVLAN_CFG_NO_SUB_LPORT                (R_MVLAN_CFG_ERROR_BASE + 8)
#define      MVLAN_CFG_EOT                         (R_MVLAN_CFG_ERROR_BASE + 9)
#define      MVLAN_CFG_LPORT_DOES_NOT_EXIST        (R_MVLAN_CFG_ERROR_BASE + 10)
#define      MVLAN_CFG_PORT_MONITOR_EXISTS         (R_MVLAN_CFG_ERROR_BASE + 11)
#define      MVLAN_CFG_MTU_NOT_ALLOWED_ON_LAG_MEMBERS \
                                                   (R_MVLAN_CFG_ERROR_BASE + 12)
#define      MVLAN_CFG_EXCEEDED_MAX_NUM_JUMBO_MTU_PORT\
                                                   (R_MVLAN_CFG_ERROR_BASE + 13)

#define      R_MVLAN_STP_ERROR_BASE                (R_MVLAN_GLOB_ERROR_BASE + 6000)
#define      MVLAN_STP_PORT_HAS_STP                (R_MVLAN_STP_ERROR_BASE  + 1)
#define      MVLAN_STP_PORT_IN_SPORT               (R_MVLAN_STP_ERROR_BASE  + 2)
#define      MVLAN_STP_SPORT_HAS_STP               (R_MVLAN_STP_ERROR_BASE  + 3)
#define      MVLAN_STP_NOT_ENABLED                 (R_MVLAN_STP_ERROR_BASE  + 4)
#define      MVLAN_STP_LPORT_HAS_LACP_ENABLED      (R_MVLAN_STP_ERROR_BASE  + 5)
#define      MVLAN_MVST_INST_EXISTS                (R_MVLAN_STP_ERROR_BASE  + 6)
#define      MVLAN_MVST_INST_DOES_NOT_EXIST        (R_MVLAN_STP_ERROR_BASE  + 7)
#define      MVLAN_MVST_INST_HAS_NO_VLAN           (R_MVLAN_STP_ERROR_BASE  + 8)
#define      MVLAN_MVST_PORT_HAS_NO_VLAN           (R_MVLAN_STP_ERROR_BASE  + 9)
#define      MVLAN_MVST_NO_MATCHING_MEMBERSHIP     (R_MVLAN_STP_ERROR_BASE  + 10)
#define      MVLAN_STP_VSTP_ALLOC_FAILED           (R_MVLAN_STP_ERROR_BASE  + 11)
#define      MVLAN_MVST_INST_NOT_ENABLED           (R_MVLAN_STP_ERROR_BASE  + 12)
#define      MVLAN_MVST_INST_HAS_MEMBER_VLANS      (R_MVLAN_STP_ERROR_BASE  + 13)
#define      MVLAN_MVST_CANT_ASSOCIATE_DEF_VLAN    (R_MVLAN_STP_ERROR_BASE  + 14)
#define      MVLAN_MVST_VLAN_ALREADY_ASSOCIATED    (R_MVLAN_STP_ERROR_BASE  + 15)
#define      MVLAN_VLAN_NOT_MEMEBER_OF_THIS_INST   (R_MVLAN_STP_ERROR_BASE  + 16)
#define      MVLAN_VLAN_SPORT_RUNNING_MVST         (R_MVLAN_STP_ERROR_BASE  + 17)
#define      MVLAN_VLAN_INSTANCE_ALREADY_ENABLED   (R_MVLAN_STP_ERROR_BASE  + 18)
#define      MVLAN_MVST_PORT_SHOULD_BE_IN_NON_DEF_VLAN (R_MVLAN_STP_ERROR_BASE  + 19)
#define      MVLAN_MVST_PORT_HAS_IP_ENABLED        (R_MVLAN_STP_ERROR_BASE + 20)
#define      MVLAN_MVST_ALREADY_ENABLED_ON_PORT    (R_MVLAN_STP_ERROR_BASE + 21)
#define      MVLAN_MVST_MULTIPLE_MVST_ON_ACCESS    (R_MVLAN_STP_ERROR_BASE + 22)
#define      MVLAN_MVST_SPORT_HAS_MVST_PARAMS      (R_MVLAN_STP_ERROR_BASE + 23)
#define      MVLAN_MVST_FAILED_ENFORCING_HELLO_TIME_TO_MAX_AGE\
                                                   (R_MVLAN_STP_ERROR_BASE + 24)
#define      MVLAN_MVST_FAILED_ENFORCING_MAX_AGE_TO_FWD_DELAY\
                                                   (R_MVLAN_STP_ERROR_BASE + 25)
#define      MVLAN_STP_ALREADY_ENABLED_ON_PORT     (R_MVLAN_STP_ERROR_BASE + 26)
                                                    

#define      R_MVLAN_LACP_ERROR_BASE               (R_MVLAN_GLOB_ERROR_BASE + 7000)
#define      MVLAN_LACP_SPORT_PARAMS_SET           (R_MVLAN_LACP_ERROR_BASE + 0)
#define      MVLAN_LACP_DUPLICATE_SPORT_PARAMS     (R_MVLAN_LACP_ERROR_BASE + 1)
#define      MVLAN_LACP_SPORT_PARAMS_NOT_FOUND     (R_MVLAN_LACP_ERROR_BASE + 2)
#define      MVLAN_LACP_LPORT_PARAM_ALREADY_SET    (R_MVLAN_LACP_ERROR_BASE + 3)
#define      MVLAN_LACP_NO_PARAMS_SET              (R_MVLAN_LACP_ERROR_BASE + 4)
#define      MVLAN_LACP_INVALID_LPORT_PARAMS       (R_MVLAN_LACP_ERROR_BASE + 5)
#define      MVLAN_LACP_LPORT_INVALID_PORT_KEY     (R_MVLAN_LACP_ERROR_BASE + 6)
#define      MVLAN_LACP_PARAMS_NOT_SET             (R_MVLAN_LACP_ERROR_BASE + 7)
#define      MVLAN_LACP_PORT_IN_SPORT              (R_MVLAN_LACP_ERROR_BASE + 8)
#define      MVLAN_LACP_PORT_HAS_STP               (R_MVLAN_LACP_ERROR_BASE + 9)
#define      MVLAN_LACP_PORT_IS_TRUNK              (R_MVLAN_LACP_ERROR_BASE + 10)
#define      MVLAN_LACP_PORT_ALREADY_IN_SPORT      (R_MVLAN_LACP_ERROR_BASE + 11)
#define      MVLAN_LACP_PORT_NOT_IN_SPORT          (R_MVLAN_LACP_ERROR_BASE + 12)
#define      MVLAN_LACP_SPORT_KEY_NOT_FOUND        (R_MVLAN_LACP_ERROR_BASE + 13)
#define      MVLAN_LACP_LPORT_IN_TRANSIT           (R_MVLAN_LACP_ERROR_BASE + 14)

#ifdef STARSHIP
#define      R_MVLAN_IGMP_SNOOP_ERROR_BASE         (R_MVLAN_GLOB_ERROR_BASE + 10000)
#define      MVLAN_IGMP_SNOOP_BAD_PARAM            (R_MVLAN_IGMP_SNOOP_ERROR_BASE + 1)
#define      MVLAN_IGMP_SNOOP_NOT_ENABLED          (R_MVLAN_IGMP_SNOOP_ERROR_BASE + 2)
#define      MVLAN_IGMP_SNOOP_UNKNOWN_REQ          (R_MVLAN_IGMP_SNOOP_ERROR_BASE + 3)
#endif

#endif /* _MVLAN_ERROR_H */
