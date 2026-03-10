# DELTA-V Software Portability Matrix

Date: 2026-03-10

This matrix validates software profile portability without requiring mission
hardware. It is focused on host/SITL compile-path coverage.

## Validation Command

```bash
cmake --build build --target portability_matrix
```

This runs `tools/portability_matrix.py` and writes artifacts to:

- `build/portability/portability_matrix.json`
- `build/portability/portability_matrix.md`
- `build/portability/logs/`

## Profile Matrix

| Profile ID | Build Type | Compile Defines | Purpose |
|---|---|---|---|
| `host-debug-default` | Debug | none | Baseline host/SITL debug portability |
| `host-release-default` | Release | none | Baseline release-mode portability |
| `host-debug-local-only` | Debug | `DELTAV_LOCAL_ONLY` | Validate local-only uplink compile path |
| `host-debug-network-disabled` | Debug | `DELTAV_DISABLE_NETWORK_STACK` | Validate no-network bridge compile path |
| `host-debug-heapguard-off` | Debug | `DELTAV_DISABLE_HOST_HEAP_GUARD` | Validate host compile path with heap-guard lock disabled |

## CI Usage (Recommended)

Configure-only sweep (fast):

```bash
python3 tools/portability_matrix.py \
  --workspace . \
  --output-dir build/portability \
  --configure-only
```

Compile sweep (full):

```bash
python3 tools/portability_matrix.py \
  --workspace . \
  --output-dir build/portability
```

## Scope Reminder

- Civilian/open baseline only.
- No command-path cryptography/encryption additions in baseline profiles.
