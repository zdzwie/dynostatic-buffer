#!/usr/bin/env bash
#
# Configure, build and test the project with MSVC using the `dev-msvc` preset
# (Debug + unit tests, Visual Studio generator).
#
# This is the local equivalent of the CI `unit-tests-msvc` job. MSVC is the one
# toolchain in CI that is not GCC/Clang-derived, so it catches a genuinely
# different class of portability issues (secure-CRT diagnostics, <stdalign.h>
# handling, its own undefined-behaviour choices).
#
# Intended to run ON WINDOWS from a shell that has `cmake` on PATH (e.g. Git
# Bash / MSYS2), with Visual Studio or the Build Tools installed so CMake can
# locate the MSVC toolchain. It will not work on Linux/WSL, which has no MSVC.
#
# Usage (Windows shell):
#   scripts/msvc/run_msvc_build.sh
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

echo "==> Configuring (dev-msvc preset)"
cmake --preset dev-msvc

echo "==> Building"
cmake --build --preset dev-msvc

echo "==> Running tests"
ctest --preset dev-msvc

echo "==> Done."
