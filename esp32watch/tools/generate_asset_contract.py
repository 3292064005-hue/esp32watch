#!/usr/bin/env python3
from __future__ import annotations
import argparse
import hashlib
import json
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

BOOTSTRAP_CONTRACT_NAME = 'contract-bootstrap.json'
ENTRYPOINTS = ['index.html', 'app.js', 'app.css']
REQUIRED_FILES = ENTRYPOINTS + [BOOTSTRAP_CONTRACT_NAME]
FNV_OFFSET = 2166136261
FNV_PRIME = 16777619
ROOT = Path(__file__).resolve().parents[1]
WEB_CONTRACT_HEADER = ROOT / 'include' / 'web' / 'web_contract.h'
WEB_CONTRACT_CPP = ROOT / 'src' / 'web' / 'web_contract.cpp'
WEB_ROUTES_API_CPP = ROOT / 'src' / 'web' / 'web_routes_api.cpp'

VERSION_RE = re.compile(r'static constexpr uint32_t (WEB_[A-Z_]+) = (\d+)U;')
ROUTE_VALUE_RE = re.compile(r'static constexpr const char \*(WEB_ROUTE_[A-Z0-9_]+) = "([^"]+)";')
APPEND_ROUTE_RE = re.compile(r'append_route\(response, "([^"]+)", (WEB_ROUTE_[A-Z0-9_]+), (?:true|false)\);')
ROUTE_SCHEMA_ENTRY_RE = re.compile(r'\{\"([^\"]+)\",\s*\"([^\"]+)\",\s*\"([^\"]+)\"\}')
STATE_VIEW_ARRAY_RE = re.compile(r'static const WebStateSectionSpec (g_web_state_view_[a-z]+)\[] = \{(.*?)\};', re.S)
STATE_VIEW_ENTRY_RE = re.compile(r'\{(WEB_STATE_SECTION_[A-Z_]+),\s*([^}]+)\}')
STRING_ARRAY_RE = re.compile(r'static constexpr const char \*(k[A-Za-z0-9_]+)\[] = \{(.*?)\};', re.S)
STRING_LITERAL_RE = re.compile(r'"([^"]+)"')
API_SCHEMA_SPEC_RE = re.compile(r'\{"([^"]+)",\s*(k[A-Za-z0-9_]+),\s*\d+U,\s*(k[A-Za-z0-9_]+),\s*\d+U\}')

STATE_SECTION_NAME_MAP = {
    'WEB_STATE_SECTION_WIFI': 'wifi',
    'WEB_STATE_SECTION_SYSTEM': 'system',
    'WEB_STATE_SECTION_CONFIG': 'config',
    'WEB_STATE_SECTION_ACTIVITY': 'activity',
    'WEB_STATE_SECTION_ALARM': 'alarm',
    'WEB_STATE_SECTION_MUSIC': 'music',
    'WEB_STATE_SECTION_SENSOR': 'sensor',
    'WEB_STATE_SECTION_STORAGE': 'storage',
    'WEB_STATE_SECTION_DIAG': 'diag',
    'WEB_STATE_SECTION_DISPLAY': 'display',
    'WEB_STATE_SECTION_WEATHER': 'weather',
    'WEB_STATE_SECTION_SUMMARY': 'summary',
    'WEB_STATE_SECTION_TERMINAL': 'terminal',
    'WEB_STATE_SECTION_OVERLAY': 'overlay',
    'WEB_STATE_SECTION_PERF': 'perf',
    'WEB_STATE_SECTION_STARTUP_RAW': 'startupRaw',
    'WEB_STATE_SECTION_QUEUE_RAW': 'queueRaw',
}

STATE_VIEW_NAME_MAP = {
    'g_web_state_view_summary': 'summary',
    'g_web_state_view_detail': 'detail',
    'g_web_state_view_perf': 'perf',
    'g_web_state_view_raw': 'raw',
    'g_web_state_view_aggregate': 'aggregate',
}



def fnv1a32(data: bytes) -> str:
    h = FNV_OFFSET
    for b in data:
        h ^= b
        h = (h * FNV_PRIME) & 0xFFFFFFFF
    return f'{h:08X}'


def parse_state_view_schemas() -> dict:
    source = WEB_ROUTES_API_CPP.read_text(encoding='utf-8')
    schemas = {}
    for array_name, body in STATE_VIEW_ARRAY_RE.findall(source):
        view_name = STATE_VIEW_NAME_MAP.get(array_name)
        if view_name is None:
            continue
        sections = []
        for section_constant, flags_expr in STATE_VIEW_ENTRY_RE.findall(body):
            section_name = STATE_SECTION_NAME_MAP.get(section_constant, section_constant)
            flags = [flag.strip() for flag in flags_expr.strip().split('|')] if flags_expr.strip() != 'WEB_STATE_SECTION_FLAG_NONE' else []
            sections.append({
                'section': section_name,
                'flags': flags,
            })
        schemas[view_name] = sections
    return schemas




