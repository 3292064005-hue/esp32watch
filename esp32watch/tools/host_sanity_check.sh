#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
trap 'rm -rf .pio' EXIT

python3 ./tools/generate_asset_contract.py
python3 ./tools/verify_partition_contract.py

if command -v node >/dev/null 2>&1; then
  node --check data/app.js
  node ./tools/web_runtime_smoke.js
else
  echo "[WARN] node not found; skipped app.js syntax check"
fi

python3 - <<'PY'
import pathlib, subprocess, sys
exclude = {'src/syscalls.c'}
files = []
for p in pathlib.Path('src').glob('**/*.c'):
    path = str(p)
    if path in exclude or path.startswith('src/legacy/'):
        continue
    files.append(path)
files = sorted(files)
failed = []
for f in files:
    cmd = ['gcc', '-std=c11', '-fsyntax-only', '-Iinclude', '-Isrc', f]
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if proc.returncode != 0:
        failed.append((f, proc.stderr))
if failed:
    for f, err in failed:
        print(f'[FAIL] {f}')
        print(err)
    sys.exit(1)
print(f'[OK] host C syntax check passed for {len(files)} translation units')
PY

bash ./tools/host_cpp_sanity.sh
python3 ./tools/host_runtime_contract_check.py
bash ./tools/host_behavior_check.sh

if command -v pio >/dev/null 2>&1; then
  if [[ "${SKIP_PIO:-0}" == "1" ]]; then
    echo "[WARN] SKIP_PIO=1 set; skipped PlatformIO build validation"
  else
    timeout "${PIO_TIMEOUT_SEC:-900}" ./tools/verify_platformio_build.sh "${PIO_ENV_NAME:-esp32s3_n16r8_dev}"
  fi
else
  echo "[WARN] pio not found; skipped PlatformIO build validation"
fi

echo "[INFO] Host sanity check completed."
