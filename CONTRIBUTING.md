# Contributing to DELTA-V

Thanks for contributing.

## Development Setup

1. Install prerequisites:
- CMake 3.15+
- C++20 compiler
- Python 3.9+

2. Build and test:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target flight_readiness
cmake --build build --target qualification_bundle
```

3. If topology changes:

```bash
./venv/bin/python tools/autocoder.py
```

## Pull Request Requirements

- Keep changes focused and atomic.
- Include tests for behavior changes.
- Update requirements mapping if adding/changing safety-critical behavior:
  - `src/Requirements.hpp`
  - `docs/REQUIREMENTS_TRACE.yaml`
- Ensure `flight_readiness` and `qualification_bundle` both pass locally.
- Ensure legal compliance gate passes locally:
  - `python3 tools/legal_compliance_check.py`

## Legal and Scope Requirements

- DELTA-V is scoped to civilian, scientific, industrial, and educational use.
- Contributions that add military, weapons, targeting, or fire-control behavior
  are out of scope and will be rejected.
- Do not add command-path cryptography/encryption features in the baseline framework.
- Do not submit controlled or restricted third-party technical data.
- Do not introduce wording that claims guaranteed legality, zero liability, or
  guaranteed ITAR/EAR exemption.
- Follow `docs/CIVILIAN_USE_POLICY.md` and `docs/EXPORT_CONTROL_NOTE.md`.
- This project does not provide legal advice or legal clearance.

## Coding Guidelines

- No dynamic allocation in flight phase after `HeapGuard::arm()`.
- Prefer bounded/static storage and deterministic behavior.
- Keep interfaces explicit and testable.
- Avoid breaking generated files manually; update `topology.yaml` and `tools/autocoder.py` when needed.
