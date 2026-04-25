#!/usr/bin/env python3
from __future__ import annotations
import json
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD = Path('/tmp/esp32watch_host_tests')
BUILD.mkdir(parents=True, exist_ok=True)
BIN = BUILD / 'web_contract_emit_test'
CPP = ROOT / 'tools' / 'host_tests' / 'test_web_contract_emit.cpp'
CXX = os.environ.get('CXX', 'g++')
CONTRACT_SOURCES = [
    'src/web/web_contract.cpp',
    'src/web/web_json.cpp',
    'src/web/web_route_catalog_registry.cpp',
    'src/web/web_route_module_registry.cpp',
    'src/web/web_route_module_manifest.cpp',
    'src/web/web_route_module_core.cpp',
    'src/web/web_route_module_actions.cpp',
    'src/web/web_route_module_config.cpp',
    'src/web/web_route_module_reset.cpp',
    'src/web/web_route_module_state.cpp',
    'src/web/web_route_module_display.cpp',
    'src/web/web_route_module_input.cpp',
]
subprocess.run([
    CXX, '-std=c++17', '-Itools/host_stubs', '-Iinclude', '-Isrc',
    str(CPP), *CONTRACT_SOURCES, '-o', str(BIN)
], cwd=ROOT, check=True)
out = subprocess.check_output([str(BIN)], cwd=ROOT, text=True)
payload = json.loads(out)
bootstrap = json.loads((ROOT / 'data' / 'contract-bootstrap.json').read_text(encoding='utf-8'))
asset = json.loads((ROOT / 'data' / 'asset-contract.json').read_text(encoding='utf-8'))
assert payload.get('ok') is True
assert payload.get('contract') == bootstrap, 'runtime /api/contract output drifted from bootstrap contract'
assert asset.get('bootstrapContract') == bootstrap, 'asset-contract bootstrapContract drifted from bootstrap contract'
print('[OK] runtime /api/contract output matches bootstrap + asset contract')
