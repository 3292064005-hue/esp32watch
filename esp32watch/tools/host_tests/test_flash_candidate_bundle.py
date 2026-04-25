#!/usr/bin/env python3
from __future__ import annotations
import json
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'tools' / 'flash_candidate_bundle.py'


def write_bundle(path: Path) -> None:
    partition = '# Name, Type, SubType, Offset, Size\nnvs,data,nvs,0x9000,0x5000\nlittlefs,data,littlefs,0x310000,0xF0000\n'
    with zipfile.ZipFile(path, 'w') as zf:
        zf.writestr('firmware.bin', b'firmware')
        zf.writestr('littlefs.bin', b'littlefs')
        zf.writestr('partitions/esp32s3_n16r8_littlefs.csv', partition)
        zf.writestr('release-validation.json', json.dumps({'bundleKind': 'candidate'}))


with tempfile.TemporaryDirectory() as tmpdir:
    root = Path(tmpdir)
    bundle = root / 'candidate.zip'
    out = root / 'flash-evidence.json'
    write_bundle(bundle)
    subprocess.run([
        sys.executable, str(SCRIPT), '--bundle', str(bundle), '--port', '/dev/ttyUSB0',
        '--python-bin', sys.executable, '--dry-run', '--output', str(out)
    ], check=True)
    payload = json.loads(out.read_text(encoding='utf-8'))
    assert payload['flashMode'] == 'DRY_RUN'
    assert payload['appOffset'] == 0x10000
    assert payload['littlefsOffset'] == 0x310000
    assert payload['command'][0:3] == [sys.executable, '-m', 'esptool']
print('[OK] flash_candidate_bundle dry-run report validated')
