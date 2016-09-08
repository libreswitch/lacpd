High level design of ops-lacpd
==============================

The ops-lacpd process manages link aggregation (see [link aggregation design](/documents/user/link_aggregation_design) for feature design information).

Responsibilities
----------------
The ops-lacpd process is responsible for managing all Link Aggregation Groups (LAGs) defined by the user. This includes both statically defined LAGs as well as LAGs where the LACP protocol is used to negotiate link membership.

Design choices
--------------
N/A

Relationships to external OpenSwitch entities
---------------------------------------------
```ditaa
+-------------+      +-------------+       +----+
|             +------>L2 interfaces+-------+PEER|
|             |      +-------------+       +----+
| ops-lacpd   |
|             |
|             |
|             |
|             |       +--------------+
|             +------->              |
+-------------+       |              |
                      |   database   |
                      |              |
+-------------+       |              |
|             +------->              |
|             |       |              |
|             |       +--------------+
|  switchd    |
|             |       +--------+
|             +------->        |
|             |       | SWITCH |
+-------------+       |        |
                      +--------+

```
The ops-lacpd process monitors the Port and Interface table in the database. As the configuration and state information for the ports and interfaces are changed, ops-lacpd examines the data to determine if there are new LAGs being defined in the configuration and if state information for interfaces has changed. The ops-lacpd process uses this information to configure static LAG membership and update the LACP protocol state machines.

The ops-lacpd process registers for LACP protocol packets on all L2 interfaces configured for LACP, and sends and receives state information to the peer LACP instance through the interfaces.

When the state information maintained by ops-lacpd changes, it updates the information in the database. Some of this information is strictly status, but this also includes hardware configuration, which is used by switchd to configure the switch.

OVSDB-Schema
------------
The following OpenSwitch database schema elements are referenced or set by ops-lacpd:
```
system table
  system:cur_cfg
    -> 1 if all hardware daemons have completed initialization
  system:system_mac
    -> system id in the absence of system or port-level override configuration
  system:lacp_config
    lacp-system-id
      -> user configuration: override the system id value
    lacp-system-priority
      -> user configuration: specifies the system priority

port table
  port:name
    -> the name of the port (potential LAG)
  port:lacp
    -> user configuration: active or passive if LACP is configured for port
  port:interfaces
    -> user configuration: list of interfaces configured in port
       (multiple interfaces implies that the port is a LAG)
  port:lacp_status
    -> written with the current status of LACP negotiation (if configured)
    bond_speed
      -> link speed of interfaces operating in LAG
    bond_status
      -> "ok", "down", or "defaulted"
    bond_status_reason
      -> string specifying why the bond_status value is "down"
  port:bond_status
    -> summarizes the LAG status according to the bond_status of the member
       interfaces.
    state
      -> "up", "blocked" or "down" to specify general LAG status.
  port:other_config
    lacp-system-id
      -> user configuration: override system id for this LAG
    lacp-system-priority
      -> user configuration: override system priority for this LAG
    lacp-time
      -> user configuration: "fast" or "slow" LACP heartbeats
    lacp-fallback
      -> user configuration: "true" or "false" to specify whether fallback
         mode is active or not.
    lacp_fallback_mode
      -> user configuration: "priority" or "all\active" to specify fallback
         mode of operation.
    lacp_fallback_timeout
      -> user configuration: specifies the time in seconds during which
         fallback is active.

  port:bond_status
    -> Key-value pairs that report bond-specific port status (both static and
       dynamic)
    state
      -> Enumeration summarizing the forwarding state of the bond. Values are:
         "up", "blocked" or "down".
    bond_speed
      -> link speed in bps of interfaces operating in LAG.

interface table
  interface:name
    -> the name of the interface
  interface:type
    -> lacpd ignores "internal" interfaces
  interface:duplex
    -> duplex state of interface (if linked)
  interface:link_state
    -> link state of interface (must be up to participate)
  interface:link_speed
    -> link speed of interface
  interface:hw_bond_config
    -> ops-lacpd writes to tell switchd how to configure LAG
    bond_hw_handle
      -> LAG identifier (allows switchd to group interfaces in the same LAG)
    rx_enabled
      -> interface is attached to a LAG and receive logic is enabled
    tx_enabled
      -> transmit logic is enabled for this interface
  interface:hw_intf_info
    switch_intf_id
      -> unique identifier for interface in switch
  interface:lacp_current
    -> ops-lacpd sets to "true" if it has current LACP partner information
  interface:lacp_status
    -> ops-lacpd writes with LACP protocol negotiation information
    actor_system_id
      -> system id in use for interface
    actor_port_id
      -> port id in use for interface
    actor_key
      -> LAG identifier
    actor_state
      -> protocol state values
    partner_system_id
      -> system id in use by peer
    partner_port_id
      -> port id in use by peer
    partner_key
      -> peer LAG identifier
    partner_state
      -> peer protocol state values
  interface:bond_status
    -> summarizes the bond state of the aggregated link.
    state
      -> "up", "blocked" or "down".
  interface:other_config
    lacp-port-id
      -> user configuration: port identifier override
    lacp-port-priority
      -> user configuration: port priority override
  interface:bond_status
    -> Key-value pairs that report the status of a bond (both static and
       dynamic)
    state
      -> Enumeration indicating the forwarding state of the interface when it is
         configured as a member of a LAG. Values are: "up", "blocked" or "down".
```

