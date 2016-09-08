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
LACP Fallback Tests
===================

## Contents

- [Fallback disabled and toggle admin flag](#fallback-disabled-and-toggle-admin-flag)
- [Fallback disabled and toggle lacp flag as active](#fallback-disabled-and-toggle-lacp-flag-as-active)
- [Fallback disabled and toggle lacp flag as passive](#fallback-disabled-and-toggle-lacp-flag-as-passive)
- [Fallback disabled with false flag and toggle admin flag](#fallback-disabled-with-false-flag-and-toggle-admin-flag)
- [Fallback disabled with false flag and toggle lacp flag as active](#fallback-disabled-with-false-flag-and-toggle-lacp-flag-as-active)
- [Fallback disabled with false flag and toggle lacp flag as passive](#fallback-disabled-with-false-flag-and-toggle-lacp-flag-as-passive)
- [Fallback enabled and toggle admin flag](#fallback-enabled-and-toggle-admin-flag)
- [Fallback enabled and toggle lacp flag as active](#fallback-enabled-and-toggle-lacp-flag-as-active)
- [Fallback enabled and toggle lacp flag as passive](#fallback-enabled-and-toggle-lacp-flag-as-passive)

## Fallback Disabled and toggle admin flag
### Objective
This test will verify fallback functionality, disabled for this test, that all
interfaces will be `Defaulted and Expired`, no interfaces will be in
`Collecting and Distributing`. For this test the **admin** flag will be toggled
from `up` to `down` and viceversa.

### Requirements
- Physical/Virtual switches
- **CT File**: `ops-tests/component/test_lacpd_ct_lag_fallback.py`

### Setup
#### Topology diagram

```ditaa
            +------------+                              +------------+
            |            | 1                          1 |            |
            |            <----------------------------->             |
            |  Switch 1  |                              |  Switch 2  |
            |            |                              |            |
            |   (LAG 1)  |                              |   (LAG 1)  |
            |            <------------------------------>            |
            |            | 2                          2 |            |
            +------------+                              +------------+
```

### Description
Configure a topology with two switches, connected as shown in the
topology diagram.

1. Configure LAG 1 on switch 1 with the following values
    * **lacp** `active`
    * **other_config:lacp-time** `fast`
    * **hw_config:enable** `true`
* Configure LAG 1 on switch 2 with the following values
    * **lacp** `active`
    * **other_config:lacp-time** `fast`
    * **hw_config:enable** `true`
* Configure interfaces 1 and 2 on switch 1 with the following values
    * **user_config:admin** `up`
    * **other_config:lacp-aggregation-key** `1`
* Configure interfaces 1 and 2 on switch 2 with the following values
    * **user_config:admin** `up`
    * **other_config:lacp-aggregation-key** `1`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
* Disable LAG 1 on switch 1 by toggling **admin** flag to `down`
* Verify that all state machines from interface 1 and 2 on switch 2 have the
following flags enabled
    * `active`
    * `aggregable`
    * `defaulted`
    * `expired`
* Enable LAG 1 on switch 1 by toggling **admin** flag to `up`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
* Disable LAG 1 on switch 2 by toggling **admin** flag to `down`
* Verify that all state machines from interface 1 and 2 on switch 1 have the
following flags enabled
    * `active`
    * `aggregable`
    * `defaulted`
    * `expired`
* Enable LAG 1 on switch 2 by toggling **admin** flag to `up`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`

### Test result criteria
#### Test pass criteria
- All interfaces state machines on switch 1 fulfill with all the expected flags
enabled when LAG 1 is disabled on switch 2.
- All interfaces state machines on switch 2 fulfill with all the expected flags
enabled when LAG 1 is disabled on switch 1.
- All interfaces state machines fullfil with all the expected flags enabled
when both LAGs 1 are enabled on both switches.
#### Test fail criteria
- At least one interface state machine on switch 1 does not fulfill with all
the expected flags enabled when LAG 1 is disabled on switch 2.
- At least one interface state machine on switch 2 does not fulfill with all
the expected flags enabled when LAG 1 is disabled on switch 1.
- At least one interface state machine does not fulfill with all the expected
flags enabled when both LAGs 1 are enabled on both switches.

## Fallback Disabled and toggle lacp flag as active
### Objective
This test will verify fallback functionality, disabled for this test, that all
interfaces will be `Defaulted and Expired`, no interfaces will be in
`Collecting and Distributing`. For this test the **lacp** flag will be toggled
from `[] (cleared value)` to `active` and viceversa.

### Requirements
- Physical/Virtual switches
- **CT File**: `ops-tests/component/test_lacpd_ct_lag_fallback.py`

### Setup
#### Topology diagram

```ditaa
            +------------+                              +------------+
            |            | 1                          1 |            |
            |            <----------------------------->             |
            |  Switch 1  |                              |  Switch 2  |
            |            |                              |            |
            |   (LAG 1)  |                              |   (LAG 1)  |
            |            <------------------------------>            |
            |            | 2                          2 |            |
            +------------+                              +------------+
```

### Description
Configure a topology with two switches, connected as shown in the
topology diagram.

1. Configure LAG 1 on switch 1 with the following values
    * **lacp** `active`
    * **other_config:lacp-time** `fast`
    * **hw_config:enable** `true`
* Configure LAG 1 on switch 2 with the following values
    * **lacp** `active`
    * **other_config:lacp-time** `fast`
    * **hw_config:enable** `true`
* Configure interfaces 1 and 2 on switch 1 with the following values
    * **user_config:admin** `up`
    * **other_config:lacp-aggregation-key** `1`
* Configure interfaces 1 and 2 on switch 2 with the following values
    * **user_config:admin** `up`
    * **other_config:lacp-aggregation-key** `1`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
* Disable LAG 1 on switch 1 by toggling **lacp** flag to `off`
* Verify that all state machines from interface 1 and 2 on switch 2 have the
following flags enabled
    * `active`
    * `aggregable`
    * `defaulted`
    * `expired`
* Enable LAG 1 on switch 1 by toggling **lacp** flag to `active`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
* Disable LAG 1 on switch 2 by toggling **lacp** flag to `off`
* Verify that all state machines from interface 1 and 2 on switch 1 have the
following flags enabled
    * `active`
    * `aggregable`
    * `defaulted`
    * `expired`
* Enable LAG 1 on switch 2 by toggling **lacp** flag to `active`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`

### Test result criteria
#### Test pass criteria
- All interfaces state machines on switch 1 fulfill with all the expected flags
enabled when LAG 1 is disabled on switch 2.
- All interfaces state machines on switch 2 fulfill with all the expected flags
enabled when LAG 1 is disabled on switch 1.
- All interfaces state machines fullfil with all the expected flags enabled
when both LAGs 1 are enabled on both switches.
#### Test fail criteria
- At least one interface state machine on switch 1 does not fulfill with all
the expected flags enabled when LAG 1 is disabled on switch 2.
- At least one interface state machine on switch 2 does not fulfill with all
the expected flags enabled when LAG 1 is disabled on switch 1.
- At least one interface state machine does not fulfill with all the expected
flags enabled when both LAGs 1 are enabled on both switches.

## Fallback Disabled and toggle lacp flag as passive
### Objective
This test will verify fallback functionality, disabled for this test, that all
interfaces will be `Defaulted and Expired`, no interfaces will be in
`Collecting and Distributing`. For this test the **lacp** flag will be toggled
from `[] (cleared value)` to `passive` and viceversa.

**Note**: Both switches cannot be in lacp mode `passive`, it will break the
connectivity and the daemon will manage as if there is no partner connected.

### Requirements
- Physical/Virtual switches
- **CT File**: `ops-tests/component/test_lacpd_ct_lag_fallback.py`

### Setup
#### Topology diagram

```ditaa
            +------------+                              +------------+
            |            | 1                          1 |            |
            |            <----------------------------->             |
            |  Switch 1  |                              |  Switch 2  |
            |            |                              |            |
            |   (LAG 1)  |                              |   (LAG 1)  |
            |            <------------------------------>            |
            |            | 2                          2 |            |
            +------------+                              +------------+
```

### Description
Configure a topology with two switches, connected as shown in the
topology diagram.

1. Configure LAG 1 on switch 1 with the following values
    * **lacp** `active`
    * **other_config:lacp-time** `fast`
    * **hw_config:enable** `true`
* Configure LAG 1 on switch 2 with the following values
    * **lacp** `active`
    * **other_config:lacp-time** `fast`
    * **hw_config:enable** `true`
* Configure interfaces 1 and 2 on switch 1 with the following values
    * **user_config:admin** `up`
    * **other_config:lacp-aggregation-key** `1`
* Configure interfaces 1 and 2 on switch 2 with the following values
    * **user_config:admin** `up`
    * **other_config:lacp-aggregation-key** `1`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
* Toggle LAG 1 **lacp** flag on switch 2 to be `passive`
* Disable LAG 1 on switch 1 by toggling **lacp** flag to `off`
* Verify that all state machines from interface 1 and 2 on switch 2 have the
following flags enabled
    * `aggregable`
    * `defaulted`
    * `expired`
* Enable LAG 1 on switch 1 by toggling **lacp** flag to `active`
* Toggle LAG 1 **lacp** flag on switch 2 to be `passive`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
* Toggle LAG 1 **lacp** flag on switch 1 to be `passive`
* Disable LAG 1 on switch 2 by toggling **lacp** flag to `off`
* Verify that all state machines from interface 1 and 2 on switch 1 have the
following flags enabled
    * `aggregable`
    * `defaulted`
    * `expired`
* Enable LAG 1 on switch 2 by toggling **lacp** flag to `active`
* Toggle LAG 1 **lacp** flag on switch 1 to be `active`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`

### Test result criteria
#### Test pass criteria
- All interfaces state machines on switch 1 fulfill with all the expected flags
enabled when LAG 1 is disabled on switch 2.
- All interfaces state machines on switch 2 fulfill with all the expected flags
enabled when LAG 1 is disabled on switch 1.
- All interfaces state machines fullfil with all the expected flags enabled
when both LAGs 1 are enabled on both switches.
#### Test fail criteria
- At least one interface state machine on switch 1 does not fulfill with all
the expected flags enabled when LAG 1 is disabled on switch 2.
- At least one interface state machine on switch 2 does not fulfill with all
the expected flags enabled when LAG 1 is disabled on switch 1.
- At least one interface state machine does not fulfill with all the expected
flags enabled when both LAGs 1 are enabled on both switches.

## Fallback Disabled with false flag and toggle admin flag
### Objective
This test will verify fallback functionality, disabled for this test, that all
interfaces will be `Defaulted and Expired`, no interfaces will be in
`Collecting and Distributing`. For this test there will be set the
`lacp-fallback-ab` flag to `false` and the **admin** flag will be toggled from
`up` to `down` and viceversa.

### Requirements
- Physical/Virtual switches
- **CT File**: `ops-tests/component/test_lacpd_ct_lag_fallback.py`

### Setup
#### Topology diagram

```ditaa
            +------------+                              +------------+
            |            | 1                          1 |            |
            |            <----------------------------->             |
            |  Switch 1  |                              |  Switch 2  |
            |            |                              |            |
            |   (LAG 1)  |                              |   (LAG 1)  |
            |            <------------------------------>            |
            |            | 2                          2 |            |
            +------------+                              +------------+
```

### Description
Configure a topology with two switches, connected as shown in the
topology diagram.

1. Configure LAG 1 on switch 1 with the following values
    * **lacp** `active`
    * **other_config:lacp-time** `fast`
    * **hw_config:enable** `true`
* Configure LAG 1 on switch 2 with the following values
    * **lacp** `active`
    * **other_config:lacp-time** `fast`
    * **hw_config:enable** `true`
* Configure interfaces 1 and 2 on switch 1 with the following values
    * **user_config:admin** `up`
    * **other_config:lacp-aggregation-key** `1`
* Configure interfaces 1 and 2 on switch 2 with the following values
    * **user_config:admin** `up`
    * **other_config:lacp-aggregation-key** `1`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
* Configure LAG 1 on both switches to set `other_config:lacp-fallback-ab` flag
to `false`
* Disable LAG 1 on switch 1 by toggling **admin** flag to `down`
* Verify that all state machines from interface 1 and 2 on switch 2 have the
following flags enabled
    * `active`
    * `aggregable`
    * `defaulted`
    * `expired`
* Enable LAG 1 on switch 1 by toggling **admin** flag to `up`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
* Disable LAG 1 on switch 2 by toggling **admin** flag to `down`
* Verify that all state machines from interface 1 and 2 on switch 1 have the
following flags enabled
    * `active`
    * `aggregable`
    * `defaulted`
    * `expired`
* Enable LAG 1 on switch 2 by toggling **admin** flag to `up`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`

### Test result criteria
#### Test pass criteria
- All interfaces state machines on switch 1 fulfill with all the expected flags
enabled when LAG 1 is disabled on switch 2.
- All interfaces state machines on switch 2 fulfill with all the expected flags
enabled when LAG 1 is disabled on switch 1.
- All interfaces state machines fullfil with all the expected flags enabled
when both LAGs 1 are enabled on both switches.
#### Test fail criteria
- At least one interface state machine on switch 1 does not fulfill with all
the expected flags enabled when LAG 1 is disabled on switch 2.
- At least one interface state machine on switch 2 does not fulfill with all
the expected flags enabled when LAG 1 is disabled on switch 1.
- At least one interface state machine does not fulfill with all the expected
flags enabled when both LAGs 1 are enabled on both switches.

## Fallback Disabled with false flag and toggle lacp flag as active
### Objective
This test will verify fallback functionality, disabled for this test, that all
interfaces will be `Defaulted and Expired`, no interfaces will be in
`Collecting and Distributing`. For this test there will be set the
`lacp-fallback-ab` flag to `false` and the **lacp** flag will be toggled from
`[] (cleared value)` to `active` and viceversa.

### Requirements
- Physical/Virtual switches
- **CT File**: `ops-tests/component/test_lacpd_ct_lag_fallback.py`

### Setup
#### Topology diagram

```ditaa
            +------------+                              +------------+
            |            | 1                          1 |            |
            |            <----------------------------->             |
            |  Switch 1  |                              |  Switch 2  |
            |            |                              |            |
            |   (LAG 1)  |                              |   (LAG 1)  |
            |            <------------------------------>            |
            |            | 2                          2 |            |
            +------------+                              +------------+
```

### Description
Configure a topology with two switches, connected as shown in the
topology diagram.

1. Configure LAG 1 on switch 1 with the following values
    * **lacp** `active`
    * **other_config:lacp-time** `fast`
    * **hw_config:enable** `true`
* Configure LAG 1 on switch 2 with the following values
    * **lacp** `active`
    * **other_config:lacp-time** `fast`
    * **hw_config:enable** `true`
* Configure interfaces 1 and 2 on switch 1 with the following values
    * **user_config:admin** `up`
    * **other_config:lacp-aggregation-key** `1`
* Configure interfaces 1 and 2 on switch 2 with the following values
    * **user_config:admin** `up`
    * **other_config:lacp-aggregation-key** `1`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
* Configure LAG 1 on both switches to set `other_config:lacp-fallback-ab` flag
to `false`
* Disable LAG 1 on switch 1 by toggling **lacp** flag to `off`
* Verify that all state machines from interface 1 and 2 on switch 2 have the
following flags enabled
    * `active`
    * `aggregable`
    * `defaulted`
    * `expired`
* Enable LAG 1 on switch 1 by toggling **lacp** flag to `active`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
* Disable LAG 1 on switch 2 by toggling **lacp** flag to `off`
* Verify that all state machines from interface 1 and 2 on switch 1 have the
following flags enabled
    * `active`
    * `aggregable`
    * `defaulted`
    * `expired`
* Enable LAG 1 on switch 2 by toggling **lacp** flag to `active`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`

### Test result criteria
#### Test pass criteria
- All interfaces state machines on switch 1 fulfill with all the expected flags
enabled when LAG 1 is disabled on switch 2.
- All interfaces state machines on switch 2 fulfill with all the expected flags
enabled when LAG 1 is disabled on switch 1.
- All interfaces state machines fullfil with all the expected flags enabled
when both LAGs 1 are enabled on both switches.
#### Test fail criteria
- At least one interface state machine on switch 1 does not fulfill with all
the expected flags enabled when LAG 1 is disabled on switch 2.
- At least one interface state machine on switch 2 does not fulfill with all
the expected flags enabled when LAG 1 is disabled on switch 1.
- At least one interface state machine does not fulfill with all the expected
flags enabled when both LAGs 1 are enabled on both switches.

## Fallback Disabled with false flag and toggle lacp flag as passive
### Objective
This test will verify fallback functionality, disabled for this test, that all
interfaces will be `Defaulted and Expired`, no interfaces will be in
`Collecting and Distributing`. For this test there will be set the
`lacp-fallback-ab` flag to `false` and the **lacp** flag will be toggled from
`[] (cleared value)` to `passive` and viceversa.

**Note**: Both switches cannot be in lacp mode `passive`, it will break the
connectivity and the daemon will manage as if there is no partner connected.

### Requirements
- Physical/Virtual switches
- **CT File**: `ops-tests/component/test_lacpd_ct_lag_fallback.py`

### Setup
#### Topology diagram

```ditaa
            +------------+                              +------------+
            |            | 1                          1 |            |
            |            <----------------------------->             |
            |  Switch 1  |                              |  Switch 2  |
            |            |                              |            |
            |   (LAG 1)  |                              |   (LAG 1)  |
            |            <------------------------------>            |
            |            | 2                          2 |            |
            +------------+                              +------------+
```

### Description
Configure a topology with two switches, connected as shown in the
topology diagram.

1. Configure LAG 1 on switch 1 with the following values
    * **lacp** `active`
    * **other_config:lacp-time** `fast`
    * **hw_config:enable** `true`
* Configure LAG 1 on switch 2 with the following values
    * **lacp** `active`
    * **other_config:lacp-time** `fast`
    * **hw_config:enable** `true`
* Configure interfaces 1 and 2 on switch 1 with the following values
    * **user_config:admin** `up`
    * **other_config:lacp-aggregation-key** `1`
* Configure interfaces 1 and 2 on switch 2 with the following values
    * **user_config:admin** `up`
    * **other_config:lacp-aggregation-key** `1`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
* Configure LAG 1 on both switches to set `other_config:lacp-fallback-ab` flag
to `false`
* Toggle LAG 1 **lacp** flag on switch 2 to be `passive`
* Disable LAG 1 on switch 1 by toggling **lacp** flag to `off`
* Verify that all state machines from interface 1 and 2 on switch 2 have the
following flags enabled
    * `aggregable`
    * `defaulted`
    * `expired`
* Enable LAG 1 on switch 1 by toggling **lacp** flag to `active`
* Toggle LAG 1 **lacp** flag on switch 2 to be `passive`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
* Toggle LAG 1 **lacp** flag on switch 1 to be `passive`
* Disable LAG 1 on switch 2 by toggling **lacp** flag to `off`
* Verify that all state machines from interface 1 and 2 on switch 1 have the
following flags enabled
    * `aggregable`
    * `defaulted`
    * `expired`
* Enable LAG 1 on switch 2 by toggling **lacp** flag to `active`
* Toggle LAG 1 **lacp** flag on switch 1 to be `active`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`

### Test result criteria
#### Test pass criteria
- All interfaces state machines on switch 1 fulfill with all the expected flags
enabled when LAG 1 is disabled on switch 2.
- All interfaces state machines on switch 2 fulfill with all the expected flags
enabled when LAG 1 is disabled on switch 1.
- All interfaces state machines fullfil with all the expected flags enabled
when both LAGs 1 are enabled on both switches.
#### Test fail criteria
- At least one interface state machine on switch 1 does not fulfill with all
the expected flags enabled when LAG 1 is disabled on switch 2.
- At least one interface state machine on switch 2 does not fulfill with all
the expected flags enabled when LAG 1 is disabled on switch 1.
- At least one interface state machine does not fulfill with all the expected
flags enabled when both LAGs 1 are enabled on both switches.

## Fallback Enabled and toggle admin flag
### Objective
This test will verify fallback functionality, enabled for this test, will have
one interface that must be in 'Collecting and Distributing' and other LAG
interfaces will be `Defaulted`. For this test the **admin** flag will be
toggled from `up` to `down` and viceversa.

### Requirements
- Physical/Virtual switches
- **CT File**: `ops-tests/component/test_lacpd_ct_lag_fallback.py`

### Setup
#### Topology diagram

```ditaa
            +------------+                              +------------+
            |            | 1                          1 |            |
            |            <----------------------------->             |
            |  Switch 1  |                              |  Switch 2  |
            |            |                              |            |
            |   (LAG 1)  |                              |   (LAG 1)  |
            |            <------------------------------>            |
            |            | 2                          2 |            |
            +------------+                              +------------+
```

### Description
Configure a topology with two switches, connected as shown in the
topology diagram.

1. Configure LAG 1 on switch 1 with the following values
    * **lacp** `active`
    * **other_config:lacp-time** `fast`
    * **hw_config:enable** `true`
* Configure LAG 1 on switch 2 with the following values
    * **lacp** `active`
    * **other_config:lacp-time** `fast`
    * **hw_config:enable** `true`
* Configure interfaces 1 and 2 on switch 1 with the following values
    * **user_config:admin** `up`
    * **other_config:lacp-aggregation-key** `1`
* Configure interfaces 1 and 2 on switch 2 with the following values
    * **user_config:admin** `up`
    * **other_config:lacp-aggregation-key** `1`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
* Enable LAG 1 Fallback on both switches
* Disable LAG 1 on switch 1 by toggling **admin** flag to `down`
* Verify that state machine from interface 1 on switch 2 has the following
flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
    * `defaulted`
* Verify that state machine from interface 2 on switch 2 has the following
flags enabled
    * `active`
    * `aggregable`
    * `defaulted`
* Enable LAG 1 on switch 1 by toggling **admin** flag to `up`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
* Disable LAG 1 on switch 2 by toggling **admin** flag to `down`
* Verify that state machine from interface 1 on switch 1 has the following
flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
    * `defaulted`
* Verify that state machine from interface 2 on switch 1 has the following
flags enabled
    * `active`
    * `aggregable`
    * `defaulted`
* Enable LAG 1 on switch 2 by toggling **admin** flag to `up`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`

### Test result criteria
#### Test pass criteria
- All interfaces state machines on switch 1 fulfill with all the expected flags
enabled when LAG 1 is disabled on switch 2.
- All interfaces state machines on switch 2 fulfill with all the expected flags
enabled when LAG 1 is disabled on switch 1.
- All interfaces state machines fullfil with all the expected flags enabled
when both LAGs 1 are enabled on both switches.
#### Test fail criteria
- At least one interface state machine on switch 1 does not fulfill with all
the expected flags enabled when LAG 1 is disabled on switch 2.
- At least one interface state machine on switch 2 does not fulfill with all
the expected flags enabled when LAG 1 is disabled on switch 1.
- At least one interface state machine does not fulfill with all the expected
flags enabled when both LAGs 1 are enabled on both switches.

## Fallback Enabled and toggle lacp flag as active
### Objective
This test will verify fallback functionality, enabled for this test, will have
one interface that must be in 'Collecting and Distributing' and other LAG
interfaces will be `Defaulted`. For this test the **lacp** flag will be toggled
from `[] (cleared value)` to `active` and viceversa.

### Requirements
- Physical/Virtual switches
- **CT File**: `ops-tests/component/test_lacpd_ct_lag_fallback.py`

### Setup
#### Topology diagram

```ditaa
            +------------+                              +------------+
            |            | 1                          1 |            |
            |            <----------------------------->             |
            |  Switch 1  |                              |  Switch 2  |
            |            |                              |            |
            |   (LAG 1)  |                              |   (LAG 1)  |
            |            <------------------------------>            |
            |            | 2                          2 |            |
            +------------+                              +------------+
```

### Description
Configure a topology with two switches, connected as shown in the
topology diagram.

1. Configure LAG 1 on switch 1 with the following values
    * **lacp** `active`
    * **other_config:lacp-time** `fast`
    * **hw_config:enable** `true`
* Configure LAG 1 on switch 2 with the following values
    * **lacp** `active`
    * **other_config:lacp-time** `fast`
    * **hw_config:enable** `true`
* Configure interfaces 1 and 2 on switch 1 with the following values
    * **user_config:admin** `up`
    * **other_config:lacp-aggregation-key** `1`
* Configure interfaces 1 and 2 on switch 2 with the following values
    * **user_config:admin** `up`
    * **other_config:lacp-aggregation-key** `1`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
* Enable LAG 1 Fallback on both switches
* Disable LAG 1 on switch 1 by toggling **lacp** flag to `off`
* Verify that state machine from interface 1 on switch 2 has the following
flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
    * `defaulted`
* Verify that state machine from interface 2 on switch 2 has the following
flags enabled
    * `active`
    * `aggregable`
    * `defaulted`
* Enable LAG 1 on switch 1 by toggling **lacp** flag to `active`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
* Disable LAG 1 on switch 2 by toggling **lacp** flag to `off`
* Verify that state machine from interface 1 on switch 1 has the following
flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
    * `defaulted`
* Verify that state machine from interface 2 on switch 1 has the following
flags enabled
    * `active`
    * `aggregable`
    * `defaulted`
* Enable LAG 1 on switch 2 by toggling **lacp** flag to `active`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`

### Test result criteria
#### Test pass criteria
- All interfaces state machines on switch 1 fulfill with all the expected flags
enabled when LAG 1 is disabled on switch 2.
- All interfaces state machines on switch 2 fulfill with all the expected flags
enabled when LAG 1 is disabled on switch 1.
- All interfaces state machines fullfil with all the expected flags enabled
when both LAGs 1 are enabled on both switches.
#### Test fail criteria
- At least one interface state machine on switch 1 does not fulfill with all
the expected flags enabled when LAG 1 is disabled on switch 2.
- At least one interface state machine on switch 2 does not fulfill with all
the expected flags enabled when LAG 1 is disabled on switch 1.
- At least one interface state machine does not fulfill with all the expected
flags enabled when both LAGs 1 are enabled on both switches.

## Fallback Enabled and toggle lacp flag as passive
### Objective
This test will verify fallback functionality, disabled for this test, that all
interfaces will be `Defaulted and Expired`, no interfaces will be in
`Collecting and Distributing`. For this test the **lacp** flag will be toggled
from `[] (cleared value)` to `passive` and viceversa.

**Note**: Both switches cannot be in lacp mode `passive`, it will break the
connectivity and the daemon will manage as if there is no partner connected.

### Requirements
- Physical/Virtual switches
- **CT File**: `ops-tests/component/test_lacpd_ct_lag_fallback.py`

### Setup
#### Topology diagram

```ditaa
            +------------+                              +------------+
            |            | 1                          1 |            |
            |            <----------------------------->             |
            |  Switch 1  |                              |  Switch 2  |
            |            |                              |            |
            |   (LAG 1)  |                              |   (LAG 1)  |
            |            <------------------------------>            |
            |            | 2                          2 |            |
            +------------+                              +------------+
```

### Description
Configure a topology with two switches, connected as shown in the
topology diagram.

1. Configure LAG 1 on switch 1 with the following values
    * **lacp** `active`
    * **other_config:lacp-time** `fast`
    * **hw_config:enable** `true`
* Configure LAG 1 on switch 2 with the following values
    * **lacp** `active`
    * **other_config:lacp-time** `fast`
    * **hw_config:enable** `true`
* Configure interfaces 1 and 2 on switch 1 with the following values
    * **user_config:admin** `up`
    * **other_config:lacp-aggregation-key** `1`
* Configure interfaces 1 and 2 on switch 2 with the following values
    * **user_config:admin** `up`
    * **other_config:lacp-aggregation-key** `1`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
* Enable LAG 1 Fallback on both switches
* Toggle LAG 1 **lacp** flag on switch 2 to be `passive`
* Disable LAG 1 on switch 1 by toggling **lacp** flag to `off`
* Verify that state machine from interface 1 on switch 2 has the following
flags enabled
    * `passive`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
    * `defaulted`
* Verify that state machine from interface 2 on switch 2 has the following
flags enabled
    * `passive`
    * `aggregable`
    * `defaulted`
* Enable LAG 1 on switch 1 by toggling **lacp** flag to `active`
* Toggle LAG 1 **lacp** flag on switch 2 to be `passive`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
* Toggle LAG 1 **lacp** flag on switch 1 to be `passive`
* Disable LAG 1 on switch 2 by toggling **lacp** flag to `off`
* Verify that state machine from interface 1 on switch 1 has the following
flags enabled
    * `passive`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`
    * `defaulted`
* Verify that state machine from interface 2 on switch 1 has the following
flags enabled
    * `passive`
    * `aggregable`
    * `defaulted`
* Enable LAG 1 on switch 2 by toggling **lacp** flag to `active`
* Toggle LAG 1 **lacp** flag on switch 1 to be `active`
* Verify that all state machines from interface 1 and 2 on both switches have
this flags enabled
    * `active`
    * `aggregable`
    * `in-sync`
    * `collecting`
    * `distributing`

### Test result criteria
#### Test pass criteria
- All interfaces state machines on switch 1 fulfill with all the expected flags
enabled when LAG 1 is disabled on switch 2.
- All interfaces state machines on switch 2 fulfill with all the expected flags
enabled when LAG 1 is disabled on switch 1.
- All interfaces state machines fullfil with all the expected flags enabled
when both LAGs 1 are enabled on both switches.
#### Test fail criteria
- At least one interface state machine on switch 1 does not fulfill with all
the expected flags enabled when LAG 1 is disabled on switch 2.
- At least one interface state machine on switch 2 does not fulfill with all
the expected flags enabled when LAG 1 is disabled on switch 1.
- At least one interface state machine does not fulfill with all the expected
flags enabled when both LAGs 1 are enabled on both switches.
