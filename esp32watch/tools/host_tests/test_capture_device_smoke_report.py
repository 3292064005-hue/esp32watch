#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import shutil
import socketserver
import subprocess
import sys
import tempfile
import threading
import time
import zipfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parents[2]
OUTPUT_DIR = ROOT / 'dist' / 'host_test_capture'
ENV = 'esp32s3_n16r8_release'


def load_bootstrap() -> dict:
    return json.loads((ROOT / 'data' / 'contract-bootstrap.json').read_text(encoding='utf-8'))


def load_asset_contract() -> dict:
    return json.loads((ROOT / 'data' / 'asset-contract.json').read_text(encoding='utf-8'))


def default_scalar(key: str):
    if key == 'ok':
        return True
    if key.endswith('Id') or key.endswith('Count') or key.endswith('Ms') or key in {'width', 'height', 'presentCount', 'queueDepth'}:
        return 1
    if key in {'wifiConnected', 'filesystemReady', 'assetContractReady'}:
        return True
    if key == 'bufferHex':
        return '00'
    if key == 'ip':
        return '192.168.4.2'
    if key == 'trackPath':
        return '/api/actions/status?id=1'
    if key == 'status':
        return 'APPLIED'
    if key == 'type' or key == 'actionType':
        return 'storageManualFlush'
    if key == 'message':
        return 'ok'
    if key == 'resetKind':
        return 'APP_STATE'
    return ''


def api_payload(schema: dict, sections: dict | None = None, overrides: dict | None = None) -> dict:
    payload = {}
    for key in schema.get('required', []):
        payload[key] = default_scalar(key)
    for section in schema.get('sections', []):
        payload[section] = {} if sections is None else sections.get(section, {})
    if overrides:
        payload.update(overrides)
    return payload


def aggregate_payload(state_schemas: dict, *, ok: bool = True) -> dict:
    payload = {'ok': ok}
    for entry in state_schemas['aggregate']:
        section = entry['section']
        payload[section] = {}
    payload['system'].update({
        'appReady': ok,
        'startupOk': ok,
        'timeValid': ok,
        'timeAuthoritative': ok,
    })
    return payload


