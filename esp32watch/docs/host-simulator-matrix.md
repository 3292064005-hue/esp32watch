# Host Simulator and Matrix Validation

The repository already contains a host runtime simulator and targeted host behavior checks. This document defines how they fit into the P1 validation matrix.

## Matrix axes

- **Runtime ordering**: `STORAGE_AUTHORITY_FIRST` and `LEGACY_CONSUMER_FIRST`
- **Runtime simulator**: page sequence and cooperative-loop stepping
- **Control plane**: Web runtime smoke + runtime contract output
- **Storage / reset**: storage semantics, facade, reset service, release bundle checks

## Required host gates

1. `tools/host_sanity_check.sh`
2. `tools/host_tests/test_host_runtime_simulator.py`
3. `tools/host_tests/test_runtime_contract_output.py`
4. release-bundle host checks

## Follow-up expansion points

- board-profile specific host matrices
- richer UI snapshot replay for new pages
- automated scenario diff baselines for config/reset flows


## Implemented profile matrix

Board/profile coverage is exercised by `./tools/run_host_board_profile_matrix.sh`, which validates the ESP32-S3 watch profile plus the Bluepill battery/full/core profiles.
