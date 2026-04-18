#!/usr/bin/env python3
from __future__ import annotations
import argparse
import json
import zipfile
from pathlib import Path

VALIDATION_STATUS_VALUES = {'PASS', 'FAIL', 'NOT_RUN'}
HOST_REPORT_SCHEMA_VERSION = 1
DEVICE_REPORT_SCHEMA_VERSION = 6
VALIDATION_SCHEMA_VERSION = 6
REQUIRED_DEVICE_CHECKS = (
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
REQUIRED_SCENARIOS = {
    'wifiProvisioning': ('apObserved', 'staRecovered', 'recoveryVerified'),
    'persistence': ('configRoundTripOk', 'appStateRoundTripOk', 'powerCycleOk', 'bootCountIncreased', 'previousBootCountObserved'),
    'bootLoopRecovery': ('faultInjected', 'safeModeObserved', 'recoveryCleared', 'bootCountIncreased', 'previousBootCountObserved'),
}


def fail(msg: str) -> int:
    print(f"[FAIL] {msg}")
    return 1


def validate_host_report(report: dict, env: str, expected_status: str) -> str | None:
    if report.get('reportType') != 'HOST_VALIDATION_REPORT':
        return 'invalid host reportType'
    if report.get('schemaVersion') != HOST_REPORT_SCHEMA_VERSION:
        return 'invalid host report schemaVersion'
    if report.get('env') != env:
        return f"host report env mismatch: {report.get('env')} != {env}"
    if report.get('status') != expected_status:
        return f"host report status mismatch: {report.get('status')} != {expected_status}"
    if report.get('runner') not in {'CI_HOST', 'LOCAL_HOST'}:
        return 'host report runner must be CI_HOST or LOCAL_HOST'
    checks = report.get('checks')
    if not isinstance(checks, list) or len(checks) == 0:
        return 'host report checks must be a non-empty list'
    for check in checks:
        if not isinstance(check, dict):
            return 'host report checks must contain objects'
        if check.get('status') not in {'PASS', 'FAIL', 'SKIP'}:
            return 'host report check status must be PASS, FAIL, or SKIP'
    if expected_status == 'PASS' and any(check.get('status') == 'FAIL' for check in checks):
        return 'host report PASS payload contains failing checks'
    return None




def validate_command_evidence(command_evidence: dict, command_name: str) -> str | None:
    if not isinstance(command_evidence, dict):
        return f'scenario evidence missing commandEvidence.{command_name}'
    if command_evidence.get('executed') is not True:
        return f'scenario evidence commandEvidence.{command_name}.executed must be true'
    if command_evidence.get('evidencePresent') is not True:
        return f'scenario evidence commandEvidence.{command_name}.evidencePresent must be true'
    evidence = command_evidence.get('evidence')
    if not isinstance(evidence, dict) or not evidence:
        return f'scenario evidence commandEvidence.{command_name}.evidence must be a non-empty object'
    if command_evidence.get('bootCountIncreased') is not True:
        return f'scenario evidence commandEvidence.{command_name}.bootCountIncreased must be true'
    previous = command_evidence.get('previousBootCountAfter')
    if not isinstance(previous, int) or previous < 1:
        return f'scenario evidence commandEvidence.{command_name}.previousBootCountAfter must be a positive integer'
    return None
def scenario_passes(scenario: dict, required_flags: tuple[str, ...]) -> bool:
    return scenario.get('executed') is True and all(scenario.get(flag) is True for flag in required_flags)


def validate_device_report(report: dict, env: str, expected_status: str) -> str | None:
    if report.get('reportType') != 'DEVICE_SMOKE_REPORT':
        return 'invalid device reportType'
    if report.get('schemaVersion') != DEVICE_REPORT_SCHEMA_VERSION:
        return 'invalid device report schemaVersion'
    if report.get('env') != env:
        return f"device report env mismatch: {report.get('env')} != {env}"
    if report.get('status') != expected_status:
        return f"device report status mismatch: {report.get('status')} != {expected_status}"
    if report.get('evidenceOrigin') != 'DEVICE_CAPTURE':
        return 'device report evidenceOrigin must be DEVICE_CAPTURE'

    generator = report.get('generator')
    if not isinstance(generator, dict):
        return 'device report generator must be an object'
    if generator.get('tool') != 'capture_device_smoke_report.py':
        return 'device report generator.tool must be capture_device_smoke_report.py'
    if generator.get('captureMode') != 'LIVE_HTTP_CAPTURE':
        return 'device report generator.captureMode must be LIVE_HTTP_CAPTURE'
    if generator.get('schemaVersion') != 4:
        return 'device report generator.schemaVersion must be 4'
    if generator.get('captureRunner') not in {'CI_DEVICE_RUNNER', 'LOCAL_DEVICE_RUNNER'}:
        return 'device report generator.captureRunner must be CI_DEVICE_RUNNER or LOCAL_DEVICE_RUNNER'

    evidence = report.get('evidence')
    if not isinstance(evidence, dict):
        return 'device report evidence must be an object'
    for required_key in ('contract', 'health', 'meta', 'aggregate', 'displayFrame', 'actionsCatalog', 'actionsLatest', 'storageSemantics', 'config', 'manualFlush', 'resetFlow', 'scenarioEvidence'):
        if required_key not in evidence:
            return f'device report evidence missing {required_key}'

    candidate = report.get('candidateBundle')
    if not isinstance(candidate, dict):
        return 'device report candidateBundle must be an object'
    if not isinstance(candidate.get('fileName'), str) or not candidate.get('fileName'):
        return 'device report candidateBundle.fileName is required'
    for key in ('sha256', 'firmwareSha256', 'littlefsSha256'):
        value = candidate.get(key)
        if not isinstance(value, str) or len(value) != 64:
            return f'device report candidateBundle.{key} must be a 64-character hex string'

    bundle_evidence = report.get('bundleEvidence')
    if not isinstance(bundle_evidence, dict):
        return 'device report bundleEvidence must be an object'
    for key in ('bootstrapContractSha256', 'assetContractSha256', 'firmwareSha256', 'littlefsSha256'):
        value = bundle_evidence.get(key)
        if not isinstance(value, str) or len(value) != 64:
            return f'device report bundleEvidence.{key} must be a 64-character hex string'
    for key in (
        'runtimeContractMatchesBootstrap',
        'routesMatchBootstrap',
        'stateSchemasMatchBootstrap',
        'apiSchemasMatchBootstrap',
        'releaseValidationMatchesBootstrap',
        'apiVersionMatchesBootstrap',
        'stateVersionMatchesBootstrap',
        'runtimeContractVersionMatchesBootstrap',
        'assetContractVersionMatchesBootstrap',
        'metaApiVersionMatchesBootstrap',
        'metaStateVersionMatchesBootstrap',
        'metaAssetContractVersionMatchesBootstrap',
        'assetContractBootstrapMatchesBundle',
        'metaDeviceIdentityPresent',
        'metaDeviceMacMatchesFlashEvidence',
        'metaBoardProfileMatchesRequest',
    ):
        if not isinstance(bundle_evidence.get(key), bool):
            return f'device report bundleEvidence.{key} must be boolean'

    flash_evidence = report.get('flashEvidence')
    if expected_status == 'PASS':
        if not isinstance(flash_evidence, dict):
            return 'device report PASS payload requires flashEvidence object'
        if flash_evidence.get('flashTool') != 'flash_candidate_bundle.py':
            return 'device report PASS payload requires flashEvidence.flashTool=flash_candidate_bundle.py'
        if flash_evidence.get('flashMode') != 'LIVE_FLASH':
            return 'device report PASS payload requires flashEvidence.flashMode=LIVE_FLASH'
        for key in ('candidateBundleSha256', 'firmwareSha256', 'littlefsSha256'):
            value = flash_evidence.get(key)
            if not isinstance(value, str) or len(value) != 64:
                return f'device report PASS payload requires flashEvidence.{key} 64-char hex'

    checks = report.get('checks')
    if not isinstance(checks, list) or len(checks) == 0:
        return 'device report checks must be a non-empty list'
    status_by_name: dict[str, str] = {}
    for check in checks:
        if not isinstance(check, dict):
            return 'device report checks must contain objects'
        name = check.get('name')
        status = check.get('status')
        if not isinstance(name, str) or not name:
            return 'device report checks require non-empty name'
        if status not in {'PASS', 'FAIL', 'NOT_RUN', 'UNSUPPORTED'}:
            return 'device report check status must be PASS, FAIL, NOT_RUN, or UNSUPPORTED'
        status_by_name[name] = status
    missing = [name for name in REQUIRED_DEVICE_CHECKS if name not in status_by_name]
    if missing:
        return f"device report missing required checks: {', '.join(missing)}"

    scenario_evidence = report.get('scenarioEvidence')
    if expected_status == 'PASS':
        if not isinstance(scenario_evidence, dict):
            return 'device report PASS payload requires scenarioEvidence object'
        if scenario_evidence.get('reportType') != 'DEVICE_SCENARIO_EVIDENCE':
            return 'device report PASS payload requires scenarioEvidence.reportType=DEVICE_SCENARIO_EVIDENCE'
        if scenario_evidence.get('schemaVersion') != 2:
            return 'device report PASS payload requires scenarioEvidence.schemaVersion=2'
        if scenario_evidence.get('candidateBundleSha256') != candidate.get('sha256'):
            return 'device report PASS payload requires scenarioEvidence.candidateBundleSha256 to match candidateBundle.sha256'
        scenarios = scenario_evidence.get('scenarios')
        if not isinstance(scenarios, dict):
            return 'device report PASS payload requires scenarioEvidence.scenarios object'
        command_evidence = scenario_evidence.get('commandEvidence')
        if not isinstance(command_evidence, dict):
            return 'device report PASS payload requires scenarioEvidence.commandEvidence object'
        for command_name in ('powerCycle', 'faultInject'):
            error = validate_command_evidence(command_evidence.get(command_name), command_name)
            if error is not None:
                return error
        for name, flags in REQUIRED_SCENARIOS.items():
            scenario = scenarios.get(name)
            if not isinstance(scenario, dict):
                return f'device report PASS payload missing scenarioEvidence.scenarios.{name}'
            if not scenario_passes(scenario, flags):
                return f'device report PASS payload scenario `{name}` did not fully pass'
        bad = [name for name in REQUIRED_DEVICE_CHECKS if status_by_name.get(name) != 'PASS']
        if bad:
            return f"device report PASS payload has non-passing required checks: {', '.join(bad)}"
        manual_flush = evidence.get('manualFlush')
        reset_flow = evidence.get('resetFlow')
        if not isinstance(manual_flush, dict) or manual_flush.get('status') != 'APPLIED':
            return 'device report PASS payload requires evidence.manualFlush.status=APPLIED'
        if not isinstance(reset_flow, dict) or reset_flow.get('ok') is not True:
            return 'device report PASS payload requires evidence.resetFlow.ok=true'
        meta = evidence.get('meta')
        aggregate = evidence.get('aggregate')
        config = evidence.get('config')
        health = evidence.get('health')
        display = evidence.get('displayFrame')
        catalog = evidence.get('actionsCatalog')
        latest_action = evidence.get('actionsLatest')
        storage_semantics = evidence.get('storageSemantics')
        contract = evidence.get('contract')
        if not isinstance(meta, dict):
            return 'device report PASS payload requires evidence.meta object'
        if not isinstance(aggregate, dict) or aggregate.get('ok') is not True:
            return 'device report PASS payload requires evidence.aggregate.ok=true'
        if not isinstance(config, dict) or config.get('ok') is not True:
            return 'device report PASS payload requires evidence.config.ok=true'
        if not isinstance(health, dict) or health.get('ok') is not True:
            return 'device report PASS payload requires evidence.health.ok=true'
        if not isinstance(display, dict) or display.get('ok') is not True:
            return 'device report PASS payload requires evidence.displayFrame.ok=true'
        if not isinstance(catalog, dict) or catalog.get('ok') is not True:
            return 'device report PASS payload requires evidence.actionsCatalog.ok=true'
        if not isinstance(latest_action, dict) or latest_action.get('ok') is not True:
            return 'device report PASS payload requires evidence.actionsLatest.ok=true'
        if not isinstance(storage_semantics, dict) or storage_semantics.get('ok') is not True:
            return 'device report PASS payload requires evidence.storageSemantics.ok=true'
        if not isinstance(contract, dict) or not isinstance(contract.get('routes'), dict):
            return 'device report PASS payload requires evidence.contract.routes'
        if evidence.get('scenarioEvidence') != scenario_evidence:
            return 'device report PASS payload requires evidence.scenarioEvidence to match scenarioEvidence'
        if not all(bundle_evidence.get(key) is True for key in (
            'runtimeContractMatchesBootstrap',
            'routesMatchBootstrap',
            'stateSchemasMatchBootstrap',
            'apiSchemasMatchBootstrap',
            'releaseValidationMatchesBootstrap',
            'apiVersionMatchesBootstrap',
            'stateVersionMatchesBootstrap',
            'runtimeContractVersionMatchesBootstrap',
            'assetContractVersionMatchesBootstrap',
            'metaApiVersionMatchesBootstrap',
            'metaStateVersionMatchesBootstrap',
            'metaAssetContractVersionMatchesBootstrap',
            'assetContractBootstrapMatchesBundle',
        )):
            return 'device report PASS payload requires all bundleEvidence match flags=true'
        if candidate.get('firmwareSha256') != bundle_evidence.get('firmwareSha256'):
            return 'device report candidateBundle.firmwareSha256 must match bundleEvidence.firmwareSha256'
        if candidate.get('littlefsSha256') != bundle_evidence.get('littlefsSha256'):
            return 'device report candidateBundle.littlefsSha256 must match bundleEvidence.littlefsSha256'
        if flash_evidence.get('candidateBundleSha256') != candidate.get('sha256'):
            return 'device report PASS payload requires flashEvidence candidate hash to match candidateBundle.sha256'
        if flash_evidence.get('firmwareSha256') != candidate.get('firmwareSha256'):
            return 'device report PASS payload requires flashEvidence firmware hash to match candidateBundle.firmwareSha256'
        if flash_evidence.get('littlefsSha256') != candidate.get('littlefsSha256'):
            return 'device report PASS payload requires flashEvidence littlefs hash to match candidateBundle.littlefsSha256'
        flash_mac = flash_evidence.get('deviceMac')
        meta_identity = evidence.get('meta', {}).get('deviceIdentity', {})
        if flash_mac and str(meta_identity.get('efuseMac', '')).upper() != str(flash_mac).upper():
            return 'device report PASS payload requires evidence.meta.deviceIdentity.efuseMac to match flashEvidence.deviceMac'
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--bundle', required=True)
    parser.add_argument('--env', required=True)
    args = parser.parse_args()

    bundle = Path(args.bundle)
    if not bundle.exists():
        return fail(f"bundle not found: {bundle}")

    required = {
        'firmware.bin',
        'littlefs.bin',
        'asset-contract.json',
        'contract-bootstrap.json',
        'bundle-manifest.json',
        'release-validation.json',
        'README.md',
        'docs/runtime-architecture.md',
        'docs/release-validation.md',
        'partitions/esp32s3_n16r8_littlefs.csv',
    }

    with zipfile.ZipFile(bundle) as zf:
        names = set(zf.namelist())
        missing = sorted(required - names)
        if missing:
            return fail(f"missing bundle entries: {', '.join(missing)}")

        manifest = json.loads(zf.read('bundle-manifest.json').decode('utf-8'))
        validation = json.loads(zf.read('release-validation.json').decode('utf-8'))
        asset_contract = json.loads(zf.read('asset-contract.json').decode('utf-8'))
        bootstrap = json.loads(zf.read('contract-bootstrap.json').decode('utf-8'))

        if manifest.get('env') != args.env:
            return fail(f"bundle manifest env mismatch: {manifest.get('env')} != {args.env}")
        if manifest.get('firmware') != 'firmware.bin' or manifest.get('littlefs') != 'littlefs.bin':
            return fail('bundle manifest binary names are not normalized')
        if manifest.get('partitionContract') != 'partitions/esp32s3_n16r8_littlefs.csv':
            return fail('bundle manifest partition contract name mismatch')
        if manifest.get('releaseValidation') != 'release-validation.json':
            return fail('bundle manifest release validation entry mismatch')
        if asset_contract.get('bootstrapContract') != bootstrap:
            return fail('asset-contract bootstrapContract does not match contract-bootstrap.json')
        if asset_contract.get('assetContractVersion') != bootstrap.get('assetContractVersion'):
            return fail('asset contract version mismatch between manifest and bootstrap')
        if validation.get('env') != args.env:
            return fail(f"release-validation env mismatch: {validation.get('env')} != {args.env}")
        if validation.get('validationSchemaVersion') != VALIDATION_SCHEMA_VERSION:
            return fail(f"release-validation schemaVersion mismatch: {validation.get('validationSchemaVersion')} != {VALIDATION_SCHEMA_VERSION}")

        host_status = validation.get('hostValidationStatus')
        device_status = validation.get('deviceSmokeStatus')
        if host_status not in VALIDATION_STATUS_VALUES:
            return fail(f'invalid hostValidationStatus: {host_status}')
        if device_status not in VALIDATION_STATUS_VALUES:
            return fail(f'invalid deviceSmokeStatus: {device_status}')

        expected_kind = 'verified' if device_status != 'NOT_RUN' else 'candidate'
        if validation.get('bundleKind') != expected_kind:
            return fail(f"release-validation bundleKind mismatch: {validation.get('bundleKind')} != {expected_kind}")

        host_report_path = validation.get('hostValidationReportPath')
        if host_status == 'NOT_RUN':
            if host_report_path is not None:
                return fail('hostValidationReportPath must be absent when host validation status is NOT_RUN')
        else:
            if not isinstance(host_report_path, str) or host_report_path not in names:
                return fail('host validation report missing from bundle')
            host_report = json.loads(zf.read(host_report_path).decode('utf-8'))
            err = validate_host_report(host_report, args.env, host_status)
            if err:
                return fail(err)

        device_report_path = validation.get('deviceSmokeReportPath')
        if device_status == 'NOT_RUN':
            if device_report_path is not None:
                return fail('deviceSmokeReportPath must be absent when device smoke status is NOT_RUN')
        else:
            if not isinstance(device_report_path, str) or device_report_path not in names:
                return fail('device smoke report missing from bundle')
            device_report = json.loads(zf.read(device_report_path).decode('utf-8'))
            err = validate_device_report(device_report, args.env, device_status)
            if err:
                return fail(err)

    print('[OK] release bundle verified')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
