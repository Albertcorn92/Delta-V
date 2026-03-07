# Export Control Note (Not Legal Advice)

Date: 2026-03-06

This repository is published as open-source software for civilian development
and research use. Export-control treatment still depends on mission context,
payload, customer, destination, and operational use.

## Important

- This document is informational only and is **not legal advice**.
- This repository alone cannot provide legal clearance or eliminate legal liability.
- Final ITAR/EAR determination must be made by qualified export counsel.
- Publishing this repo does not create a blanket ITAR/EAR exemption for every use.

## Practical Guidance

- Publicly available source-code treatment under U.S. rules is often relevant
  for open-source distribution.
- The baseline repository profile intentionally avoids command-path
  cryptographic features to reduce export-compliance complexity.
- For typical public open-source publication of this civilian baseline, users
  often do not perform special filings solely to publish code; deployment and
  export operations can still require formal review.
- ITAR applicability depends on whether software/technical data is specifically
  controlled as a defense article/technical data for USML use.
- Sanctions obligations (for end user, destination, and prohibited parties)
  remain applicable to deployment and support activities.

Plain-language Q&A: see `docs/LEGAL_FAQ.md`.

## Suggested Pre-Release Controls

1. Keep the project scoped to civilian, non-weaponized use.
2. Keep command-path cryptography/encryption features out of the baseline.
3. Obtain an export-classification memo from counsel before production release.
4. Prohibit committed production secrets in CI policy.
5. Maintain destination/end-user screening in deployment operations.
