#!/usr/bin/env python3
from __future__ import annotations
import argparse
import hashlib
import json
import zipfile
from pathlib import Path

VALIDATION_STATUS_VALUES = {'PASS', 'FAIL', 'NOT_RUN'}
HOST_REPORT_SCHEMA_VERSION = 1
DEVICE_REPORT_SCHEMA_VERSION = 6
VALIDATION_SCHEMA_VERSION = 7
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

def canonical_sha256(payload: object) -> str:
    return hashlib.sha256(json.dumps(payload, sort_keys=True, separators=(',', ':')).encode('utf-8')).hexdigest()


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
    if generator.get('schemaVersion') != 5:
        return 'device report generator.schemaVersion must be 5'
    if generator.get('captureRunner') not in {'CI_DEVICE_RUNNER', 'LOCAL_DEVICE_RUNNER'}:
        return 'device report generator.captureRunner must be CI_DEVICE_RUNNER or LOCAL_DEVICE_RUNNER'
    if not isinstance(generator.get('runnerIdentity'), str) or not generator.get('runnerIdentity'):
        return 'device report generator.runnerIdentity must be a non-empty string'
    if not isinstance(generator.get('labIdentity'), str) or not generator.get('labIdentity'):
        return 'device report generator.labIdentity must be a non-empty string'


    attestation = report.get('attestation')
    if not isinstance(attestation, dict):
        return 'device report attestation must be an object'
    if not isinstance(attestation.get('physicalActionsAttested'), bool):
        return 'device report attestation.physicalActionsAttested must be boolean'
    if attestation.get('runnerIdentity') != generator.get('runnerIdentity'):
        return 'device report attestation.runnerIdentity must match generator.runnerIdentity'
    if attestation.get('labIdentity') != generator.get('labIdentity'):
        return 'device report attestation.labIdentity must match generator.labIdentity'

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
        if attestation.get('physicalActionsAttested') is not True:
            return 'device report PASS payload requires attestation.physicalActionsAttested=true'
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
        if scenario_evidence.get('schemaVersion') != 3:
            return 'device report PASS payload requires scenarioEvidence.schemaVersion=3'
        if scenario_evidence.get('candidateBundleSha256') != candidate.get('sha256'):
            return 'device report PASS payload requires scenarioEvidence.candidateBundleSha256 to match candidateBundle.sha256'
        scenario_attestation = scenario_evidence.get('attestation')
        if not isinstance(scenario_attestation, dict) or scenario_attestation.get('physicalActionsAttested') is not True:
            return 'device report PASS payload requires scenarioEvidence.attestation.physicalActionsAttested=true'
        if scenario_attestation.get('runnerIdentity') != generator.get('runnerIdentity'):
            return 'device report PASS payload requires scenarioEvidence runner identity to match generator'
        if scenario_attestation.get('labIdentity') != generator.get('labIdentity'):
            return 'device report PASS payload requires scenarioEvidence lab identity to match generator'
        runner_caps = scenario_evidence.get('runnerCapabilities')
        if not isinstance(runner_caps, dict):
            return 'device report PASS payload requires scenarioEvidence.runnerCapabilities object'
        if runner_caps.get('schemaVersion') != 1:
            return 'device report PASS payload requires scenarioEvidence.runnerCapabilities.schemaVersion=1'
        for command_key in ('powerCycle', 'faultInject'):
            command = runner_caps.get(command_key)
            if not isinstance(command, dict):
                return f'device report PASS payload requires scenarioEvidence.runnerCapabilities.{command_key} object'
            argv = command.get('argv')
            if not isinstance(argv, list) or len(argv) == 0 or any((not isinstance(entry, str) or entry == '') for entry in argv):
                return f'device report PASS payload requires scenarioEvidence.runnerCapabilities.{command_key}.argv to be a non-empty string array'
        runner_caps_sha = scenario_evidence.get('runnerCapabilitiesSha256')
        if not isinstance(runner_caps_sha, str) or len(runner_caps_sha) != 64:
            return 'device report PASS payload requires scenarioEvidence.runnerCapabilitiesSha256 64-char hex'
        if runner_caps_sha != canonical_sha256(runner_caps):
            return 'device report PASS payload runnerCapabilitiesSha256 mismatch'
        if scenario_attestation.get('runnerCapabilitiesSha256') != runner_caps_sha:
            return 'device report PASS payload requires scenarioEvidence.attestation.runnerCapabilitiesSha256 to match runnerCapabilitiesSha256'
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
        build_status = validation.get('buildValidationStatus')
        device_status = validation.get('deviceSmokeStatus')
        lab_status = validation.get('labAttestationStatus')
        if host_status not in VALIDATION_STATUS_VALUES:
            return fail(f'invalid hostValidationStatus: {host_status}')
        if build_status not in VALIDATION_STATUS_VALUES:
            return fail(f'invalid buildValidationStatus: {build_status}')
        if device_status not in VALIDATION_STATUS_VALUES:
            return fail(f'invalid deviceSmokeStatus: {device_status}')
        if lab_status not in VALIDATION_STATUS_VALUES:
            return fail(f'invalid labAttestationStatus: {lab_status}')

        gates = validation.get('releaseGates')
        if not isinstance(gates, dict):
            return fail('release-validation releaseGates must be an object')
        if gates.get('hostValid') != host_status or gates.get('buildValid') != build_status or gates.get('deviceValid') != device_status or gates.get('labTruthAttested') != lab_status:
            return fail('release-validation releaseGates must mirror top-level validation statuses')

        evidence_hashes = validation.get('evidenceHashes')
        if not isinstance(evidence_hashes, dict):
            return fail('release-validation evidenceHashes must be an object')
        for key in ('hostValidationReportSha256', 'deviceSmokeReportSha256', 'flashEvidenceSha256', 'scenarioEvidenceSha256'):
            value = evidence_hashes.get(key)
            if value is not None and (not isinstance(value, str) or len(value) != 64):
                return fail(f'release-validation evidenceHashes.{key} must be null or 64-char hex')

        attestation = validation.get('attestation')
        if not isinstance(attestation, dict) or not isinstance(attestation.get('physicalActionsAttested'), bool):
            return fail('release-validation attestation.physicalActionsAttested must be boolean')

        expected_kind = 'verified' if device_status != 'NOT_RUN' else 'candidate'
        if validation.get('bundleKind') != expected_kind:
            return fail(f"release-validation bundleKind mismatch: {validation.get('bundleKind')} != {expected_kind}")


        if build_status != 'PASS':
            return fail('release-validation buildValidationStatus must be PASS for packaged bundles')
        if device_status == 'NOT_RUN':
            if lab_status != 'NOT_RUN' or attestation.get('physicalActionsAttested') is not False:
                return fail('candidate bundle must not claim lab attestation')
        else:
            if lab_status != 'PASS' or attestation.get('physicalActionsAttested') is not True:
                return fail('verified bundle requires PASS lab attestation')
            if not isinstance(validation.get('runnerIdentity'), str) or not validation.get('runnerIdentity'):
                return fail('verified bundle requires runnerIdentity')
            if not isinstance(validation.get('labIdentity'), str) or not validation.get('labIdentity'):
                return fail('verified bundle requires labIdentity')
            if not isinstance(validation.get('sourceCandidateBundleName'), str) or not validation.get('sourceCandidateBundleName'):
                return fail('verified bundle requires sourceCandidateBundleName')
            source_hash = validation.get('sourceCandidateBundleSha256')
            if not isinstance(source_hash, str) or len(source_hash) != 64:
                return fail('verified bundle requires sourceCandidateBundleSha256')

        host_report_path = validation.get('hostValidationReportPath')
        if host_status == 'NOT_RUN':
            if host_report_path is not None:
                return fail('hostValidationReportPath must be absent when host validation status is NOT_RUN')
        else:
            if not isinstance(host_report_path, str) or host_report_path not in names:
                return fail('host validation report missing from bundle')
            host_report_bytes = zf.read(host_report_path)
            host_report = json.loads(host_report_bytes.decode('utf-8'))
            err = validate_host_report(host_report, args.env, host_status)
            if err:
                return fail(err)
            expected_hash = evidence_hashes.get('hostValidationReportSha256')
            if expected_hash and __import__('hashlib').sha256(host_report_bytes).hexdigest() != expected_hash:
                return fail('host validation report hash mismatch')

        device_report_path = validation.get('deviceSmokeReportPath')
        if device_status == 'NOT_RUN':
            if device_report_path is not None:
                return fail('deviceSmokeReportPath must be absent when device smoke status is NOT_RUN')
        else:
            if not isinstance(device_report_path, str) or device_report_path not in names:
                return fail('device smoke report missing from bundle')
            device_report_bytes = zf.read(device_report_path)
            device_report = json.loads(device_report_bytes.decode('utf-8'))
            err = validate_device_report(device_report, args.env, device_status)
            if err:
                return fail(err)
            expected_hash = evidence_hashes.get('deviceSmokeReportSha256')
            if expected_hash and __import__('hashlib').sha256(device_report_bytes).hexdigest() != expected_hash:
                return fail('device smoke report hash mismatch')
            if device_status != 'NOT_RUN':
                def canonical_sha256(payload):
                    return __import__('hashlib').sha256(json.dumps(payload, sort_keys=True, separators=(',', ':')).encode('utf-8')).hexdigest()
                flash_hash = evidence_hashes.get('flashEvidenceSha256')
                if flash_hash and canonical_sha256(device_report.get('flashEvidence')) != flash_hash:
                    return fail('flash evidence hash mismatch')
                scenario_hash = evidence_hashes.get('scenarioEvidenceSha256')
                if scenario_hash and canonical_sha256(device_report.get('scenarioEvidence')) != scenario_hash:
                    return fail('scenario evidence hash mismatch')
                if validation.get('runnerIdentity') != device_report.get('generator', {}).get('runnerIdentity'):
                    return fail('verified bundle runnerIdentity mismatch device report generator.runnerIdentity')
                if validation.get('labIdentity') != device_report.get('generator', {}).get('labIdentity'):
                    return fail('verified bundle labIdentity mismatch device report generator.labIdentity')

    print('[OK] release bundle verified')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
