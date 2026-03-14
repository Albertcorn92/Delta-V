# Static Analysis Deviation Log

Date: 2026-03-14
Scope: public DELTA-V framework baseline

This record tracks static-analysis deviations for the public repository. It is
meant to make review decisions explicit. It is not a waiver authority record
for a flight program.

## Rules

- Blocking `tidy_safety` findings must be fixed before the public baseline is
  released unless a deviation is recorded here.
- Each deviation must identify the rule, affected files, rationale, and closure
  condition.
- Deviations must not be used to hide safety-significant defects.

## Current Deviations

At the current public baseline, there are no approved open deviations against
the blocking `tidy_safety` gate.

## Future Entry Template

| ID | Rule / Tool | Files | Reason | Risk Control | Owner | Status |
|---|---|---|---|---|---|---|
| SAD-001 | example-check | `src/example.hpp` | Why the issue is accepted temporarily | What prevents unsafe use | Maintainer | OPEN |
