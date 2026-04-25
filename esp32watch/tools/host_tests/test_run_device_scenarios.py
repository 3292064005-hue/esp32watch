#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import shutil
import subprocess
import sys
import threading
import zipfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parents[2]
TMP = ROOT / 'dist' / 'host_test_scenarios'
BUNDLE = TMP / 'candidate.zip'
OUTPUT = TMP / 'device-scenario-evidence.json'
CAPABILITIES = TMP / 'runner-capabilities.json'
POWER_FLAG = TMP / 'power.flag'
FAULT_FLAG = TMP / 'fault.flag'


class Fixture:
    def __init__(self, candidate_sha256: str):
        self.candidate_sha256 = candidate_sha256
        self.config = {
            'wifiSsid': 'lab-net',
            'timezonePosix': 'PST8PDT,M3.2.0,M11.1.0',
            'timezoneId': 'America/Los_Angeles',
            'latitude': 37.7749,
            'longitude': -122.4194,
            'locationName': 'San Francisco',
            'wifiMode': 'STA',
            'apActive': False,
        }
        self.safe_mode = False
        self.boot_count = 1
        self.brightness = '1/4'
        self.face = 'F1'
        self.action_id = 1
        self.actions: dict[int, dict] = {}
        self.contract = json.loads((ROOT / 'data' / 'contract-bootstrap.json').read_text(encoding='utf-8'))

    def aggregate(self):
        if POWER_FLAG.exists():
            self.boot_count += 1
            POWER_FLAG.unlink()
        if FAULT_FLAG.exists():
            self.safe_mode = True
            self.boot_count += 1
            FAULT_FLAG.unlink()
        previous_boot = self.boot_count - 1 if self.boot_count > 0 else 0
        return {
            'ok': True,
            'system': {'appReady': True, 'startupOk': True},
            'terminal': {'brightnessLabel': self.brightness, 'systemFace': self.face},
            'diag': {'safeModeActive': self.safe_mode, 'consecutiveIncompleteBoots': 1 if self.safe_mode else 0, 'bootCount': self.boot_count, 'previousBootCount': previous_boot},
            'storage': {'appStateDurability': 'PERSISTED', 'appStatePowerLossGuaranteed': True},
        }


class Handler(BaseHTTPRequestHandler):
    fixture: Fixture

    def log_message(self, format, *args):
        return

    def _send(self, payload: dict, status: int = 200):
        body = json.dumps(payload).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _json_body(self):
        length = int(self.headers.get('Content-Length', '0'))
        if length <= 0:
            return {}
        return json.loads(self.rfile.read(length).decode('utf-8'))

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == '/api/contract':
            self._send({'ok': True, 'contract': self.fixture.contract})
            return
        if parsed.path == '/api/config/device':
            self._send({'ok': True, 'config': dict(self.fixture.config)})
            return
        if parsed.path == '/api/state/aggregate':
            self._send(self.fixture.aggregate())
            return
        if parsed.path == '/api/actions/status':
            params = parse_qs(parsed.query)
            action_id = int(params['id'][0])
            self._send(self.fixture.actions[action_id])
            return
        self._send({'ok': False, 'message': 'not found'}, status=404)

    def do_POST(self):
        body = self._json_body()
        if self.path == '/api/reset/device-config':
            self.fixture.config['apActive'] = True
            self.fixture.config['wifiMode'] = 'AP'
            self._send({'ok': True})
            return
        if self.path == '/api/config/device':
            self.fixture.config.update({
                'wifiSsid': body.get('ssid', self.fixture.config['wifiSsid']),
                'timezonePosix': body.get('timezonePosix', self.fixture.config['timezonePosix']),
                'timezoneId': body.get('timezoneId', self.fixture.config['timezoneId']),
                'latitude': body.get('latitude', self.fixture.config['latitude']),
                'longitude': body.get('longitude', self.fixture.config['longitude']),
                'locationName': body.get('locationName', self.fixture.config['locationName']),
                'apActive': False,
                'wifiMode': 'STA',
            })
            self._send({'ok': True})
            return
        if self.path == '/api/command':
            command = body['type']
            if command == 'setBrightness':
                self.fixture.brightness = f"{int(body['value']) + 1}/4"
            elif command == 'setWatchface':
                self.fixture.face = f"F{int(body['value']) + 1}"
            elif command == 'clearSafeMode':
                self.fixture.safe_mode = False
            action_id = self.fixture.action_id
            self.fixture.action_id += 1
            self.fixture.actions[action_id] = {'ok': True, 'id': action_id, 'status': 'APPLIED', 'type': command}
            self._send({'ok': True, 'actionId': action_id, 'requestId': action_id, 'trackPath': f'/api/actions/status?id={action_id}'})
            return
        self._send({'ok': False, 'message': 'not found'}, status=404)


