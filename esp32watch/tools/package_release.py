#!/usr/bin/env python3
from __future__ import annotations
import argparse
import hashlib
import json
import zipfile
from pathlib import Path

VALIDATION_STATUS_VALUES = ('PASS', 'FAIL', 'NOT_RUN')
VALIDATION_SCHEMA_VERSION = 7
HOST_REPORT_NAME = 'reports/host-validation.json'
DEVICE_REPORT_NAME = 'reports/device-smoke-report.json'


def require(path: Path) -> Path:
    if not path.exists():
        raise FileNotFoundError(path)
    return path


def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def normalize_report_path(source: Path | None, normalized_name: str) -> tuple[Path | None, str | None]:
    if source is None:
        return None, None
    return require(source), normalized_name


def build_validation_payload(env: str,
                             host_validation_status: str,
                             host_validation_report_path: str | None,
                             device_smoke_status: str,
                             device_smoke_report_path: str | None) -> dict:
    payload = {
        'validationSchemaVersion': VALIDATION_SCHEMA_VERSION,
        'env': env,
        'hostValidationStatus': host_validation_status,
        'buildValidationStatus': 'PASS',
        'deviceSmokeStatus': device_smoke_status,
        'labAttestationStatus': 'NOT_RUN',
        'bundleKind': 'candidate' if device_smoke_status == 'NOT_RUN' else 'verified',
        'releaseGates': {
            'hostValid': host_validation_status,
            'buildValid': 'PASS',
            'deviceValid': device_smoke_status,
            'labTruthAttested': 'NOT_RUN',
        },
        'runnerIdentity': None,
        'labIdentity': None,
        'attestation': {
            'physicalActionsAttested': False,
        },
        'evidenceHashes': {
            'hostValidationReportSha256': None,
            'deviceSmokeReportSha256': None,
            'flashEvidenceSha256': None,
            'scenarioEvidenceSha256': None,
        },
    }
    if host_validation_report_path is not None:
        payload['hostValidationReportPath'] = host_validation_report_path
    if device_smoke_report_path is not None:
        payload['deviceSmokeReportPath'] = device_smoke_report_path
    return payload


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--env', required=True)
    parser.add_argument('--output-dir', default='dist')
    parser.add_argument('--host-validation-status', choices=VALIDATION_STATUS_VALUES, default='NOT_RUN')
    parser.add_argument('--host-validation-report')
    parser.add_argument('--device-smoke-status', choices=VALIDATION_STATUS_VALUES, default='NOT_RUN')
    parser.add_argument('--device-smoke-report')
    args = parser.parse_args()

    env = args.env
    build = Path('.pio/build') / env
    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    firmware = require(build / 'firmware.bin')
    littlefs = require(build / 'littlefs.bin')
    manifest = require(Path('data/asset-contract.json'))
    bootstrap_contract = require(Path('data/contract-bootstrap.json'))
    partition_contract = require(Path('partitions/esp32s3_n16r8_littlefs.csv'))
    readme = Path('README.md')
    runtime_arch = Path('docs/runtime-architecture.md')
    release_guide = Path('docs/release-validation.md')

    host_report_source, host_report_internal = normalize_report_path(
        Path(args.host_validation_report) if args.host_validation_report else None,
        HOST_REPORT_NAME,
    )
    device_report_source, device_report_internal = normalize_report_path(
        Path(args.device_smoke_report) if args.device_smoke_report else None,
        DEVICE_REPORT_NAME,
    )

    if args.host_validation_status == 'NOT_RUN' and host_report_source is not None:
        raise SystemExit('host validation report cannot be attached when host validation status is NOT_RUN')
    if args.host_validation_status != 'NOT_RUN' and host_report_source is None:
        raise SystemExit('host validation report is required when host validation status is PASS or FAIL')
    if args.device_smoke_status == 'NOT_RUN' and device_report_source is not None:
        raise SystemExit('device smoke report cannot be attached when device smoke status is NOT_RUN')
    if args.device_smoke_status != 'NOT_RUN' and device_report_source is None:
        raise SystemExit('device smoke report is required when device smoke status is PASS or FAIL')

    if args.device_smoke_status != 'NOT_RUN':
        raise SystemExit('package_release.py only emits candidate bundles; use promote_release_bundle.py for verified bundles')
    if args.host_validation_status != 'PASS':
        raise SystemExit('package_release.py requires --host-validation-status PASS for candidate bundles')

    validation = build_validation_payload(env,
                                          args.host_validation_status,
                                          host_report_internal,
                                          args.device_smoke_status,
                                          device_report_internal)
    if host_report_source is not None:
        validation['evidenceHashes']['hostValidationReportSha256'] = sha256_file(host_report_source)

    bundle = out_dir / f'esp32watch-{env}-candidate.zip'
    meta = {
        'env': env,
        'bundleKind': 'candidate',
        'firmware': firmware.name,
        'littlefs': littlefs.name,
        'assetContract': manifest.name,
        'bootstrapContract': bootstrap_contract.name,
        'partitionContract': partition_contract.as_posix(),
        'releaseValidation': 'release-validation.json',
        'hostValidationReport': host_report_internal,
        'deviceSmokeReport': device_report_internal,
    }
    with zipfile.ZipFile(bundle, 'w', compression=zipfile.ZIP_DEFLATED) as zf:
        zf.write(firmware, firmware.name)
        zf.write(littlefs, littlefs.name)
        zf.write(manifest, manifest.name)
        zf.write(bootstrap_contract, bootstrap_contract.name)
        zf.write(partition_contract, partition_contract.as_posix())
        if readme.exists():
            zf.write(readme, 'README.md')
        if runtime_arch.exists():
            zf.write(runtime_arch, 'docs/runtime-architecture.md')
        if release_guide.exists():
            zf.write(release_guide, 'docs/release-validation.md')
        if host_report_source is not None and host_report_internal is not None:
            zf.write(host_report_source, host_report_internal)
        if device_report_source is not None and device_report_internal is not None:
            zf.write(device_report_source, device_report_internal)
        zf.writestr('release-validation.json', json.dumps(validation, indent=2) + '\n')
        zf.writestr('bundle-manifest.json', json.dumps(meta, indent=2) + '\n')
    print(bundle)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
