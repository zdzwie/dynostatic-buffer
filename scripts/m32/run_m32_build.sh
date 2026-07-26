#!/usr/bin/env bash
#
# Configure, build and test the project as a 32-bit binary using the `dev-m32`
# preset (Debug + unit tests, compiled/linked with -m32).
#
# This is the local equivalent of the CI `unit-tests-32bit` job. Building for a
# 32-bit target exercises a different pointer / size_t / alignof width, which is
# exactly where this allocator's size and alignment assumptions could break.
#
# Usage:
#   scripts/m32/run_m32_build.sh
#
# Requires: cmake and a C/C++ toolchain with 32-bit support. On Debian/Ubuntu:
#   sudo apt-get install gcc-multilib g++-multilib
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

# Preflight: fail early with a clear hint if the toolchain can't target 32-bit,
# rather than deep inside the CMake configure step.
if ! echo 'int main(void){return 0;}' | "${CC:-cc}" -m32 -x c - -o /dev/null 2>/dev/null; then
    echo "error: the C compiler cannot build 32-bit (-m32) binaries." >&2
    echo "       On Debian/Ubuntu: sudo apt-get install gcc-multilib g++-multilib" >&2
    exit 1
fi

echo "==> Configuring (dev-m32 preset, 32-bit)"
cmake --preset dev-m32

echo "==> Building"
cmake --build --preset dev-m32

echo "==> Running tests"
ctest --preset dev-m32

echo "==> Done."
