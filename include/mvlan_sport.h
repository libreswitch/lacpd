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
   File               : mvlan_sport.h
   Description        : The file defines the data structures used by the
                        super port manager to create and maintain super
                        ports.
*****************************************************************************/

#ifndef _MVLAN_SPORT_H
#define _MVLAN_SPORT_H

#include "mvlan_lacp.h"

#define MVLAN_SPORT_NO_MEM                 -2
#define MVLAN_SPORT_EXISTS                 -3
#define MVLAN_SPORT_LPORT_ATTACHED         -4
#define MVLAN_SPORT_IS_TRUNK               -5
#define MVLAN_LACP_SPORT_PARAMS_SET        -6
#define MVLAN_SPORT_NOT_FOUND              -7
#define MVLAN_SPORT_EOT                    -8
#define MVLAN_LACP_SPORT_KEY_NOT_FOUND     -9
#define MVLAN_LACP_SPORT_PARAMS_NOT_FOUND  -10

#define STYPE_802_3AD         (0x4)     /*  Is of type IEEE 803.3ad and has multiple logical ports */

typedef struct super_port_s super_port_t;

/*********************************************************************
 * Structure defining the super-port information maintained by vpm
 * in the user-space.
 *********************************************************************/

#define  SPORT_MAX_NAME_SIZE  (20)

struct super_port_s
{
    port_handle_t handle;              /* The handle for this super port */
    char name[SPORT_MAX_NAME_SIZE+1];  /* 1 byte for null */

    u_char type;                       /* Type of super port */
    u_long info_flags;                 /* Various bit flags as below */

    u_char admin_state;                /*  The administrative state */
#define SPORT_ADMIN_UP        (0x01)

    u_char oper_state_bits;            /* These are bit flags that determine
                                        * the operational state of the port for
                                        * various kinds of traffic (bridged/
                                        * routed IP/routed IPv6/MPLS/OSI). The
                                        * bit flag definitions are in
                                        * vlan_cmn.h.
                                        */
   u_char true_oper_state_bits;        /* For all other tasks, this is the
                                        * true state of the port for traffic
                                        * forwarding purposes. This is a
                                        * function of the fields admin_state,
                                        * oper_state_bits and stp_state_1vst
                                        * (the last for bridged traffic only).
                                        */
   int     num_lports;                  /* Number of lports in this list */
   void    *plport_list;                /* Logical Port Handle List: A list of
                                           ports that are supported. A linklist
                                           of the handles of all the logical
                                           ports that this SmartTrunk contains. */

   void   *placp_params;                /* pointer to the the superport parameters
                                            as defined in mvlan_lacp.h */

   int     aggr_mode;                   /* Mac, l3, l4 or port based */
};

extern int mvlan_sport_init(u_long  first_time);
extern int mvlan_sport_create(struct MLt_vpm_api__create_sport  *psport_create,
                              super_port_t **ppsport);
extern int mvlan_sport_delete(super_port_t  *psport);
extern int mvlan_destroy_sport(super_port_t *psport);
extern int mvlan_get_sport(port_handle_t handle, super_port_t **ppsport, int type);

#endif /* _MVLAN_SPORT_H */
