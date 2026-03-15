# Export Control Note

Date: 2026-03-07

This repository is published as public open-source software for civilian
development, research, and learning. That helps keep the baseline in a simpler
compliance posture, but it does not answer every export-control question by
itself.

## What This Note Means

- This note is general guidance only. It is **not legal advice**.
- A public GitHub repository does not provide automatic legal clearance for
  every reuse, deployment, or support activity.
- Final ITAR/EAR classification decisions belong with qualified export counsel.
- End user, destination, sanctions, and mission context still matter.

## Practical Reading Of The Repo

- The repository is meant to stay in a civilian, public-source lane.
- The baseline intentionally avoids command-path crypto/auth features.
- Public publication is different from private deployment or direct operational
  support.
- If the repository is reused in a real program, that program owns the final
  export and sanctions review.

## What Usually Changes The Risk

Risk goes up when a project moves away from the public baseline and into one of
these areas:

- military or weapons-related use
- private distribution or customer-specific delivery
- operational support tied to a real mission
- controlled third-party technical data
- features that change the export classification

## Good Default Rules

1. Keep the repository civilian and non-weaponized.
2. Keep command-path crypto/auth out of the public baseline.
3. Do not commit controlled third-party data or private defense material.
4. Use a private repository if access needs to be limited.
5. Get legal review before production deployment, private transfer, or export
   support work.

## Related Documents

- `docs/LEGAL_FAQ.md`
- `docs/LEGAL_SCOPE_CHECKLIST.md`
- `docs/MAINTAINER_BOUNDARY_POLICY.md`
