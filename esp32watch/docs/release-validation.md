# Release Validation

## 1. Purpose
This document describes the supported path from local build outputs to a candidate bundle and then to a verified bundle.
It replaces ad hoc smoke notes with one authoritative release-validation procedure.

## 2. Bundle types
### Candidate bundle
Produced by `tools/package_release.py`.
A candidate bundle contains:
- `firmware.bin`
- `littlefs.bin`
- `asset-contract.json`
- `contract-bootstrap.json`
- partition contract
- `release-validation.json`
- normalized host validation report
- repository docs needed for review

Candidate bundles require:
- `hostValidationStatus=PASS`
- a normalized host validation report
- no embedded verified device evidence

### Verified bundle
Produced only by `tools/promote_release_bundle.py`.
A verified bundle is a candidate bundle plus:
- normalized live device smoke report
- scenario evidence suitable for promotion
- manifest and validation metadata consistent with the exact source candidate

## 3. Required evidence layers
### Host validation
Minimum host validation covers:
- partition contract check
- Web runtime smoke
- host C/C++ sanity
- runtime contract checks
- host behavior checks
- bundle verification

### Flash evidence
Flash evidence must bind the exact candidate firmware/LittleFS hashes to the device that was flashed.

### Device smoke report
The device smoke report must be captured against the exact candidate bundle and must validate:
- runtime contract/schema responses
- aggregate/meta alignment
- tracked reset/flush behavior
- device identity and bundle-hash binding

### Scenario evidence
Verified promotion additionally requires scenario evidence for:
- provisioning recovery
- persistence across restart/power-cycle action
- boot-loop recovery

Scenario evidence is not accepted as PASS unless command evidence is present for the required external actions.

## 4. Local release flow
### Step 1 — build firmware and LittleFS
```bash
pio run -e <env>
pio run -e <env> -t buildfs
```

### Step 2 — run host validation
```bash
./tools/host_sanity_check.sh
```
Or run the relevant host checks individually if needed.

### Step 3 — write host validation report
```bash
python3 tools/write_host_validation_report.py \
  --env <env> \
  --output dist/host-validation.json \
  --runner LOCAL_HOST \
  --status PASS \
  --check asset-contract=PASS \
  --check partition-contract=PASS \
  --check web-runtime-smoke=PASS \
  --check host-c-syntax=PASS \
  --check host-cpp-sanity=PASS \
  --check runtime-contract=PASS \
  --check host-behavior=PASS \
  --check firmware-build=PASS \
  --check littlefs-build=PASS \
  --check bundle-verify=PASS
```

### Step 4 — package candidate bundle
```bash
python3 tools/package_release.py \
  --env <env> \
  --output-dir dist \
  --host-validation-status PASS \
  --host-validation-report dist/host-validation.json
```

### Step 5 — verify candidate bundle
```bash
python3 tools/verify_release_bundle.py \
  --env <env> \
  --bundle dist/esp32watch-<env>-candidate.zip
```

### Step 6 — flash exact candidate and capture flash evidence
```bash
python3 tools/flash_candidate_bundle.py \
  --bundle dist/esp32watch-<env>-candidate.zip \
  --port <serial-port> \
  --output dist/flash-evidence.json
```

### Step 7 — run live scenarios and capture command evidence
```bash
python3 tools/run_device_scenarios.py \
  --base-url http://<device-ip> \
  --auth-token <token> \
  --device-id <id> \
  --wifi-password <wifi-pass> \
  --power-cycle-cmd <cmd> \
  --fault-inject-cmd <cmd> \
  --candidate-bundle dist/esp32watch-<env>-candidate.zip \
  --output dist/device-scenario-evidence.json
```

### Step 8 — capture device smoke report with embedded scenario evidence
```bash
python3 tools/capture_device_smoke_report.py \
  --base-url http://<device-ip> \
  --env <env> \
  --device-id <serial-or-label> \
  --candidate-bundle dist/esp32watch-<env>-candidate.zip \
  --flash-evidence dist/flash-evidence.json \
  --scenario-evidence dist/device-scenario-evidence.json \
  --output dist/device-smoke-report.json \
  --auth-token <token-if-required>
```

`promote_release_bundle.py` consumes only the device smoke report, so the smoke report used for verified promotion must already embed the final scenario evidence payload.

### Step 9 — promote verified bundle
```bash
python3 tools/promote_release_bundle.py \
  --env <env> \
  --bundle dist/esp32watch-<env>-candidate.zip \
  --device-smoke-report dist/device-smoke-report.json
```

### Step 10 — verify verified bundle
```bash
python3 tools/verify_release_bundle.py \
  --env <env> \
  --bundle dist/esp32watch-<env>-verified.zip
```

## 5. CI validation sequence
The repository CI flow is defined in `.github/workflows/platformio-build.yml`.
The intended sequence is:
1. generate contract assets and run host validation
2. build firmware/LittleFS for the target environment
3. package the exact candidate bundle
4. flash the exact candidate and capture flash evidence on the device runner
5. run live scenarios and capture command evidence
6. capture a device smoke report that embeds the scenario evidence
7. promote the exact candidate into a verified bundle
8. re-run bundle verification on the verified artifact

CI is expected to fail closed when:
- host validation is incomplete or failing
- flash evidence does not match the candidate payload
- scenario evidence is missing required command evidence
- device smoke evidence does not bind to the exact candidate hashes
- verified bundle contents diverge from the promoted candidate lineage

## 6. Promotion gate rules
Verified promotion must reject reports when any of the following is false:
- top-level device smoke status is `PASS`
- candidate hash/name matches the promoted bundle source
- flash evidence hashes match the candidate payload
- runtime contract/schema evidence matches the bundle contract payload
- scenario evidence exists and matches the smoke report evidence copy
- command evidence exists for required external actions
- required boot-count / recovery proof fields are present

## 7. What this process proves
This flow can prove:
- host validation attached to the exact candidate bundle
- device smoke report bound to the exact candidate bundle
- scenario runner executed and produced required evidence artifacts
- verified bundle promotion only accepted evidence that passed schema and bundle-identity checks

## 8. What this process still does not prove by itself
This process does not magically prove physical truth beyond the runner environment.
The following still depend on the real lab/runner setup:
- whether `power-cycle-cmd` truly performs physical power removal and restoration
- whether `fault-inject-cmd` truly triggers the intended low-level failure path
- board-specific electrical correctness and timing margins
- real-world Wi-Fi/network reliability outside the controlled environment

## 9. Documentation hygiene note
`data/contract-bootstrap.json` and `data/asset-contract.json` are runtime assets. They are intentionally not treated as prose documentation and must stay in `data/`.
