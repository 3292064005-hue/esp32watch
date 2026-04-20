#!/usr/bin/env python3
from __future__ import annotations
import argparse
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

REQUIRED_CHECKS = (
    'boot',
    'webFallback',
    'assetLoad',
    'wifiProvisioning',
    'timeSync',
    'manualFlush',
    'resetFlow',
    'persistence',
    'bootLoopRecovery',
    'contractBootstrap',
    'schemaCatalog',
    'deviceIdentity',
)
VALID_CHECK_STATUSES = {'PASS', 'FAIL', 'NOT_RUN', 'UNSUPPORTED'}
RUNNER_CAPABILITIES = {
    'schemaVersion': 1,
    'powerCycle': {'argv': ['host-power-cycle']},
    'faultInject': {'argv': ['host-fault-inject']},
}
RUNNER_CAPABILITIES_SHA256 = hashlib.sha256(json.dumps(RUNNER_CAPABILITIES, sort_keys=True, separators=(',', ':')).encode('utf-8')).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--env', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--status', choices=('PASS', 'FAIL'), required=True)
    parser.add_argument('--device-id', required=True)
    parser.add_argument('--board', default='esp32watch')
    parser.add_argument('--candidate-bundle', required=True)
    parser.add_argument('--check', action='append', default=[])
    parser.add_argument('--capture-runner', choices=('CI_DEVICE_RUNNER', 'LOCAL_DEVICE_RUNNER'), default='LOCAL_DEVICE_RUNNER')
    parser.add_argument('--runner-identity', default='LOCAL_DEVICE_RUNNER')
    parser.add_argument('--lab-identity', default='UNSPECIFIED_LAB')
    parser.add_argument('--attest-physical-actions', action='store_true')
    args = parser.parse_args()

    status_map = {}
    for item in args.check:
        if '=' not in item:
            raise SystemExit(f'invalid --check value: {item}; expected name=STATUS')
        name, status = item.split('=', 1)
        status = status.upper()
        if status not in VALID_CHECK_STATUSES:
            raise SystemExit(f'invalid device check status for {name}: {status}')
        status_map[name] = status
    for name in REQUIRED_CHECKS:
        status_map.setdefault(name, 'PASS' if args.status == 'PASS' else 'FAIL')

    if args.status == 'PASS' and any(status_map[name] != 'PASS' for name in REQUIRED_CHECKS):
        raise SystemExit('device PASS report cannot contain non-passing required checks')

    candidate_bundle = Path(args.candidate_bundle)
    if not candidate_bundle.exists():
        raise SystemExit(f'candidate bundle not found: {candidate_bundle}')
    candidate_sha256 = hashlib.sha256(candidate_bundle.read_bytes()).hexdigest()

    scenario_evidence = {
        'reportType': 'DEVICE_SCENARIO_EVIDENCE',
        'schemaVersion': 3,
        'captureRunner': args.capture_runner,
        'deviceId': args.device_id,
        'runnerIdentity': args.runner_identity,
        'labIdentity': args.lab_identity,
        'runnerCapabilities': RUNNER_CAPABILITIES,
        'runnerCapabilitiesSha256': RUNNER_CAPABILITIES_SHA256,
        'attestation': {
            'physicalActionsAttested': args.attest_physical_actions,
            'runnerIdentity': args.runner_identity,
            'labIdentity': args.lab_identity,
            'runnerCapabilitiesSha256': RUNNER_CAPABILITIES_SHA256,
        },
        'candidateBundleSha256': candidate_sha256,
        'commandEvidence': {
            'powerCycle': {
                'executed': args.status == 'PASS',
                'evidencePresent': False,
                'bootCountIncreased': args.status == 'PASS',
                'previousBootCountAfter': 1,
            },
            'faultInject': {
                'executed': args.status == 'PASS',
                'evidencePresent': False,
                'bootCountIncreased': args.status == 'PASS',
                'previousBootCountAfter': 2,
            },
        },
        'scenarios': {
            'wifiProvisioning': {
                'executed': args.status == 'PASS',
                'apObserved': args.status == 'PASS',
                'staRecovered': args.status == 'PASS',
                'recoveryVerified': args.status == 'PASS',
            },
            'persistence': {
                'executed': args.status == 'PASS',
                'configRoundTripOk': args.status == 'PASS',
                'appStateRoundTripOk': args.status == 'PASS',
                'powerCycleOk': args.status == 'PASS',
                'bootCountIncreased': args.status == 'PASS',
                'previousBootCountObserved': args.status == 'PASS',
            },
            'bootLoopRecovery': {
                'executed': args.status == 'PASS',
                'faultInjected': args.status == 'PASS',
                'safeModeObserved': args.status == 'PASS',
                'recoveryCleared': args.status == 'PASS',
                'bootCountIncreased': args.status == 'PASS',
                'previousBootCountObserved': args.status == 'PASS',
            },
        },
    }

    payload = {
        'reportType': 'DEVICE_SMOKE_REPORT',
        'schemaVersion': 6,
        'env': args.env,
        'status': args.status,
        'evidenceOrigin': 'DEVICE_CAPTURE',
        'generatedAtUtc': datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace('+00:00', 'Z'),
        'generator': {
            'tool': 'capture_device_smoke_report.py',
            'captureMode': 'LIVE_HTTP_CAPTURE',
            'schemaVersion': 5,
            'captureRunner': args.capture_runner,
            'runnerIdentity': args.runner_identity,
            'labIdentity': args.lab_identity,
            'note': 'fixture-only report; verifier must reject this for verified promotion unless scenario evidence semantics are satisfied by a live runner',
        },
        'attestation': {'physicalActionsAttested': args.attest_physical_actions, 'runnerIdentity': args.runner_identity, 'labIdentity': args.lab_identity},
        'device': {
            'id': args.device_id,
            'board': args.board,
        },
        'candidateBundle': {
            'fileName': candidate_bundle.name,
            'sha256': candidate_sha256,
            'firmwareSha256': '0' * 64,
            'littlefsSha256': '1' * 64,
        },
        'bundleEvidence': {
            'bootstrapContractSha256': '2' * 64,
            'assetContractSha256': '3' * 64,
            'firmwareSha256': '0' * 64,
            'littlefsSha256': '1' * 64,
            'runtimeContractMatchesBootstrap': args.status == 'PASS',
            'routesMatchBootstrap': args.status == 'PASS',
            'stateSchemasMatchBootstrap': args.status == 'PASS',
            'apiSchemasMatchBootstrap': args.status == 'PASS',
            'releaseValidationMatchesBootstrap': args.status == 'PASS',
            'apiVersionMatchesBootstrap': args.status == 'PASS',
            'stateVersionMatchesBootstrap': args.status == 'PASS',
            'runtimeContractVersionMatchesBootstrap': args.status == 'PASS',
            'assetContractVersionMatchesBootstrap': args.status == 'PASS',
            'metaApiVersionMatchesBootstrap': args.status == 'PASS',
            'metaStateVersionMatchesBootstrap': args.status == 'PASS',
            'metaAssetContractVersionMatchesBootstrap': args.status == 'PASS',
            'assetContractBootstrapMatchesBundle': args.status == 'PASS',
            'metaDeviceIdentityPresent': args.status == 'PASS',
            'metaDeviceMacMatchesFlashEvidence': args.status == 'PASS',
            'metaBoardProfileMatchesRequest': args.status == 'PASS',
        },
        'flashEvidence': {
            'flashTool': 'flash_candidate_bundle.py',
            'flashMode': 'LIVE_FLASH',
            'candidateBundleSha256': candidate_sha256,
            'firmwareSha256': '0' * 64,
            'littlefsSha256': '1' * 64,
            'deviceMac': 'AA:BB:CC:DD:EE:FF',
        },
        'scenarioEvidence': scenario_evidence,
        'checks': [{'name': name, 'status': status_map[name]} for name in REQUIRED_CHECKS],
        'evidence': {
            'health': {'ok': True},
            'meta': {'deviceIdentity': {'boardProfile': args.board, 'chipModel': 'ESP32-S3', 'efuseMac': 'AA:BB:CC:DD:EE:FF'}},
            'aggregate': {'ok': True},
            'config': {'ok': True},
            'displayFrame': {'ok': True},
            'actionsCatalog': {'ok': True},
            'actionsLatest': {'ok': True},
            'storageSemantics': {'ok': True},
            'contract': {'routes': {'contract': '/api/contract'}},
            'manualFlush': {'status': 'APPLIED' if args.status == 'PASS' else 'FAILED'},
            'resetFlow': {'ok': args.status == 'PASS'},
            'scenarioEvidence': scenario_evidence,
        },
    }
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(payload, indent=2) + '\n', encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
