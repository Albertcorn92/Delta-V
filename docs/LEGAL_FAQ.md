# Legal FAQ

Date: 2026-03-07

This FAQ is general process guidance only. It is **not legal advice**.

## Is it okay to publish this repo on GitHub?

That is the intended use for the public DELTA-V baseline, as long as the repo
stays civilian, public, and free of restricted third-party material.

## Does public GitHub publication solve export-control questions by itself?

No. Public publication helps define the baseline, but it does not answer every
question about export controls, sanctions, destinations, or mission use.

## Do I usually need to file something just to publish the baseline repo?

For normal public open-source publication of this baseline, most people do not
file special paperwork just to push code. That changes once the work turns into
private delivery, operational support, or production deployment.

## What keeps the repo in the lowest-risk posture?

1. Keep it civilian and non-weaponized.
2. Do not add command-path crypto/auth to the public baseline.
3. Do not add controlled third-party technical data.
4. Run `python3 tools/legal_compliance_check.py` before release.
5. Run `cmake --build build --target software_final` before release.

## What kinds of changes increase the risk?

- military, weapons, targeting, or fire-control behavior
- private or restricted technical data
- security features that change classification
- support for restricted destinations or end users without screening

## Does this repo protect anyone from legal liability?

No. The repository can narrow project scope, but it does not give blanket legal
protection. Anyone deploying or distributing the software is responsible for
their own compliance decisions.

## When is export counsel a good idea?

- before a commercial or production deployment
- before private international distribution or support
- before adding features that may change classification
- when destination or end-user restrictions are not clear

## What support does the maintainer provide?

The public repo provides source code and documentation. The maintainer may
decline requests for deployment consulting, export consulting, or direct
operational support.

See `docs/MAINTAINER_BOUNDARY_POLICY.md` for the maintainer boundary.
