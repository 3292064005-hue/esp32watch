#!/usr/bin/env python3
from __future__ import annotations
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
workflow_path = ROOT / '.github' / 'workflows' / 'platformio-build.yml'
requirements_text = (ROOT / 'tools' / 'requirements-host.txt').read_text(encoding='utf-8')
text = workflow_path.read_text(encoding='utf-8')

assert 'name: platformio-build' in text, 'unexpected workflow name'
for job in ('host-sanity:', 'build-firmware:', 'capture-device-smoke:', 'verify-verified-release:'):
    assert job in text, f'missing workflow job: {job}'

gate_expr = "github.event_name == 'release' || (github.event_name == 'workflow_dispatch' && github.event.inputs.release_validation == 'true')"
assert f'if: "{gate_expr}"' in text, 'release/device validation gate must be preserved'
assert text.count(f'if: "{gate_expr}"') >= 2, 'capture and verified-release jobs must both be gated'

assert 'python3 -m pip install -r tools/requirements-host.txt' in text, 'host-sanity job must preserve requirements install step for future deps'
assert 'Host validation is intentionally stdlib-only' in requirements_text, 'requirements file must document stdlib-only host validation'
assert '--runner-capabilities dist/device-capture-input/runner-capabilities.json' in text, 'workflow must pass runner capability manifest to scenario runner'
assert 'DEVICE_RUNNER_CAPABILITIES_JSON' in text, 'workflow must materialize runner capability manifest from CI vars'
assert '--power-cycle-cmd' not in text, 'workflow must not use legacy power-cycle shell command argument'
assert '--fault-inject-cmd' not in text, 'workflow must not use legacy fault-inject shell command argument'
assert "printf '%s\\n' \"$DEVICE_RUNNER_CAPABILITIES_JSON\"" in text, 'workflow must write runner capability manifest with a single-line printf format string'

print('[OK] workflow YAML gating/capability manifest checks passed without non-stdlib parser dependencies')
