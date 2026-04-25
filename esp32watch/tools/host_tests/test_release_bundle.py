#!/usr/bin/env python3
from __future__ import annotations
import hashlib
import json
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ENV = 'esp32s3_n16r8_dev'
BOOTSTRAP = json.loads((ROOT / 'data' / 'contract-bootstrap.json').read_text(encoding='utf-8'))
BUILD_DIR = ROOT / '.pio' / 'build' / ENV
OUTPUT_DIR = ROOT / 'dist' / 'host_test_bundle'
CANDIDATE_BUNDLE = OUTPUT_DIR / f'esp32watch-{ENV}-candidate.zip'
VERIFIED_BUNDLE = OUTPUT_DIR / f'esp32watch-{ENV}-verified.zip'
CORRUPT_BUNDLE = OUTPUT_DIR / f'esp32watch-{ENV}-candidate-corrupt.zip'
RUNNER_CAPABILITIES = {'schemaVersion': 1, 'powerCycle': {'argv': ['host-power-cycle']}, 'faultInject': {'argv': ['host-fault-inject']}}
RUNNER_CAPABILITIES_SHA256 = hashlib.sha256(json.dumps(RUNNER_CAPABILITIES, sort_keys=True, separators=(',', ':')).encode('utf-8')).hexdigest()