def create_candidate_bundle(path: Path, contract: dict, asset_contract: dict) -> tuple[str, str]:
    firmware = b'FW'
    littlefs = b'FS'
    host_report = {
        'reportType': 'HOST_VALIDATION_REPORT',
        'schemaVersion': 2,
        'env': ENV,
        'status': 'PASS',
        'generatedAtUtc': '2026-04-15T00:00:00Z',
        'runner': 'LOCAL_HOST',
        'checks': [
            {'name': 'asset-contract', 'status': 'PASS'},
            {'name': 'partition-contract', 'status': 'PASS'},
            {'name': 'firmware-build', 'status': 'PASS'},
            {'name': 'littlefs-build', 'status': 'PASS'},
            {'name': 'bundle-verify', 'status': 'PASS'},
        ],
    }
    validation = {
        'validationSchemaVersion': 7,
        'env': ENV,
        'bundleKind': 'candidate',
        'hostValidationStatus': 'PASS',
        'hostValidationReportPath': 'reports/host-validation.json',
        'deviceSmokeStatus': 'NOT_RUN',
    }
    manifest = {
        'bundleKind': 'candidate',
        'files': ['firmware.bin', 'littlefs.bin', 'contract-bootstrap.json', 'asset-contract.json', 'release-validation.json', 'bundle-manifest.json', 'reports/host-validation.json'],
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr('firmware.bin', firmware)
        zf.writestr('littlefs.bin', littlefs)
        zf.writestr('contract-bootstrap.json', json.dumps(contract, indent=2) + '\n')
        zf.writestr('asset-contract.json', json.dumps(asset_contract, indent=2) + '\n')
        zf.writestr('release-validation.json', json.dumps(validation, indent=2) + '\n')
        zf.writestr('bundle-manifest.json', json.dumps(manifest, indent=2) + '\n')
        zf.writestr('reports/host-validation.json', json.dumps(host_report, indent=2) + '\n')
    return hashlib.sha256(firmware).hexdigest(), hashlib.sha256(littlefs).hexdigest()


class DeviceFixture:
    def __init__(self, contract: dict, asset_contract: dict, *, valid: bool = True):
        self.contract = contract
        self.asset_contract = asset_contract
        self.valid = valid
        self.action_id = 42
        self.routes = contract['routes']
        self.api_schemas = contract['apiSchemas']
        self.state_schemas = contract['stateSchemas']
        self.meta = api_payload(self.api_schemas['meta'], sections={
            'versions': {'apiVersion': contract['apiVersion'], 'stateVersion': contract['stateVersion']},
            'storageRuntime': {},
            'assetContract': {'assetContractVersion': contract['assetContractVersion']},
            'runtimeEvents': {},
            'platformSupport': {},
            'deviceIdentity': {'boardProfile': 'esp32watch', 'chipModel': 'ESP32-S3', 'efuseMac': 'AA:BB:CC:DD:EE:FF'},
            'capabilities': {},
            'releaseValidation': contract['releaseValidation'],
        }, overrides={
            'ok': True,
            'apiVersion': contract['apiVersion'],
            'stateVersion': contract['stateVersion'],
            'storageSchemaVersion': 7,
            'runtimeContractVersion': contract['runtimeContractVersion'],
            'commandCatalogVersion': contract['commandCatalogVersion'],
            'flashStorageSupported': True,
            'flashStorageReady': True,
            'storageMigrationAttempted': False,
            'storageMigrationOk': True,
            'appStateBackend': 'preferences',
            'deviceConfigBackend': 'preferences',
            'appStateDurability': 'PERSISTED',
            'deviceConfigDurability': 'PERSISTED',
            'appStatePowerLossGuaranteed': True,
            'deviceConfigPowerLossGuaranteed': True,
            'appStateMixedDurability': False,
            'appStateResetDomainObjectCount': 0,
            'appStateDurableObjectCount': 1,
            'timeAuthority': 'NETWORK',
            'timeSource': 'NTP',
            'timeConfidence': 'HIGH',
            'authRequired': False,
            'controlLockedInProvisioningAp': False,
            'provisioningSerialPasswordLogEnabled': False,
            'webControlPlaneReady': True,
            'webConsoleReady': True,
            'filesystemReady': True,
            'filesystemAssetsReady': True,
            'assetContractReady': True,
            'assetContractHashVerified': True,
            'assetContractVersion': contract['assetContractVersion'],
            'assetContractHash': 123,
            'assetContractGeneratedAt': '2026-04-15T00:00:00Z',
            'assetExpectedHashIndexHtml': 'a',
            'assetActualHashIndexHtml': 'a',
            'assetExpectedHashAppJs': 'b',
            'assetActualHashAppJs': 'b',
            'assetExpectedHashAppCss': 'c',
            'assetActualHashAppCss': 'c',
            'assetExpectedHashContractBootstrap': 'd',
            'assetActualHashContractBootstrap': 'd',
            'filesystemStatus': 'READY',
            'runtimeEventHandlers': 1,
            'runtimeEventCapacity': 8,
            'runtimeEventRegistrationRejectCount': 0,
            'runtimeEventPublishCount': 1,
            'runtimeEventPublishFailCount': 0,
            'runtimeEventLastSuccessCount': 1,
            'runtimeEventLastFailureCount': 0,
            'runtimeEventLastCriticalFailureCount': 0,
            'runtimeEventLast': 'NONE',
            'runtimeEventLastFailed': 'NONE',
            'runtimeEventLastFailedHandlerIndex': -1,
        })
        self.health = api_payload(self.api_schemas['health'], sections={'healthStatus': {}}, overrides={
            'ok': True,
            'wifiConnected': True,
            'ip': '192.168.4.2',
            'uptimeMs': 1000,
            'timeAuthority': 'NETWORK',
            'timeSource': 'NTP',
            'timeConfidence': 'HIGH',
            'filesystemReady': True,
            'assetContractReady': True,
        })
        self.display = api_payload(self.api_schemas['displayFrame'], sections={'displayFrame': {}}, overrides={
            'ok': True,
            'width': 128,
            'height': 64,
            'presentCount': 5,
            'bufferHex': '00' * 4,
        })
        self.catalog = api_payload(self.api_schemas['actionsCatalog'], sections={'commandCatalog': {}}, overrides={'ok': True})
        self.storage_semantics = api_payload(self.api_schemas['storageSemantics'], sections={'storageSemantics': []}, overrides={'ok': True, 'apiVersion': contract['apiVersion'], 'objects': [], 'backendCapabilities': {'available': True}})
        self.config = api_payload(self.api_schemas['configDeviceReadback'], sections={
            'deviceConfigReadback': {},
            'capabilities': {},
        }, overrides={
            'ok': True,
            'config': {
                'wifiConfigured': True,
                'weatherConfigured': True,
                'apiTokenConfigured': False,
                'apActive': False,
                'wifiMode': 'STA',
            },
        })
        self.aggregate = aggregate_payload(self.state_schemas, ok=valid)
        self.aggregate['system'].update({'appReady': True, 'startupOk': True, 'timeValid': True, 'timeAuthoritative': True})
        self.aggregate['storage'].update({'appStateDurability': 'DURABLE', 'appStatePowerLossGuaranteed': True})
        self.aggregate['diag'].update({'consecutiveIncompleteBoots': 0})
        self.ack = api_payload(self.api_schemas['trackedActionAccepted'], sections={'trackedActionAccepted': {}}, overrides={
            'ok': True,
            'actionId': self.action_id,
            'requestId': self.action_id,
            'actionType': 'storageManualFlush',
            'trackPath': f"{self.routes['actionsStatus']}?id={self.action_id}",
            'queueDepth': 0,
        })
        self.action = api_payload(self.api_schemas['actionsStatus'], sections={'actionStatus': {}}, overrides={
            'ok': True,
            'id': self.action_id,
            'status': 'APPLIED' if valid else 'FAILED',
            'type': 'storageManualFlush',
        })
        self.latest_action = dict(self.action)
        self.reset = api_payload(self.api_schemas['resetActionResponse'], sections={'resetAction': {}, 'runtimeReload': {}}, overrides={
            'ok': valid,
            'message': 'reset ok' if valid else 'reset failed',
            'resetKind': 'APP_STATE',
            'runtimeReload': {'requested': False},
        })
        if not valid:
            self.catalog.pop('commandCatalog', None)


class Handler(BaseHTTPRequestHandler):
    fixture: DeviceFixture

    def log_message(self, format, *args):
        return

    def _send(self, status: int, payload: dict):
        body = json.dumps(payload).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = urlparse(self.path).path
        qs = parse_qs(urlparse(self.path).query)
        routes = self.fixture.routes
        if path == routes['contract']:
            self._send(200, {'ok': True, 'contract': self.fixture.contract})
        elif path == routes['health']:
            self._send(200, self.fixture.health)
        elif path == routes['meta']:
            self._send(200, self.fixture.meta)
        elif path == routes['stateAggregate']:
            self._send(200, self.fixture.aggregate)
        elif path == routes['configDevice']:
            self._send(200, self.fixture.config)
        elif path == routes['displayFrame']:
            self._send(200, self.fixture.display)
        elif path == routes['actionsCatalog']:
            self._send(200, self.fixture.catalog)
        elif path == routes['storageSemantics']:
            self._send(200, self.fixture.storage_semantics)
        elif path == routes['actionsLatest']:
            self._send(200, self.fixture.latest_action)
        elif path == routes['actionsStatus']:
            if qs.get('id', [''])[0] != str(self.fixture.action_id):
                self._send(404, {'ok': False, 'message': 'unknown action'})
            else:
                self._send(200, self.fixture.action)
        else:
            self._send(404, {'ok': False, 'message': path})

    def do_POST(self):
        path = urlparse(self.path).path
        routes = self.fixture.routes
        length = int(self.headers.get('Content-Length', '0'))
        if length:
            self.rfile.read(length)
        if path == routes['command']:
            self._send(200, self.fixture.ack)
        elif path == routes['resetAppState']:
            self._send(200, self.fixture.reset)
        else:
            self._send(404, {'ok': False, 'message': path})


def run_capture(base_url: str, candidate: Path, output: Path) -> subprocess.CompletedProcess:
    return subprocess.run([
        sys.executable, 'tools/capture_device_smoke_report.py',
        '--base-url', base_url,
        '--env', ENV,
        '--device-id', 'HOST-DEVICE',
        '--board', 'esp32watch',
        '--candidate-bundle', str(candidate),
        '--output', str(output),
        '--runner-identity', 'HOST_TEST_RUNNER',
        '--lab-identity', 'HOST_TEST_LAB',
        '--attest-physical-actions',
    ], cwd=ROOT, capture_output=True, text=True)


def main() -> int:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    contract = load_bootstrap()
    asset_contract = load_asset_contract()
    candidate = OUTPUT_DIR / f'esp32watch-{ENV}-candidate.zip'
    create_candidate_bundle(candidate, contract, asset_contract)

    try:
        for valid, name in ((True, 'capture-pass.json'), (False, 'capture-fail.json')):
            fixture = DeviceFixture(contract, asset_contract, valid=valid)
            Handler.fixture = fixture
            server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            try:
                base_url = f'http://127.0.0.1:{server.server_address[1]}'
                output = OUTPUT_DIR / name
                proc = run_capture(base_url, candidate, output)
                if valid:
                    assert proc.returncode == 0, proc.stdout + proc.stderr
                    payload = json.loads(output.read_text(encoding='utf-8'))
                    assert payload['status'] == 'PASS'
                    assert payload['evidenceOrigin'] == 'DEVICE_CAPTURE'
                    assert payload['bundleEvidence']['runtimeContractMatchesBootstrap'] is True
                    assert payload['generator']['schemaVersion'] == 5
                    assert payload['generator']['runnerIdentity'] == 'HOST_TEST_RUNNER'
                    assert payload['generator']['labIdentity'] == 'HOST_TEST_LAB'
                    assert payload['attestation']['physicalActionsAttested'] is True
                else:
                    assert proc.returncode != 0, 'invalid fixture unexpectedly captured'
            finally:
                server.shutdown()
                thread.join(timeout=2)
                server.server_close()
        print('[OK] live device smoke capture host check passed')
        return 0
    finally:
        shutil.rmtree(OUTPUT_DIR, ignore_errors=True)


if __name__ == '__main__':
    raise SystemExit(main())