def parse_string_arrays(source: str) -> dict:
    arrays = {}
    for name, body in STRING_ARRAY_RE.findall(source):
        arrays[name] = STRING_LITERAL_RE.findall(body)
    return arrays


def parse_api_schemas() -> dict:
    cpp_text = WEB_CONTRACT_CPP.read_text(encoding='utf-8')
    arrays = parse_string_arrays(cpp_text)
    marker = 'static constexpr ApiSchemaSpec kApiSchemas[] = {'
    start = cpp_text.find(marker)
    if start < 0:
        return {}
    end = cpp_text.find('};', start)
    if end < 0:
        return {}
    body = cpp_text[start:end]
    schemas = {}
    for name, required_key, sections_key in API_SCHEMA_SPEC_RE.findall(body):
        schemas[name] = {
            'type': 'object',
            'required': arrays.get(required_key, []),
            'sections': arrays.get(sections_key, []),
        }
    return schemas


def parse_route_schemas() -> dict:
    cpp_text = WEB_CONTRACT_CPP.read_text(encoding='utf-8')
    marker = 'static constexpr RouteSchemaSpec kRouteSchemas[] = {'
    start = cpp_text.find(marker)
    if start < 0:
        return {}
    end = cpp_text.find('};', start)
    if end < 0:
        return {}
    body = cpp_text[start:end]
    route_schemas = {}
    for route_key, schema_kind, schema_name in ROUTE_SCHEMA_ENTRY_RE.findall(body):
        route_schemas[route_key] = {'kind': schema_kind, 'name': schema_name}
    return route_schemas

def emit_runtime_contract_bootstrap() -> dict:
    build_dir = ROOT / 'dist' / 'host_contract_emit'
    build_dir.mkdir(parents=True, exist_ok=True)
    binary = build_dir / 'web_contract_emit'
    source = ROOT / 'tools' / 'host_tests' / 'test_web_contract_emit.cpp'
    subprocess.run([
        'g++', '-std=c++17', '-Itools/host_stubs', '-Iinclude', '-Isrc',
        str(source), 'src/web/web_contract.cpp', 'src/web/web_json.cpp', '-o', str(binary)
    ], cwd=ROOT, check=True)
    payload = json.loads(subprocess.check_output([str(binary)], cwd=ROOT, text=True))
    contract = payload.get('contract')
    if payload.get('ok') is not True or not isinstance(contract, dict):
        raise SystemExit('runtime contract emitter did not return an ok contract payload')
    return contract


def build_web_contract_bootstrap() -> dict:
    contract = emit_runtime_contract_bootstrap()
    header_text = WEB_CONTRACT_HEADER.read_text(encoding='utf-8')
    versions = {name: int(value) for name, value in VERSION_RE.findall(header_text)}
    expected = {
        'runtimeContractVersion': versions['WEB_RUNTIME_CONTRACT_VERSION'],
        'apiVersion': versions['WEB_API_VERSION'],
        'stateVersion': versions['WEB_STATE_VERSION'],
        'commandCatalogVersion': versions['WEB_COMMAND_CATALOG_VERSION'],
        'assetContractVersion': versions['WEB_ASSET_CONTRACT_VERSION'],
    }
    for key, expected_value in expected.items():
        if contract.get(key) != expected_value:
            raise SystemExit(f'runtime contract emitter drifted for {key}: {contract.get(key)} != {expected_value}')
    if contract.get('releaseValidation', {}).get('validationSchemaVersion') != versions['WEB_RELEASE_VALIDATION_SCHEMA_VERSION']:
        raise SystemExit('runtime contract releaseValidation.validationSchemaVersion drifted from header')
    return contract


def write_bootstrap_contract(data_dir: Path) -> dict:
    contract = build_web_contract_bootstrap()
    out_path = data_dir / BOOTSTRAP_CONTRACT_NAME
    out_path.write_text(json.dumps(contract, indent=2) + '\n', encoding='utf-8')
    return contract


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--data-dir', default='data')
    parser.add_argument('--output', default='data/asset-contract.json')
    args = parser.parse_args()

    data_dir = Path(args.data_dir)
    out_path = Path(args.output)

    contract = write_bootstrap_contract(data_dir)

    files = {}
    for name in REQUIRED_FILES:
        payload = (data_dir / name).read_bytes()
        files[name] = {
            'bytes': len(payload),
            'fnv1a32': fnv1a32(payload),
            'sha256': hashlib.sha256(payload).hexdigest(),
        }

    doc = {
        'assetContractVersion': contract['assetContractVersion'],
        'generatedAtUtc': datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace('+00:00', 'Z'),
        'app': 'esp32watch-web-console',
        'entrypoints': ENTRYPOINTS,
        'requiredFiles': REQUIRED_FILES,
        'bootstrapContract': contract,
        'files': files,
    }
    out_path.write_text(json.dumps(doc, indent=2) + '\n', encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
