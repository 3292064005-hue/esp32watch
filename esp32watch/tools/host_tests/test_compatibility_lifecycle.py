#!/usr/bin/env python3
from __future__ import annotations
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
DOC = (ROOT / 'docs' / 'compatibility-lifecycle.md').read_text(encoding='utf-8')
DOC_NORMALIZED = DOC.replace('`', '')
README = (ROOT / 'README.md').read_text(encoding='utf-8')
REGISTRY = json.loads((ROOT / 'tools' / 'compatibility_registry.json').read_text(encoding='utf-8'))
CONTRACT = json.loads((ROOT / 'data' / 'contract-bootstrap.json').read_text(encoding='utf-8'))
assert REGISTRY.get('version') == 1, 'compatibility registry version must be declared'
surfaces = REGISTRY.get('surfaces', [])
assert surfaces, 'compatibility registry must enumerate retained surfaces'
ids=set()
for surface in surfaces:
    sid=surface.get('id')
    assert sid and sid not in ids, f'duplicate or missing surface id: {sid!r}'
    ids.add(sid)
    assert surface.get('label'), f'{sid} must declare a label'
    assert surface.get('owner'), f'{sid} must declare an owner'
    assert surface.get('reason_retained'), f'{sid} must declare a retained reason'
    assert surface.get('removal_condition'), f'{sid} must declare a removal condition'
    assert surface.get('evidence'), f'{sid} must declare evidence entries'
assert 'compatibility-lifecycle.md' in README, 'README must link compatibility lifecycle governance doc'
assert 'compatibility_registry.json' in README, 'README must link machine-readable compatibility registry'
assert 'machine-readable source of truth' in DOC.lower(), 'compatibility lifecycle doc must declare the registry as source of truth'
assert 'engineering guardrail' in DOC.lower(), 'compatibility lifecycle doc must describe engineering guardrails'
for surface in surfaces:
    doc_anchor = surface.get('doc_anchor') or surface['label']
    assert doc_anchor.replace('`','') in DOC_NORMALIZED, f'compatibility doc must mention registered surface: {doc_anchor}'
    for evidence in surface['evidence']:
        path = ROOT / evidence['file']
        assert path.exists(), f'{surface["id"]} evidence file missing: {evidence["file"]}'
        evidence_text = path.read_text(encoding='utf-8')
        normalized_text = evidence_text.replace('`','') if path.suffix in {'.md','.json'} else evidence_text
        for snippet in evidence['snippets']:
            target = snippet.replace('`','') if path.suffix in {'.md','.json'} else snippet
            assert target in normalized_text, f'{surface["id"]} missing evidence snippet in {evidence["file"]}: {snippet}'
route_schema = CONTRACT.get('routeSchemas', {}).get('stateSummary', {})
assert 'stateSummary' in CONTRACT.get('routes', {}), 'runtime contract must expose stateSummary compatibility route'
assert route_schema.get('kind') == 'state' and route_schema.get('name') == 'summary', 'runtime contract must bind stateSummary route to compatibility summary state schema'
print('[OK] compatibility lifecycle registry, declared surfaces, and host coverage checks passed')
