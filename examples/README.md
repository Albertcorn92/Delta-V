# Examples

This directory is the entry point for reference mission profiles and adoption
examples.

The public DELTA-V baseline currently ships one main example path:

- [cubesat_attitude_control](cubesat_attitude_control/README.md): a civilian
  CubeSat-style reference mission built from the repository's default topology
  and runtime components

Important boundary:

- the current example is not a separate product line or independent example
  binary
- it is a documented reference mission profile layered on top of the baseline
  runtime in `topology.yaml`
- the example stays inside the same civilian/public-source scope as the rest of
  the repository

If you are new to the repo, use this order:

1. `README.md`
2. `docs/QUICKSTART_10_MIN.md`
3. `examples/cubesat_attitude_control/README.md`
4. `docs/REFERENCE_MISSION_WALKTHROUGH.md`
