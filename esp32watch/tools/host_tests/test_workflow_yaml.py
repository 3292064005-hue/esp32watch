#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
workflow_path = ROOT / '.github' / 'workflows' / 'platformio-build.yml'
requirements_text = (ROOT / 'tools' / 'requirements-host.txt').read_text(encoding='utf-8')
text = workflow_path.read_text(encoding='utf-8')

assert 'PyYAML' in requirements_text, 'host Python requirements must declare PyYAML for workflow YAML validation'
assert importlib.util.find_spec('yaml') is not None, 'PyYAML must be importable before running workflow validation'
yaml = __import__('yaml')
data = yaml.safe_load(text)

assert isinstance(data, dict), 'workflow must parse into a mapping'
assert data.get('name') == 'platformio-build', 'unexpected workflow name'

jobs = data.get('jobs', {})
for name in ('host-sanity', 'build-firmware', 'capture-device-smoke', 'verify-verified-release'):
    assert name in jobs, f'missing workflow job: {name}'

gate_expr = "github.event_name == 'release' || (github.event_name == 'workflow_dispatch' && github.event.inputs.release_validation == 'true')"
assert jobs['capture-device-smoke'].get('if') == gate_expr, 'capture-device-smoke gate must only allow release/manual release validation'
assert jobs['verify-verified-release'].get('if') == gate_expr, 'verify-verified-release gate must only allow release/manual release validation'

host_steps = jobs['host-sanity'].get('steps', [])
host_step_runs = '\n'.join(step.get('run', '') for step in host_steps if isinstance(step, dict))
assert 'python3 -m pip install -r tools/requirements-host.txt' in host_step_runs, 'host-sanity job must install declared host Python test dependencies'

assert '--runner-capabilities dist/device-capture-input/runner-capabilities.json' in text, 'workflow must pass runner capability manifest to scenario runner'
assert 'DEVICE_RUNNER_CAPABILITIES_JSON' in text, 'workflow must materialize runner capability manifest from CI vars'
assert '--power-cycle-cmd' not in text, 'workflow must not use legacy power-cycle shell command argument'
assert '--fault-inject-cmd' not in text, 'workflow must not use legacy fault-inject shell command argument'
assert "printf '%s\\n' \"$DEVICE_RUNNER_CAPABILITIES_JSON\"" in text, 'workflow must write runner capability manifest with a single-line printf format string'

print('[OK] workflow YAML parse/gating/capability manifest checks passed')
