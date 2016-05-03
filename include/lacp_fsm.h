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

#ifndef __LACP_FSM_H__
#define __LACP_FSM_H__

/*****************************************************************************
 *                   EVENT NUMBERS
 *****************************************************************************/
#define E1 0
#define E2 1
#define E3 2
#define E4 3
#define E5 4
#define E6 5
#define E7 6
#define E8 7
#define E9 8
#define NO_ACTION 0

/*****************************************************************************/
/*                   MACROS DEFINING ACTIONS FOR RECV FSM                    */
/*****************************************************************************/
#define RECV_FSM_NUM_INPUTS  9
#define RECV_FSM_NUM_STATES  7

#define ACTION_CURRENT 1
#define ACTION_EXPIRED 2
#define ACTION_DEFAULTED 3
#define ACTION_LACP_DISABLED 4
#define ACTION_PORT_DISABLED 5
#define ACTION_INITIALIZE 6

#define RECV_FSM_BEGIN_STATE 0
#define RECV_FSM_CURRENT_STATE 1
#define RECV_FSM_EXPIRED_STATE 2
#define RECV_FSM_DEFAULTED_STATE 3
#define RECV_FSM_LACP_DISABLED_STATE 4
#define RECV_FSM_PORT_DISABLED_STATE 5
#define RECV_FSM_INITIALIZE_STATE 6
#define RECV_FSM_RETAIN_STATE 7

/*****************************************************************************/
/*                   MACROS DEFINING ACTIONS FOR MUX FSM                     */
/*****************************************************************************/
#define MUX_FSM_NUM_INPUTS  9
#define MUX_FSM_NUM_STATES  6

#define ACTION_DETACHED 1
#define ACTION_WAITING 2
#define ACTION_ATTACHED 3
#define ACTION_COLLECTING 4
#define ACTION_COLLECTING_DISTRIBUTING 5

#define MUX_FSM_BEGIN_STATE 0
#define MUX_FSM_DETACHED_STATE 1
#define MUX_FSM_WAITING_STATE 2
#define MUX_FSM_ATTACHED_STATE 3
#define MUX_FSM_COLLECTING_STATE 4
#define MUX_FSM_COLLECTING_DISTRIBUTING_STATE 5
#define MUX_FSM_RETAIN_STATE 6

/*****************************************************************************/
/*                   MACROS DEFINING ACTIONS FOR PERIODIC_TX  FSM            */
/*****************************************************************************/
#define PERIODIC_TX_FSM_NUM_INPUTS  9
#define PERIODIC_TX_FSM_NUM_STATES  5

#define ACTION_NO_PERIODIC 1
#define ACTION_FAST_PERIODIC 2
#define ACTION_SLOW_PERIODIC 3
#define ACTION_PERIODIC_TX 4

#define PERIODIC_TX_FSM_BEGIN_STATE 0
#define PERIODIC_TX_FSM_NO_PERIODIC_STATE 1
#define PERIODIC_TX_FSM_FAST_PERIODIC_STATE 2
#define PERIODIC_TX_FSM_SLOW_PERIODIC_STATE 3
#define PERIODIC_TX_FSM_PERIODIC_TX_STATE 4
#define PERIODIC_TX_FSM_RETAIN_STATE 5

/*  Macro to get the contents of the cell  */
#define GET_FSM_TABLE_CELL_CONTENTS(FSM, INPUT, STATE, ACTION) \
                                    ACTION = FSM[INPUT][STATE].action; \
                                    STATE = FSM[INPUT][STATE].next_state;

typedef struct fsm_entry {
    unsigned int next_state;
    unsigned int action;
} FSM_ENTRY;

#endif /* __LACP_FSM_H__ */
