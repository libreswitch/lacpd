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

/*------------------------------------------------------------------------------
 *  MODULE:
 *
 *     mux_fsm.c
 *
 *  SUB-SYSTEM:
 *
 *  ABSTRACT
 *    This file contains the routines implementing the mux state machine
 *
 *  EXPORTED LOCAL ROUTINES:
 *
 *  STATIC LOCAL ROUTINES:
 *
 *  AUTHOR:
 *
 *    Gowrishankar, Riverstone Networks
 *
 *  CREATION DATE:
 *
 *    March 5, 2000
 *
 *
 *----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <nemo/nemo_types.h>
#include <nemo/avl.h>
#include <nemo/pm/pm_cmn.h>
#include <nemo/lacp/lacp_cmn.h>
#include <nemo/lacp/mlacp_debug.h>
#include <nemo/lacp/lacp_fsm.h>

#include "lacp.h"
#include "lacp_stubs.h"
#include "lacp_support.h"
#include "mlacp_fproto.h"


/*****************************************************************************
 *                    Global Variables Definition
 ****************************************************************************/



/*****************************************************************************
 *   Static Variables
 ****************************************************************************/

/* mux machine fsm table */
static FSM_ENTRY mux_machine_fsm_table[MUX_FSM_NUM_INPUTS]
                                      [MUX_FSM_NUM_STATES] =
{

/*****************************************************************************
 * Input Event E1 - selected = SELECTED 
 *****************************************************************************/
  {{MUX_FSM_RETAIN_STATE,       NO_ACTION},         // Begin state
   {MUX_FSM_WAITING_STATE,      ACTION_WAITING},    // detached
   {MUX_FSM_RETAIN_STATE,       NO_ACTION},         // waiting
   {MUX_FSM_RETAIN_STATE,       NO_ACTION},         // attached
   {MUX_FSM_RETAIN_STATE,       NO_ACTION},         // collecting
   {MUX_FSM_RETAIN_STATE,       NO_ACTION}},        // collecting_distributing


/*****************************************************************************
 * Input Event E2 - selected = UNSELECTED
 *****************************************************************************/
  {{MUX_FSM_RETAIN_STATE,      NO_ACTION},          // Begin state
   {MUX_FSM_RETAIN_STATE,      NO_ACTION},          // detached
   {MUX_FSM_DETACHED_STATE,    ACTION_DETACHED},    // waiting
   {MUX_FSM_DETACHED_STATE,    ACTION_DETACHED},    // attached
   {MUX_FSM_ATTACHED_STATE,    ACTION_ATTACHED},    // collecting
   {MUX_FSM_ATTACHED_STATE,    ACTION_ATTACHED}},   // collecting_distributing


/*****************************************************************************
 * Input Event E3 - selected = SELECTED and Ready = TRUE
 *****************************************************************************/
  {{MUX_FSM_RETAIN_STATE,      NO_ACTION},          // Begin state
   {MUX_FSM_RETAIN_STATE,      NO_ACTION},          // detached
   {MUX_FSM_ATTACHED_STATE,    ACTION_ATTACHED},    // waiting
   {MUX_FSM_RETAIN_STATE,      NO_ACTION},          // attached
   {MUX_FSM_RETAIN_STATE,      NO_ACTION},          // collecting
   {MUX_FSM_RETAIN_STATE,      NO_ACTION}},         // collecting_distributing


/*****************************************************************************
 * Input Event E4 - selected = StandBy
 *****************************************************************************/
  {{MUX_FSM_RETAIN_STATE,      NO_ACTION},          // Begin state
   {MUX_FSM_RETAIN_STATE,      NO_ACTION},          // detached
   {MUX_FSM_RETAIN_STATE,      NO_ACTION},          // waiting
   {MUX_FSM_DETACHED_STATE,    ACTION_DETACHED},    // attached
   {MUX_FSM_ATTACHED_STATE,    ACTION_ATTACHED},    // collecting
   {MUX_FSM_ATTACHED_STATE,    ACTION_ATTACHED}},   // collecting_distributing


/*****************************************************************************
 * Input Event E5 - selected = SELECTED and partner.sync = True
 *****************************************************************************/
  {{MUX_FSM_RETAIN_STATE,      NO_ACTION},         // Begin state
   {MUX_FSM_RETAIN_STATE,      NO_ACTION},         // detached
   {MUX_FSM_RETAIN_STATE,      NO_ACTION},         // waiting
   {MUX_FSM_COLLECTING_STATE,  ACTION_COLLECTING}, // attached
   {MUX_FSM_RETAIN_STATE,      NO_ACTION},         // collecting
   {MUX_FSM_RETAIN_STATE,      NO_ACTION}},        // collecting_distributing


/*****************************************************************************
 * Input Event E6 -  partner.sync = False
 *****************************************************************************/
  {{MUX_FSM_RETAIN_STATE,      NO_ACTION},           // Begin state
   {MUX_FSM_RETAIN_STATE,      NO_ACTION},           // detached
   {MUX_FSM_RETAIN_STATE,      NO_ACTION},           // waiting
   {MUX_FSM_RETAIN_STATE,      NO_ACTION},           // attached
   {MUX_FSM_ATTACHED_STATE,    ACTION_ATTACHED},     // collecting
   {MUX_FSM_ATTACHED_STATE,    ACTION_ATTACHED}},    // collecting_distributing


/*****************************************************************************
 * Input Event E7 - Begin = True
 *****************************************************************************/
  {{MUX_FSM_DETACHED_STATE,      ACTION_DETACHED},  // Begin state
   {MUX_FSM_DETACHED_STATE,      ACTION_DETACHED},  // detached
   {MUX_FSM_DETACHED_STATE,      ACTION_DETACHED},  // waiting
   {MUX_FSM_DETACHED_STATE,      ACTION_DETACHED},  // attached
   {MUX_FSM_DETACHED_STATE,      ACTION_DETACHED},  // collecting
   {MUX_FSM_DETACHED_STATE,      ACTION_DETACHED}}, // collecting_distributing


/*****************************************************************************
 * Input Event E8 - selected = SELECTED, partner.sync = True and
 *                  partner.collecting = TRUE
 *****************************************************************************/
  {{MUX_FSM_RETAIN_STATE,      NO_ACTION},          // Begin state
   {MUX_FSM_RETAIN_STATE,      NO_ACTION},          // detached
   {MUX_FSM_RETAIN_STATE,      NO_ACTION},          // waiting
   {MUX_FSM_RETAIN_STATE,      NO_ACTION},          // attached
   {MUX_FSM_COLLECTING_DISTRIBUTING_STATE, ACTION_COLLECTING_DISTRIBUTING},  // collecting
   {MUX_FSM_RETAIN_STATE,      NO_ACTION}},         // collecting_distributing


/*****************************************************************************
 * Input Event E9 - selected = SELECTED, partner.sync = True and
 *                  partner.collecting = FALSE
 *****************************************************************************/
  {{MUX_FSM_RETAIN_STATE,      NO_ACTION},          // Begin state
   {MUX_FSM_RETAIN_STATE,      NO_ACTION},          // detached
   {MUX_FSM_RETAIN_STATE,      NO_ACTION},          // waiting
   {MUX_FSM_RETAIN_STATE,      NO_ACTION},          // attached
   {MUX_FSM_ATTACHED_STATE,    ACTION_ATTACHED},    // collecting
   {MUX_FSM_ATTACHED_STATE,    ACTION_ATTACHED}}    // collecting_distributing

};