def create_candidate_bundle(path: Path) -> str:
    path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr('firmware.bin', b'FW')
        zf.writestr('littlefs.bin', b'FS')
        zf.writestr('contract-bootstrap.json', (ROOT / 'data' / 'contract-bootstrap.json').read_bytes())
        zf.writestr('asset-contract.json', (ROOT / 'data' / 'asset-contract.json').read_bytes())
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    shutil.rmtree(TMP, ignore_errors=True)
    candidate_sha256 = create_candidate_bundle(BUNDLE)
    fixture = Fixture(candidate_sha256)
    capabilities = {
        'schemaVersion': 1,
        'runnerIdentity': 'HOST_TEST_RUNNER',
        'labIdentity': 'HOST_TEST_LAB',
        'powerCycle': {
            'argv': [sys.executable, '-c', (
                "import json, os; from pathlib import Path; "
                f"Path(r'{POWER_FLAG}').write_text('1'); "
                "Path(os.environ['ESP32WATCH_SCENARIO_EVIDENCE']).write_text(json.dumps({'label': os.environ['ESP32WATCH_SCENARIO_LABEL'], 'kind': 'power-cycle', 'source': 'host-test'}))"
            )]
        },
        'faultInject': {
            'argv': [sys.executable, '-c', (
                "import json, os; from pathlib import Path; "
                f"Path(r'{FAULT_FLAG}').write_text('1'); "
                "Path(os.environ['ESP32WATCH_SCENARIO_EVIDENCE']).write_text(json.dumps({'label': os.environ['ESP32WATCH_SCENARIO_LABEL'], 'kind': 'fault-inject', 'source': 'host-test'}))"
            )]
        },
    }
    CAPABILITIES.parent.mkdir(parents=True, exist_ok=True)
    CAPABILITIES.write_text(json.dumps(capabilities, indent=2) + '\n', encoding='utf-8')
    Handler.fixture = fixture
    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        subprocess.run([
            sys.executable, 'tools/run_device_scenarios.py',
            '--base-url', f'http://127.0.0.1:{server.server_address[1]}',
            '--candidate-bundle', str(BUNDLE),
            '--device-id', 'HOST-DEVICE',
            '--wifi-password', 'secret123',
            '--runner-capabilities', str(CAPABILITIES),
            '--output', str(OUTPUT),
            '--runner-identity', 'HOST_TEST_RUNNER',
            '--lab-identity', 'HOST_TEST_LAB',
            '--attest-physical-actions',
        ], cwd=ROOT, check=True)
        payload = json.loads(OUTPUT.read_text(encoding='utf-8'))
        assert payload['reportType'] == 'DEVICE_SCENARIO_EVIDENCE'
        assert payload['schemaVersion'] == 3
        assert payload['candidateBundleSha256'] == candidate_sha256
        assert payload['runnerIdentity'] == 'HOST_TEST_RUNNER'
        assert payload['labIdentity'] == 'HOST_TEST_LAB'
        assert payload['attestation']['physicalActionsAttested'] is True
        assert payload['runnerCapabilities']['schemaVersion'] == 1
        assert payload['attestation']['runnerCapabilitiesSha256'] == payload['runnerCapabilitiesSha256']
        for name in ('wifiProvisioning', 'persistence', 'bootLoopRecovery'):
            assert payload['scenarios'][name]['executed'] is True
        assert payload['scenarios']['wifiProvisioning']['apObserved'] is True
        assert payload['scenarios']['wifiProvisioning']['staRecovered'] is True
        assert payload['scenarios']['persistence']['appStateRoundTripOk'] is True
        assert payload['scenarios']['persistence']['bootCountIncreased'] is True
        assert payload['scenarios']['persistence']['previousBootCountObserved'] is True
        assert payload['scenarios']['bootLoopRecovery']['bootCountIncreased'] is True
        assert payload['scenarios']['bootLoopRecovery']['previousBootCountObserved'] is True
        assert payload['commandEvidence']['powerCycle']['evidencePresent'] is True
        assert payload['commandEvidence']['powerCycle']['evidence']['kind'] == 'power-cycle'
        assert payload['commandEvidence']['powerCycle']['bootCountIncreased'] is True
        assert payload['commandEvidence']['faultInject']['evidence']['kind'] == 'fault-inject'
        assert payload['commandEvidence']['faultInject']['bootCountIncreased'] is True
        assert payload['scenarios']['bootLoopRecovery']['safeModeObserved'] is True
        assert payload['scenarios']['bootLoopRecovery']['recoveryCleared'] is True
        print('[OK] device scenario runner host check passed')
        return 0
    finally:
        server.shutdown()
        thread.join(timeout=2)
        shutil.rmtree(TMP, ignore_errors=True)


if __name__ == '__main__':
    raise SystemExit(main())
