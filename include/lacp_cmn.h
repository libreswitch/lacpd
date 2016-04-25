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

#ifndef __LACP_CMN_H__
#define __LACP_CMN_H__

#define FALSE         (0)
#define TRUE          (1)
#define R_SUCCESS     (0)

/* MAC Address length in bytes */
#define MAC_BYTEADDR_SIZE  (6)

/* MAC Address string length, function L2_hexmac_to_strmac()
 * needs at least 20 bytes in the buffer */
#define MAC_STRING_ADDR_SIZE  (20)

/* MAC Address length in short int */
#define MAC_SHORTADDR_SIZE (3)

/* MAC address in 3 elements (ushort) */
typedef unsigned short        macaddr_3_t[MAC_SHORTADDR_SIZE];

/* MAC address in 6 elements (bytes)  */
typedef unsigned char         macaddr_6_t[MAC_BYTEADDR_SIZE];

/* Aggregation Key defines */
#define AGG_KEY_MAX_LENGTH  (6)

/* LAG Port Name prefix */
#define LAG_PORT_NAME_PREFIX "lag"
#define LAG_PORT_NAME_PREFIX_LENGTH (3)

/* LAG ID String, for debugging purposes, it has the following format: */
/* [(<local_system_priority>, <local_system_mac_addr>, <local_port_key>,
 * <local_port_priority>, <local_port_number>),
 * (<remote_system_priority>, <remote_system_mac_addr>, <remote_port_key>,
 * <remote_port_priority>, <remote_port_number>)] */
#define LAG_ID_STRING_SIZE (80)


/******************************************************************************************/
/**                             MsgLib related                                           **/
/******************************************************************************************/

/* Msg Sender ID */
#define ml_timer_index   0x11
#define ml_lport_index   0x22
#define ml_rx_pdu_index  0x33
#define ml_cfgMgr_index  0x44

/******************************************************************************************/
/**                             ML_event & related                                       **/
/******************************************************************************************/
struct ML_event;         /* Forward declaration. */

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
    int dummy;    // zero-length structs make me nervous
};

/* For msglib internals only. Pay no attention to the code inside that curtain. */
struct ML_event_internal {
    int do_free;
    struct ML_protocol *protocol;
    struct ML_protocol_version *protocol_version;
    struct ML_peer *peer;
    ML_peer_callback_func_t callback; // per-message callback, not per-peer
    void *callback_data;
};

struct ML_peer_instance {
    int peer;
    int instance;
    int lifetime;
    struct ML_version version; // Version of message by the sender
};

enum {
    ML_event_flags_donot_free = (1 << 0),
};

/* The event structure handed to the application in ml_get_next_event. */
typedef struct ML_event {
    int flags;
    struct ML_event_info info;
    struct ML_event_internal internal;
    struct ML_peer_instance sender;
    int serial;
    int replyto;
    int msgnum;         // enum MLm_$protocol
    void *msg;          // struct MLt_$protocol__$type
} ML_event;


/******************************************************************************************/
/**                             Misc Utilities...                                        **/
/******************************************************************************************/
extern enum PM_lport_type speed_to_lport_type(int speed);
extern int lport_type_to_speed(enum PM_lport_type ptype);

#endif  // __LACP_CMN_H__