/*****************************************************************************
 *                 Prototypes for global functions
 ****************************************************************************/
void start_wait_while_timer(lacp_per_port_variables_t *);
int detach_mux_from_aggregator(lacp_per_port_variables_t *);
int attach_mux_to_aggregator(lacp_per_port_variables_t *);

/*****************************************************************************
 *                 Prototypes for static functions
 ****************************************************************************/
static void detached_state_action(lacp_per_port_variables_t *);
static void waiting_state_action(lacp_per_port_variables_t *);
static void attached_state_action(lacp_per_port_variables_t *);
static void collecting_state_action(lacp_per_port_variables_t *);
static void collecting_distributing_state_action(lacp_per_port_variables_t *);
static void disable_collecting_distributing(lacp_per_port_variables_t *);
static void enable_collecting(lacp_per_port_variables_t *);
static void enable_distributing(lacp_per_port_variables_t *);

/*----------------------------------------------------------------------
 * Function: LACP_mux_fsm(int event, int current_state, int port_number)
 * Synopsis: Entry routine for mux state machine. 
 * Input  :  
 *           event = the event that occured
 *           current_state = the current state of the fsm
 *           port_number = port number on which to act upon.
 * Returns:  void
 *----------------------------------------------------------------------*/
void
LACP_mux_fsm(int event, 
              int current_state, 
              lacp_per_port_variables_t *plpinfo)
{

  int action;
  char previous_state_string[STATE_STRING_SIZE];
  char current_state_string[STATE_STRING_SIZE];


  RENTRY();
  RDEBUG(DL_MUX_FSM, "MuxFSM: event %d current_state %d\n", event, current_state);

  GET_FSM_TABLE_CELL_CONTENTS(mux_machine_fsm_table,
			  event,
			  current_state,
			  action);


  if (current_state != MUX_FSM_RETAIN_STATE) {
    // if (plpinfo->mux_machine_debug == TRUE)
    if (plpinfo->debug_level & DBG_MUX_FSM) {

      //***********************************************************
      // receive_fsm  debug is DBG_RX_FSM
      // periodic_fsm debug is DBG_TX_FSM
      // mux_fsm      debug is DBG_MUX_FSM
      //***********************************************************

      mux_state_string(plpinfo->mux_fsm_state, previous_state_string);
      
      mux_state_string(current_state, current_state_string);
      
      RDBG("%s : transitioning from %s to %s, action %d "
            "(lport 0x%llx)\n",
             __FUNCTION__,
             previous_state_string,
             current_state_string,
             action,
             plpinfo->lport_handle);

    }
    plpinfo->prev_mux_fsm_state = plpinfo->mux_fsm_state; // YAGqa36972
    plpinfo->mux_fsm_state = current_state;
  } else {
      if (plpinfo->debug_level & DBG_MUX_FSM) {
         RDBG("%s : retain old state (%d)\n", __FUNCTION__, plpinfo->mux_fsm_state);
      }
  }
  
  switch(action)
    {
      
    case ACTION_DETACHED :
      {
	detached_state_action(plpinfo);
      }
    break;
    
    case ACTION_WAITING :
      {
	waiting_state_action(plpinfo);
      }
    break;

    case ACTION_ATTACHED :
      {
	attached_state_action(plpinfo);
      }
    break;
    
     case ACTION_COLLECTING :
       {
	 collecting_state_action(plpinfo);
       }
     break;

     case ACTION_COLLECTING_DISTRIBUTING :
       {
	 collecting_distributing_state_action(plpinfo);
       }
     break;

    case NO_ACTION :
    default :
      break;
      
    }

    REXIT();

}

