#!/usr/bin/env python3
from __future__ import annotations
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOC = (ROOT / 'docs' / 'runtime-reload-event-matrix.md').read_text(encoding='utf-8')
MANIFEST = (ROOT / 'src' / 'services' / 'runtime_reload_event_manifest.c').read_text(encoding='utf-8')
MANIFEST_HEADER = (ROOT / 'include' / 'services' / 'runtime_reload_event_manifest.h').read_text(encoding='utf-8')
COORD = (ROOT / 'src' / 'services' / 'runtime_reload_coordinator.c').read_text(encoding='utf-8')
EVENTS = (ROOT / 'src' / 'services' / 'runtime_event_service.c').read_text(encoding='utf-8')

RELOAD_ENTRY_RE = re.compile(
    r'\{(RUNTIME_RELOAD_DOMAIN_[A-Z_]+),\s*"([A-Z]+)",\s*'
    r'(RUNTIME_SERVICE_EVENT_[A-Z_]+),\s*\n\s*'
    r'"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*\n\s*'
    r'"([^"]+)",\s*"([^"]+)",\s*(true|false),\s*(true|false)\}'
)
EVENT_ENTRY_RE = re.compile(
    r'\{(RUNTIME_SERVICE_EVENT_[A-Z_]+),\s*"([^"]+)",\s*\n\s*'
    r'"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*(true|false)\}'
)

reload_entries = RELOAD_ENTRY_RE.findall(MANIFEST)
event_entries = EVENT_ENTRY_RE.findall(MANIFEST)
assert reload_entries, 'runtime reload manifest must declare at least one consumer row'
assert event_entries, 'runtime event manifest must declare at least one producer row'

assert 'RuntimeReloadConsumerManifestEntry' in MANIFEST_HEADER
assert 'RuntimeEventProducerManifestEntry' in MANIFEST_HEADER
assert 'runtime_reload_consumer_manifest_find' in MANIFEST
assert 'runtime_event_producer_manifest_find' in MANIFEST

seen_domains: set[str] = set()
for domain_const, domain_name, event_const, consumer, source_file, strategy, verify_symbol, generation_symbol, requires_subscription, strong_consumer in reload_entries:
    assert domain_const not in seen_domains, f'duplicate reload domain manifest row: {domain_const}'
    seen_domains.add(domain_const)
    assert domain_const in COORD, f'coordinator missing declared domain: {domain_const}'
    assert domain_const in DOC, f'matrix doc missing domain constant: {domain_const}'
    assert domain_name in DOC, f'matrix doc missing domain name: {domain_name}'
    assert consumer in DOC, f'matrix doc missing consumer: {consumer}'
    assert strategy in DOC, f'matrix doc missing apply strategy: {strategy}'
    assert verify_symbol in DOC, f'matrix doc missing verification hook: {verify_symbol}'
    source = (ROOT / source_file).read_text(encoding='utf-8')
    assert verify_symbol in COORD or verify_symbol in source, f'verification hook not wired: {verify_symbol}'
    assert generation_symbol in COORD or generation_symbol in source, f'applied-generation hook not wired: {generation_symbol}'
    if requires_subscription == 'true':
        assert 'RuntimeEventSubscription' in source, f'{domain_name} must declare a RuntimeEventSubscription'
        assert event_const in source, f'{domain_name} subscriber must listen to {event_const}'
        assert domain_const in source, f'{domain_name} subscriber must filter by {domain_const}'
        assert strong_consumer == 'true', f'{domain_name} hot-apply subscriber must be marked as a strong consumer'
    else:
        assert strategy in {'PERSISTED_ONLY', 'REQUIRES_REBOOT'}, f'{domain_name} without subscription must be explicitly non-hot-apply'

for required in [
    'RUNTIME_RELOAD_DOMAIN_WIFI',
    'RUNTIME_RELOAD_DOMAIN_NETWORK',
    'RUNTIME_RELOAD_DOMAIN_AUTH',
    'RUNTIME_RELOAD_DOMAIN_DISPLAY',
    'RUNTIME_RELOAD_DOMAIN_POWER',
    'RUNTIME_RELOAD_DOMAIN_SENSOR',
    'RUNTIME_RELOAD_DOMAIN_COMPANION',
]:
    assert required in seen_domains, f'manifest missing required domain row: {required}'

seen_events: set[str] = set()
for event_const, event_name, producer, source_file, consumer_class, strong_required in event_entries:
    assert event_const == event_name, f'event manifest constant/name drift: {event_const} != {event_name}'
    assert event_const not in seen_events, f'duplicate event manifest row: {event_const}'
    seen_events.add(event_const)
    assert event_const in EVENTS, f'event service missing event-name mapping for {event_const}'
    assert event_const in DOC, f'matrix doc missing event: {event_const}'
    assert producer in DOC, f'matrix doc missing producer: {producer}'
    assert consumer_class in DOC, f'matrix doc missing consumer class: {consumer_class}'
    source = (ROOT / source_file).read_text(encoding='utf-8')
    publish_in_source = event_const in source and ('runtime_event_service_publish' in source or 'runtime_reload_device_config_domains' in source)
    publish_via_coordinator = event_const == 'RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED' and event_const in COORD and 'runtime_event_service_publish_domains' in COORD
    assert publish_in_source or publish_via_coordinator, f'event producer path not wired for {event_const}'
    if strong_required == 'true':
        assert 'hot-apply reload consumers' in consumer_class
        assert any(row[2] == event_const and row[8] == 'true' for row in reload_entries), f'{event_const} requires strong consumers but none are subscribed'

assert 'RUNTIME_SERVICE_EVENT_DEVICE_CONFIG_CHANGED' in seen_events
assert 'RUNTIME_SERVICE_EVENT_FACTORY_RESET_COMPLETED' in seen_events
print('[OK] runtime reload / event manifest, code, and documentation stayed aligned')
