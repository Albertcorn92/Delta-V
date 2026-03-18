# Tailoring And Scope Deviations Baseline

Date: 2026-03-14
Scope: public DELTA-V framework baseline

This record explains which high-assurance expectations are intentionally kept
outside the public DELTA-V baseline and why. It exists so reviewers do not have
to infer whether a gap is accidental or deliberate.

## Intentional Public-Baseline Deviations

| Area | Public Baseline Position | Why | Closure Owner |
|---|---|---|---|
| Program-approved compliance/tailoring matrix | Not included as a signed authority artifact | A public maintainer cannot approve requirement applicability on behalf of a real mission | Adopting mission + technical authority |
| Protected uplink security controls | Excluded from the repository baseline | Keeps the public repo civilian and avoids presenting the baseline as an operationally secure uplink stack | Adopting mission |
| Independent assurance authority | Not provided by a single-maintainer public repo | Cannot be claimed honestly without a real separate authority chain | Adopting mission |
| Formal IV&V / software assurance findings | Not closed in the repository baseline | Requires an independent organization or mission software-assurance function | Adopting mission |
| Formal review/inspection pedigree | Partially approximated with review records only | Repository review records are useful, but they are not a substitute for project-governed formal inspections and closure boards | Adopting mission |
| Tool qualification or formally approved tool deviations | Not claimed | Tool acceptance depends on mission classification, process, and authority approval | Adopting mission |
| Mission hardware qualification | Outside repository scope | Hardware, TVAC, EMI/EMC, vibration, and sensor-attached evidence require real mission articles | Adopting mission |
| Flight approval / certification | Not claimed | Public GitHub evidence is not program approval | Adopting mission |
| Tagged release pedigree for in-progress worktree | Left open until release ceremony | Exact-tag/clean-tree evidence only makes sense on the release commit | Maintainer |

## Public Baseline Principle

If an item is intentionally out of scope, the repository should:

- say so directly,
- avoid language that implies the item is already closed,
- and provide the next owner for real closure.
