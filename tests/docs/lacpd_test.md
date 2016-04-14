# lacpd Test Cases

# Contents
- [Static LAG Membership](#static-lag-membership)
  - [Objective](#objective)
  - [Requirements](#requirements)
  - [Setup](#setup)
    - [Topology Diagram](#topology-diagram)
  - [Description](#description)
  - [Test Result Criteria](#test-result-criteria)
    - [Test Pass Criteria](#test-pass-criteria)
    - [Test Fail Criteria](#test-fail-criteria)
- [Static LAG Membership Exclusion Rules](#static-lag-membership-exclusion-rules)
  - [Objective](#objective)
  - [Requirements](#requirements)
  - [Setup](#setup)
    - [Topology Diagram](#topology-diagram)
  - [Description](#description)
  - [Test Result Criteria](#test-result-criteria)
    - [Test Pass Criteria](#test-pass-criteria)
    - [Test Fail Criteria](#test-fail-criteria)
- [LACP User Configuration](#lacp-user-configuration)
  - [Objective](#objective)
  - [Requirements](#requirements)
  - [Setup](#setup)
    - [Topology Diagram](#topology-diagram)
  - [Description](#description)
  - [Test Result Criteria](#test-result-criteria)
    - [Test Pass Criteria](#test-pass-criteria)
    - [Test Fail Criteria](#test-fail-criteria)
- [Transferring interface to another LAG](#transferring-interface-to-another-lag)
  - [Objective](#objective)
  - [Requirements](#requirements)
  - [Setup](#setup)
    - [Topology Diagram](#topology-diagram)
  - [Test Setup](#test-setup)
  - [Description](#description)
  - [Test Result Criteria](#test-result-criteria)
    - [Test Pass Criteria](#test-pass-criteria)
    - [Test Fail Criteria](#test-fail-criteria)
- [LAG is created only with one other LAG at the same time](#lag-is-created-only-with-one-other-lag-at-the-same-time)
  - [Objective](#objective)
  - [Requirements](#requirements)
  - [Setup](#setup)
    - [Topology Diagram](#topology-diagram)
  - [Test Setup](#test-setup)
  - [Description](#description)
  - [Test Result Criteria](#test-result-criteria)
    - [Test Pass Criteria](#test-pass-criteria)
    - [Test Fail Criteria](#test-fail-criteria)
- [LAG created with cross links with same aggregation key](#lag-created-with-cross-links-with-same-aggregation-key)
  - [Objective](#objective)
  - [Requirements](#requirements)
  - [Setup](#setup)
    - [Topology Diagram](#topology-diagram)
  - [Test Setup](#test-setup)
  - [Description](#description)
  - [Test Result Criteria](#test-result-criteria)
    - [Test Pass Criteria](#test-pass-criteria)
    - [Test Fail Criteria](#test-fail-criteria)
- [LAG created with different aggregation key](#lag-created-with-different-aggregation-key)
  - [Objective](#objective)
  - [Requirements](#requirements)
  - [Setup](#setup)
    - [Topology Diagram](#topology-diagram)
  - [Test Setup](#test-setup)
  - [Description](#description)
  - [Test Result Criteria](#test-result-criteria)
    - [Test Pass Criteria](#test-pass-criteria)
    - [Test Fail Criteria](#test-fail-criteria)
- [LACP port priority](#LACP-port-priority)
  - [Objective](#objective)
  - [Requirements](#requirements)
  - [Setup](#setup)
    - [Topology Diagram](#topology-diagram)
  - [Test Setup](#test-setup)
  - [Description](#description)
  - [Test Result Criteria](#test-result-criteria)
    - [Test Pass Criteria](#test-pass-criteria)
    - [Test Fail Criteria](#test-fail-criteria)
- [LACP partner priority](#LACP-partner-priority)
  - [Objective](#objective)
  - [Requirements](#requirements)
  - [Setup](#setup)
    - [Topology Diagram](#topology-diagram)
  - [Test Setup](#test-setup)
  - [Description](#description)
  - [Test Result Criteria](#test-result-criteria)
    - [Test Pass Criteria](#test-pass-criteria)
    - [Test Fail Criteria](#test-fail-criteria)
- [LACP system priority](#LACP-system-priority)
  - [Objective](#objective)
  - [Requirements](#requirements)
  - [Setup](#setup)
    - [Topology Diagram](#topology-diagram)
  - [Test Setup](#test-setup)
  - [Description](#description)
  - [Test Result Criteria](#test-result-criteria)
    - [Test Pass Criteria](#test-pass-criteria)
    - [Test Fail Criteria](#test-fail-criteria)
- [Dynamic LAG created to disable all ports or to fallback](#dynamic-lag-created-to-disable-all-ports-or-to-fallback)
  - [Objective](#objective)
  - [Requirements](#requirements)
  - [Setup](#setup)
    - [Topology Diagram](#topology-diagram)
  - [Test Setup](#test-setup)
  - [Description](#description)
  - [Test Result Criteria](#test-result-criteria)
    - [Test Pass Criteria](#test-pass-criteria)
    - [Test Fail Criteria](#test-fail-criteria)
- [LAG ovs-appctl command getlacpinterfaces format](#lag-ovs-appctl-command-getlacpinterfaces-format)
  - [Objective](#objective)
  - [Requirements](#requirements)
  - [Setup](#setup)
    - [Topology Diagram](#topology-diagram)
  - [Test Setup](#test-setup)
  - [Description](#description)
  - [Test Result Criteria](#test-result-criteria)
    - [Test Pass Criteria](#test-pass-criteria)
    - [Test Fail Criteria](#test-fail-criteria)
- [LACP ovs-appctl command getlacpcounters format](#lacp-ovs-appctl-command-getlacpcounters-format)
  - [Objective](#objective)
  - [Requirements](#requirements)
  - [Setup](#setup)
    - [Topology Diagram](#topology-diagram)
  - [Test Setup](#test-setup)
  - [Description](#description)
  - [Test Result Criteria](#test-result-criteria)
    - [Test Pass Criteria](#test-pass-criteria)
    - [Test Fail Criteria](#test-fail-criteria)
- [LACP ovs-appctl command getlacpstate format](#lacp-ovs-appctl-command-getlacpinterfaces-format)
  - [Objective](#objective)
  - [Requirements](#requirements)
  - [Setup](#setup)
    - [Topology Diagram](#topology-diagram)
  - [Test Setup](#test-setup)
  - [Description](#description)
  - [Test Result Criteria](#test-result-criteria)
    - [Test Pass Criteria](#test-pass-criteria)
    - [Test Fail Criteria](#test-fail-criteria)


## Static LAG Membership
### Objective
Verify that configured interfaces are included or excluded from a static LAG depending on their link status.
### Requirements
 - Virtual Mininet Test Setup
 - Script is in tests/test_lacp_ct_lag_config.py

### Setup
#### Topology Diagram
```
[s1] <--> [s2]
```

### Description
1. Create LAGs with 1 Gb, 10 Gb, and 40 Gb interfaces.
   Verify that interfaces are not operating in LAGs (because they are not enabled)
* Enable all the interfaces
   Verify that interfaces are operating in LAGs (because they have been enabled and are linked)
* Remove an interface from a LAG
   Verify that removed interface is not operating in LAG (because it has been removed from configuration)
* Add interface back to LAG
   Verify that added interface is operating in LAG
* Remove all but two interfaces from a LAG
   Verify that remaining interfaces are still in LAG
   Verify that removed interfaces are not in LAG
* Disable one of two interfaces in LAG
   Verify that removed interface is not in LAG
* Enable interface in LAG
   Verify that interface is in LAG
* Clear all user configuration

### Test Result Criteria
#### Test Pass Criteria
All verifications succeed.
#### Test Fail Criteria
One or more verifications fail.

## Static LAG Membership Exclusion Rules
### Objective
Verify that interfaces that violate one or more LAG exclusion rules do not operate in LAGs.
### Requirements
 - Virtual Mininet Test Setup
 - Script is in tests/test_lacp_ct_lag_config.py

### Setup
#### Topology Diagram
```
[s1] <--> [s2]
```

### Description
1. Enable 1 Gb and 10 Gb interfaces
* Create a LAG with 1 Gb interfaces
* Add a 10 Gb interfaces to LAG
   Verify that 1 Gb interfaces are operating in LAG
   Verify that 10 Gb interfaces are not operating in LAG (mismatched speed)
* Remove 1 Gb interfaces from LAG
   Verify that 10 Gb interfaces are operating in LAG (no 1 Gb links remain)
* Clear all user configuration

### Test Result Criteria
#### Test Pass Criteria
All verifications succeed.
#### Test Fail Criteria
One or more verifications fail.

## LACP User Configuration
### Objective
Verify that lacpd is processing LACP configuration and protocol correctly.
### Requirements
 - Virtual Mininet Test Setup
 - Script is in tests/test_lacp_ct_lag_config.py

### Setup
#### Topology Diagram
```
[s1] <--> [s2]
```

### Description
1. Get system\_mac (system:system\_mac) for s1 and s2
* Create a LACP LAG on s1 and s2 (connected interfaces)
    * Apply aggregation key for both lags
    * Verify lacp\_status in interfaces configured in LACP LAGs (LAGs are formed and are using default system\_mac values)
* Change lacp-time to slow
    * Validate heartbeat is slow now
* Change lacp-time to fast
    * Validate heartbeat rate is fast now
* Override system parameters on s1 and s2 (set system:lacp\_config lacp-system-id and lacp-system-priority)
    * Verify lacp\_status in interfaces configured in LACP LAGs (LAGs are formed and are using newly specified [system] override values)
* Set port:other\_config lacp-system-id and lacp-system-priority to override system-level override
    * Verify lacp\_status in interfaces configured in LACP LAGs (LAGs are formed and are using newly specified [port] override values)
* Delete and recreate LACP LAG on s1 and s2
    * Verify lacp\_status in interfaces configured in LACP LAGs (LAGs are formed and are using [system] override values)
* Delete LAGs
* Create a LACP LAG on s1 and s2 using 1 Gb interfaces
    * Set lacp-time to fast
    * Both lags are active
    * Verify that interfaces are linked and operating at 1 Gb
    * Verify that interfaces are operating in LAG
* Override system parameters on s1 (set system:lacp\_config lacp-system-id and lacp-system-priority)
    * Verify that system-level overrides are used in interfaces
* Clear system parameters on s1
    * Verify that default values for system id and system priority are used in interfaces
* Set invalid system:lacp\_config values
    * Verify that invalid values are ignored
* Change lacp in port from "active" to "passive"
    * Verify that interface:lacp\_status actor\_state has Active:0
* Change lacp in port from "passive" to "off"
    * Verify interface:lacp\_status actor\_state is empty (LACP is disabled)
* Change lacp in port from "off" to "active"
    * Verify that interface:lacp\_status is correct (Active:1)
* Set port:other\_config lacp-timer to "slow"
    * Verify that TmOut in interface:actor\_state is 0
* Set port:other\_config lacp-timer to "fast"
    * Verify that TmOut in interface:actor\_state is 1
* Set aggregation key for s1
    * Validate actor_key changed
    * Verify interface is not part of any LAG
* Get interface back to the correct LAG by changing the aggregation key aggregation
* Set interface:other\_config lacp-port-id and lacp-port-priority
    * Verify that interface:lacp\_status actor\_port\_id uses new values
* Set interface:other\_config lacp-port-id and lacp-port-priority to invalid values
    * Verify that interface:lacp\_status actor\_port\_id uses default values (ignores invalid values)
* Remove interface:other\_config lacp-port-id and lacp-port-priority values
    * Verify that interface:lacp\_status actor\_port\_id uses default values
    * Verify that port:lacp\_status bond\_speed is 1000 and bond\_status is "ok"
    * Verify interface:lacp\_status values are correct
* Set system:lacp\_config lacp-system-id and lacp-system-priority
    * Verify that interfaces are using system-level overrides
* Create a new LAG (lag1) on s2
* Enable interfaces in lag1
    * Verify interface:lacp\_status values in lag1 are using system-level overrides
* Set lag0:other\_config lacp-system-id and lacp-system-priority
    * Verify that lag0 interfaces are using port-level overrides
    * Verify that lag1 interfaces are using system-level overrides
* Add interface to lag0
    * Verify that added interface is using port-level overrides
* Clear port-level overrides from lag0
    * Verify that lag0 interfaces are using system-level overrides

### Test Result Criteria
#### Test Pass Criteria
All verifications succeed.
#### Test Fail Criteria
One or more verifications fail.

## Transferring interface to another LAG
### Objective
Transferring an interface to another LAG without passing the interface on the other side of the link cause the link to get in state Out of Sync and not Collecting/Distributing
### Requirements
 - Virtual Mininet Test Setup
 - Script is in tests/test_ct_aggregation_key.py

### Setup
#### Topology Diagram
```
+--------+                  +--------+
|        1------------------2        |
|   s1   |                  |   s2   |
|        2------------------2        |
+--------+                  +--------+
```
### Test Setup
```
 Switch 1
   LAG 100:
       Interface 1
       Interface 2

 Switch 2
   LAG 100:
       Interface 1
       Interface 2
```

### Description
1. Enable all interfaces use for test
* Create LAG 100 with interfaces 1,2 in both switches
  * Apply same aggregation key in both LAGs
  * Validate LAG has state Sync and Collecting/Distributing in both end of links
* Delete LAG 100 from switch 1
* Create LAG 200 with interfaces 1 and 8 in switch 1
  * Apply aggregation key 200 in interface 1 and 8
* Recreate LAG 100 with interfaces 2 and 9 in switch 1
  * Apply aggregation key 100 to interfaces 2 and 9
* Verify interface 1 is Out of Sync and not in Collecting/Distributing in both switches
* Clean configuration

### Test Result Criteria
#### Test Pass Criteria
Interface 1 should be in state Out of Sync and not in Collecting/Distributing in both switches
#### Test Fail Criteria
Interface 1 stays In Sync or in Collecting/Distributing

## LAG is created only with one other LAG at the same time
### Objective
The switch is creating the LAG with only one type of partner key, interfaces inside the same LAG can't have two different partner keys associated.
### Requirements
 - Virtual Mininet Test Setup
 - Script is in tests/test_ct_aggregation_key.py

### Setup
#### Topology Diagram
```
+------------+
|            |
|     s1     |
|            |
+-1--2--3--4-+
  |  |  |  |
  |  |  |  |
  |  |  |  |
  |  |  |  |
+-1--2--3--4-+
|            |
|     s2     |
|            |
+------------+
```
### Test Setup
```
 Switch 1
   LAG 50:
       Interface 1
       Interface 2
       Interface 3
       Interface 4

 Switch 2
   LAG 50:
       Interface 1
       Interface 2
   LAG 60:
       Interface 3
       Interface 4
```

### Description
1. Enable all interfaces use for test
* Create LAG 50 with interfaces 1-4 in switch 1
  * Apply aggregation key 50 to interfaces 1-4
* Create LAG 50 with interfaces 1 and 2 in switch 2
  * Apply aggregation key 50 to interfaces 1 and 2
* Create LAG 60 with interfaces 3 and 4 in switch 2
  * Apply aggregation key 60 to interfaces 3 and 4
* Validate state is In Sync and Collecting/Distributing for interfaces 1 and 2 in both switches
* Validate state is Out of Sync and not in Collecting/Distributing for interfaces 3 and 4 in switch 1
* Validate state is Sync but not in Collecting/Distributing for interface 3 and 4 in switch 2
* Clean configuration

### Test Result Criteria
#### Test Pass Criteria
* In both switches, interfaces 1 and 2 should be In Sync and Collecting/Distributing
* Interfaces 3 and 4 for switch 1 should be Out of Sync and not in Collecting/Distributing
* Interfaces 3 and 4 for switch 2 should be In Sync and not in Collecting/Distributing
#### Test Fail Criteria
* Interfaces 1 or 2 are Out of Sync
* Interfaces 3 and 4 are In Sync for switch 1
* Interfaces 3 and 4 are Out of Sync for switch 2

## LAG created with cross links with same aggregation key
### Objective
LAGs should be formed independent of port ids as long as aggregation key is the same
### Requirements
 - Virtual Mininet Test Setup
 - Script is in tests/test_ct_aggregation_key.py

### Setup
#### Topology Diagram
```
+--------------------+
|                    |
|         s1         |
|                    |
+-1--2--3----5--6--7-+
  |  |  |    |  |  |
  |  |  |    |  |  |
  |  |  |    |  |  |
  |  |  |    |  |  |
+-1--2--3----6--7--5-+
|                    |
|         s2         |
|                    |
+--------------------+
```
### Test Setup
```
 Switch 1
   LAG 150:
       Interface 5
       Interface 1
   LAG 250:
       Interface 6
       Interface 2
   LAG 350:
       Interface 7
       Interface 3

 Switch 2
   LAG 150:
       Interface 6
       Interface 1
   LAG 250:
       Interface 7
       Interface 2
   LAG 350:
       Interface 5
       Interface 3
```

### Description
1. Enable all interfaces use for test
* Create LAG 150 in switch 1 in active mode with interfaces 5 and 1
  * Apply aggregation key 150 to interfaces 5 and 1
* Create LAG 150 in switch 2 in active mode with interfaces 6 and 1
  * Apply aggregation key 150 to interfaces 6 and 1
* Create LAG 250 in switch 1 in active mode with interfaces 6 and 2
  * Apply aggregation key 250 to interfaces 6 and 2
* Create LAG 250 in switch 2 in active mode with interfaces 7 and 2
  * Apply aggregation key 250 to interfaces 7 and 2
* Create LAG 350 in switch 1 in active mode with interfaces 7 and 3
  * Apply aggregation key 350 to interfaces 7 and 3
* Create LAG 350 in switch 2 in active mode with interfaces 5 and 3
  * Apply aggregation key 350 to interfaces 5 and 3
* Validate interfaces from 1-3 and 5-7 are In Sync and Collecting/Distributing in both switches
* Clean configuration

### Test Result Criteria
#### Test Pass Criteria
* All interfaces in both switches should be in state In Sync and Collecting/Distributing
#### Test Fail Criteria
* Any interfaces is not In Sync or in Collecting/Distributing state

## LAG created with different aggregation key
### Objective
When the aggregation key is different on both sides of a LAG, it should be formed as long as it is the same through all members of a single switch
### Requirements
 - Virtual Mininet Test Setup
 - Script is in tests/test_ct_aggregation_key.py

### Setup
#### Topology Diagram
```
+--------+                  +--------+
|        1------------------2        |
|   s1   |                  |   s2   |
|        2------------------2        |
+--------+                  +--------+
```
### Test Setup
```
 Switch 1
   LAG 100:
       Interface 1
       Interface 2

 Switch 2
   LAG 200:
       Interface 1
       Interface 2
```

### Description
1. Enable all interfaces use for test
* Create LAG 100 with active mode with interfaces 1 and 2 in switch 1
  * Apply aggregation key 100 to LAG
* Create LAG 200 with active mode with interfaces 1 and 2 in switch 2
  * Apply aggregation key 200 to LAG
* Validate interfaces 1 and 2 are in state In Sync and Collecting/Distributing in both switches
* Clean configuration


### Test Result Criteria
#### Test Pass Criteria
* All interfaces in both switches should be in state In Sync and Collecting/Distributing
#### Test Fail Criteria
* Any interfaces is not In Sync or in Collecting/Distributing state

## LACP port priority
### Objective
The interface with higher Port Priority is the one deciding which LAG interfaces are going to agreggate despite other configurations.
### Requirements
 - Virtual Mininet Test Setup
 - Script is in ops-test/component/test_lacpd_ct_port_priority.py

### Setup
#### Topology Diagram
```
+-------+     +-------+
|       1-----1       |
|       |     |       |
|       2-----2       |
|       |     |       |
| sw1   3-----3  sw2  |
|       |     |       |
|       4-----4       |
|       |     |       |
|       5-----5       |
+-------+     +-------+
```
### Test Setup

### Description
1. Enable all interfaces use for test
2. Create LAG 1 with active mode in fast rate with interfaces 1-2 in switch 1
3. Create LAG 2 with active mode in fast rate with interfaces 3-5 in switch 1
4. Create LAG 1 with active mode in fast rate with interfaces 1-3 in switch 2
5. Create LAG 2 with active mode in fast rate with interfaces 4-5 in switch 2
6. Wait for the configuration to apply
7. Apply verifications
8. Apply port priority 100 to interfaces 1, 2, 4, 5 in switch 1
9. Apply port priority 200 to interface 3 in switch 1
10. Apply port priority 100 to interfaces 1, 2, 4, 5 in switch 2
11. Apply port priority 200 to interface 3 in switch 2
12. Wait for the configuration to apply
13. Apply verifications
14. Clean configuration

### Test Result Criteria
#### Test Pass Criteria
1. Verifications for step 7
   - In switch 1:
   * Interface 1 must be collecting and distributing
   * Interface 2 must be collecting and distributing
   * Interface 3 must not be collecting and distributing
   * Interface 4 must not be collecting and distributing
   * Interface 5 must not be collecting and distributing
   - In switch 2:
   * Interface 1 must be collecting and distributing
   * Interface 2 must be collecting and distributing
   * Interface 3 must not be collecting and distributing
   * Interface 4 must not be collecting and distributing
   * Interface 5 must not be collecting and distributing
2. Verifications for step 13
   - In switch 1:
   * Interface 1 must be collecting and distributing
   * Interface 2 must be collecting and distributing
   * Interface 3 must not be collecting and distributing
   * Interface 4 must be collecting and distributing
   * Interface 5 must be collecting and distributing
   - In switch 2:
   * Interface 1 must be collecting and distributing
   * Interface 2 must be collecting and distributing
   * Interface 3 must not be collecting and distributing
   * Interface 4 must be collecting and distributing
   * Interface 5 must be collecting and distributing
#### Test Fail Criteria
1. Verifications for step 7
   - In switch 1:
   * Interface 1 must not be collecting and distributing
   * Interface 2 must not be collecting and distributing
   * Interface 3 must be collecting and distributing
   * Interface 4 must be collecting and distributing
   * Interface 5 must be collecting and distributing
   - In switch 2:
   * Interface 1 must not be collecting and distributing
   * Interface 2 must not be collecting and distributing
   * Interface 3 must be collecting and distributing
   * Interface 4 must be collecting and distributing
   * Interface 5 must be collecting and distributing
2. Verifications for step 13
   - In switch 1:
   * Interface 1 must not be collecting and distributing
   * Interface 2 must not be collecting and distributing
   * Interface 3 must be collecting and distributing
   * Interface 4 must not be collecting and distributing
   * Interface 5 must not be collecting and distributing
   - In switch 2:
   * Interface 1 must not be collecting and distributing
   * Interface 2 must not be collecting and distributing
   * Interface 3 must be collecting and distributing
   * Interface 4 must not be collecting and distributing
   * Interface 5 must not be collecting and distributing

## LACP partner priority
### Objective
The interface with higher Port Priority is the one deciding which LAG interfaces are going to agreggate despite other configurations, if two interfaces has the same port
priority, the interfaces with higher partner priority should decide which LAG interfaces are going to agreggate despite other configurations.
### Requirements
 - Virtual Mininet Test Setup
 - Script is in ops-test/component/test_lacpd_ct_port_priority.py

### Setup
#### Topology Diagram
```
+-------+     +-------+
|       1-----1       |
|       |     |       |
|       2-----2       |
|       |     |       |
| sw1   3-----3  sw2  |
|       |     |       |
|       4-----4       |
+-------+     +-------+
```
### Test Setup

### Description
1. Enable all interfaces use for test
2. Create LAG 1 with active mode in fast rate with interfaces 1-4 in switch 1
3. Create LAG 1 with active mode in fast rate with interfaces 1-2 in switch 2
4. Create LAG 2 with active mode in fast rate with interfaces 3-4 in switch 2
5. Apply port priority 100 to interfaces 1, 2, 3, 4 in switch 1
6. Apply port priority 100 to interface2 1 and 2 in switch 2
7. Apply port priority 1 to interfaces 3 and 4 in switch 2
8. Wait for the configuration to apply
9. Apply verifications
10. Clean configuration

### Test Result Criteria
#### Test Pass Criteria
1. Verifications for step 9
   - In switch 1:
   * Interface 1 must not be collecting and distributing
   * Interface 2 must not be collecting and distributing
   * Interface 3 must be collecting and distributing
   * Interface 4 must be collecting and distributing
   - In switch 2:
   * Interface 1 must not be collecting and distributing
   * Interface 2 must not be collecting and distributing
   * Interface 3 must be collecting and distributing
   * Interface 4 must be collecting and distributing
#### Test Fail Criteria
1. Verifications for step 9
   - In switch 1:
   * Interface 1 must be collecting and distributing
   * Interface 2 must be collecting and distributing
   * Interface 3 must not be collecting and distributing
   * Interface 4 must not be collecting and distributing
   - In switch 2:
   * Interface 1 must be collecting and distributing
   * Interface 2 must be collecting and distributing
   * Interface 3 must not be collecting and distributing
   * Interface 4 must not be collecting and distributing

## LACP system priority
### Objective
The system with higher System Priority is the one deciding which LAG interfaces are going to agreggate despite other configurations
### Requirements
 - Virtual Mininet Test Setup
 - Script is in ops-test/component/test_lacpd_ct_system_priority.py

### Setup
#### Topology Diagram
```
+-----+    +-----+
|     1----1     |
|     |    | sw2 |
|     2----2     |
|     |    +-----+
| sw1 |
|     |    +-----+
|     3----1     |
|     |    | sw3 |
|     4----2     |
+-----+    +-----+
```
### Test Setup

### Description
1. Enable all interfaces use for test
2. Apply system priority 1 to switch 1
3. Apply system priority 100 to switch 2
4. Apply system priority 50 to switch 3
5. Create LAG 1 with active mode in fast rate with interfaces 1-4 in switch 1
6. Create LAG 1 with active mode in fast rate with interfaces 1-2 in switch 2
7. Create LAG 1 with active mode in fast rate with interfaces 1-2 in switch 3
8. Wait for the configuration to apply
9. Apply verifications
10. Clean configuration

### Test Result Criteria
#### Test Pass Criteria
1. Verifications for step 9
   - In switch 1:
   * Interface 1 must not be collecting and distributing
   * Interface 2 must not be collecting and distributing
   * Interface 3 must be collecting and distributing
   * Interface 4 must be collecting and distributing
   - In switch 2:
   * Interface 1 must not be collecting and distributing
   * Interface 2 must not be collecting and distributing
   - In switch 3:
   * Interface 1 must be collecting and distributing
   * Interface 2 must be collecting and distributing
#### Test Fail Criteria
1. Verifications for step 9
   - In switch 1:
   * Interface 1 must be collecting and distributing
   * Interface 2 must be collecting and distributing
   * Interface 3 must not be collecting and distributing
   * Interface 4 must not be collecting and distributing
   - In switch 2:
   * Interface 1 must be collecting and distributing
   * Interface 2 must be collecting and distributing
   - In switch 3:
   * Interface 1 must not be collecting and distributing
   * Interface 2 must not be collecting and distributing

## dynamic lag created to disable all ports or to fallback
### Objective
Verify that configured LAG to either disable all ports or to fallback to active/backup when dynamic LACP is configured and the LACP negotiation fail.
### Requirements
 - Virtual Mininet Test Setup
 - Script is in tests/test_lacpd_ct_lag_fallback.py

### Setup
#### Topology Diagram
```
+--------+                  +--------+
|        1------------------1        |
|   s1   |                  |   s2   |
|        2------------------2        |
+--------+                  +--------+
```

### Description
1. Create LAGs with 1 Gb interfaces.
   Create LAG 1 with active mode with interfaces 1 and 2 in switch 1
   Create LAG 1 with active mode with interfaces 1 and 2 in switch 2
   Verify that interfaces are not operating in LAGs (because they are not enabled)
2. Enable all the interfaces
   Verify that interfaces are operating in LAGs (because they have been enabled and are linked)
3. Verify lacp_status in interfaces configured in LACP LAGs
4. Override port parameters on s1 and s2 (set other_config lacp-time to "fast")
5. Verify that interfaces are linked and operating at 1 Gb
   Verify that interfaces are operating in LAG
6. Override port parameters on s1 and s2 (set other_config lacp-fallback-ab as true)
7. Change lacp mode in port from "active" to "off" on s2
   Vefiry that interface 1 continue up
   Verify that interface 2 were down on LAG
8. LACP mode back to active on s2
   Verify that interfaces are in LAG
   Verify that interface 1 and interface 2 are in state up for switch 1
9. Set port:other_config lacp-fallback to "false"
10. Change lacp mode in port from "active" to "off" on s2
    Verify that interfaces were down on LAG on switch 1
11. Clear all user configuration
### Test Result Criteria
#### Test Pass Criteria
* Interfaces 1 and 2 in both switches changing state from up to down or down to up according with fallback and switch lacp mode.
#### Test Fail Criteria
One or more verifications fail.


# LAG ovs-appctl command getlacpinterfaces format
### Objective
Verify correct output format of ovs-appctl command getlacpinterfaces <lag_name>.
### Requirements
 - Virtual Mininet Test Setup
 - Script is in ops-tests/component/test_lag_ct_appctl_getlacpinterfaces.py

### Setup
#### Topology Diagram
```
+------------+
|            |
|     s1     |
|            |
+-1--2--3--4-+
  |  |  |  |
  |  |  |  |
  |  |  |  |
  |  |  |  |
+-1--2--3--4-+
|            |
|     s2     |
|            |
+------------+
```

### Description
1. Create 1 dynamic LAG in switch 1 and 2 with interfaces 1 and 2.
2. Create 1 static LAG in switch 1 and 2 with interfaces 3 and 4.
3. Execute ovs-appctl command getlacpinterfaces without any LAG.
4. Execute ovs-appctl command getlacpinterfaces with existent LAG.
5. Execute ovs-appctl command getlacpinterfaces with non existent LAG.

### Test Result Criteria
#### Test Pass Criteria
* Output of step 3 has the following format:
```
Port lag_name: <static_lag_name>
	configured_members   :
	eligible_members     :
	participant_members  :
Port lag_name: <dynamic_lag_name>
	configured_members   :
	eligible_members     :
	participant_members  :
```
* Output of step 4 has the following format:
```
Port lag_name: <specified_lag_name>
	configured_members   :
	eligible_members     :
	participant_members  :
```
* Output of step 5 is empty.
#### Test Fail Criteria
One or more verifications fail.


# LACP ovs-appctl command getlacpcounters format
### Objective
Verify correct output format of ovs-appctl command getlacpcounters <lag_name>.
### Requirements
 - Virtual Mininet Test Setup
 - Script is in ops-tests/component/test_lag_ct_appctl_getlacpcounters.py

### Setup
#### Topology Diagram
```
+------------+
|            |
|     s1     |
|            |
+-1--2--3--4-+
  |  |  |  |
  |  |  |  |
  |  |  |  |
  |  |  |  |
+-1--2--3--4-+
|            |
|     s2     |
|            |
+------------+
```

### Description
1. Create 1 dynamic LAG in switch 1 and 2 with interfaces 1 and 2.
2. Create 1 static LAG in switch 1 and 2 with interfaces 3 and 4.
3. Execute ovs-appctl command getlacpcounters without any LAG.
4. Execute ovs-appctl command getlacpcounters with dynamic LAG.
5. Execute ovs-appctl command getlacpcounters with static LAG.
6. Execute ovs-appctl command getlacpcounters with non existent LAG.

### Test Result Criteria
#### Test Pass Criteria
* Output of step 3 and 4 has the following format:
```
LAG <dynamic_lag_name>:
   Configured interfaces:
      Interface: 1
        lacp_pdus_sent: 0
        marker_response_pdus_sent: 0
        lacp_pdus_received: 0
        marker_pdus_received: 0
      Interface: 2
        lacp_pdus_sent: 0
        marker_response_pdus_sent: 0
        lacp_pdus_received: 0
        marker_pdus_received: 0
```
* Output of step 5 and 6 is empty.
#### Test Fail Criteria
One or more verifications fail.


# LACP ovs-appctl command getlacpstate format
### Objective
Verify correct output format of ovs-appctl command getlacpstate <lag_name>.
### Requirements
 - Virtual Mininet Test Setup
 - Script is in ops-tests/component/test_lag_ct_appctl_getlacpstate.py

### Setup
#### Topology Diagram
```
+------------+
|            |
|     s1     |
|            |
+-1--2--3--4-+
  |  |  |  |
  |  |  |  |
  |  |  |  |
  |  |  |  |
+-1--2--3--4-+
|            |
|     s2     |
|            |
+------------+
```

### Description
1. Create 1 dynamic LAG in switch 1 and 2 with interfaces 1 and 2.
2. Create 1 static LAG in switch 1 and 2 with interfaces 3 and 4.
3. Execute ovs-appctl command getlacpstate without any LAG.
4. Execute ovs-appctl command getlacpstate with dynamic LAG.
5. Execute ovs-appctl command getlacpstate with static LAG.
6. Execute ovs-appctl command getlacpstate with non existent LAG.

### Test Result Criteria
#### Test Pass Criteria
* Output of step 3 and 4 has the following format:
```
LAG <dynamic_lag_name>:
 Configured interfaces:
  Interface: 1
	actor_oper_port_state
	   lacp_activity:0 time_out:0 aggregation:0 sync:0 collecting:0
	   distributing:0 defaulted:0 expired:0
	partner_oper_port_state
	   lacp_activity:0 time_out:0 aggregation:0 sync:0 collecting:0
	   distributing:0 defaulted:1 expired:0
	lacp_control
	   begin:0 actor_churn:0 partner_churn:0 ready_n:0 selected:1
	   port_moved:0 ntt:0 port_enabled:0
  Interface: 2
	actor_oper_port_state
	   lacp_activity:0 time_out:0 aggregation:0 sync:0 collecting:0
	   distributing:0 defaulted:0 expired:0
	partner_oper_port_state
	   lacp_activity:0 time_out:0 aggregation:0 sync:0 collecting:0
	   distributing:0 defaulted:0 expired:0
	lacp_control
	   begin:0 actor_churn:0 partner_churn:0 ready_n:0 selected:0
	   port_moved:0 ntt:0 port_enabled:0
```
* Output of step 5 and 6 is empty.
#### Test Fail Criteria
One or more verifications fail.
