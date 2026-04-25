#!/usr/bin/env python3
from __future__ import annotations
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ALLOW = {
    Path('include/services/device_config_backend.h'),
    Path('include/services/storage_facade_device_config_backend.h'),
    Path('src/services/device_config.cpp'),
    Path('src/services/storage_facade.c'),
    Path('src/services/device_config_authority.c'),
    Path('tools/host_tests/test_storage_facade.c'),
}
FORBIDDEN = {
    'device_config_backend_apply_update': 'device_config backend writes must stay behind device_config_authority/storage_facade backend boundary',
    'device_config_backend_restore_defaults': 'device_config backend reset must stay behind device_config_authority/storage_facade backend boundary',
    'storage_facade_device_config_backend_apply_update': 'storage_facade backend write path must only be used by authority',
    'storage_facade_device_config_backend_restore_defaults': 'storage_facade backend reset path must only be used by authority',
}

for base in (ROOT / 'src', ROOT / 'include'):
    for rel in base.rglob('*'):
        if not rel.is_file():
            continue
        path = rel.relative_to(ROOT)
        if path in ALLOW:
            continue
        if path.suffix not in {'.c', '.cpp', '.h', '.hpp'}:
            continue
        text = rel.read_text(encoding='utf-8')
        for needle, message in FORBIDDEN.items():
            if needle in text:
                raise AssertionError(f'{path}: {message} ({needle})')

public_header = (ROOT / 'include' / 'services' / 'device_config.h').read_text(encoding='utf-8')
for symbol in ('device_config_save_wifi', 'device_config_save_network_profile', 'device_config_save_api_token', 'device_config_restore_defaults'):
    if symbol not in public_header:
        raise AssertionError(f'include/services/device_config.h missing source-compatible authority wrapper declaration {symbol}')

print('[OK] device-config authority boundary scan passed')
