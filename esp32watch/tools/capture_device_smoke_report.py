#!/usr/bin/env python3
from __future__ import annotations
import argparse
import hashlib
import json
import time
import zipfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Any
from urllib import error, parse, request

TERMINAL_ACTION_STATES = {'APPLIED', 'REJECTED', 'FAILED'}
SCENARIO_EVIDENCE_SCHEMA_VERSION = 2
REQUIRED_SCENARIOS = ('wifiProvisioning', 'persistence', 'bootLoopRecovery')


def now_utc() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace('+00:00', 'Z')


def json_sha256(payload: Any) -> str:
    return hashlib.sha256(json.dumps(payload, sort_keys=True, separators=(',', ':')).encode('utf-8')).hexdigest()


def load_candidate(bundle_path: Path) -> dict[str, Any]:
    bundle_sha256 = hashlib.sha256(bundle_path.read_bytes()).hexdigest()
    with zipfile.ZipFile(bundle_path) as zf:
        bootstrap = json.loads(zf.read('contract-bootstrap.json').decode('utf-8'))
        asset_contract = json.loads(zf.read('asset-contract.json').decode('utf-8'))
        firmware_sha256 = hashlib.sha256(zf.read('firmware.bin')).hexdigest()
        littlefs_sha256 = hashlib.sha256(zf.read('littlefs.bin')).hexdigest()
    return {
        'fileName': bundle_path.name,
        'sha256': bundle_sha256,
        'firmwareSha256': firmware_sha256,
        'littlefsSha256': littlefs_sha256,
        'bootstrapContractSha256': json_sha256(bootstrap),
        'assetContractSha256': json_sha256(asset_contract),
        'bootstrapContract': bootstrap,
        'assetContract': asset_contract,
    }


def merge_url(base_url: str, path: str) -> str:
    return parse.urljoin(base_url if base_url.endswith('/') else base_url + '/', path.lstrip('/'))


def http_json(method: str, url: str, *, auth_token: str | None = None, payload: dict[str, Any] | None = None, timeout_sec: float = 10.0) -> tuple[int, dict[str, Any]]:
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
            data = json.loads(raw) if raw else {}
        except json.JSONDecodeError:
            data = {'ok': False, 'message': raw or str(exc)}
        return exc.code, data


def require_api_schema(payload: dict[str, Any], schema: dict[str, Any], schema_name: str) -> None:
    if not isinstance(schema, dict) or not schema:
        raise SystemExit(f'missing api schema `{schema_name}` in runtime contract')
    required = schema.get('required', [])
    missing = [key for key in required if key not in payload]
    if missing:
        raise SystemExit(f'{schema_name} missing required keys: {", ".join(missing)}')
    sections = schema.get('sections', [])
    missing_sections = [key for key in sections if key not in payload]
    if missing_sections:
        raise SystemExit(f'{schema_name} missing declared sections: {", ".join(missing_sections)}')


def require_state_schema(payload: dict[str, Any], schema_entries: list[dict[str, Any]], view_name: str) -> None:
    if not isinstance(schema_entries, list) or not schema_entries:
        raise SystemExit(f'missing state schema `{view_name}` in runtime contract')
    missing = [entry['section'] for entry in schema_entries if entry['section'] not in payload]
    if missing:
        raise SystemExit(f'{view_name} payload missing state sections: {", ".join(missing)}')


def poll_action(track_url: str, auth_token: str | None, timeout_sec: float, api_schema: dict[str, Any]) -> dict[str, Any]:
    deadline = time.time() + timeout_sec
    last: dict[str, Any] = {}
    while time.time() < deadline:
        _, payload = http_json('GET', track_url, auth_token=auth_token, timeout_sec=timeout_sec)
        last = payload
        require_api_schema(last, api_schema, 'actionsStatus')
        status = payload.get('status') or payload.get('actionStatus', {}).get('status')
        if status in TERMINAL_ACTION_STATES:
            return payload
        time.sleep(0.2)
    raise SystemExit(f'action polling timed out for {track_url}: {last}')


def check_status(pass_condition: bool) -> str:
    return 'PASS' if pass_condition else 'FAIL'


