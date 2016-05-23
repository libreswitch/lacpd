LACP Component Tests
===================

## Contents

- [Static LAG port and member interfaces bond_status](#static-lag-port-and-member-interfaces-bond-status)
- [LACP port and member interfaces bond_status](#lacp-port-and-member-interfaces-bond-status)


# Static LAG port and member interfaces bond_status
### Objective
Verify LAG port and member interfaces bond_status value is correctly updated.
### Requirements
 - Modular framework
 - Script is in ops-tests/component/test_lag_ct_bond_status.py

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
1. Turn on all the interfaces used in this test.
*  Create 1 static LAG in switch 1 and 2 with interfaces 1, 2, 3 and 4.

Note: The following steps are only executed in switch 1.

*  Check the member interfaces and the lag port bond_status.
*  Turn off 3 of the member interfaces of the LAG.
*  Check the member interfaces and the lag port bond_status.
*  Turn back on the interfaces turned off in step 4.
*  Remove one interface from the LAG.
*  Check the interfaces and the lag port bond_status.
*  Add the interface removed in step 7 back to the LAG.
*  Check the member interfaces and the lag port bond_status.
*  Turn off all the member interfaces of the LAG.
*  Check the member interfaces and the lag port bond_status.
*  Turn on all the member interfaces of the LAG.
*  Remove all the member interfaces of the LAG except by one.
*  Add an interface to the LAG with different configured speed as the interface
   that is already in the LAG, so one interface is not eligible.
*  Check the member interfaces bond_status.
*  Remove all the member interfaces of the LAG.
*  Check the bond_status of the removed interfaces and the LAG port.

### Test Result Criteria
#### Test Pass Criteria
- When all the member interfaces are up, the interface bond_status is up and
  the LAG bond_status is also up.
- When 3 member interfaces are turned off, the interface bond_status is down
  for the interfaces that are off; the bond_status is up for the interface that
  is on, and the bond_status of the LAG is up.
- When an interface is removed from the LAG, its bond_status is empty, the
  bond_status of the interfaces member of the LAG is up, and the LAG
  bond_status is up.
- When an interface is added back to the LAG, its bond_status is up, and the
  LAG bond_status is up.
- When all the member interfaces are turned off, their bond_status is down and
  the LAG bond_status is also down.
- When an interface is not eligible, its bond_status is blocked.
- When an interface is not part of any LAG, its bond_status is empty.
- When a LAG has no member interfaces its bond_status is down.
#### Test Fail Criteria
- When all the member interfaces are up, the interface bond_status is not up or
  the LAG bond_status is not up.
- When 3 member interfaces are turned off, the interface bond_status is not down
  for the interfaces that are off; the bond_status is not up for the interface
  that is on, or the bond_status of the LAG is not up.
- When an interface is removed from the LAG, its bond_status is not empty, the
  bond_status of the interfaces member of the LAG is not up, or the LAG
  bond_status is not up.
- When an interface is added back to the LAG, its bond_status is not up, or the
  LAG bond_status is not up.
- When all the member interfaces are turned off, their bond_status is not down
  or the LAG bond_status is not down.
- When an interface is not eligible, its bond_status is not blocked.
- When an interface is not part of any LAG, its bond_status is not empty.
- When a LAG has no member interfaces its bond_status is not down.


# LACP port and member interfaces bond_status
### Objective
Verify dynamic LAG port and member interfaces bond_status value is correctly updated.
### Requirements
 - Modular framework
 - Script is in ops-tests/component/test_lacp_ct_bond_status.py

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
1. Turn on all the interfaces used in this test.
*  Create 1 dynamic LAG in switch 1 and 2 with interfaces 1, 2, 3 and 4.

Note: Unless explicitly stated, the following steps are only executed in switch 1.

*  Check the member interfaces and the lag port bond_status.
*  Turn off 3 of the member interfaces of the LAG.
*  Check the member interfaces and the lag port bond_status.
*  Turn back on the interfaces turned off in step 4.
*  Remove one interface from the LAG.
*  Check the interfaces and the lag port bond_status.
*  Add the interface removed in step 7 back to the LAG.
*  Check the member interfaces and the lag port bond_status.
*  Turn off all the member interfaces of the LAG.
*  Check the member interfaces and the lag port bond_status.
*  Turn on all the member interfaces of the LAG.
*  Remove one interface from LAG in switch2, so one interface in switch1 is not
   eligible.
*  Check the member interfaces bond_status.
*  Disable fallback in switch 1 and 2.
*  Disable lacp for the LAG in switch2.
*  Check the lag port bond_status.
*  Enable fallback in switch 1 and 2.
*  Disable lacp for the LAG in switch2.
*  Check the lag port bond_status.
*  Remove all the member interfaces of the LAG.
*  Check the bond_status of the removed interfaces and the LAG port.

### Test Result Criteria
#### Test Pass Criteria
- When all the member interfaces are up, the interface bond_status is up and
  the LAG bond_status is also up.
- When 3 member interfaces are turned off, the interface bond_status is down
  for the interfaces that are off; the bond_status is up for the interface that
  is on, and the bond_status of the LAG is up.
- When an interface is removed from the LAG, its bond_status is empty, the
  bond_status of the interfaces member of the LAG is up, and the LAG
  bond_status is up.
- When an interface is added back to the LAG, its bond_status is up, and the
  LAG bond_status is up.
- When all the member interfaces are turned off, their bond_status is down and
  the LAG bond_status is also down.
- When an interface is not eligible, its bond_status is blocked.
- When lacp_status:state is defaulted and fallback is disabled, the LAG
  bond_status is blocked.
- When lacp_status:state is defaulted and fallback is enabled, the LAG
  bond_status is up.
- When an interface is not part of any LAG, its bond_status is empty.
- When a LAG has no member interfaces its bond_status is down.
#### Test Fail Criteria
- When all the member interfaces are up, the interface bond_status is not up or
  the LAG bond_status is not up.
- When 3 member interfaces are turned off, the interface bond_status is not down
  for the interfaces that are off; the bond_status is not up for the interface
  that is on, or the bond_status of the LAG is not up.
- When an interface is removed from the LAG, its bond_status is not empty, the
  bond_status of the interfaces member of the LAG is not up, or the LAG
  bond_status is not up.
- When an interface is added back to the LAG, its bond_status is not up, or the
  LAG bond_status is not up.
- When all the member interfaces are turned off, their bond_status is not down
  or the LAG bond_status is not down.
- When an interface is not eligible, its bond_status is not blocked.
- When lacp_status:state is defaulted and fallback is disabled, the LAG
  bond_status is not blocked.
- When lacp_status:state is defaulted and fallback is enabled, the LAG
  bond_status is not up.
- When an interface is not part of any LAG, its bond_status is not empty.
- When a LAG has no member interfaces its bond_status is not down.