Internal structure
------------------
The ops-lacpd process has three operational threads:
* ovs_if_thread
  This thread processes the typical OVSDB main loop, and handles any changes. Some changes are handled by passing messages to the lacpd_thread thread.
* lacpd_thread
  This thread processes messages sent to it by the other two threads. Processing of the messages includes operating the finite state machines.
* lacpdu_rx_thread
  This thread waits for LACP packets on interfaces. When a packet is received, it sends a message (including the packet data) to the lacpd_thread thread for processing through the state machines.

The ops-lacpd process can be logically divided into two parts:
* static LAG operation
  Determines LAG interface membership based on configuration and interface status (e.g., interfaces in a LAG must have the same speed and duplex values).
* LACP protocol operation
  Determines LAG interface membership based on configuration and LACP protocol negotiation.

Supportabilty
------------------
It is possible to get the current daemon state and information using ovs-appctl
functionality. The commands available are the following:

* ovs-appctl -t ops-lacpd lacpd/dump <interface/port>:
  Shows the link state (up or down), link speed and duplex for each interface
  of the switch. It also shows the lacp mode (active, passive, off), lag member
  speed, configured, eligible and participant interface members, as well as the
  interface count for each port defined in the switch.
```
# ovs-appctl -t ops-lacpd lacpd/dump
================ Interfaces ================
Interface 7:
    link_state           : down
    link_speed           : 0 Mbps
    duplex               : half
    .
    .
    .
Interface 53-4:
    link_state           : down
    link_speed           : 0 Mbps
    duplex               : half
================ Ports ================
Port lag2:
    lacp                 : active
    lag_member_speed     : 1000
    configured_members   : 5 4
    eligible_members     : 5 4
    participant_members  : 5 4
    interface_count      : 2
Port 1:
    lacp                 : off
    lag_member_speed     : 1000
    configured_members   : 1
    eligible_members     : 1
    participant_members  :
    interface_count      : 0
Port bridge_normal:
    lacp                 : off
    lag_member_speed     : 0
    configured_members   : bridge_normal
    eligible_members     :
    participant_members  :
    interface_count      : 0
```

* ovs-appctl -t ops-lacpd lacpd/getlacpinterfaces <lag_name>:
  Shows the configured, eligible and participant interface members of all the
  LAGs in the system or for a specific given LAG.

```
# ovs-appctl -t ops-lacpd lacpd/getlacpinterfaces
Port lag2:
    configured_members   : 5 4
    eligible_members     : 5 4
    participant_members  : 5 4
Port lag10:
    configured_members   : 3 2
    eligible_members     : 3 2
    participant_members  : 3 2
```

* ovs-appctl -t ops-lacpd lacpd/getlacpcounters <lag_name>:
  Shows the amount of PDUs and marker PDUs sent and received by each interface
  configured as member of one LAG for all the dynamic LAGs in the system or for
  aspecific given dynamic LAG.

```
# ovs-appctl -t ops-lacpd lacpd/getlacpcounters
LAG lag2:
 Configured interfaces:
  Interface: 5
    lacp_pdus_sent: 9
    marker_response_pdus_sent: 0
    lacp_pdus_received: 5
    marker_pdus_received: 0
  Interface: 4
    lacp_pdus_sent: 8
    marker_response_pdus_sent: 0
    lacp_pdus_received: 6
    marker_pdus_received: 0
LAG lag10:
 Configured interfaces:
  Interface: 3
    lacp_pdus_sent: 43
    marker_response_pdus_sent: 0
    lacp_pdus_received: 40
    marker_pdus_received: 0
  Interface: 2
    lacp_pdus_sent: 43
    marker_response_pdus_sent: 0
    lacp_pdus_received: 41
    marker_pdus_received: 0
```