def route_path(routes: dict[str, Any], key: str, fallback: str) -> str:
    value = routes.get(key, fallback)
    return value if isinstance(value, str) and value else fallback


def compare_runtime_to_bundle(contract: dict[str, Any], candidate: dict[str, Any], meta: dict[str, Any]) -> dict[str, Any]:
    bootstrap = candidate['bootstrapContract']
    asset_contract = candidate['assetContract']
    bundle_evidence = {
        'runtimeContractMatchesBootstrap': contract == bootstrap,
        'routesMatchBootstrap': contract.get('routes') == bootstrap.get('routes'),
        'stateSchemasMatchBootstrap': contract.get('stateSchemas') == bootstrap.get('stateSchemas'),
        'apiSchemasMatchBootstrap': contract.get('apiSchemas') == bootstrap.get('apiSchemas'),
        'releaseValidationMatchesBootstrap': contract.get('releaseValidation') == bootstrap.get('releaseValidation'),
        'apiVersionMatchesBootstrap': contract.get('apiVersion') == bootstrap.get('apiVersion'),
        'stateVersionMatchesBootstrap': contract.get('stateVersion') == bootstrap.get('stateVersion'),
        'runtimeContractVersionMatchesBootstrap': contract.get('runtimeContractVersion') == bootstrap.get('runtimeContractVersion'),
        'assetContractVersionMatchesBootstrap': contract.get('assetContractVersion') == bootstrap.get('assetContractVersion'),
        'metaApiVersionMatchesBootstrap': meta.get('apiVersion') == bootstrap.get('apiVersion'),
        'metaStateVersionMatchesBootstrap': meta.get('stateVersion') == bootstrap.get('stateVersion'),
        'metaAssetContractVersionMatchesBootstrap': meta.get('assetContractVersion') == bootstrap.get('assetContractVersion'),
        'assetContractBootstrapMatchesBundle': asset_contract.get('bootstrapContract') == bootstrap,
        'metaDeviceIdentityPresent': isinstance(meta.get('deviceIdentity'), dict),
    }
    bundle_evidence.update({
        'bootstrapContractSha256': candidate['bootstrapContractSha256'],
        'assetContractSha256': candidate['assetContractSha256'],
        'firmwareSha256': candidate['firmwareSha256'],
        'littlefsSha256': candidate['littlefsSha256'],
    })
    return bundle_evidence


def load_scenario_evidence(path: str | None, candidate: dict[str, Any], device_id: str, capture_runner: str) -> dict[str, Any] | None:
    if not path:
        if capture_runner == 'CI_DEVICE_RUNNER':
            raise SystemExit('CI_DEVICE_RUNNER capture requires --scenario-evidence')
        return None
    payload = json.loads(Path(path).read_text(encoding='utf-8'))
    if payload.get('reportType') != 'DEVICE_SCENARIO_EVIDENCE':
        raise SystemExit('scenario evidence reportType must be DEVICE_SCENARIO_EVIDENCE')
    if payload.get('schemaVersion') != SCENARIO_EVIDENCE_SCHEMA_VERSION:
        raise SystemExit('scenario evidence schemaVersion mismatch')
    if payload.get('candidateBundleSha256') != candidate['sha256']:
        raise SystemExit('scenario evidence candidateBundleSha256 does not match target candidate bundle')
    if payload.get('deviceId') not in {'', None, device_id}:
        raise SystemExit('scenario evidence deviceId does not match capture target')
    scenarios = payload.get('scenarios')
    if not isinstance(scenarios, dict):
        raise SystemExit('scenario evidence scenarios must be an object')
    for name in REQUIRED_SCENARIOS:
        scenario = scenarios.get(name)
        if not isinstance(scenario, dict):
            raise SystemExit(f'scenario evidence missing scenario `{name}`')
        if scenario.get('executed') is not True:
            raise SystemExit(f'scenario evidence `{name}` must set executed=true')
    return payload


