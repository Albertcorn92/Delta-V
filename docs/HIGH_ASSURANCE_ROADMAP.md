# DELTA-V High-Assurance Roadmap

Date: 2026-03-06

## Current Baseline (verified)

- `flight_software` builds successfully.
- `run_tests` builds and all unit tests pass (`ctest`).
- `clang-tidy` target runs successfully (advisory warnings remain).
- Heap guard replacement operators are linked once per binary (`HeapGuard.cpp`).

## High-Priority Gaps to Operational-Grade FSW

1. Process and assurance evidence
- Add formal requirements-to-test trace matrix (`DV-*` requirement -> test case -> result).
- Add code review evidence templates and signed approvals for safety-critical changes.
- Add release artifacts for audits: exact toolchain versions, hashes, and reproducible build metadata.

2. Safety and determinism policy
- Define and enforce no-heap-after-init policy in CI with dedicated runtime tests.
- Add deterministic startup/shutdown sequencing requirements and verification points.
- Remove exception-prone APIs from flight paths (or document bounded exception handling strategy).

3. Static analysis and coding standard conformance
- Split static analysis into:
  - blocking safety profile (must pass),
  - advisory modernization profile.
- Establish MISRA/CERT deviation records for every intentional violation.
- Reduce high-signal warnings first (bounds, varargs, nullability, unchecked conversions).

4. Verification depth
- Add integration tests for end-to-end command/telemetry/event chains.
- Add fault-injection campaign tests (sensor faults, route overflow, watchdog escalation, recovery).
- Add stress and soak tests with pass/fail budgets (drops, latency, restart counts).

5. Security and uplink hardening
- Add command-frame parser robustness tests and anti-replay persistence tests.
- Add invalid/malformed frame fuzz tests for uplink parser and COBS/serializer boundaries.
- Add secure-by-default command allowlist per mission state.

## Changes Implemented in This Pass

- Fixed event hub fan-out typing and runtime behavior.
- Fixed logger event string handling (`std::array<char,...>` path).
- Reworked port abstraction for mixed queue depths and restored queue `size()`.
- Fixed topology mission-state wiring for command gating.
- Removed `CommandHub` null function-pointer path that caused segfault.
- Fixed scheduler ownership bug (dangling pointer dereference in destructor).
- Added explicit scheduler `shutdown()` API for lifecycle control.
- Fixed COBS decode precondition bug causing false decode failure.
- Added compatibility hooks for legacy tests in TMR and COBS interfaces.
- Improved `clang-tidy` target behavior and macOS sysroot resolution.
- Moved global `new/delete` overrides out of header into `HeapGuard.cpp`.

## Next Code Batch (recommended immediate execution)

1. Safety profile CI gate
- Promote a curated `clang-tidy` safety subset to blocking in CI.
- Keep modernization checks advisory.

2. Mission-critical string and formatting cleanup
- Replace `snprintf` patterns with bounded helpers and explicit truncation checks.
- Remove suspicious `string_view::data()` usage in watchdog diagnostics.

3. Traceability automation
- Generate requirement coverage report from unit test names/tags in CI artifact output.

4. Fault campaign harness
- Add a dedicated test binary for long-running fault injection scenarios with deterministic seeds.