def write_device_report(path: Path, candidate_bundle: Path, status: str) -> None:
    candidate_sha256 = hashlib.sha256(candidate_bundle.read_bytes()).hexdigest()
    payload = {
        'reportType': 'DEVICE_SMOKE_REPORT',
        'schemaVersion': 6,
        'env': ENV,
        'status': status,
        'evidenceOrigin': 'DEVICE_CAPTURE',
        'generatedAtUtc': '2026-04-19T00:00:00Z',
        'generator': {
            'tool': 'capture_device_smoke_report.py',
            'captureMode': 'LIVE_HTTP_CAPTURE',
            'schemaVersion': 5,
            'captureRunner': 'LOCAL_DEVICE_RUNNER',
            'runnerIdentity': 'HOST_TEST_RUNNER',
            'labIdentity': 'HOST_TEST_LAB',
        },
        'attestation': {'physicalActionsAttested': status == 'PASS', 'runnerIdentity': 'HOST_TEST_RUNNER', 'labIdentity': 'HOST_TEST_LAB'},
        'device': {'id': 'HOST-TEST', 'board': 'esp32watch'},
        'candidateBundle': {
            'fileName': candidate_bundle.name,
            'sha256': candidate_sha256,
            'firmwareSha256': hashlib.sha256(b'FW').hexdigest(),
            'littlefsSha256': hashlib.sha256(b'FS').hexdigest(),
        },
        'bundleEvidence': {
            'bootstrapContractSha256': hashlib.sha256(json.dumps(BOOTSTRAP, sort_keys=True, separators=(',', ':')).encode('utf-8')).hexdigest(),
            'assetContractSha256': '3' * 64,
            'firmwareSha256': hashlib.sha256(b'FW').hexdigest(),
            'littlefsSha256': hashlib.sha256(b'FS').hexdigest(),
            'runtimeContractMatchesBootstrap': status == 'PASS',
            'routesMatchBootstrap': status == 'PASS',
            'stateSchemasMatchBootstrap': status == 'PASS',
            'apiSchemasMatchBootstrap': status == 'PASS',
            'releaseValidationMatchesBootstrap': status == 'PASS',
            'apiVersionMatchesBootstrap': status == 'PASS',
            'stateVersionMatchesBootstrap': status == 'PASS',
            'runtimeContractVersionMatchesBootstrap': status == 'PASS',
            'assetContractVersionMatchesBootstrap': status == 'PASS',
            'metaApiVersionMatchesBootstrap': status == 'PASS',
            'metaStateVersionMatchesBootstrap': status == 'PASS',
            'metaAssetContractVersionMatchesBootstrap': status == 'PASS',
            'assetContractBootstrapMatchesBundle': status == 'PASS',
            'metaDeviceIdentityPresent': status == 'PASS',
            'metaDeviceMacMatchesFlashEvidence': status == 'PASS',
            'metaBoardProfileMatchesRequest': status == 'PASS',
        },
        'flashEvidence': {
            'flashTool': 'flash_candidate_bundle.py',
            'flashMode': 'LIVE_FLASH',
            'candidateBundleSha256': candidate_sha256,
            'firmwareSha256': hashlib.sha256(b'FW').hexdigest(),
            'littlefsSha256': hashlib.sha256(b'FS').hexdigest(),
            'deviceMac': 'AA:BB:CC:DD:EE:FF',
        },
        'scenarioEvidence': {
            'reportType': 'DEVICE_SCENARIO_EVIDENCE',
            'schemaVersion': 3,
            'captureRunner': 'LOCAL_DEVICE_RUNNER',
            'runnerIdentity': 'HOST_TEST_RUNNER',
            'labIdentity': 'HOST_TEST_LAB',
            'runnerCapabilities': RUNNER_CAPABILITIES,
            'runnerCapabilitiesSha256': RUNNER_CAPABILITIES_SHA256,
            'attestation': {'physicalActionsAttested': status == 'PASS', 'runnerIdentity': 'HOST_TEST_RUNNER', 'labIdentity': 'HOST_TEST_LAB', 'runnerCapabilitiesSha256': RUNNER_CAPABILITIES_SHA256},
            'deviceId': 'HOST-TEST',
            'candidateBundleSha256': candidate_sha256,
            'commandEvidence': {
                'powerCycle': {'executed': status == 'PASS', 'evidencePresent': status == 'PASS', 'evidence': {'source': 'host-test', 'kind': 'power-cycle'} if status == 'PASS' else {}, 'bootCountIncreased': status == 'PASS', 'previousBootCountAfter': 1},
                'faultInject': {'executed': status == 'PASS', 'evidencePresent': status == 'PASS', 'evidence': {'source': 'host-test', 'kind': 'fault-inject'} if status == 'PASS' else {}, 'bootCountIncreased': status == 'PASS', 'previousBootCountAfter': 2},
            },
            'scenarios': {
                'wifiProvisioning': {'executed': status == 'PASS', 'apObserved': status == 'PASS', 'staRecovered': status == 'PASS', 'recoveryVerified': status == 'PASS'},
                'persistence': {'executed': status == 'PASS', 'configRoundTripOk': status == 'PASS', 'appStateRoundTripOk': status == 'PASS', 'powerCycleOk': status == 'PASS', 'bootCountIncreased': status == 'PASS', 'previousBootCountObserved': status == 'PASS'},
                'bootLoopRecovery': {'executed': status == 'PASS', 'faultInjected': status == 'PASS', 'safeModeObserved': status == 'PASS', 'recoveryCleared': status == 'PASS', 'bootCountIncreased': status == 'PASS', 'previousBootCountObserved': status == 'PASS'},
            },
        },
        'checks': [
            {'name': 'boot', 'status': 'PASS' if status == 'PASS' else 'FAIL'},
            {'name': 'webFallback', 'status': 'PASS' if status == 'PASS' else 'FAIL'},
            {'name': 'assetLoad', 'status': 'PASS' if status == 'PASS' else 'FAIL'},
            {'name': 'wifiProvisioning', 'status': 'PASS' if status == 'PASS' else 'FAIL'},
            {'name': 'timeSync', 'status': 'PASS' if status == 'PASS' else 'FAIL'},
            {'name': 'manualFlush', 'status': 'PASS' if status == 'PASS' else 'FAIL'},
            {'name': 'resetFlow', 'status': 'PASS' if status == 'PASS' else 'FAIL'},
            {'name': 'persistence', 'status': 'PASS' if status == 'PASS' else 'FAIL'},
            {'name': 'bootLoopRecovery', 'status': 'PASS' if status == 'PASS' else 'FAIL'},
            {'name': 'contractBootstrap', 'status': 'PASS' if status == 'PASS' else 'FAIL'},
            {'name': 'schemaCatalog', 'status': 'PASS' if status == 'PASS' else 'FAIL'},
            {'name': 'deviceIdentity', 'status': 'PASS' if status == 'PASS' else 'FAIL'},
        ],
        'evidence': {
            'contract': {'routes': {'meta': '/api/meta'}},
            'health': {'ok': True},
            'meta': {'apiVersion': BOOTSTRAP['apiVersion'], 'stateVersion': BOOTSTRAP['stateVersion'], 'assetContractVersion': BOOTSTRAP['assetContractVersion'], 'deviceIdentity': {'boardProfile': 'esp32watch', 'chipModel': 'ESP32-S3', 'efuseMac': 'AA:BB:CC:DD:EE:FF'}},
            'aggregate': {'ok': True},
            'displayFrame': {'ok': True},
            'actionsCatalog': {'ok': True},
            'actionsLatest': {'ok': True},
            'storageSemantics': {'ok': True},
            'config': {'ok': True},
            'manualFlush': {'status': 'APPLIED' if status == 'PASS' else 'FAILED'},
            'resetFlow': {'ok': status == 'PASS'},
            'scenarioEvidence': {
                'reportType': 'DEVICE_SCENARIO_EVIDENCE',
                'schemaVersion': 3,
                'captureRunner': 'LOCAL_DEVICE_RUNNER',
                'runnerIdentity': 'HOST_TEST_RUNNER',
                'labIdentity': 'HOST_TEST_LAB',
                'runnerCapabilities': RUNNER_CAPABILITIES,
                'runnerCapabilitiesSha256': RUNNER_CAPABILITIES_SHA256,
                'attestation': {'physicalActionsAttested': status == 'PASS', 'runnerIdentity': 'HOST_TEST_RUNNER', 'labIdentity': 'HOST_TEST_LAB', 'runnerCapabilitiesSha256': RUNNER_CAPABILITIES_SHA256},
                'deviceId': 'HOST-TEST',
                'candidateBundleSha256': candidate_sha256,
                'commandEvidence': {
                    'powerCycle': {'executed': status == 'PASS', 'evidencePresent': status == 'PASS', 'evidence': {'source': 'host-test', 'kind': 'power-cycle'} if status == 'PASS' else {}, 'bootCountIncreased': status == 'PASS', 'previousBootCountAfter': 1},
                    'faultInject': {'executed': status == 'PASS', 'evidencePresent': status == 'PASS', 'evidence': {'source': 'host-test', 'kind': 'fault-inject'} if status == 'PASS' else {}, 'bootCountIncreased': status == 'PASS', 'previousBootCountAfter': 2},
                },
                'scenarios': {
                    'wifiProvisioning': {'executed': status == 'PASS', 'apObserved': status == 'PASS', 'staRecovered': status == 'PASS', 'recoveryVerified': status == 'PASS'},
                    'persistence': {'executed': status == 'PASS', 'configRoundTripOk': status == 'PASS', 'appStateRoundTripOk': status == 'PASS', 'powerCycleOk': status == 'PASS', 'bootCountIncreased': status == 'PASS', 'previousBootCountObserved': status == 'PASS'},
                    'bootLoopRecovery': {'executed': status == 'PASS', 'faultInjected': status == 'PASS', 'safeModeObserved': status == 'PASS', 'recoveryCleared': status == 'PASS', 'bootCountIncreased': status == 'PASS', 'previousBootCountObserved': status == 'PASS'},
                },
            },
        },
    }
    path.write_text(json.dumps(payload, indent=2) + '\n', encoding='utf-8')