def scenario_check(scenario: dict[str, Any] | None, *required_flags: str) -> bool:
    if not isinstance(scenario, dict):
        return False
    if scenario.get('executed') is not True:
        return False
    return all(scenario.get(flag) is True for flag in required_flags)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--base-url', required=True)
    parser.add_argument('--env', required=True)
    parser.add_argument('--device-id', required=True)
    parser.add_argument('--candidate-bundle', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--auth-token', default='')
    parser.add_argument('--board', default='esp32watch')
    parser.add_argument('--timeout-sec', type=float, default=10.0)
    parser.add_argument('--flash-evidence')
    parser.add_argument('--scenario-evidence')
    parser.add_argument('--capture-runner', choices=('CI_DEVICE_RUNNER', 'LOCAL_DEVICE_RUNNER'), default='LOCAL_DEVICE_RUNNER')
    args = parser.parse_args()

    candidate_bundle = Path(args.candidate_bundle)
    if not candidate_bundle.exists():
        raise SystemExit(f'candidate bundle not found: {candidate_bundle}')

    candidate = load_candidate(candidate_bundle)
    flash_evidence = None
    if args.flash_evidence:
        flash_evidence = json.loads(Path(args.flash_evidence).read_text(encoding='utf-8'))
        if flash_evidence.get('flashMode') != 'LIVE_FLASH':
            raise SystemExit('flash evidence must come from LIVE_FLASH mode')
        if flash_evidence.get('candidateBundleSha256') != candidate['sha256']:
            raise SystemExit('flash evidence candidate bundle sha256 does not match capture target')
        if flash_evidence.get('firmwareSha256') != candidate['firmwareSha256']:
            raise SystemExit('flash evidence firmware sha256 does not match capture target')
        if flash_evidence.get('littlefsSha256') != candidate['littlefsSha256']:
            raise SystemExit('flash evidence littlefs sha256 does not match capture target')
    scenario_evidence = load_scenario_evidence(args.scenario_evidence, candidate, args.device_id, args.capture_runner)

    base_url = args.base_url.rstrip('/')
    auth_token = args.auth_token or None
    contract_status, contract_payload = http_json('GET', merge_url(base_url, '/api/contract'), auth_token=auth_token, timeout_sec=args.timeout_sec)
    if contract_status != 200 or not contract_payload.get('ok'):
        raise SystemExit('failed to fetch runtime contract from device')
    contract = contract_payload.get('contract')
    if not isinstance(contract, dict) or 'routes' not in contract:
        raise SystemExit('runtime contract payload missing contract.routes')

    routes = contract['routes']
    api_schemas = contract.get('apiSchemas', {})
    state_schemas = contract.get('stateSchemas', {})
    require_api_schema(contract_payload, api_schemas.get('contractDocument', {}), 'contractDocument')

    health_status, health = http_json('GET', merge_url(base_url, route_path(routes, 'health', '/api/health')), auth_token=auth_token, timeout_sec=args.timeout_sec)
    meta_status, meta = http_json('GET', merge_url(base_url, route_path(routes, 'meta', '/api/meta')), auth_token=auth_token, timeout_sec=args.timeout_sec)
    aggregate_status, aggregate = http_json('GET', merge_url(base_url, route_path(routes, 'stateAggregate', '/api/state/aggregate')), auth_token=auth_token, timeout_sec=args.timeout_sec)
    config_status, config = http_json('GET', merge_url(base_url, route_path(routes, 'configDevice', '/api/config/device')), auth_token=auth_token, timeout_sec=args.timeout_sec)
    display_status, display = http_json('GET', merge_url(base_url, route_path(routes, 'displayFrame', '/api/display/frame')), auth_token=auth_token, timeout_sec=args.timeout_sec)
    catalog_status, catalog = http_json('GET', merge_url(base_url, route_path(routes, 'actionsCatalog', '/api/actions/catalog')), auth_token=auth_token, timeout_sec=args.timeout_sec)
    storage_semantics_status, storage_semantics = http_json('GET', merge_url(base_url, route_path(routes, 'storageSemantics', '/api/storage/semantics')), auth_token=auth_token, timeout_sec=args.timeout_sec)

    if min(health_status, meta_status, aggregate_status, config_status, display_status, catalog_status, storage_semantics_status) < 200 or max(health_status, meta_status, aggregate_status, config_status, display_status, catalog_status, storage_semantics_status) >= 300:
        raise SystemExit('device smoke capture failed to fetch required runtime endpoints')
    require_api_schema(health, api_schemas.get('health', {}), 'health')
    require_api_schema(meta, api_schemas.get('meta', {}), 'meta')
    require_state_schema(aggregate, state_schemas.get('aggregate', []), 'aggregate')
    require_api_schema(display, api_schemas.get('displayFrame', {}), 'displayFrame')
    require_api_schema(catalog, api_schemas.get('actionsCatalog', {}), 'actionsCatalog')
    require_api_schema(config, api_schemas.get('configDeviceReadback', {}), 'configDeviceReadback')
    require_api_schema(storage_semantics, api_schemas.get('storageSemantics', {}), 'storageSemantics')

    device_identity = meta.get('deviceIdentity', {}) if isinstance(meta.get('deviceIdentity'), dict) else {}
    flash_mac = (flash_evidence or {}).get('deviceMac')
    if flash_mac and device_identity.get('efuseMac') and str(device_identity.get('efuseMac')).upper() != str(flash_mac).upper():
        raise SystemExit('device smoke capture efuseMac does not match flash evidence deviceMac')
    if args.board and device_identity.get('boardProfile') and str(device_identity.get('boardProfile')) != str(args.board):
        raise SystemExit('device smoke capture boardProfile does not match requested board')

    bundle_evidence = compare_runtime_to_bundle(contract, candidate, meta)
    bundle_evidence['metaDeviceMacMatchesFlashEvidence'] = (not flash_mac) or (str(device_identity.get('efuseMac', '')).upper() == str(flash_mac).upper())
    bundle_evidence['metaBoardProfileMatchesRequest'] = (not args.board) or (str(device_identity.get('boardProfile', '')) == str(args.board))

    command_url = merge_url(base_url, route_path(routes, 'command', '/api/command'))
    ack_status, flush_ack = http_json('POST', command_url, auth_token=auth_token, payload={'type': 'storageManualFlush'}, timeout_sec=args.timeout_sec)
    if ack_status != 200 or not flush_ack.get('ok'):
        raise SystemExit('device smoke capture failed to enqueue manual flush action')
    require_api_schema(flush_ack, api_schemas.get('trackedActionAccepted', {}), 'trackedActionAccepted')
    track_path = flush_ack.get('trackPath') or f"{route_path(routes, 'actionsStatus', '/api/actions/status')}?id={flush_ack.get('actionId')}"
    flush_result = poll_action(merge_url(base_url, track_path), auth_token, args.timeout_sec, api_schemas.get('actionsStatus', {}))
    flush_status = flush_result.get('status') or flush_result.get('actionStatus', {}).get('status')
    latest_status_code, latest_action = http_json('GET', merge_url(base_url, route_path(routes, 'actionsLatest', '/api/actions/latest')), auth_token=auth_token, timeout_sec=args.timeout_sec)
    if latest_status_code != 200:
        raise SystemExit(f'device smoke capture failed to fetch latest action with HTTP {latest_status_code}')
    require_api_schema(latest_action, api_schemas.get('actionsStatus', {}), 'actionsStatus')

    reset_url = merge_url(base_url, route_path(routes, 'resetAppState', '/api/reset/app-state'))
    reset_status_code, reset_result = http_json('POST', reset_url, auth_token=auth_token, payload={'token': auth_token or ''}, timeout_sec=args.timeout_sec)
    if reset_status_code not in (200, 202):
        raise SystemExit(f'device smoke capture reset flow failed with HTTP {reset_status_code}')
    require_api_schema(reset_result, api_schemas.get('resetActionResponse', {}), 'resetActionResponse')

    system = aggregate.get('system', {})
    config_obj = config.get('config', {})
    storage_obj = aggregate.get('storage', {}) if isinstance(aggregate.get('storage'), dict) else {}
    diag_obj = aggregate.get('diag', {}) if isinstance(aggregate.get('diag'), dict) else {}
    scenarios = scenario_evidence.get('scenarios', {}) if isinstance(scenario_evidence, dict) else {}
    checks = {
        'boot': check_status(bool(aggregate.get('ok')) and bool(system.get('appReady')) and bool(system.get('startupOk', True))),
        'webFallback': check_status(bool(health.get('ok')) and bool(meta.get('webControlPlaneReady', True))),
        'assetLoad': check_status(bool(meta.get('assetContractReady')) and bool(meta.get('assetContractHashVerified')) and bool(meta.get('filesystemAssetsReady', True))),
        'wifiProvisioning': check_status(
            scenario_check(scenarios.get('wifiProvisioning'), 'apObserved', 'staRecovered', 'recoveryVerified')
            if scenario_evidence is not None else
            (config.get('ok') and ('wifiConfigured' in config_obj or 'apActive' in config_obj or 'wifiMode' in config_obj))
        ),
        'timeSync': check_status(bool(system.get('timeValid')) and bool(system.get('timeAuthoritative'))),
        'manualFlush': check_status(flush_status == 'APPLIED'),
        'resetFlow': check_status(bool(reset_result.get('ok'))),
        'persistence': check_status(
            scenario_check(scenarios.get('persistence'), 'configRoundTripOk', 'appStateRoundTripOk', 'powerCycleOk', 'bootCountIncreased', 'previousBootCountObserved')
            if scenario_evidence is not None else
            (bool(storage_obj.get('appStatePowerLossGuaranteed')) and str(storage_obj.get('appStateDurability', '')).upper() == 'DURABLE')
        ),
        'bootLoopRecovery': check_status(
            scenario_check(scenarios.get('bootLoopRecovery'), 'faultInjected', 'safeModeObserved', 'recoveryCleared', 'bootCountIncreased', 'previousBootCountObserved')
            if scenario_evidence is not None else
            (int(diag_obj.get('consecutiveIncompleteBoots', 0)) == 0)
        ),
        'contractBootstrap': check_status(bundle_evidence['runtimeContractMatchesBootstrap'] and bundle_evidence['routesMatchBootstrap']),
        'schemaCatalog': check_status(bundle_evidence['stateSchemasMatchBootstrap'] and bundle_evidence['apiSchemasMatchBootstrap'] and bundle_evidence['releaseValidationMatchesBootstrap']),
        'deviceIdentity': check_status(bundle_evidence['metaDeviceIdentityPresent'] and bundle_evidence['metaDeviceMacMatchesFlashEvidence'] and bundle_evidence['metaBoardProfileMatchesRequest']),
    }
    final_status = 'PASS' if all(status == 'PASS' for status in checks.values()) else 'FAIL'

    payload = {
        'reportType': 'DEVICE_SMOKE_REPORT',
        'schemaVersion': 6,
        'env': args.env,
        'status': final_status,
        'evidenceOrigin': 'DEVICE_CAPTURE',
        'generatedAtUtc': now_utc(),
        'generator': {
            'tool': 'capture_device_smoke_report.py',
            'captureMode': 'LIVE_HTTP_CAPTURE',
            'schemaVersion': 4,
            'captureRunner': args.capture_runner,
        },
        'device': {
            'id': args.device_id,
            'board': args.board,
            'baseUrl': base_url,
        },
        'candidateBundle': {
            'fileName': candidate['fileName'],
            'sha256': candidate['sha256'],
            'firmwareSha256': candidate['firmwareSha256'],
            'littlefsSha256': candidate['littlefsSha256'],
        },
        'bundleEvidence': bundle_evidence,
        'flashEvidence': flash_evidence,
        'scenarioEvidence': scenario_evidence,
        'checks': [{'name': name, 'status': status} for name, status in checks.items()],
        'evidence': {
            'contract': contract,
            'health': health,
            'meta': meta,
            'aggregate': aggregate,
            'displayFrame': display,
            'actionsCatalog': catalog,
            'actionsLatest': latest_action,
            'storageSemantics': storage_semantics,
            'config': config,
            'manualFlush': flush_result,
            'resetFlow': reset_result,
            'scenarioEvidence': scenario_evidence,
        },
    }
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(payload, indent=2) + '\n', encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
