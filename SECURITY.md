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

- The default uplink key in source is for development/SITL only.
- Set `DELTAV_UPLINK_KEY_HEX` in deployment environments (32 hex chars).
- Set `DELTAV_REPLAY_SEQ_FILE` to persist anti-replay sequence state.
- Production deployments must provision mission keys outside public source control.
- Validate replay protection behavior in on-target test campaigns.
