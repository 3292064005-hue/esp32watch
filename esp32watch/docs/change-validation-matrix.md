# Change Validation Matrix

This document defines the mandatory evidence set for every user-visible change.
It is part of the release gate and is referenced by the PR template and host sanity checks.

## Change classes

### 1. Route change
Required:
- runtime contract updated when the exposed schema or semantics change
- `data/contract-bootstrap.json` regenerated
- `data/asset-contract.json` regenerated
- host contract output validation passes
- route owner / consumer / deprecation policy declared

### 2. Config change
Required:
- authority-layer validation updated
- runtime reload domain mapping reviewed
- runtime reload/event manifest updated when domain or event topology changes
- hot-apply vs reboot-required behavior documented
- host behavior coverage added or updated
- migration/backward-compatibility impact recorded

### 3. Runtime request change
Required:
- unified runtime side-effect executor updated
- normal-path and reset-path behavior covered
- storage commit policy impact reviewed
- host behavior coverage added or updated

### 4. Reset semantic change
Required:
- reset response schema reviewed
- storage staging / commit behavior reviewed
- factory reset and app-state reset paths both checked
- host reset behavior coverage added or updated

### 5. Storage semantic change
Required:
- storage semantics catalog reviewed
- runtime detail/diagnostic surface reviewed
- release validation bundle expectations reviewed
- host storage semantics coverage added or updated

## Device validation policy

Device validation is mandatory when a change alters one of the following:
- flash / persistence guarantees
- board profile wiring or peripheral bring-up
- display transport behavior
- sleep / wake sequencing
- Wi-Fi provisioning or credential application

When device validation is not run, the delivery must explicitly state that it is limited to host/static verification.
