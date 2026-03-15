# Legal Scope Checklist

Date: 2026-03-07

Use this checklist before pushing or releasing DELTA-V.

## Required Checks

```bash
python3 tools/legal_compliance_check.py
cmake --build build --target software_final
```

Both commands should pass. If the project boundary is unclear, read
`docs/LEGAL_FAQ.md` before releasing.

## Confirm The Repository Still Matches Its Scope

- no military, weapons, targeting, fire-control, munition, missile, or combat
  behavior
- no command-path crypto/auth additions in the public baseline
- no controlled third-party technical data
- no private export-controlled documents committed to the repo

## Do Not Claim More Than The Repo Actually Provides

- do not claim blanket legality
- do not claim this repo provides legal clearance by itself
- do not claim blanket ITAR/EAR exemption for every user or deployment

## Keep The Remaining Responsibilities Clear

- deployment and export decisions still require end-user, destination, and
  sanctions checks
- production or operational use may still require qualified legal review

## Maintainer Boundary

- the maintainer does not provide direct operational support to non-U.S. users
- the maintainer may decline deployment or export consulting requests
- mission-specific records and hardware evidence stay outside the public repo

See `docs/MAINTAINER_BOUNDARY_POLICY.md`.

This checklist is project guidance only. It is not legal advice.