* ovs-appctl -t ops-lacpd lacpd/getlacpstate <lag_name>:
  Shows the LACP state machine state for each dyanmic LAG in the system or for
  a specific given dynamic LAG. For each interface configured as member of the
  LAG it shows the actor and the partner state:
  - lacp_activity: lacp mode, 0 means passive, 1 means active.
  - time_out: lacp timeout, 0 means slow, 1 means fast.
  - aggregation: if the interface is aggregable (1) or not (0).
  - sync: if the interface is synchronized (1) or not (0).
  - collecting: if the interface is in collecting state (1) or not (0).
  - distributing: if the interface is in distributing state (1) or not (0).
  - defaulted: if the interface is in defaulted state (1) or not (0).
  - expired: if the interface is in expired state (1) or not (0).

  It also shows lacp control variables:
  - begin: 1 indicates initialization or reinitialization of the LACP protocol.
  - actor_churn: 1 indicates that the Actor Churn Detection machine has detected
    that a local Aggregation Port configuration has failed to converge within a
    specified time.
  - partner_churn: 1 indicates that the Partner Churn Detection machine has
    detected that a remote Aggregation Port configuration has failed to
    converge within a specified time.
  - ready_n: 1 if the timer has expired and MUX machine is waiting to attach an
    aggregator, zero otherwise.
  - selected: 1 means that the selection Logic has selected an appropriate
    aggregator, zero indicates that no aggregator is currently selected.
  - port_moved: 1 if the Receive machine for an Aggregation Port is in the
    PORT_DISABLED state, and the combination of Partner_Oper_System and
    Partner_Oper_Port_Number in use by that Aggregation Port has been received
    in an incoming LACPDU on a different Aggregation Port. It is zero once the
    INITIALIZE state of the Receive machine has set the Partner information
    for the Aggregation Port to administrative default values.
  - ntt: 1 indicates there is need to transmit something to the partner,
    zero otherwise.
  - port_enabled: 1 if the link has been established and the Aggregation
    Port is operable, zero otherwise.

```
# ovs-appctl -t ops-lacpd lacpd/getlacpstate
LAG lag2:
 Configured interfaces:
  Interface: 5
    actor_oper_port_state
       lacp_activity:1 time_out:0 aggregation:1 sync:1 collecting:1 distributing:1 defaulted:0 expired:0
    partner_oper_port_state
       lacp_activity:1 time_out:0 aggregation:1 sync:1 collecting:1 distributing:1 defaulted:0 expired:0
    lacp_control
       begin:0 actor_churn:0 partner_churn:0 ready_n:1 selected:1 port_moved:0 ntt:0 port_enabled:1
  Interface: 4
    actor_oper_port_state
       lacp_activity:1 time_out:0 aggregation:1 sync:1 collecting:1 distributing:1 defaulted:0 expired:0
    partner_oper_port_state
       lacp_activity:1 time_out:0 aggregation:1 sync:1 collecting:1 distributing:1 defaulted:0 expired:0
    lacp_control
       begin:0 actor_churn:0 partner_churn:0 ready_n:1 selected:1 port_moved:0 ntt:0 port_enabled:1
LAG lag10:
 Configured interfaces:
  Interface: 3
    actor_oper_port_state
       lacp_activity:1 time_out:1 aggregation:1 sync:1 collecting:1 distributing:1 defaulted:0 expired:0
    partner_oper_port_state
       lacp_activity:1 time_out:1 aggregation:1 sync:1 collecting:1 distributing:1 defaulted:0 expired:0
    lacp_control
       begin:0 actor_churn:0 partner_churn:0 ready_n:1 selected:1 port_moved:0 ntt:0 port_enabled:1
  Interface: 2
    actor_oper_port_state
       lacp_activity:1 time_out:1 aggregation:1 sync:1 collecting:1 distributing:1 defaulted:0 expired:0
    partner_oper_port_state
       lacp_activity:1 time_out:1 aggregation:1 sync:1 collecting:1 distributing:1 defaulted:0 expired:0
    lacp_control
       begin:0 actor_churn:0 partner_churn:0 ready_n:1 selected:1 port_moved:0 ntt:0 port_enabled:1
```

References
----------
* [link aggregation design](/documents/user/link_aggregation_design)
