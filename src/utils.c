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

//**************************************************************************
//    File               : utils.c
//
//    Description        : This file provides common utility functions for
//                         Halon Cyclone adaptations.
//**************************************************************************

#include <stdlib.h>
#include <string.h>

#include <halon_cmn.h>

int
speed_str_to_speed(char *cfg_speed)
{
    int speed = 0;

    if (cfg_speed != NULL) {
        if (strcmp(cfg_speed, "40000") == 0) {
            speed = 40000;
        } else if (strcmp(cfg_speed, "20000") == 0) {
            speed = 20000;
        } else if (strcmp(cfg_speed, "10000") == 0) {
            speed = 10000;
        } else if (strcmp(cfg_speed, "1000") == 0) {
            speed = 1000;
        } else if (strcmp(cfg_speed, "2500") == 0) {
            speed = 2500;
        } else if (strcmp(cfg_speed, "100") == 0) {
            speed = 100;
        } else if (strcmp(cfg_speed, "10") == 0) {
            speed = 10;
        }
    }

    return speed;

} // speed_str_to_speed

enum PM_lport_type
speed_to_lport_type(int speed)
{
    enum PM_lport_type ptype = PM_LPORT_INVALID;

    if (40000 == speed) {
        ptype = PM_LPORT_40GIGE;
    } else if (20000 == speed) {
        ptype = PM_LPORT_20GIGE;
    } else if (10000 == speed) {
        ptype = PM_LPORT_10GIGE;
    } else if (1000 == speed) {
        ptype = PM_LPORT_GIGE;
    } else if (2500 == speed) {
        ptype = PM_LPORT_2_5GIGE;
    } else if (100 == speed) {
        ptype = PM_LPORT_FAE;
    } else if (10 == speed) {
        ptype = PM_LPORT_10E;
    }

    return ptype;

} // speed_to_lport_type

int
lport_type_to_speed(enum PM_lport_type ptype)
{
    int speed = 0;

    switch (ptype) {
    case PM_LPORT_10E:      speed = 10;     break;
    case PM_LPORT_FAE:      speed = 100;    break;
    case PM_LPORT_GIGE:     speed = 1000;   break;
    case PM_LPORT_2_5GIGE:  speed = 2500;   break;
    case PM_LPORT_10GIGE:   speed = 10000;  break;
    case PM_LPORT_20GIGE:   speed = 20000;  break;
    case PM_LPORT_40GIGE:   speed = 40000;  break;
    default:                speed = 0;      break;
    }

    return speed;

} // lport_type_to_speed
