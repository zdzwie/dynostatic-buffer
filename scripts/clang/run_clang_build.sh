#!/usr/bin/env bash
#
# Configure, build and test the project with Clang using the `dev` preset
# (Debug + AddressSanitizer + unit tests).
#
# Usage:
#   scripts/clang/run_clang_build.sh
#
# Environment overrides:
#   CC / CXX  explicit compiler binaries (default: resolved clang / clang++)
#
# Requires: cmake and a Clang toolchain.
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

# Resolve clang/clang++: honour $CC/$CXX, otherwise prefer the unversioned name
# and fall back to a known versioned one (matching scripts/clang-tidy).
resolve() {
    local override="$1"; shift
    if [[ -n "${override}" ]]; then
        echo "${override}"
        return 0
    fi
    for candidate in "$@"; do
        if command -v "${candidate}" >/dev/null 2>&1; then
            echo "${candidate}"
            return 0
        fi
    done
    return 1
}

if ! CC="$(resolve "${CC:-}" clang clang-22)"; then
    echo "error: clang not found on PATH (set \$CC to override)" >&2
    exit 1
fi
if ! CXX="$(resolve "${CXX:-}" clang++ clang++-22)"; then
    echo "error: clang++ not found on PATH (set \$CXX to override)" >&2
    exit 1
fi
export CC CXX

echo "==> Using CC=${CC} CXX=${CXX}"

echo "==> Configuring (dev preset)"
cmake --preset dev

echo "==> Building"
cmake --build --preset dev

echo "==> Running tests"
ctest --preset dev

echo "==> Done."
