#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from web_contract_sources import WEB_CONTRACT_SOURCE_PATHS

ROOT = Path(__file__).resolve().parents[1]


def sha256_sources(paths: list[str]) -> str:
    digest = hashlib.sha256()
    for rel in sorted(paths):
        path = ROOT / rel
        digest.update(rel.encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify generated web contract source hashes.")
    parser.add_argument("--asset-contract", default="data/asset-contract.json")
    args = parser.parse_args()

    contract_path = ROOT / args.asset_contract
    doc = json.loads(contract_path.read_text(encoding="utf-8"))

    contract_sources = WEB_CONTRACT_SOURCE_PATHS
    required_files = doc.get("requiredFiles", [])
    asset_sources = [f"data/{name}" for name in required_files]

    expected_contract = doc.get("contractSourceSha256")
    expected_assets = doc.get("assetSourceSha256")
    actual_contract = sha256_sources(contract_sources)
    actual_assets = sha256_sources(asset_sources)

    if expected_contract != actual_contract:
        raise SystemExit(f"contract source hash drift: {expected_contract} != {actual_contract}")
    if expected_assets != actual_assets:
        raise SystemExit(f"asset source hash drift: {expected_assets} != {actual_assets}")
    print("contract source hashes verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
