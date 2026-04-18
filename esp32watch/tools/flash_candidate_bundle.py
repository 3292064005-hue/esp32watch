#!/usr/bin/env python3
from __future__ import annotations
import argparse
import hashlib
import json
import re
import subprocess
import tempfile
import zipfile
from datetime import datetime, timezone
from pathlib import Path

APP_OFFSET = 0x10000
MAC_RE = re.compile(r"([0-9A-Fa-f]{2}(?::[0-9A-Fa-f]{2}){5})")


def now_utc() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace('+00:00', 'Z')


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def parse_littlefs_offset(csv_bytes: bytes) -> int:
    for raw in csv_bytes.decode('utf-8').splitlines():
        line = raw.strip()
        if not line or line.startswith('#'):
            continue
        parts = [part.strip() for part in line.split(',')]
        if len(parts) < 5:
            continue
        _name, part_type, sub_type, offset, _size = parts[:5]
        if part_type == 'data' and sub_type in {'spiffs', 'littlefs'}:
            return int(offset, 0)
    raise SystemExit('unable to locate LittleFS partition offset in partition contract')


def load_candidate(bundle_path: Path) -> dict:
    bundle_bytes = bundle_path.read_bytes()
    bundle_sha256 = sha256_bytes(bundle_bytes)
    with zipfile.ZipFile(bundle_path) as zf:
        validation = json.loads(zf.read('release-validation.json').decode('utf-8'))
        if validation.get('bundleKind') != 'candidate':
            raise SystemExit('flash_candidate_bundle.py requires a candidate bundle')
        firmware = zf.read('firmware.bin')
        littlefs = zf.read('littlefs.bin')
        partition_csv = zf.read('partitions/esp32s3_n16r8_littlefs.csv')
    return {
        'bundleSha256': bundle_sha256,
        'firmware': firmware,
        'littlefs': littlefs,
        'firmwareSha256': sha256_bytes(firmware),
        'littlefsSha256': sha256_bytes(littlefs),
        'littlefsOffset': parse_littlefs_offset(partition_csv),
    }


def probe_device_mac(python_bin: str, port: str) -> str | None:
    try:
        output = subprocess.check_output([
            python_bin, '-m', 'esptool', '--chip', 'esp32s3', '--port', port, 'read_mac'
        ], text=True, stderr=subprocess.STDOUT)
    except Exception:
        return None
    match = MAC_RE.search(output)
    return match.group(1).upper() if match else None


def build_command(python_bin: str, port: str, baud: int, firmware_path: Path, littlefs_path: Path, littlefs_offset: int) -> list[str]:
    return [
        python_bin,
        '-m',
        'esptool',
        '--chip',
        'esp32s3',
        '--port',
        port,
        '--baud',
        str(baud),
        'write_flash',
        hex(APP_OFFSET),
        str(firmware_path),
        hex(littlefs_offset),
        str(littlefs_path),
    ]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--bundle', required=True)
    parser.add_argument('--port', required=True)
    parser.add_argument('--baud', type=int, default=921600)
    parser.add_argument('--python-bin', default='python3')
    parser.add_argument('--output', required=True)
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()

    bundle = Path(args.bundle)
    if not bundle.exists():
        raise SystemExit(f'candidate bundle not found: {bundle}')

    candidate = load_candidate(bundle)

    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp_root = Path(tmp_dir)
        firmware_path = tmp_root / 'firmware.bin'
        littlefs_path = tmp_root / 'littlefs.bin'
        firmware_path.write_bytes(candidate['firmware'])
        littlefs_path.write_bytes(candidate['littlefs'])
        command = build_command(args.python_bin, args.port, args.baud, firmware_path, littlefs_path, candidate['littlefsOffset'])
        device_mac = probe_device_mac(args.python_bin, args.port)
        if not args.dry_run:
            subprocess.run(command, check=True)
            device_mac = probe_device_mac(args.python_bin, args.port) or device_mac

    payload = {
        'flashTool': 'flash_candidate_bundle.py',
        'flashMode': 'DRY_RUN' if args.dry_run else 'LIVE_FLASH',
        'generatedAtUtc': now_utc(),
        'port': args.port,
        'baud': args.baud,
        'appOffset': APP_OFFSET,
        'littlefsOffset': candidate['littlefsOffset'],
        'candidateBundleSha256': candidate['bundleSha256'],
        'firmwareSha256': candidate['firmwareSha256'],
        'littlefsSha256': candidate['littlefsSha256'],
        'command': command,
        'deviceMac': device_mac,
    }
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(payload, indent=2) + '\n', encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
