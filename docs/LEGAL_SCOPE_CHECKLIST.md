# Legal Scope Checklist (Civilian Baseline)

Date: 2026-03-07

Use this checklist before pushing or releasing DELTA-V.

## 1. Required Local Checks

```bash
python3 tools/legal_compliance_check.py
cmake --build build --target software_final
```

Both commands must pass.
Read `docs/LEGAL_FAQ.md` before public release if you are unsure about scope.

## 2. Civilian Scope Boundaries

- No military, weapons, targeting, fire-control, munition, missile, or combat behavior.
- No command-path cryptography/encryption additions in the baseline framework.
- No controlled third-party technical data or private export-controlled documents.

## 3. Claims You Must Not Make

- Do not claim blanket legality, no-responsibility status, or blanket export-control exemption.
- Do not claim this repo gives legal clearance by itself.
- Do not claim blanket ITAR/EAR exemption status for all users or all deployments.

## 4. Remaining User Responsibility

- Deployment/export decisions still require end-user, destination, and sanctions checks.
- Final ITAR/EAR determination for production use must be made by qualified counsel.

## 5. Maintainer Boundary (Public Repo Mode)

- Maintainer does not provide direct operational support to non-U.S. users.
- Maintainer may decline requests for deployment/export consulting.
- Maintainer keeps mission/customer-specific records and hardware evidence logs private.
- Maintainer follows `docs/MAINTAINER_BOUNDARY_POLICY.md`.

This checklist is process guidance and not legal advice.
