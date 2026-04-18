#!/usr/bin/env python3
from __future__ import annotations

import json
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit('usage: test_host_runtime_simulator.py <output-json>')
    payload = json.loads(Path(sys.argv[1]).read_text(encoding='utf-8'))
    assert payload['simulator'] == 'host_runtime_simulator'
    assert payload['schemaVersion'] == 2
    frames = payload['frames']
    assert isinstance(frames, list) and len(frames) == 4
    expected_pages = ['WATCHFACE', 'ACTIVITY', 'STORAGE', 'ABOUT']
    for index, frame in enumerate(frames):
        assert frame['index'] == index
        assert frame['page'] == expected_pages[index]
        assert frame['nowMs'] == index * 16
        assert frame['loopCount'] >= index + 1
        assert frame['stageHistoryCount'] >= 0
        assert isinstance(frame['frameCrc32'], int) and frame['frameCrc32'] > 0
        assert isinstance(frame['litPixels'], int) and frame['litPixels'] > 0
    # Guard against an accidentally blank renderer or duplicated frames.
    assert len({frame['frameCrc32'] for frame in frames}) == len(frames)
    runtime = payload['runtime']
    assert runtime['finalLoopCount'] >= len(frames)
    assert runtime['stageHistoryCount'] >= 0
    assert runtime['currentPage'] == 'ABOUT'
    print('[OK] host runtime simulator check passed')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
