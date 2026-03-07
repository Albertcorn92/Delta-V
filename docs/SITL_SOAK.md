# SITL Soak Gate

Date: 2026-03-06

This gate runs an extended local SITL runtime to catch issues that short
smoke tests may miss (unexpected exits, delayed runtime faults, allocator
regressions, and fatal signatures).

## Run

```bash
cmake --build build --target sitl_soak
```

Default soak duration is 180 seconds. Override at configure time:

```bash
cmake -B build -DDELTAV_SITL_SOAK_SECONDS=300
cmake --build build --target sitl_soak
```

## Direct Script Use

```bash
python3 tools/sitl_soak.py --build-dir build --duration 180
```

## Pass Criteria

- Process remains alive through the configured soak duration.
- Startup/runtime markers are present.
- No fatal signatures are detected in captured logs.

This is software-only evidence and does not replace hardware HIL campaigns.
