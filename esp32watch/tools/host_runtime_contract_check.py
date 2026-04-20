#!/usr/bin/env python3
from __future__ import annotations
from pathlib import Path
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / 'include' / 'web' / 'web_contract.h'
BOOTSTRAP = ROOT / 'data' / 'contract-bootstrap.json'
ASSET = ROOT / 'data' / 'asset-contract.json'
WORKFLOW = ROOT / '.github' / 'workflows' / 'platformio-build.yml'
WEB_CONTRACT_CPP = ROOT / 'src' / 'web' / 'web_contract.cpp'
WEB_ROUTES_API_CPP = ROOT / 'src' / 'web' / 'web_routes_api.cpp'
APP_JS = ROOT / 'data' / 'app.js'

VERSION_RE = re.compile(r'static constexpr uint32_t (WEB_[A-Z_]+) = (\d+)U;')
APPEND_ROUTE_RE = re.compile(r'append_route\(response, "([^"]+)", (WEB_ROUTE_[A-Z0-9_]+), (?:true|false)\);')
ROUTE_CONST_RE = re.compile(r'static constexpr const char \*(WEB_ROUTE_[A-Z0-9_]+) = "([^"]+)";')
ROUTE_SCHEMA_ENTRY_RE = re.compile(r'\{"([^"]+)",\s*"([^"]+)",\s*"([^"]+)"\}')
SERVER_ON_CONST_RE = re.compile(r'server\.on\((WEB_ROUTE_[A-Z0-9_]+),\s*HTTP_(GET|POST)')
CONTRACT_ROUTE_USAGE_RE = re.compile(r'contractRoute\(\"([^\"]+)\"\)')


def parse_route_schemas(contract_text: str) -> dict[str, dict[str, str]]:
    marker = 'static constexpr RouteSchemaSpec kRouteSchemas[] = {'
    start = contract_text.find(marker)
    if start < 0:
        return {}
    end = contract_text.find('};', start)
    if end < 0:
        return {}
    body = contract_text[start:end]
    return {key: {'kind': kind, 'name': name} for key, kind, name in ROUTE_SCHEMA_ENTRY_RE.findall(body)}


