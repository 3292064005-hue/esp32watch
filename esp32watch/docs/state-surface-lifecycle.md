# State Surface Lifecycle Matrix

This document classifies runtime state fields by consumer tier so that production, diagnostics, and compatibility data do not drift together.

## Tiers

- **control**: required by operational control paths and automated tooling.
- **console**: used by the rich Web console and debugging workflows.
- **diagnostics**: emitted for troubleshooting and lab validation.
- **compatibility**: retained only for migration safety.

## Current matrix

| Surface | Tier | Primary consumer | Notes |
| --- | --- | --- | --- |
| `/api/config/device` | control | provisioning + config writes | Must remain stable and schema-validated |
| `/api/actions/*` | control | tracked action flow | authoritative command acknowledgement path |
| `/api/state/aggregate` | console | rich Web console | may include diagnostic sections but should remain coherent |
| `/api/state/detail`, `/api/state/perf`, `/api/state/raw` | diagnostics | lab / debugging | additive diagnostics allowed; removals require doc update |
| `model_get`, `model_get_domain_state` | compatibility | legacy readers | read-only shim surface |
| `/api/storage/semantics` | control + diagnostics | tooling + console | now includes backend capability metadata |

## Governance

1. New fields must declare a consumer tier before landing.
2. Compatibility fields may not expand without a documented migration owner.
3. Diagnostic-only fields should not be consumed by the control plane.
