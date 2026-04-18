#!/usr/bin/env python3
from __future__ import annotations
import argparse
import json
import hashlib
import shutil
import tempfile
import zipfile
from pathlib import Path

from verify_release_bundle import validate_device_report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--bundle', required=True)
    parser.add_argument('--env', required=True)
    parser.add_argument('--device-smoke-report', required=True)
    parser.add_argument('--output')
    args = parser.parse_args()

    bundle = Path(args.bundle)
    report_path = Path(args.device_smoke_report)
    if not bundle.exists():
        raise SystemExit(f'bundle not found: {bundle}')
    if not report_path.exists():
        raise SystemExit(f'device smoke report not found: {report_path}')

    with zipfile.ZipFile(bundle) as zf:
        validation = json.loads(zf.read('release-validation.json').decode('utf-8'))
        manifest = json.loads(zf.read('bundle-manifest.json').decode('utf-8'))
        if validation.get('env') != args.env:
            raise SystemExit('candidate bundle env mismatch')
        if validation.get('deviceSmokeStatus') != 'NOT_RUN':
            raise SystemExit('bundle is not a candidate (deviceSmokeStatus must be NOT_RUN)')
        if validation.get('hostValidationStatus') != 'PASS':
            raise SystemExit('candidate bundle host validation must already be PASS')
        device_report = json.loads(report_path.read_text(encoding='utf-8'))
        if device_report.get('status') != 'PASS':
            raise SystemExit('verified bundle promotion requires a PASS device smoke report')
        report_error = validate_device_report(device_report, args.env, 'PASS')
        if report_error is not None:
            raise SystemExit(report_error)
        expected_candidate_name = bundle.name
        actual_candidate_hash = hashlib.sha256(bundle.read_bytes()).hexdigest()
        report_candidate = device_report.get('candidateBundle', {})
        if report_candidate.get('fileName') != expected_candidate_name:
            raise SystemExit(f'device report target bundle mismatch: {report_candidate.get("fileName")} != {expected_candidate_name}')
        if report_candidate.get('sha256') != actual_candidate_hash:
            raise SystemExit('device report candidate bundle sha256 does not match the promotion target')

        out_path = Path(args.output) if args.output else bundle.with_name(bundle.name.replace('-candidate.zip', '-verified.zip'))
        with tempfile.TemporaryDirectory() as tmp_dir:
            tmp_root = Path(tmp_dir)
            zf.extractall(tmp_root)
            normalized_report = tmp_root / 'reports' / 'device-smoke-report.json'
            normalized_report.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(report_path, normalized_report)
            validation['deviceSmokeStatus'] = 'PASS'
            validation['deviceSmokeReportPath'] = 'reports/device-smoke-report.json'
            validation['bundleKind'] = 'verified'
            validation['sourceCandidateBundleName'] = expected_candidate_name
            validation['sourceCandidateBundleSha256'] = actual_candidate_hash
            manifest['deviceSmokeReport'] = 'reports/device-smoke-report.json'
            manifest['bundleKind'] = 'verified'
            manifest['sourceCandidateBundleName'] = expected_candidate_name
            manifest['sourceCandidateBundleSha256'] = actual_candidate_hash
            (tmp_root / 'release-validation.json').write_text(json.dumps(validation, indent=2) + '\n', encoding='utf-8')
            (tmp_root / 'bundle-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
            with zipfile.ZipFile(out_path, 'w', compression=zipfile.ZIP_DEFLATED) as out_zip:
                for path in sorted(tmp_root.rglob('*')):
                    if path.is_file():
                        out_zip.write(path, path.relative_to(tmp_root).as_posix())
    print(out_path)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
