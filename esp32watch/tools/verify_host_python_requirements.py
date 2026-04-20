#!/usr/bin/env python3
from __future__ import annotations
import importlib
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parent
REQ = ROOT / 'requirements-host.txt'
MODULE_MAP = {'PyYAML': 'yaml'}
missing=[]
for raw in REQ.read_text(encoding='utf-8').splitlines():
    line=raw.strip()
    if not line or line.startswith('#'): continue
    name=line.split('=')[0].split('<')[0].split('>')[0].strip()
    module=MODULE_MAP.get(name,name.replace('-','_'))
    try: importlib.import_module(module)
    except ModuleNotFoundError: missing.append((name,module))
if missing:
    for name,module in missing:
        print(f'[FAIL] Missing host Python dependency: {name} (import {module})', file=sys.stderr)
    print('[HINT] Run: python3 -m pip install -r tools/requirements-host.txt', file=sys.stderr)
    sys.exit(1)
print('[OK] host Python requirements satisfied')