checks = [
    (ROOT / 'src' / 'services' / 'storage_facade.c', [
        'storage_facade_get_runtime_snapshot',
        'device_config_backend_name()',
        'device_config_backend_ready()',
        'device_config_last_commit_ok()',
    ], 'storage facade runtime truth surface'),
    (ROOT / 'src' / 'services' / 'runtime_reload_coordinator.c', [
        'RuntimeReloadReport report = {0};',
        'runtime_reload_report_copy(out, &report);',
        'runtime_reload_preflight_mask(requested_domain_mask, &report)',
        'report.verify_ok = runtime_reload_verify_mask(requested_domain_mask, &report);',
    ], 'runtime reload coordinator applies preflight/verify even without direct output storage'),
    (ROOT / 'src' / 'services' / 'device_config_public.cpp', [
        'device_config_backend_name',
        'device_config_backend_ready',
        'device_config_last_commit_ok',
    ], 'device-config backend runtime reporting'),
    (ROOT / 'src' / 'web' / 'web_runtime_snapshot.cpp', [
        'collect_storage_detail(out);',
        'collect_perf_detail(out);',
        'runtime.device_config_backend',
        'runtime.device_config_power_loss_guaranteed',
    ], 'runtime snapshot detail/perf aggregation'),
    (ROOT / 'src' / 'web' / 'web_route_state_payload.cpp', [
        'collect_api_state_bundle',
        'append_state_versions(response);',
    ], 'web API aggregate/runtime telemetry contract core helpers'),
    (ROOT / 'src' / 'web' / 'web_route_module_state.cpp', [
        'WEB_ROUTE_STATE_AGGREGATE',
        'server.on(WEB_ROUTE_STATE_AGGREGATE',
    ], 'web API aggregate/runtime telemetry route module'),
    (ROOT / 'include' / 'model.h', [
        'const WatchModel *model_get(void);',
        'const ModelDomainState *model_get_domain_state(ModelDomainState *out);',
    ], 'model read-only compatibility surface retained without compat telemetry'),
    (ROOT / 'src' / 'model.c', [
        'const WatchModel *model_get(void)',
        'refresh_time_from_service_authority',
    ], 'model read-only compatibility surface implementation'),
    (ROOT / 'src' / 'services' / 'runtime_event_service.c', [
        'bool runtime_event_service_register_ex(const RuntimeEventSubscription *subscription)',
        'bool runtime_event_service_publish(RuntimeServiceEvent event)',
        'bool runtime_event_service_publish_notify(RuntimeServiceEvent event)',
    ], 'runtime-event authoritative subscription-only path'),
    (ROOT / 'src' / 'watch_app_runtime.c', [
        'WatchAppStageDescriptor',
        'g_watch_app_stage_descriptors',
        'watch_app_runtime_stage_name',
    ], 'runtime stages sourced from descriptor manifest'),
    (ROOT / 'src' / 'ui_app_catalog.c', [
        'g_ui_app_catalog',
        'ui_app_catalog_count',
        'ui_app_catalog_get',
    ], 'apps menu sourced from static manifest'),
    (ROOT / 'src' / 'ui_page_catalog.c', [
        'ui_page_registry_catalog_count',
        'ui_page_registry_catalog_at',
        'ui_page_registry_find_descriptor',
    ], 'page registry wrapper delegates to module registry'),
    (ROOT / 'src' / 'ui_page_module_core.c', [
        'g_ui_page_catalog',
        'ui_page_register_core_module',
        'ui_page_module_register("core"',
    ], 'page registry sourced from module manifest'),
    (ROOT / 'tools' / 'verify_release_bundle.py', [
        'bundle-manifest.json',
        'contract-bootstrap.json',
        'docs/release-validation.md',
        'release-validation.json',
        'hostValidationReportPath',
        'validationSchemaVersion',
        'candidateBundle',
        'device report PASS payload requires scenarioEvidence object',
        'device report PASS payload requires scenarioEvidence.reportType=DEVICE_SCENARIO_EVIDENCE',
        'device report PASS payload requires evidence.scenarioEvidence to match scenarioEvidence',
        'partitions/esp32s3_n16r8_littlefs.csv',
    ], 'release bundle verifier'),
    (ROOT / 'src' / 'web' / 'web_contract.cpp', [
        '\\"stateSchemas\\":{',
        '\\"apiSchemas\\":{',
        'append_state_schemas(response);',
        'append_api_schemas(response);',
        'web_contract_get_api_schema',
        'web_contract_get_route_api_schema',
    ], 'runtime contract publishes schema catalogs'),
    (ROOT / 'data' / 'app.js', [
        'function contractStateSchema(viewName)',
        'function contractApiSchema(name)',
        'function validateApiPayload(payload, schemaName, errorMessage)',
        'apiWithSchema("displayFrame", "/api/display/frame", "displayFrame")',
        'apiWithSchema("health", "/api/health", "health")',
        'apiWithSchema("configDevice", "/api/config/device", "configDeviceReadback")',
        'tracked action ack payload does not match runtime contract schema',
        'function validateContractCatalogCoverage()',
        'apiWithSchema("storageSemantics", "/api/storage/semantics", "storageSemantics")',
        'validateApiPayload(payload, "contractDocument", "runtime contract payload does not match runtime contract schema")',
    ], 'frontend consumes runtime schema catalogs'),
    (ROOT / 'tools' / 'verify_platformio_build.sh', [
        'verify_release_bundle.py',
        'package_release.py',
        'write_host_validation_report.py',
        '--host-validation-status PASS',
        '--host-validation-report',
        '--device-smoke-status NOT_RUN',
    ], 'build verification includes release bundle validation'),
    (ROOT / 'tools' / 'generate_asset_contract.py', [
        "'assetContractVersion': contract['assetContractVersion']",
        'contract-bootstrap.json',
        'bootstrapContract',
        'emit_runtime_contract_bootstrap()',
        'runtime contract emitter drifted',
    ], 'asset contract generator syncs header version'),
    (WORKFLOW, [
        'host-sanity:',
        'build-firmware:',
        'capture-device-smoke:',
        'self-hosted, esp32watch-device',
        'flash_candidate_bundle.py',
        'capture_device_smoke_report.py',
        'promote_release_bundle.py',
        'Upload verified release artifacts',
        'verify-verified-release:',
        'Verify verified release bundle',
    ], 'CI build matrix + candidate/verified bundle validation'),
    (ROOT / 'tools' / 'capture_device_smoke_report.py', [
        'LIVE_HTTP_CAPTURE',
        'storageManualFlush',
        'resetActionResponse',
        'configDeviceReadback',
        'bundleEvidence',
        'flashEvidence',
        'trackedActionAccepted',
        'displayFrame',
        'actionsCatalog',
    ], 'device smoke capture tool exists'),
    (ROOT / 'tools' / 'host_tests' / 'test_runtime_contract_output.py', [
        'runtime /api/contract output matches bootstrap + asset contract',
    ], 'runtime contract output end-to-end host test exists'),
    (ROOT / 'tools' / 'flash_candidate_bundle.py', [
        'flash_candidate_bundle.py',
        'candidateBundleSha256',
        'firmwareSha256',
        'littlefsSha256',
        'LIVE_FLASH',
    ], 'candidate flashing tool exists'),
]

missing: list[str] = []
for path, needles, label in checks:
    text = path.read_text(encoding='utf-8')
    for needle in needles:
        if needle not in text:
            missing.append(f'[{label}] missing `{needle}` in {path.relative_to(ROOT)}')

