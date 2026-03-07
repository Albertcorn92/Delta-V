# Security Policy

## Supported Versions

Security fixes are applied to the latest `main` branch baseline.

## Reporting a Vulnerability

- Do not open public GitHub issues for vulnerabilities.
- Report privately to the project maintainer with:
  - affected commit/tag
  - impact description
  - minimal reproduction steps
  - suggested fix (if available)

## Security Notes for Integrators

- Baseline DELTA-V intentionally excludes command-path cryptography/encryption.
- On host/SITL, unauthenticated UDP uplink ingest is disabled by default.
- Set `DELTAV_ENABLE_UNAUTH_UPLINK=1` only for controlled local testing.
- Optionally restrict host uplink source IP with `DELTAV_UPLINK_ALLOW_IP` (defaults to `127.0.0.1`).
- Set `DELTAV_REPLAY_SEQ_FILE` to persist anti-replay sequence state.
- Production missions should implement private, program-owned command security controls outside this baseline.
- Validate replay protection behavior in on-target test campaigns.
