# Export Control Note (Not Legal Advice)

Date: 2026-03-06

This repository is published as open-source software for civilian development
and research use. Export-control treatment still depends on mission context,
payload, customer, destination, and operational use.

## Important

- This document is informational only and is **not legal advice**.
- This repository alone cannot provide legal clearance or eliminate legal liability.
- Final ITAR/EAR determination must be made by qualified export counsel.

## Practical Guidance

- Publicly available source-code treatment under U.S. rules is often relevant
  for open-source distribution.
- The baseline repository profile intentionally avoids command-path
  cryptographic features to reduce export-compliance complexity.
- ITAR applicability depends on whether software/technical data is specifically
  controlled as a defense article/technical data for USML use.
- Sanctions obligations (for end user, destination, and prohibited parties)
  remain applicable to deployment and support activities.

## Suggested Pre-Release Controls

1. Keep the project scoped to civilian, non-weaponized use.
2. Obtain an export-classification memo from counsel before production release.
3. Prohibit committed production secrets in CI policy.
4. Maintain destination/end-user screening in deployment operations.
