High level design of ops-lacpd
==============================

The ops-lacpd process manages link aggregation (see [link aggregation design](https://www.openswitch.net/documents/dev/link_aggregation_design) for feature design information).

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
  port:other_config
    lacp-system-id
      -> user configuration: override system id for this LAG
    lacp-system-priority
      -> user configuration: override system priority for this LAG
    lacp-time
      -> user configuration: "fast" or "slow" LACP heartbeats

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
  interface:other_config
    lacp-port-id
      -> user configuration: port identifier override
    lacp-port-priority
      -> user configuration: port priority override
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

References
----------
* [link aggregation design](https://www.openswitch.net/documents/dev/link_aggregation_design)
