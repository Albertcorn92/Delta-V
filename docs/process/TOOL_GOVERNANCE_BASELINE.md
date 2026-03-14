# DELTA-V Tool Governance Baseline

Date: 2026-03-14
Scope: public DELTA-V framework baseline

This document records how the repository uses build, generation, verification,
and reporting tools. It is a governance record, not a claim of formal tool
qualification.

## Tool Categories

| Tool | Category | Role | Primary control |
|---|---|---|---|
| `clang++`, `g++`, `cmake`, `ninja` | Build | Compile and link the runtime and tests | version capture in reports, CI cross-checks |
| `git` | Configuration management | Baseline identity, tags, and worktree state | release pedigree checks |
| `tools/autocoder.py` | Generation | Generate topology manager, type headers, and dictionary | `autocoder_check` and CI determinism check |
| `tools/dv-util.py` | Development support | Update topology and scaffold components | review generated outputs and rerun generation checks |
| `ctest` + GTest | Verification | Unit and system test execution | blocking gates and traceability |
| `clang-tidy` | Static analysis | Safety-oriented static checks | `tidy_safety` |
| `lcov` + coverage scripts | Coverage reporting | Coverage metrics and trend capture | dedicated GCC build and threshold guard |
| Benchmark and readiness scripts in `tools/` | Reporting | Summaries, guards, readiness snapshots | source control, review, and CI syntax checks |
| GitHub Actions | CI orchestration | Repeatable gate execution | workflow review and artifact archiving |

## Failure Modes and Controls

- Build-tool defects can hide compiler-specific faults.
  Control: build on multiple compilers and platforms.
- Generation-tool defects can produce stale or incorrect wiring.
  Control: generation is deterministic and checked against source-of-truth files.
- Verification-tool defects can overstate quality.
  Control: multiple gate types are used, and release artifacts retain raw inputs.
- Reporting-tool defects can produce misleading closeout documents.
  Control: generated reports depend on checked artifacts and are version-controlled.

## Release Use Rules

- `software_final` requires the controlled process baseline documents to exist.
- `release_candidate` requires a clean tagged worktree and generates current
  release manifest and pedigree artifacts.
- Any tool change that affects generated code, release evidence, or safety gates
  must be reviewed as a safety-relevant change.

## Mission-Team Handoff

Mission adopters must decide whether any of these tools require formal
qualification, independent verification, or a documented deviation disposition
under their program rules.
