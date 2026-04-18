#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Any
from urllib import error, parse, request

TERMINAL_ACTION_STATES = {"APPLIED", "REJECTED", "FAILED"}
SCHEMA_VERSION = 2


def now_utc() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace('+00:00', 'Z')


def merge_url(base_url: str, path: str) -> str:
    return parse.urljoin(base_url if base_url.endswith('/') else base_url + '/', path.lstrip('/'))


def http_json(method: str, url: str, *, auth_token: str | None = None, payload: dict[str, Any] | None = None, timeout_sec: float = 15.0) -> tuple[int, dict[str, Any]]:
    body = None
    headers = {'Content-Type': 'application/json'}
    if auth_token:
        headers['X-Auth-Token'] = auth_token
    if payload is not None:
        body = json.dumps(payload).encode('utf-8')
    req = request.Request(url, data=body, headers=headers, method=method)
    try:
        with request.urlopen(req, timeout=timeout_sec) as resp:
            raw = resp.read().decode('utf-8')
            return resp.status, json.loads(raw) if raw else {}
    except error.HTTPError as exc:
        raw = exc.read().decode('utf-8')
        try:
            return exc.code, json.loads(raw) if raw else {}
        except json.JSONDecodeError:
            return exc.code, {'ok': False, 'message': raw or str(exc)}


def load_candidate_sha256(bundle_path: Path) -> str:
    if not bundle_path.exists():
        raise SystemExit(f'candidate bundle not found: {bundle_path}')
    return hashlib.sha256(bundle_path.read_bytes()).hexdigest()


def load_runtime_contract(base_url: str, auth_token: str | None, timeout_sec: float) -> dict[str, Any]:
    status, payload = http_json('GET', merge_url(base_url, '/api/contract'), auth_token=auth_token, timeout_sec=timeout_sec)
    if status != 200 or not payload.get('ok'):
        raise SystemExit('failed to fetch runtime contract')
    contract = payload.get('contract')
    if not isinstance(contract, dict) or not isinstance(contract.get('routes'), dict):
        raise SystemExit('runtime contract missing routes')
    return contract


def route(contract: dict[str, Any], key: str, fallback: str) -> str:
    value = contract.get('routes', {}).get(key, fallback)
    return value if isinstance(value, str) and value else fallback


def wait_until(predicate, *, timeout_sec: float, interval_sec: float = 0.5, label: str) -> dict[str, Any]:
    deadline = time.time() + timeout_sec
    last = None
    while time.time() < deadline:
        last = predicate()
        if last is not None:
            return last
        time.sleep(interval_sec)
    raise SystemExit(f'timed out waiting for {label}')


def poll_action(base_url: str, contract: dict[str, Any], action_id: int, auth_token: str | None, timeout_sec: float) -> dict[str, Any]:
    actions_status = route(contract, 'actionsStatus', '/api/actions/status')
    def check():
        status, payload = http_json('GET', merge_url(base_url, f'{actions_status}?id={action_id}'), auth_token=auth_token, timeout_sec=timeout_sec)
        if status != 200:
            return None
        state = payload.get('status') or payload.get('actionStatus', {}).get('status')
        if state in TERMINAL_ACTION_STATES:
            return payload
        return None
    return wait_until(check, timeout_sec=timeout_sec, interval_sec=0.2, label=f'action {action_id}')


def post_tracked_command(base_url: str,
                         contract: dict[str, Any],
                         auth_token: str | None,
                         timeout_sec: float,
                         command_type: str,
                         **payload: Any) -> dict[str, Any]:
    body = {'type': command_type}
    body.update(payload)
    status, ack = http_json('POST', merge_url(base_url, route(contract, 'command', '/api/command')), auth_token=auth_token, payload=body, timeout_sec=timeout_sec)
    if status != 200 or not ack.get('ok'):
        raise SystemExit(f'command `{command_type}` rejected: HTTP {status} {ack}')
    action_id = ack.get('actionId') or ack.get('requestId')
    if not isinstance(action_id, int) or action_id <= 0:
        raise SystemExit(f'command `{command_type}` missing actionId: {ack}')
    result = poll_action(base_url, contract, action_id, auth_token, timeout_sec)
    state = result.get('status') or result.get('actionStatus', {}).get('status')
    if state != 'APPLIED':
        raise SystemExit(f'command `{command_type}` did not apply cleanly: {result}')
    return result