header_text = HEADER.read_text(encoding='utf-8')
contract_text = WEB_CONTRACT_CPP.read_text(encoding='utf-8')
routes_api_text = WEB_ROUTES_API_CPP.read_text(encoding='utf-8')
route_module_texts = []
for module_path in sorted((ROOT / 'src' / 'web').glob('web_route_module_*.cpp')):
    route_module_texts.append(module_path.read_text(encoding='utf-8'))
route_registration_text = '\n'.join([routes_api_text] + route_module_texts)
app_js_text = APP_JS.read_text(encoding='utf-8')
header_versions = {name: int(value) for name, value in VERSION_RE.findall(header_text)}
route_consts = {const: path for const, path in ROUTE_CONST_RE.findall(header_text)}
contract_routes = APPEND_ROUTE_RE.findall(contract_text)
route_schemas = parse_route_schemas(contract_text)
registered_consts = {const for const, _method in SERVER_ON_CONST_RE.findall(route_registration_text)}
frontend_route_keys = set(CONTRACT_ROUTE_USAGE_RE.findall(app_js_text))

bootstrap = json.loads(BOOTSTRAP.read_text(encoding='utf-8'))
asset = json.loads(ASSET.read_text(encoding='utf-8'))

expected = {
    'apiVersion': header_versions['WEB_API_VERSION'],
    'stateVersion': header_versions['WEB_STATE_VERSION'],
    'runtimeContractVersion': header_versions['WEB_RUNTIME_CONTRACT_VERSION'],
    'assetContractVersion': header_versions['WEB_ASSET_CONTRACT_VERSION'],
}
for key, expected_value in expected.items():
    if bootstrap.get(key) != expected_value:
        missing.append(f'[contract version sync] contract-bootstrap.json `{key}`={bootstrap.get(key)} expected {expected_value}')
    if key == 'assetContractVersion' and asset.get(key) != expected_value:
        missing.append(f'[contract version sync] asset-contract.json `{key}`={asset.get(key)} expected {expected_value}')
if asset.get('bootstrapContract') != bootstrap:
    missing.append('[contract version sync] asset-contract bootstrapContract does not exactly match contract-bootstrap.json')

bootstrap_route_keys = set(bootstrap.get('routes', {}).keys())
contract_route_keys = {key for key, _const in contract_routes}
route_schema_keys = set(route_schemas.keys())
if bootstrap_route_keys != contract_route_keys:
    missing.append(f'[contract route sync] bootstrap routes {sorted(bootstrap_route_keys)} != emitter routes {sorted(contract_route_keys)}')
if contract_route_keys != route_schema_keys:
    missing.append(f'[contract route sync] route schemas {sorted(route_schema_keys)} != emitter routes {sorted(contract_route_keys)}')

for key, const in contract_routes:
    if const not in route_consts:
        missing.append(f'[contract route sync] {key} references missing header constant {const}')
        continue
    emitted_path = bootstrap.get('routes', {}).get(key)
    if emitted_path != route_consts[const]:
        missing.append(f'[contract route sync] bootstrap route {key}={emitted_path} expected header path {route_consts[const]}')
    if const not in registered_consts:
        missing.append(f'[route registration] contract route {key} path {route_consts[const]} is not registered in web_register_api_routes()')

for key in sorted(frontend_route_keys):
    if key not in bootstrap_route_keys:
        missing.append(f'[frontend route coverage] app.js references contractRoute("{key}") but runtime contract does not expose it')

if 'routeSchemas' not in bootstrap:
    missing.append('[contract schema sync] contract-bootstrap.json missing routeSchemas')
else:
    for key in ('contract', 'health', 'meta', 'actionsCatalog', 'actionsLatest', 'actionsStatus', 'stateAggregate', 'displayFrame', 'command', 'configDevice', 'resetAppState', 'storageSemantics'):
        if key not in bootstrap['routeSchemas']:
            missing.append(f'[contract schema sync] contract-bootstrap.json missing routeSchemas.{key}')

if 'apiSchemas' not in bootstrap:
    missing.append('[contract schema sync] contract-bootstrap.json missing apiSchemas')
else:
    for key in ('contractDocument', 'health', 'meta', 'displayFrame', 'trackedActionAccepted', 'actionsStatus', 'actionsCatalog', 'resetActionResponse', 'deviceConfigUpdate', 'configDeviceReadback', 'storageSemantics', 'releaseValidation'):
        if key not in bootstrap['apiSchemas']:
            missing.append(f'[contract schema sync] contract-bootstrap.json missing apiSchemas.{key}')
if 'stateSchemas' not in bootstrap:
    missing.append('[contract schema sync] contract-bootstrap.json missing stateSchemas')

expected_validation_schema = header_versions['WEB_RELEASE_VALIDATION_SCHEMA_VERSION']
if bootstrap.get('releaseValidation', {}).get('validationSchemaVersion') != expected_validation_schema:
    missing.append(f'[contract schema sync] releaseValidation.validationSchemaVersion expected {expected_validation_schema}')

if missing:
    print('\n'.join(missing))
    sys.exit(1)

print('[OK] runtime contract static checks passed')
