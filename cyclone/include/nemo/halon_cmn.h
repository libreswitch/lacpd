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

/*
 * halon_cmn.h
 *
 *   This file houses all defines needed by Cyclone code that we
 *   do not want to pull in from the rest of Cyclone source base.
 */
#ifndef __HALON_CMN_H__
#define __HALON_CMN_H__

#include <nemo/pm/pm_cmn.h>

#ifndef ulong
#define ulong unsigned long
#endif

#ifndef u_long
#define u_long unsigned long
#endif

#ifndef uchar
#define uchar unsigned char
#endif

#ifndef u_char
#define u_char unsigned char
#endif

/******************************************************************************************/
/**                             MsgLib related                                           **/
/******************************************************************************************/

// Msg Sender ID (instead of Cyclone msgLib infrastructure)
#define ml_timer_index   0x11
#define ml_lport_index   0x22
#define ml_rx_pdu_index  0x33
#define ml_cfgMgr_index  0x44
#define ml_showMgr_index 0x55
#define ml_diagMgr_index 0x66

#define MSGLIB_INVALID_INDEX (-1)

/******************************************************************************************/
/**                             ML_event & related                                       **/
/******************************************************************************************/
#define ML_MAX_MSG_SIZE  4096

// Forward declaration
struct ML_event;

typedef void (*ML_peer_callback_func_t)(struct ML_event *event, void *data);

struct ML_version {
    short major;
    short minor;
};

struct ML_protocol_version {
    struct ML_version version;
    struct ML_protocol *p;
    int num_elements; struct ML_element *elements;
    int num_types; struct ML_type *types;
    int num_messages; struct ML_message *messages;
    int num_enum_types; struct ML_enum_type *enum_types;
};

struct ML_protocol {
    char *name; char *md5;
    int num_versions; struct ML_version *versions;  // pointer to version table
    struct ML_protocol_version *protocol_versions;  // versions supported for this protocol.
};

struct ML_event_info {
    int dummy;                          // zero-length structs make me nervous
};

// For msglib internals only. Pay no attention to the code inside that
// curtain.
struct ML_event_internal {
    int do_free; // should ml_event_free() really free the ML_event ?
    struct ML_protocol *protocol;
    struct ML_protocol_version *protocol_version;
    struct ML_peer *peer;
    ML_peer_callback_func_t callback; // per-message callback, not per-peer
    void *callback_data;
};

struct ML_peer_instance {
    // Keep Order same as that of struct MLt_include_msglib_common__peer_instance
    int peer; // 0 for special: protocol is "msglib"
    int instance;
    int lifetime;
    struct ML_version version; // Version of message by the sender
};

enum {
    ML_event_flags_donot_free = (1 << 0),
};

// The event structure handed to the application in ml_get_next_event.
typedef struct ML_event {
    int flags;
    struct ML_event_info info;
    struct ML_event_internal internal;
    struct ML_peer_instance sender;
    int serial;
    int replyto;
    int msgnum; // enum MLm_$protocol
    void *msg; // struct MLt_$protocol__$type
    //char msgBody[4096]; // Halon: *msg will point to here...
} ML_event;


/******************************************************************************************/
/**                             Timer stuff...                                           **/
/******************************************************************************************/
struct MLt_msglib__timer {
    int timer_index;
    int data;
};

/******************************************************************************************/
/**                             Misc Utilities...                                        **/
/******************************************************************************************/
extern int speed_str_to_speed(char *cfg_speed);
extern enum PM_lport_type speed_to_lport_type(int speed);
extern int lport_type_to_speed(enum PM_lport_type ptype);

#endif  // __HALON_CMN_H__