//******************************************************************
// Function : detached_state_action
//******************************************************************
static void
detached_state_action(lacp_per_port_variables_t *plpinfo)
{

  if (plpinfo->debug_level & DBG_MUX_FSM) {
     RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
  }

  detach_mux_from_aggregator(plpinfo);
  
  plpinfo->actor_oper_port_state.synchronization = FALSE;
  
  plpinfo->actor_oper_port_state.collecting = FALSE;

  disable_collecting_distributing(plpinfo);
  
  plpinfo->actor_oper_port_state.distributing = FALSE;  

  plpinfo->lacp_control.ntt = TRUE;

  /*********************************************************************
   * call the routine to transmit a LACPdu on the port
   *********************************************************************/
  LACP_async_transmit_lacpdu(plpinfo);


  /**********************************************************************
   * Check if the forward exit condition prevail.
   **********************************************************************/
  if ((plpinfo->lacp_control.selected == SELECTED)) {
    LACP_mux_fsm(E1, 
                 plpinfo->mux_fsm_state,
		 plpinfo);
  }

  if ((plpinfo->lacp_control.selected == STANDBY)) {
    LACP_mux_fsm(E4, 
                 plpinfo->mux_fsm_state,
		 plpinfo);
  }
  
  if (plpinfo->debug_level & DBG_MUX_FSM) {
     RDBG("%s : exit\n", __FUNCTION__);
  }
}

//******************************************************************
// Function : waiting_state_action
//******************************************************************
static void
waiting_state_action(lacp_per_port_variables_t *plpinfo)
{
  // XXX LAG_t *lag = lacp_lags[port_number];
  LAG_t *lag = plpinfo->lag;

  if (plpinfo->debug_level & DBG_MUX_FSM) {
     RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
  }

  start_wait_while_timer(plpinfo);

  if ((plpinfo->lacp_control.selected == UNSELECTED)) {
    LACP_mux_fsm(E2, 
                 plpinfo->mux_fsm_state,
		 plpinfo);
  }

  /**********************************************************************
   * Check if the forward exit condition prevail.
   **********************************************************************/
  if ((plpinfo->lacp_control.selected == SELECTED) && 
          lag && (lag->ready == TRUE)) {
    LACP_mux_fsm(E3, plpinfo->mux_fsm_state, plpinfo);
  }
  if (plpinfo->debug_level & DBG_MUX_FSM) {
     RDBG("%s : exit\n", __FUNCTION__);
  }
}

