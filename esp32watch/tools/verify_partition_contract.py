#!/usr/bin/env python3
from __future__ import annotations
import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / 'include' / 'esp32_partition_contract.h'
CSV_FILE = ROOT / 'partitions' / 'esp32s3_n16r8_littlefs.csv'

HEADER_RE = re.compile(r'#define\s+(ESP32_PARTITION_[A-Z0-9_]+)\s+(?:\(?)(0x[0-9A-Fa-f]+|[0-9]+)UL')
EXPECTED = {
    'nvs': ('ESP32_PARTITION_NVS_OFFSET', 'ESP32_PARTITION_NVS_SIZE'),
    'otadata': ('ESP32_PARTITION_OTADATA_OFFSET', 'ESP32_PARTITION_OTADATA_SIZE'),
    'app0': ('ESP32_PARTITION_APP0_OFFSET', 'ESP32_PARTITION_APP0_SIZE'),
    'app1': ('ESP32_PARTITION_APP1_OFFSET', 'ESP32_PARTITION_APP1_SIZE'),
    'littlefs': ('ESP32_PARTITION_LITTLEFS_OFFSET', 'ESP32_PARTITION_LITTLEFS_SIZE'),
    'coredump': ('ESP32_PARTITION_COREDUMP_OFFSET', 'ESP32_PARTITION_COREDUMP_SIZE'),
}


def parse_int(value: str) -> int:
    return int(value.strip(), 0)


def load_header_macros() -> dict[str, int]:
    text = HEADER.read_text(encoding='utf-8')
    macros = {name: parse_int(value) for name, value in HEADER_RE.findall(text)}
    missing = [m for pair in EXPECTED.values() for m in pair if m not in macros]
    if missing:
        raise SystemExit(f'missing partition macros: {missing}')
    return macros


def load_partition_csv() -> dict[str, tuple[int, int]]:
    rows: dict[str, tuple[int, int]] = {}
    with CSV_FILE.open(encoding='utf-8') as fh:
        reader = csv.reader(line for line in fh if line.strip() and not line.lstrip().startswith('#'))
        for row in reader:
            if len(row) < 5:
                continue
            name = row[0].strip()
            offset = parse_int(row[3])
            size = parse_int(row[4])
            rows[name] = (offset, size)
    return rows


def main() -> int:
    macros = load_header_macros()
    rows = load_partition_csv()
    errors = []
    for name, (offset_macro, size_macro) in EXPECTED.items():
        if name not in rows:
            errors.append(f'partition csv missing `{name}`')
            continue
        offset, size = rows[name]
        if offset != macros[offset_macro]:
            errors.append(f'{name} offset mismatch csv={offset:#x} header={macros[offset_macro]:#x}')
        if size != macros[size_macro]:
            errors.append(f'{name} size mismatch csv={size:#x} header={macros[size_macro]:#x}')
    if errors:
        print('\n'.join(errors))
        return 1
    print('[OK] partition contract matches partitions csv')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
