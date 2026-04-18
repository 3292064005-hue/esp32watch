#!/usr/bin/env python3
from __future__ import annotations
import argparse
import json
from datetime import datetime, timezone
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--env', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--runner', choices=('CI_HOST', 'LOCAL_HOST'), default='LOCAL_HOST')
    parser.add_argument('--status', choices=('PASS', 'FAIL'), default='PASS')
    parser.add_argument('--check', action='append', dest='checks', default=[])
    args = parser.parse_args()

    checks = []
    for item in args.checks:
        if '=' not in item:
            raise SystemExit(f'invalid --check value: {item}; expected name=STATUS')
        name, status = item.split('=', 1)
        status = status.upper()
        if status not in {'PASS', 'FAIL', 'SKIP'}:
            raise SystemExit(f'invalid host check status for {name}: {status}')
        checks.append({'name': name, 'status': status})
    if not checks:
        raise SystemExit('at least one --check entry is required')
    if args.status == 'PASS' and any(check['status'] == 'FAIL' for check in checks):
        raise SystemExit('host PASS report cannot contain failing checks')

    payload = {
        'reportType': 'HOST_VALIDATION_REPORT',
        'schemaVersion': 1,
        'env': args.env,
        'status': args.status,
        'runner': args.runner,
        'generatedAtUtc': datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace('+00:00', 'Z'),
        'checks': checks,
    }
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(payload, indent=2) + '\n', encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
