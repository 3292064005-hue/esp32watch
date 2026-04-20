#!/usr/bin/env python3
from __future__ import annotations
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
DOC = ROOT / 'docs' / 'change-validation-matrix.md'
PR_TEMPLATE = ROOT / '.github' / 'pull_request_template.md'

required_doc = [
    'Route change',
    'Config change',
    'Runtime request change',
    'Reset semantic change',
    'Storage semantic change',
    'Device validation policy',
    'runtime contract updated',
    'host behavior coverage added or updated',
]

required_template = [
    'route change',
    'config change',
    'runtime request change',
    'reset semantic change',
    'storage semantic change',
    'I reviewed `docs/change-validation-matrix.md`',
    'I updated runtime contract / bootstrap / asset contract if the API surface changed',
    'I updated host tests or documented why existing coverage is sufficient',
    'I documented migration / compatibility / rollback impact',
    'device validation required',
    'host/static validation only',
]

missing: list[str] = []

def read_changed_files() -> list[str]:
    cli_args = [arg for arg in sys.argv[1:] if arg]
    if cli_args:
        return cli_args

    env_value = os.environ.get('VALIDATION_CHANGED_FILES', '').strip()
    if env_value:
        return [item for item in env_value.split(os.pathsep) if item]

    release_manifest = ROOT / 'tools' / 'release_changed_files.txt'
    if release_manifest.exists():
        return [line.strip() for line in release_manifest.read_text(encoding='utf-8').splitlines() if line.strip() and not line.strip().startswith('#')]

    if (ROOT / '.git').exists():
        try:
            output = subprocess.check_output(
                ['git', 'diff', '--name-only', '--diff-filter=ACMRTUXB', 'HEAD'],
                cwd=ROOT,
                text=True,
            )
            return [line.strip() for line in output.splitlines() if line.strip()]
        except Exception:
            return []
    return []


def matches_prefix(path: str, prefixes: tuple[str, ...]) -> bool:
    return any(path.startswith(prefix) for prefix in prefixes)


doc_text = DOC.read_text(encoding='utf-8') if DOC.exists() else ''
pr_text = PR_TEMPLATE.read_text(encoding='utf-8') if PR_TEMPLATE.exists() else ''
changed_files = read_changed_files()
changed_set = set(changed_files)

if not DOC.exists():
    missing.append('missing docs/change-validation-matrix.md')
if not PR_TEMPLATE.exists():
    missing.append('missing .github/pull_request_template.md')

for needle in required_doc:
    if needle not in doc_text:
        missing.append(f'docs/change-validation-matrix.md missing `{needle}`')

for needle in required_template:
    if needle not in pr_text:
        missing.append(f'.github/pull_request_template.md missing `{needle}`')

if changed_files:
    route_related = [
        path for path in changed_files
        if matches_prefix(path, ('src/web/', 'include/web/', 'data/app.js', 'data/index.html'))
    ]
    config_related = [
        path for path in changed_files
        if matches_prefix(path, ('src/services/device_config', 'include/services/device_config', 'src/storage_facade', 'src/persist', 'include/persist'))
    ]
    reset_related = [path for path in changed_files if 'reset_service' in path]
    runtime_related = [
        path for path in changed_files
        if 'runtime_side_effect' in path or 'watch_app_effects' in path or 'watch_app_runtime' in path or 'runtime_reload' in path
    ]

    if route_related:
        required_companions = {
            'src/web/web_contract.cpp',
            'data/contract-bootstrap.json',
            'data/asset-contract.json',
            'docs/control-plane-surfaces.md',
        }
        if not (required_companions & changed_set):
            missing.append('route-related changes detected without runtime contract/bootstrap/asset/doc companion updates')

    if config_related and not any(path.startswith('tools/host_tests/') or path.endswith('_check.py') or path.endswith('_check.sh') for path in changed_files):
        missing.append('config/storage changes detected without host test or validation-script updates')

    if reset_related and 'docs/change-validation-matrix.md' not in changed_set:
        missing.append('reset-related changes detected without change-validation-matrix update evidence')

    if runtime_related and not any('runtime' in path for path in changed_files if path.startswith('tools/host_tests/')):
        missing.append('runtime behavior changes detected without runtime-focused host test updates')

if missing:
    print('\n'.join(missing))
    sys.exit(1)

print('[OK] change validation matrix policy checks passed')
