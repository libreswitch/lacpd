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
   File               : mlacp_debug.h
   Description        : The file contains all definitions necessary for
                        debugging the mcpu LACP module.
*****************************************************************************/
#ifndef __MLACP_DEBUG_H__
#define __MLACP_DEBUG_H__

//----------------------slog daemon/library identification strings------
#define LACPD_ID        "lacpd"

/*****************************************************************************
 * Debug macros
 *****************************************************************************/
// daemon-specific mask values all of which translate to LOG_DEBUG
//    lacpd specific mask values =
//      SLOG_MAKE_SERVICEPRI(23) >= pri >= SLOG_MAKE_SERVICEPRI(0)
//    use:
//          SLOG(SLOG_LACPD_LACP_TASK, "task info=%d", x);
//      will translate to...
//          syslog(LOG_DEBUG, "task info=%d", x);

// Port-based debug level
#define  DBG_FATAL        (SLOG_EMERG)
#define  DBG_ERROR        (SLOG_ERR)
#define  DBG_WARNING      (SLOG_WARNING)
#define  DBG_INFO         (SLOG_INFO)
#define  DBG_LACPDU       (SLOG_MAKE_SERVICEPRI(0))  // 0x100
#define  DBG_LACP_TASK    (SLOG_MAKE_SERVICEPRI(1))  // 0x200
#define  DBG_LACP_SEND    (SLOG_MAKE_SERVICEPRI(2))  // 0x400
#define  DBG_LACP_RCV     (SLOG_MAKE_SERVICEPRI(3))  // 0x800
#define  DBG_RX_FSM       (SLOG_MAKE_SERVICEPRI(4))  // 0x1000
#define  DBG_TX_FSM       (SLOG_MAKE_SERVICEPRI(5))  // 0x2000
#define  DBG_MUX_FSM      (SLOG_MAKE_SERVICEPRI(6))  // 0x4000
#define  DBG_SELECT       (SLOG_MAKE_SERVICEPRI(7))  // 0x8000
#define  DBG_TIMERS       (SLOG_MAKE_SERVICEPRI(8))  // 0x10000
#define  DBG_VPM          (SLOG_MAKE_SERVICEPRI(9))  // 0x20000
#define  DBG_HW           (SLOG_MAKE_SERVICEPRI(10)) // 0x40000
#define  DBG_DAL          (SLOG_MAKE_SERVICEPRI(11)) // 0x80000
#define  DBG_F_ENTRY      (SLOG_MAKE_SERVICEPRI(12)) // 0x100000

// Some helper defines
#define  DBG_ALL          (0xFFFF)
#define  DBG_BASIC        (DBG_FATAL | DBG_ERROR | DBG_WARNING)
#define  DBG_FSM_ALL      (DBG_RX_FSM | DBG_TX_FSM | DBG_MUX_FSM)

// Global debug level
#define  DL_FATAL         (DBG_FATAL)
#define  DL_ERROR         (DBG_ERROR)
#define  DL_WARNING       (DBG_WARNING)
#define  DL_INFO          (DBG_INFO)
#define  DL_LACPDU        (DBG_LACPDU)
#define  DL_LACP_TASK     (DBG_LACP_TASK)
#define  DL_LACP_SEND     (DBG_LACP_SEND)
#define  DL_LACP_RCV      (DBG_LACP_RCV)
#define  DL_RX_FSM        (DBG_RX_FSM)
#define  DL_TX_FSM        (DBG_TX_FSM)
#define  DL_MUX_FSM       (DBG_MUX_FSM)
#define  DL_SELECT        (DBG_SELECT)
#define  DL_TIMERS        (DBG_TIMERS)
#define  DL_VPM           (DBG_VPM)
#define  DL_HW            (DBG_HW)
#define  DL_DAL           (DBG_DAL)

#define RDEBUG(category, m...)  SLOG(category, m)
#define RDBG(m...)  SLOG(SLOG_NOTICE, m)

#if 0
#define RENTRY()     RDEBUG(DBG_F_ENTRY, "Entry: %s", __FUNCTION__);
#define REXIT()      RDEBUG(DBG_F_ENTRY, "Exit: %s", __FUNCTION__);
#define REXITS(s)    RDEBUG(DBG_F_ENTRY, "Exit: %s status=%d\n", __FUNCTION__, s);
#else
#define RENTRY()
#define REXIT()
#define REXITS(s)
#endif

#endif //__MLACP_DEBUG_H__