//******************************************************************
// Function : attached_state_action
//******************************************************************
static void
attached_state_action(lacp_per_port_variables_t *plpinfo)
{

  if (plpinfo->debug_level & DBG_MUX_FSM) {
     RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
  }

  attach_mux_to_aggregator(plpinfo);

  plpinfo->actor_oper_port_state.synchronization = TRUE;

  plpinfo->actor_oper_port_state.collecting = FALSE;

  disable_collecting_distributing(plpinfo);
  
  plpinfo->actor_oper_port_state.distributing = FALSE;

  plpinfo->lacp_control.ntt = TRUE;

  /**********************************************************************
   * call the routine to transmit a LACPdu on the port
   **********************************************************************/
  LACP_async_transmit_lacpdu(plpinfo);

  if ((plpinfo->lacp_control.selected == UNSELECTED)) {
    LACP_mux_fsm(E2, 
                 plpinfo->mux_fsm_state,
		 plpinfo);
  }

  if ((plpinfo->lacp_control.selected == STANDBY)) {
    LACP_mux_fsm(E4, 
                 plpinfo->mux_fsm_state,
		 plpinfo);
  }

  /**********************************************************************
   * Check if the forward exit condition prevail.
   **********************************************************************/
  if ((plpinfo->partner_oper_port_state.synchronization == TRUE) &&
      (plpinfo->lacp_control.selected == SELECTED)) {
    LACP_mux_fsm(E5, 
                 plpinfo->mux_fsm_state,
		 plpinfo);
  }

  if (plpinfo->debug_level & DBG_MUX_FSM) {
     RDBG("%s : exit\n", __FUNCTION__);
  }
}

//******************************************************************
// Function : collecting_state_action
//******************************************************************
static void
collecting_state_action(lacp_per_port_variables_t *plpinfo)
{

  if (plpinfo->debug_level & DBG_MUX_FSM) {
     RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
  }

  plpinfo->actor_oper_port_state.distributing = FALSE;

  enable_collecting(plpinfo);

  if (plpinfo->hw_collecting) {
      plpinfo->actor_oper_port_state.collecting = TRUE;
  } else {
      plpinfo->actor_oper_port_state.collecting = FALSE;
  }

  plpinfo->lacp_control.ntt = TRUE;

  /**********************************************************************
   * call the routine to transmit a LACPdu on the port
   **********************************************************************/
  LACP_async_transmit_lacpdu(plpinfo);


  /**********************************************************************
   * Check if any of the backward exit conditions prevail.
   **********************************************************************/
  if ((plpinfo->lacp_control.selected == UNSELECTED)) {
    LACP_mux_fsm(E2, 
                 plpinfo->mux_fsm_state,
                 plpinfo);
  }

  if ((plpinfo->lacp_control.selected == STANDBY)) {
    LACP_mux_fsm(E4, 
                 plpinfo->mux_fsm_state,
                 plpinfo);
  }


  if (plpinfo->partner_oper_port_state.synchronization == FALSE) {
    LACP_mux_fsm(E6, 
                 plpinfo->mux_fsm_state,
                 plpinfo);
  }

  /**********************************************************************
   * Check if the forward exit condition prevail.
   **********************************************************************/
  if ((plpinfo->lacp_control.selected == SELECTED) &&
      (plpinfo->partner_oper_port_state.synchronization == TRUE) &&
      (plpinfo->partner_oper_port_state.collecting == TRUE) &&
      (plpinfo->actor_oper_port_state.collecting == TRUE)) {

    LACP_mux_fsm(E8, 
                 plpinfo->mux_fsm_state,
                 plpinfo);
  }

  if (plpinfo->debug_level & DBG_MUX_FSM) {
     RDBG("%s : exit\n", __FUNCTION__);
  }
}

//******************************************************************
// Function : collecting_distributing_state_action
//******************************************************************
static void
collecting_distributing_state_action(lacp_per_port_variables_t *plpinfo)
{

  if (plpinfo->debug_level & DBG_MUX_FSM) {
     RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
  }

  plpinfo->actor_oper_port_state.distributing = TRUE;

  enable_distributing(plpinfo);


  plpinfo->lacp_control.ntt = TRUE;

  /**********************************************************************
   * call the routine to transmit a LACPdu on the port
   **********************************************************************/
  LACP_async_transmit_lacpdu(plpinfo);


  /**********************************************************************
   * Check if any of the backward exit conditions prevail.
   **********************************************************************/
  if ((plpinfo->lacp_control.selected == UNSELECTED)) {
    LACP_mux_fsm(E2, 
                 plpinfo->mux_fsm_state,
		 plpinfo);
  }

  if ((plpinfo->lacp_control.selected == STANDBY)) {
    LACP_mux_fsm(E4, 
                 plpinfo->mux_fsm_state,
		 plpinfo);
  }

  if (plpinfo->partner_oper_port_state.synchronization == FALSE) {
    LACP_mux_fsm(E6, 
                 plpinfo->mux_fsm_state,
		 plpinfo);
  }

  if ((plpinfo->partner_oper_port_state.synchronization == TRUE) &&
      (plpinfo->partner_oper_port_state.collecting == FALSE)) {
    LACP_mux_fsm(E9, 
                 plpinfo->mux_fsm_state,
                 plpinfo);
  }

  if (plpinfo->debug_level & DBG_MUX_FSM) {
     RDBG("%s : exit\n", __FUNCTION__);
  }
}