def main() -> int:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    firmware = BUILD_DIR / 'firmware.bin'
    littlefs = BUILD_DIR / 'littlefs.bin'
    firmware.write_bytes(b'FW')
    littlefs.write_bytes(b'FS')
    host_report = OUTPUT_DIR / 'host-validation.json'
    device_report = OUTPUT_DIR / 'device-smoke-report.json'
    forged_device_report = OUTPUT_DIR / 'forged-device-smoke-report.json'
    try:
        subprocess.run([
            sys.executable, 'tools/write_host_validation_report.py',
            '--env', ENV,
            '--output', str(host_report),
            '--runner', 'LOCAL_HOST',
            '--status', 'PASS',
            '--check', 'asset-contract=PASS',
            '--check', 'partition-contract=PASS',
            '--check', 'firmware-build=PASS',
            '--check', 'littlefs-build=PASS',
            '--check', 'bundle-verify=PASS',
        ], cwd=ROOT, check=True)
        proc = subprocess.run([
            sys.executable, 'tools/package_release.py',
            '--env', ENV,
            '--output-dir', str(OUTPUT_DIR),
            '--host-validation-status', 'PASS',
            '--host-validation-report', str(host_report),
            '--device-smoke-status', 'NOT_RUN',
        ], cwd=ROOT, check=True, capture_output=True, text=True)
        assert CANDIDATE_BUNDLE.exists(), proc.stdout + proc.stderr
        subprocess.run([
            sys.executable, 'tools/verify_release_bundle.py',
            '--env', ENV,
            '--bundle', str(CANDIDATE_BUNDLE),
        ], cwd=ROOT, check=True)
        with zipfile.ZipFile(CANDIDATE_BUNDLE) as src_zip, zipfile.ZipFile(CORRUPT_BUNDLE, 'w', compression=zipfile.ZIP_DEFLATED) as dst_zip:
            for info in src_zip.infolist():
                if info.filename == 'littlefs.bin':
                    continue
                dst_zip.writestr(info, src_zip.read(info.filename))
        proc = subprocess.run([
            sys.executable, 'tools/verify_release_bundle.py',
            '--env', ENV,
            '--bundle', str(CORRUPT_BUNDLE),
        ], cwd=ROOT, capture_output=True, text=True)
        assert proc.returncode != 0
        forged_device_report.write_text('device smoke not executed\n', encoding='utf-8')
        proc = subprocess.run([
            sys.executable, 'tools/package_release.py',
            '--env', ENV,
            '--output-dir', str(OUTPUT_DIR),
            '--host-validation-status', 'PASS',
            '--host-validation-report', str(host_report),
            '--device-smoke-status', 'PASS',
            '--device-smoke-report', str(forged_device_report),
        ], cwd=ROOT, capture_output=True, text=True)
        assert proc.returncode != 0
        write_device_report(device_report, CANDIDATE_BUNDLE, 'FAIL')
        proc = subprocess.run([
            sys.executable, 'tools/promote_release_bundle.py',
            '--env', ENV,
            '--bundle', str(CANDIDATE_BUNDLE),
            '--device-smoke-report', str(device_report),
            '--output', str(VERIFIED_BUNDLE),
        ], cwd=ROOT, capture_output=True, text=True)
        assert proc.returncode != 0
        write_device_report(device_report, CANDIDATE_BUNDLE, 'PASS')
        proc = subprocess.run([
            sys.executable, 'tools/promote_release_bundle.py',
            '--env', ENV,
            '--bundle', str(CANDIDATE_BUNDLE),
            '--device-smoke-report', str(device_report),
            '--output', str(VERIFIED_BUNDLE),
        ], cwd=ROOT, check=True, capture_output=True, text=True)
        assert VERIFIED_BUNDLE.exists(), proc.stdout + proc.stderr
        subprocess.run([
            sys.executable, 'tools/verify_release_bundle.py',
            '--env', ENV,
            '--bundle', str(VERIFIED_BUNDLE),
        ], cwd=ROOT, check=True)
        with zipfile.ZipFile(VERIFIED_BUNDLE) as src_zip, zipfile.ZipFile(CORRUPT_BUNDLE, 'w', compression=zipfile.ZIP_DEFLATED) as dst_zip:
            for info in src_zip.infolist():
                data = src_zip.read(info.filename)
                if info.filename == 'release-validation.json':
                    payload = json.loads(data.decode('utf-8'))
                    payload['deviceSmokeStatus'] = 'FAIL'
                    data = (json.dumps(payload, indent=2) + '\n').encode('utf-8')
                dst_zip.writestr(info, data)
        proc = subprocess.run([
            sys.executable, 'tools/verify_release_bundle.py',
            '--env', ENV,
            '--bundle', str(CORRUPT_BUNDLE),
        ], cwd=ROOT, capture_output=True, text=True)
        assert proc.returncode != 0
        print('[OK] release bundle packaging/verification host check passed')
        return 0
    finally:
        if firmware.exists():
            firmware.unlink()
        if littlefs.exists():
            littlefs.unlink()
        shutil.rmtree(OUTPUT_DIR, ignore_errors=True)
        if BUILD_DIR.exists() and not any(BUILD_DIR.iterdir()):
            BUILD_DIR.rmdir()
            parent = BUILD_DIR.parent
            if parent.exists() and not any(parent.iterdir()):
                parent.rmdir()


if __name__ == '__main__':
    raise SystemExit(main())
