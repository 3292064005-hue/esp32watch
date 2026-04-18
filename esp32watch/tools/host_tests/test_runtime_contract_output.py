#!/usr/bin/env python3
from __future__ import annotations
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD = Path('/tmp/esp32watch_host_tests')
BUILD.mkdir(parents=True, exist_ok=True)
BIN = BUILD / 'web_contract_emit_test'
CPP = ROOT / 'tools' / 'host_tests' / 'test_web_contract_emit.cpp'
subprocess.run([
    'g++', '-std=c++17', '-Itools/host_stubs', '-Iinclude', '-Isrc',
    str(CPP), 'src/web/web_contract.cpp', 'src/web/web_json.cpp', '-o', str(BIN)
], cwd=ROOT, check=True)
out = subprocess.check_output([str(BIN)], cwd=ROOT, text=True)
payload = json.loads(out)
bootstrap = json.loads((ROOT / 'data' / 'contract-bootstrap.json').read_text(encoding='utf-8'))
asset = json.loads((ROOT / 'data' / 'asset-contract.json').read_text(encoding='utf-8'))
assert payload.get('ok') is True
assert payload.get('contract') == bootstrap, 'runtime /api/contract output drifted from bootstrap contract'
assert asset.get('bootstrapContract') == bootstrap, 'asset-contract bootstrapContract drifted from bootstrap contract'
print('[OK] runtime /api/contract output matches bootstrap + asset contract')
