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

/*****************************************************************************
   File               : mvlan_lacp.h
   Description        : The file defines the data structures needed by
                        LACP config.
*****************************************************************************/

#ifndef _MVLAN_LACP_H_
#define _MVLAN_LACP_H_

#include <nemo_types.h>
#include "lacp_halon.h"

/*********************************************************************
 * Structure defining the sport LACP information.
 *********************************************************************/
typedef struct lacp_sport_params_s {
    int      flags;                     /* see lacp_cmn.h */
    int      port_type;
    int      actor_key;                 /* A value between 1- 65535 */
    int      partner_key;               /* A value between 1- 65535 */
    int      partner_system_priority;   /* A value between 1- 65535 */
    char     partner_system_id[6];      /* The mac address */
    int      aggr_type;                 /* whether aggregateable or indiv */
} lacp_sport_params_t;

/*********************************************************************
 *  Internal representation of the LACP info.
 *********************************************************************/
typedef struct lacp_int_sport_params_s {
    lacp_sport_params_t  lacp_params;   /* Should be the first field in this struct */
    void                *psport;        /* Pointer to the super port */
} lacp_int_sport_params_t;


extern int mvlan_api_modify_sport_params(struct  MLt_vpm_api__lacp_sport_params *placp_params,
                                         int operations);
extern int mvlan_api_validate_unset_sport_params(struct MLt_vpm_api__lacp_sport_params *placp_params);
extern int mvlan_api_validate_set_sport_params(struct MLt_vpm_api__lacp_sport_params *placp_params);
extern int mvlan_set_sport_params(struct MLt_vpm_api__lacp_sport_params *pin_lacp_params);
extern int mvlan_unset_sport_params(struct MLt_vpm_api__lacp_sport_params *pin_lacp_params);

// Halon - called to clear out LAG information when all ports are really detached.
//         This essentially clears the LAG and put it back to the "free" pool
extern int mvlan_api_clear_sport_params(unsigned long long sport_handle);

extern int mvlan_api_select_aggregator(struct MLt_vpm_api__lacp_match_params *placp_match_params);
extern int mvlan_api_attach_lport_to_aggregator(struct MLt_vpm_api__lacp_attach *placp_attach_params);
extern int mvlan_api_detach_lport_from_aggregator(struct MLt_vpm_api__lacp_attach *placp_detach_params);

#endif /* _MVLAN_LACP_H_ */
