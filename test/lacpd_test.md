# lacpd Test Cases

[TOC]

## Static LAG Membership
### Objective
Verify that configured interfaces are included or excluded from a static LAG depending on their link status.
### Requirements
 - Virtual Mininet Test Setup
### Setup
#### Topology Diagram
```
[h1] <--> [s1] <--> [s2] <--> [h2]
```
### Description ###
1. Create LAGs with 1 Gb, 10 Gb, and 40 Gb interfaces.
   Verify that interfaces are not operating in LAGs (because they are not enabled)
2. Enable all the interfaces
   Verify that interfaces are operating in LAGs (because they have been enabled and are linked)
3. Remove an interface from a LAG
   Verify that removed interface is not operating in LAG (because it has been removed from configuration)
4. Add interface back to LAG
   Verify that added interface is operating in LAG
5. Remove all but two interfaces from a LAG
   Verify that remaining interfaces are still in LAG
   Verify that removed interfaces are not in LAG
6. Disable one of two interfaces in LAG
   Verify that removed interface is not in LAG
7. Enable interface in LAG
   Verify that interface is in LAG
8. Clear all user configuration
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
### Setup
#### Topology Diagram
```
[h1] <--> [s1] <--> [s2] <--> [h2]
```
### Description
1. Enable 1 Gb and 10 Gb interfaces
2. Create a LAG with 1 Gb interfaces
3. Add a 10 Gb interfaces to LAG
   Verify that 1 Gb interfaces are operating in LAG
   Verify that 10 Gb interfaces are not operating in LAG (mismatched speed)
4. Remove 1 Gb interfaces from LAG
   Verify that 10 Gb interfaces are operating in LAG (no 1 Gb links remain)
5. Clear all user configuration
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
### Setup
#### Topology Diagram
```
[h1] <--> [s1] <--> [s2] <--> [h2]
```
### Description
1. Get system\_mac (system:system\_mac) for s1 and s2
2. Create a LACP LAG on s1 and s2 (connected interfaces)
   Verify lacp\_status in interfaces configured in LACP LAGs (LAGs are formed and are using default system\_mac values)
3. Override system parameters on s1 and s2 (set system:lacp\_config lacp-system-id and lacp-system-priority)
   Verify lacp\_status in interfaces configured in LACP LAGs (LAGs are formed and are using newly specified [system] override values)
4. Set port:other\_config lacp-system-id and lacp-system-priority to override system-level override
   Verify lacp\_status in interfaces configured in LACP LAGs (LAGs are formed and are using newly specified [port] override values)
5. Delete and recreate LACP LAG on s1 and s2
   Verify lacp\_status in interfaces configured in LACP LAGs (LAGs are formed and are using [system] override values)
6. Delete LAGs
7. Create a LACP LAG on s1 and s2 using 1 Gb interfaces
   Verify that interfaces are linked and operating at 1 Gb
   Verify that interfaces are operating in LAG
8. Override system parameters on s1 (set system:lacp\_config lacp-system-id and lacp-system-priority)
   Verify that system-level overrides are used in interfaces
9. Clear system parameters on s1
   Verify that default values for system id and system priority are used in interfaces
10. Set invalid system:lacp\_config values
   Verify that invalid values are ignored
11. Change lacp in port from "active" to "passive"
   Verify that interface:lacp\_status actor\_state has Active:0
12. Change lacp in port from "passive" to "off"
   Verify interface:lacp\_status actor\_state is empty (LACP is disabled)
13. Change lacp in port from "off" to "active"
   Verify that interface:lacp\_status is correct (Active:1)
14. Set port:other\_config lacp-timer to "slow"
   Verify that TmOut in interface:actor\_state is 0
15. Set port:other\_config lacp-timer to "fast"
   Verify that TmOut in interface:actor\_state is 1
16. Set interface:other\_config lacp-port-id and lacp-port-priority
   Verify that interface:lacp\_status actor\_port\_id uses new values
17. Set interface:other\_config lacp-port-id and lacp-port-priority to invalid values
   Verify that interface:lacp\_status actor\_port\_id uses default values (ignores invalid values)
18. Remove interface:other\_config lacp-port-id and lacp-port-priority values
   Verify that interface:lacp\_status actor\_port\_id uses default values
   Verify that port:lacp\_status bond\_speed is 1000 and bond\_status is "ok"
   Verify interface:lacp\_status values are correct
19. Set system:lacp\_config lacp-system-id and lacp-system-priority
   Verify that interfaces are using system-level overrides
20. Create a new LAG (lag1) on s2
21. Enable interfaces in lag1
   Verify interface:lacp\_status values in lag1 are using system-level overrides
22. Set lag0:other\_config lacp-system-id and lacp-system-priority
   Verify that lag0 interfaces are using port-level overrides
   Verify that lag1 interfaces are using system-level overrides
23. Add interface to lag0
   Verify that added interface is using port-level overrides
24. Clear port-level overrides from lag0
   Verify that lag0 interfaces are using system-level overrides
### Test Result Criteria
#### Test Pass Criteria
All verifications succeed.
#### Test Fail Criteria
One or more verifications fail.