void
start_wait_while_timer(lacp_per_port_variables_t *plpinfo)
{

  plpinfo->wait_while_timer_expiry_counter = AGGREGATE_WAIT_COUNT;
}
void
stop_wait_while_timer(lacp_per_port_variables_t *plpinfo)
{

  plpinfo->wait_while_timer_expiry_counter = 0;
}

//******************************************************************
// Function : disable_collecting_distributing
//            enable_collecting_distributing
//******************************************************************
static void
disable_collecting_distributing(lacp_per_port_variables_t *plpinfo)
{
  (void)mlacp_blocking_send_disable_collect_dist(plpinfo);

  if (plpinfo->debug_level & DBG_MUX_FSM) {
    RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
  }
}

static void
enable_collecting(lacp_per_port_variables_t *plpinfo)
{
  (void)mlacp_blocking_send_enable_collecting(plpinfo);

  if (plpinfo->debug_level & DBG_MUX_FSM) {
    RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
  }
}

static void
enable_distributing(lacp_per_port_variables_t *plpinfo)
{
  (void)mlacp_blocking_send_enable_distributing(plpinfo);

  if (plpinfo->debug_level & DBG_MUX_FSM) {
	  RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
  }
}

//******************************************************************
// Function : attach_mux_to_aggregator
//******************************************************************
int
attach_mux_to_aggregator(lacp_per_port_variables_t *plpinfo)
{
    LAG_t *lag;
    int    status   = R_SUCCESS;


    RENTRY();

    if (plpinfo->debug_level & DBG_MUX_FSM) {
       RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
    }
  // YAGqa36972 : Don't send lport_attach during the reverse transition
  if ((plpinfo->prev_mux_fsm_state == MUX_FSM_COLLECTING_STATE) ||
      (plpinfo->prev_mux_fsm_state == MUX_FSM_COLLECTING_DISTRIBUTING_STATE)) {

     if (plpinfo->debug_level & DBG_MUX_FSM) {
        RDBG("%s : prev_mux_fsm_state is COLLECTING_DISTRIBUTING "
         "and so returning (lport 0x%llx)\n", __FUNCTION__, plpinfo->lport_handle);
     }
     return(status);
  }

    lag = plpinfo->lag;
 /* XXX PI
    if (!lag || !lag->lnk) */
    if (!lag) {

      goto end;
    }

    /* XXX Extract the lag & hand over to SmartTrunk module ?? XXX

    aggr = lag->lnk->aggregator;
    if (!aggr)
        return;
    */


    /*
     * Complete the aggregator initialization by what is now known
     * through protocol hand shaking. 
       PI Done in the blocking send
    
     XXX Ports[port]->Get_IP_Router_MAC(port, &aggr->mac_address);

    memcpy((macaddr_3_t *)(aggr->partner_system_mac_addr),
           lacp_port->partner_oper_system_variables.system_mac_addr,
           MAC_ADDR_LENGTH);
    
    aggr->partner_system_priority =
        lacp_port->partner_oper_system_variables.system_priority;
    */

    status = mlacp_blocking_send_attach_aggregator(plpinfo);
end:
    REXITS(status);
    return status;    
}

//******************************************************************
// Function : detach_mux_from_aggregator
//******************************************************************
int
detach_mux_from_aggregator(lacp_per_port_variables_t *plpinfo)
{
    LAG_t *lag;
    int          status  = R_SUCCESS;

            
    RENTRY();

  if (plpinfo->debug_level & DBG_MUX_FSM) {
     RDBG("%s : lport_handle 0x%llx\n", __FUNCTION__, plpinfo->lport_handle);
  }

    lag = plpinfo->lag;
/* XXX PI    if (!lag || !lag->lnk) */
    if (!lag ) {

      goto end;
    }
    status = mlacp_blocking_send_detach_aggregator(plpinfo);
    if(status == R_SUCCESS) {
      plpinfo->sport_handle = 0;
    }
end:
    REXITS(status);
    return status;    
}