def get_config(base_url: str, contract: dict[str, Any], auth_token: str | None, timeout_sec: float) -> dict[str, Any]:
    status, payload = http_json('GET', merge_url(base_url, route(contract, 'configDevice', '/api/config/device')), auth_token=auth_token, timeout_sec=timeout_sec)
    if status != 200 or not payload.get('ok'):
        raise SystemExit(f'failed to read config: HTTP {status} {payload}')
    return payload.get('config', {})


def get_aggregate(base_url: str, contract: dict[str, Any], auth_token: str | None, timeout_sec: float) -> dict[str, Any]:
    status, payload = http_json('GET', merge_url(base_url, route(contract, 'stateAggregate', '/api/state/aggregate')), auth_token=auth_token, timeout_sec=timeout_sec)
    if status != 200 or not payload.get('ok'):
        raise SystemExit(f'failed to read aggregate state: HTTP {status} {payload}')
    return payload


def command_evidence(path: Path) -> dict[str, Any] | None:
    if not path.exists():
        return None
    payload = json.loads(path.read_text(encoding='utf-8'))
    if not isinstance(payload, dict):
        raise SystemExit(f'invalid command evidence payload: {path}')
    return payload


def run_shell(command: str, label: str, evidence_path: Path) -> dict[str, Any]:
    env = os.environ.copy()
    env['ESP32WATCH_SCENARIO_LABEL'] = label
    env['ESP32WATCH_SCENARIO_EVIDENCE'] = str(evidence_path)
    started_at = now_utc()
    proc = subprocess.run(command, shell=True, env=env)
    finished_at = now_utc()
    evidence = command_evidence(evidence_path)
    if proc.returncode != 0:
        raise SystemExit(f'{label} command failed: {command}')
    if evidence is None:
        raise SystemExit(f'{label} command completed without scenario evidence: {command}')
    return {
        'command': command,
        'startedAtUtc': started_at,
        'finishedAtUtc': finished_at,
        'exitCode': proc.returncode,
        'evidencePresent': True,
        'evidence': evidence,
    }


def aggregate_boot_count(aggregate: dict[str, Any]) -> int:
    diag = aggregate.get('diag', {}) if isinstance(aggregate.get('diag'), dict) else {}
    return int(diag.get('bootCount', 0) or 0)


def aggregate_previous_boot_count(aggregate: dict[str, Any]) -> int:
    diag = aggregate.get('diag', {}) if isinstance(aggregate.get('diag'), dict) else {}
    return int(diag.get('previousBootCount', 0) or 0)


