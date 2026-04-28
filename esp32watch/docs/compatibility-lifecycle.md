# Compatibility Lifecycle Registry

This document codifies the active compatibility and rollback surfaces retained by the runtime after the P1 compatibility cleanup pass. The machine-readable source of truth for these surfaces lives in `tools/compatibility_registry.json`, and host tests enumerate that registry rather than relying on hand-maintained one-off assertions.

## Active compatibility surfaces

| Surface | Owner | Reason retained | Removal condition |
| --- | --- | --- | --- |
| `LEGACY_CONSUMER_FIRST` init profile | system init | Validate migration order and rollback during storage authority transition | Remove after two release cycles with no rollback use |
| Compatibility state snapshot (`model_get`, `model_get_domain_state`) | model runtime | Preserve read-only consumers while telemetry moved to authoritative services | Remove when all consumers switch to authoritative runtime surfaces |
| Legacy command aliases in Web/API | web control plane | Support previously shipped automation callers | Remove after command catalog version bump and migration note |

## Rules

1. No new feature may depend on a compatibility-only surface.
2. Every retained compatibility path must have an owner, a removal condition, and a host test.
3. Deprecation warnings belong in docs and release notes, not in runtime-only tribal knowledge.


## Deprecated surface handling
- Deprecated compatibility aliases must remain discoverable through the command catalog with `lifecycle=DEPRECATED` and a `canonicalType` pointer.
- Deprecated paths may remain callable only for bounded migration windows; new code and tests must target the active contract surface.


## Engineering guardrail

Compatibility governance is backed by a machine-readable registry at `tools/compatibility_registry.json` and the following host tests:

- `tools/host_tests/test_compatibility_lifecycle.py` — enumerates every registered compatibility surface, validates ownership/removal metadata, and checks declared evidence snippets.
- `tools/host_tests/test_command_catalog_compatibility.c` — asserts deprecated command aliases resolve to the canonical command without reappearing as active catalog entries.
- `tools/host_tests/test_model_runtime_split.c` — keeps the compatibility read-side shims (`model_get`, `model_get_domain_state`) under host coverage.
- `tools/host_tests/test_system_runtime_service_init.cpp` — keeps the `LEGACY_CONSUMER_FIRST` init profile under host coverage.


## Archived legacy boundary

See `docs/legacy-archive.md` for the active rule: legacy/Bluepill surfaces are retained as archive material and are not part of the active ESP32-S3 runtime path.
