# Runtime / Control Plane / Rich Console Boundaries

This document formalizes the three surface layers used by the project after the P2 surface-boundary pass.

## 1. Runtime core

The runtime core owns device truth, persistence, staged loop execution, and authority services.
It must remain functional even when the richer console assets are unavailable.

## 2. Control plane

The control plane exposes the minimum operational HTTP surfaces required for health, config, tracked actions, and reset flows.
This layer must remain schema-stable and degrade gracefully when the console asset pack is absent.

## 3. Rich console

The rich console is the LittleFS-delivered Web application (`index.html`, `app.js`, `app.css`, `contract-bootstrap.json`).
It is allowed to evolve faster than the control plane, but only through versioned contracts and verified asset manifests.

## Degradation policy

- Runtime alive + control plane alive + console unavailable: allowed degraded mode.
- Runtime alive + console alive but control plane drifted from contract: not allowed.
- Runtime unavailable: console must not imply false readiness.


## Route ownership and tiers

The runtime contract now classifies each exposed route with four governance fields:

- `tier`: `control`, `console`, `diagnostic`, `tooling`, or `compatibility`
- `producerOwner`: module that owns the emitted payload
- `consumerOwner`: primary consumer expected to depend on the route
- `deprecationPolicy`: stability or retirement rule for the route

Rules:

- `control` routes must remain backward compatible unless the runtime contract version and bootstrap assets are updated together.
- `diagnostic` and `tooling` routes may grow optional fields, but must keep their documented core schema intact.
- `compatibility` routes must not be expanded indefinitely; they need a named replacement and an explicit retirement target.
- New routes are not accepted unless owner, consumer, and deprecation policy are declared in the runtime contract.

## Payload and mutation metadata

### Aggregate state revision

`/api/state/aggregate` remains section-driven, but it also publishes a top-level `stateRevision` field.
The rich console uses `stateRevision` as the primary render gate for aggregate snapshots, and contract route metadata now records that this field is part of the documented response surface for state routes.

### Explicit runtime reload requests

`/api/config/device` accepts an optional `runtimeReloadDomains` mutation field.
It may be supplied as a comma-separated form value or JSON string and is resolved into the runtime reload registry domain mask.
This field is documented as part of the route schema metadata so route consumers and validation tooling can distinguish persisted configuration writes from explicit runtime follow-up requests.

Ordinary configuration writes now also fan out into the runtime reload registry without requiring an explicit override.
- Wi-Fi credential changes naturally request `WIFI`, `POWER`, and `COMPANION` follow-up domains.
- Network profile changes naturally request `NETWORK`, `DISPLAY`, `POWER`, `SENSOR`, and `COMPANION` follow-up domains.
- API token changes naturally request `AUTH` and `COMPANION` follow-up domains.

`runtimeReloadDomains` remains additive: it can request extra follow-up domains, but the default configuration path no longer relies on it to reach non-Wi-Fi / non-Network runtime consumers.