def main() -> int:
    parser = argparse.ArgumentParser(description='Execute live device scenarios and emit scenario evidence JSON.')
    parser.add_argument('--base-url', required=True)
    parser.add_argument('--candidate-bundle', required=True)
    parser.add_argument('--device-id', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--auth-token', default='')
    parser.add_argument('--wifi-password', required=True)
    parser.add_argument('--power-cycle-cmd', required=True)
    parser.add_argument('--fault-inject-cmd', required=True)
    parser.add_argument('--timeout-sec', type=float, default=20.0)
    parser.add_argument('--post-reset-wait-sec', type=float, default=1.0)
    args = parser.parse_args()

    auth_token = args.auth_token or None
    candidate_sha256 = load_candidate_sha256(Path(args.candidate_bundle))
    base_url = args.base_url.rstrip('/')
    contract = load_runtime_contract(base_url, auth_token, args.timeout_sec)

    original_config = get_config(base_url, contract, auth_token, args.timeout_sec)
    original_aggregate = get_aggregate(base_url, contract, auth_token, args.timeout_sec)
    original_face = str(original_aggregate.get('terminal', {}).get('systemFace', 'F1'))
    original_brightness = str(original_aggregate.get('terminal', {}).get('brightnessLabel', '1/4'))
    boot_before_power_cycle = aggregate_boot_count(original_aggregate)

    reset_device_config_url = merge_url(base_url, route(contract, 'resetDeviceConfig', '/api/reset/device-config'))
    status, payload = http_json('POST', reset_device_config_url, auth_token=auth_token, payload={'token': auth_token or ''}, timeout_sec=args.timeout_sec)
    if status not in (200, 202) or not payload.get('ok'):
        raise SystemExit(f'device-config reset failed: HTTP {status} {payload}')
    time.sleep(args.post_reset_wait_sec)

    def wait_for_ap():
        cfg = get_config(base_url, contract, auth_token, args.timeout_sec)
        if cfg.get('apActive') is True and str(cfg.get('wifiMode', '')).upper() == 'AP':
            return cfg
        return None
    ap_config = wait_until(wait_for_ap, timeout_sec=args.timeout_sec, label='provisioning AP')

    reprovision_body = {
        'ssid': original_config.get('wifiSsid', ''),
        'password': args.wifi_password,
        'timezonePosix': original_config.get('timezonePosix', ''),
        'timezoneId': original_config.get('timezoneId', ''),
        'latitude': original_config.get('latitude', 0.0),
        'longitude': original_config.get('longitude', 0.0),
        'locationName': original_config.get('locationName', ''),
    }
    status, payload = http_json('POST', merge_url(base_url, route(contract, 'configDevice', '/api/config/device')),
                                auth_token=auth_token,
                                payload=reprovision_body,
                                timeout_sec=args.timeout_sec)
    if status != 200 or not payload.get('ok'):
        raise SystemExit(f'reprovisioning config update failed: HTTP {status} {payload}')

    def wait_for_sta():
        cfg = get_config(base_url, contract, auth_token, args.timeout_sec)
        if cfg.get('apActive') is False and str(cfg.get('wifiMode', '')).upper() in {'STA', 'STA_CONNECTED', 'STA_CONNECTING'}:
            return cfg
        return None
    recovered_config = wait_until(wait_for_sta, timeout_sec=args.timeout_sec, label='STA recovery')

    post_tracked_command(base_url, contract, auth_token, args.timeout_sec, 'setBrightness', value=3)
    post_tracked_command(base_url, contract, auth_token, args.timeout_sec, 'setWatchface', value=2)
    post_tracked_command(base_url, contract, auth_token, args.timeout_sec, 'storageManualFlush')

    aggregate_before_cycle = get_aggregate(base_url, contract, auth_token, args.timeout_sec)
    if aggregate_before_cycle.get('terminal', {}).get('brightnessLabel') != '4/4':
        raise SystemExit('pre-power-cycle brightness did not update to expected 4/4')
    if aggregate_before_cycle.get('terminal', {}).get('systemFace') != 'F3':
        raise SystemExit('pre-power-cycle systemFace did not update to expected F3')

    with TemporaryDirectory(prefix='esp32watch_scenarios_') as tmpdir:
        tmp = Path(tmpdir)
        power_meta = run_shell(args.power_cycle_cmd, 'power-cycle', tmp / 'power-cycle.json')

        def wait_for_persisted_state():
            agg = get_aggregate(base_url, contract, auth_token, args.timeout_sec)
            if agg.get('terminal', {}).get('brightnessLabel') == '4/4' and agg.get('terminal', {}).get('systemFace') == 'F3' and aggregate_boot_count(agg) > boot_before_power_cycle:
                return agg
            return None
        persisted_aggregate = wait_until(wait_for_persisted_state, timeout_sec=args.timeout_sec, label='persisted app-state after power cycle')
        boot_after_power_cycle = aggregate_boot_count(persisted_aggregate)

        fault_meta = run_shell(args.fault_inject_cmd, 'fault-inject', tmp / 'fault-inject.json')

        def wait_for_safe_mode():
            agg = get_aggregate(base_url, contract, auth_token, args.timeout_sec)
            diag = agg.get('diag', {}) if isinstance(agg.get('diag'), dict) else {}
            if diag.get('safeModeActive') is True and int(diag.get('consecutiveIncompleteBoots', 0)) > 0 and aggregate_boot_count(agg) > boot_after_power_cycle:
                return agg
            return None
        safe_mode_aggregate = wait_until(wait_for_safe_mode, timeout_sec=args.timeout_sec, label='safe mode after boot-loop injection')

        post_tracked_command(base_url, contract, auth_token, args.timeout_sec, 'clearSafeMode')

        def wait_for_safe_mode_clear():
            agg = get_aggregate(base_url, contract, auth_token, args.timeout_sec)
            diag = agg.get('diag', {}) if isinstance(agg.get('diag'), dict) else {}
            if diag.get('safeModeActive') is False:
                return agg
            return None
        cleared_aggregate = wait_until(wait_for_safe_mode_clear, timeout_sec=args.timeout_sec, label='safe mode clear')

        # Restore visible app-state knobs to the original values before exiting.
        if original_brightness.endswith('/4') and original_brightness[0].isdigit():
            post_tracked_command(base_url, contract, auth_token, args.timeout_sec, 'setBrightness', value=max(0, int(original_brightness[0]) - 1))
        if len(original_face) == 2 and original_face.startswith('F') and original_face[1].isdigit():
            post_tracked_command(base_url, contract, auth_token, args.timeout_sec, 'setWatchface', value=max(0, int(original_face[1]) - 1))
        post_tracked_command(base_url, contract, auth_token, args.timeout_sec, 'storageManualFlush')

        report = {
            'reportType': 'DEVICE_SCENARIO_EVIDENCE',
            'schemaVersion': SCHEMA_VERSION,
            'generatedAtUtc': now_utc(),
            'deviceId': args.device_id,
            'candidateBundleSha256': candidate_sha256,
            'commandEvidence': {
                'powerCycle': {
                    **power_meta,
                    'bootCountBefore': boot_before_power_cycle,
                    'bootCountAfter': boot_after_power_cycle,
                    'bootCountIncreased': boot_after_power_cycle > boot_before_power_cycle,
                    'previousBootCountAfter': aggregate_previous_boot_count(persisted_aggregate),
                },
                'faultInject': {
                    **fault_meta,
                    'bootCountBefore': boot_after_power_cycle,
                    'bootCountAfter': aggregate_boot_count(safe_mode_aggregate),
                    'bootCountIncreased': aggregate_boot_count(safe_mode_aggregate) > boot_after_power_cycle,
                    'previousBootCountAfter': aggregate_previous_boot_count(safe_mode_aggregate),
                },
            },
            'scenarios': {
                'wifiProvisioning': {
                    'executed': True,
                    'apObserved': ap_config.get('apActive') is True,
                    'staRecovered': recovered_config.get('apActive') is False,
                    'recoveryVerified': str(recovered_config.get('wifiMode', '')).upper() in {'STA', 'STA_CONNECTED', 'STA_CONNECTING'},
                },
                'persistence': {
                    'executed': True,
                    'configRoundTripOk': reprovision_body['ssid'] == recovered_config.get('wifiSsid') and reprovision_body['timezonePosix'] == recovered_config.get('timezonePosix'),
                    'appStateRoundTripOk': persisted_aggregate.get('terminal', {}).get('brightnessLabel') == '4/4' and persisted_aggregate.get('terminal', {}).get('systemFace') == 'F3',
                    'powerCycleOk': True,
                    'bootCountIncreased': boot_after_power_cycle > boot_before_power_cycle,
                    'previousBootCountObserved': aggregate_previous_boot_count(persisted_aggregate) == boot_before_power_cycle,
                },
                'bootLoopRecovery': {
                    'executed': True,
                    'faultInjected': True,
                    'safeModeObserved': safe_mode_aggregate.get('diag', {}).get('safeModeActive') is True,
                    'recoveryCleared': cleared_aggregate.get('diag', {}).get('safeModeActive') is False,
                    'bootCountIncreased': aggregate_boot_count(safe_mode_aggregate) > boot_after_power_cycle,
                    'previousBootCountObserved': aggregate_previous_boot_count(safe_mode_aggregate) == boot_after_power_cycle,
                },
            },
        }

    Path(args.output).write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
