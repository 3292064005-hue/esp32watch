#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]

checks = [
    (ROOT / 'src' / 'main.cpp', [
        'if (!system_board_peripheral_init())',
        'if (!system_runtime_service_init())',
        'if (!system_web_service_init())'
    ], 'startup transaction gating'),
    (ROOT / 'src' / 'system_init_esp32.cpp', [
        'SYSTEM_INIT_STAGE_NETWORK_SYNC_SERVICE',
        'SYSTEM_INIT_STAGE_WEB_SERVER',
        'system_web_service_init(void)',
        'system_get_startup_status'
    ], 'system init stage/report coverage'),
    (ROOT / 'src' / 'web' / 'web_routes_api.cpp', [
        'server.on("/api/actions/status"',
        'server.on("/api/state/detail"',
        'server.on("/api/state/raw"',
        'web_action_enqueue_tracked',
        'append_command_catalog'
    ], 'web API tracked contract'),
    (ROOT / 'src' / 'web' / 'web_server.cpp', [
        'web_action_mark_running',
        'app_command_execute_detailed',
        'web_action_mark_completed'
    ], 'tracked action execution'),
    (ROOT / 'data' / 'app.js', [
        '/api/state/summary',
        '/api/state/detail',
        '/api/actions/status?id=',
        'waitForActionResult',
        'runTrackedAction'
    ], 'frontend tracked contract usage'),
]

missing = []
for path, needles, label in checks:
    text = path.read_text(encoding='utf-8')
    for needle in needles:
        if needle not in text:
            missing.append(f'[{label}] missing `{needle}` in {path.relative_to(ROOT)}')

if missing:
    print('\n'.join(missing))
    sys.exit(1)

print('[OK] runtime contract static checks passed')
