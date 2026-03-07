# Legal FAQ (Civilian Open-Source Baseline)

Date: 2026-03-07

This FAQ is process guidance only and is **not legal advice**.

## 1) Can I publish DELTA-V publicly on GitHub?

For this baseline repository, public publication is generally aligned with its
civilian open-source scope, provided you do not include restricted third-party
technical data, private defense data, or prohibited content.

## 2) Do I have to file paperwork or email a government office to publish this repo?

For normal public open-source publication of this civilian baseline, most users
do not perform special filings just to push code.
Production/export operations are different and may require formal review.

## 3) How do I keep risk low and stay in civilian scope?

1. Keep the repo civilian/non-weaponized.
2. Do not add command-path cryptography/encryption to the baseline.
3. Do not add controlled technical data or private defense documentation.
4. Run `python3 tools/legal_compliance_check.py` before release.
5. Run `cmake --build build --target software_final` before release.

## 4) What changes increase ITAR/EAR or sanctions risk?

- Adding military/weapon targeting behavior.
- Importing or distributing controlled third-party technical data.
- Adding command-path crypto/security protocol implementation to baseline.
- Supporting restricted destinations/end-users without screening.

## 5) Can this repo guarantee I am immune from legal liability?

No. No software repository can provide blanket legal immunity.
Users and deployers are responsible for export, sanctions, and local-law
compliance in their own mission and business context.

## 6) When should I ask qualified export counsel?

- Before commercial/production deployments.
- Before international customer support or distribution.
- If you add features that may affect export classification.
- If you are uncertain about destination/end-user restrictions.

## 7) What is the maintainer support boundary for this public repo?

- Framework code and docs are published publicly for civilian use.
- The maintainer does not provide direct operational support to non-U.S. users.
- Requests for deployment/export consulting may be declined.
- Users remain responsible for their own compliance decisions.

See `docs/MAINTAINER_BOUNDARY_POLICY.md`.
