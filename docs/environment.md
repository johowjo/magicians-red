# Development environment check

Checked 2026-11-27 on the development host:

- macOS 26.3.1, Apple Silicon (`arm64`).
- CMake 4.1.2 executable available at `/opt/homebrew/bin/cmake`.
- AppleClang 15.0.0 C++ compiler selected by CMake.
- `hipcc` and `rocminfo` are not installed; no AMD ROCm/HIP target GPU is
  available on this host.

The CPU reference can be developed and checked here. HIP compilation, device
execution, and GPU measurements require an AMD GPU host with a compatible
ROCm toolchain. The exact ROCm version, driver, GPU model, and wavefront width
must be recorded on that host before implementing or tuning kernels.
