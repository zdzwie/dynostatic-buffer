#!/usr/bin/env bash
#
# Build the unit tests with coverage instrumentation, run them, and produce an
# lcov coverage report (text summary + HTML).
#
# Usage:
#   scripts/coverage/run_coverage.sh [build_dir] [output_dir]
#
# Defaults:
#   build_dir  = build/coverage
#   output_dir = coverage
#
# Requires: cmake, a C/C++ toolchain, gcov, lcov (>= 1.16) and genhtml.
#
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

BUILD_DIR="${1:-build/coverage}"
OUT_DIR="${2:-coverage}"

# The available --ignore-errors categories and the branch-coverage rc key differ
# between lcov 1.x and 2.x, so pick flags based on the installed version.
LCOV_MAJOR="$(lcov --version 2>/dev/null | grep -oE '[0-9]+' | head -1)"
if [ "${LCOV_MAJOR:-1}" -ge 2 ]; then
    # lcov 2.x is strict about minor gcov/source inconsistencies; downgrade them.
    LCOV_IGNORE=(--ignore-errors mismatch,gcov,source,empty,negative,unused)
    GENHTML_IGNORE=(--ignore-errors mismatch,source,empty,negative,unused)
    LCOV_RC=(--rc branch_coverage=1)
    GENHTML_RC=(--rc branch_coverage=1)
else
    LCOV_IGNORE=(--ignore-errors gcov,source,graph)
    GENHTML_IGNORE=(--ignore-errors source)
    LCOV_RC=(--rc lcov_branch_coverage=1)
    GENHTML_RC=(--rc genhtml_branch_coverage=1)
fi

mkdir -p "$OUT_DIR"

echo "==> Configuring unit tests with coverage flags"
cmake -S tests/unit -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS="--coverage -O0 -g" \
    -DCMAKE_CXX_FLAGS="--coverage -O0 -g" \
    -DCMAKE_EXE_LINKER_FLAGS="--coverage"

echo "==> Building"
cmake --build "$BUILD_DIR"

echo "==> Resetting coverage counters"
lcov "${LCOV_RC[@]}" "${LCOV_IGNORE[@]}" --directory "$BUILD_DIR" --zerocounters

echo "==> Running tests"
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo "==> Capturing coverage data"
lcov "${LCOV_RC[@]}" "${LCOV_IGNORE[@]}" \
    --capture --directory "$BUILD_DIR" \
    --output-file "$OUT_DIR/coverage.info"

echo "==> Filtering out external / test code"
lcov "${LCOV_RC[@]}" "${LCOV_IGNORE[@]}" \
    --remove "$OUT_DIR/coverage.info" \
    '/usr/*' \
    '*/googletest/*' \
    '*/_deps/*' \
    '*/tests/*' \
    --output-file "$OUT_DIR/coverage.filtered.info"

echo "==> Coverage summary"
lcov "${LCOV_RC[@]}" "${LCOV_IGNORE[@]}" --list "$OUT_DIR/coverage.filtered.info"

echo "==> Generating HTML report in $OUT_DIR/html"
genhtml "${GENHTML_RC[@]}" "${GENHTML_IGNORE[@]}" \
    --legend --title "dynostatic-buffer coverage" \
    "$OUT_DIR/coverage.filtered.info" \
    --output-directory "$OUT_DIR/html"

echo "==> Done. Open $OUT_DIR/html/index.html"
