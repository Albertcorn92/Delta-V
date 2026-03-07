# Destination and End-User Screening Procedure

Date: 2026-03-07
Scope: DELTA-V framework releases and any maintainer-supported deployment activity

This procedure is process guidance only and not legal advice.

## 1. Trigger

Run screening before:

- any operational deployment support,
- any non-public technical transfer,
- any customer/program onboarding tied to this framework.

## 2. Required Checks

1. Destination-country check against current sanctions/restriction obligations.
2. End-user and organization screening against prohibited/restricted-party lists.
3. End-use screening for prohibited military/weaponized use.
4. Scope check that release remains civilian and no command-path crypto is added.
5. Confirm whether activity is public-source publication only or operational support.

## 3. Decision Matrix

- `Publish-only (public source, no private transfer/support)`: may proceed.
- `Operational support or private transfer`: requires documented screening log and counsel escalation where needed.
- `Restricted party/destination/end-use hit`: do not proceed.

## 4. Required Record

Log each decision in a dated record:

- `docs/process/DEPLOYMENT_SCREENING_LOG_YYYYMMDD.md`

Minimum fields:

- activity,
- destination,
- end user,
- checks performed,
- decision,
- owner/date.

## 5. Maintainer Boundary

- Maintainer may decline non-U.S. operational support.
- Maintainer does not provide legal determinations.
- Mission owners are responsible for final export/sanctions compliance.
