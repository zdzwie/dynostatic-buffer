#!/usr/bin/env bash
#
# Run clang-tidy over the given C/C++ sources using a CMake-generated
# compilation database.
#
# This is the entry point of the `clang-tidy` pre-commit hook (and, through the
# CI `lint` job that runs pre-commit, of CI as well). pre-commit passes the
# staged source files as arguments.
#
# The compilation database is produced by the `tidy` CMake preset, which
# configures the library and example with CMAKE_EXPORT_COMPILE_COMMANDS=ON and
# DS_BUILD_TESTS=OFF -- so configuring it does NOT fetch googletest. It is
# generated on demand, so a fresh checkout works without a manual build step.
set -euo pipefail

build_dir="build/tidy"
compile_db="${build_dir}/compile_commands.json"

# Resolve a clang-tidy binary: honour $CLANG_TIDY, otherwise prefer the
# unversioned name and fall back to a known versioned one.
clang_tidy="${CLANG_TIDY:-}"
if [[ -z "${clang_tidy}" ]]; then
    for candidate in clang-tidy clang-tidy-18; do
        if command -v "${candidate}" >/dev/null 2>&1; then
            clang_tidy="${candidate}"
            break
        fi
    done
fi
if [[ -z "${clang_tidy}" ]]; then
    echo "error: clang-tidy not found on PATH (set \$CLANG_TIDY to override)" >&2
    exit 1
fi

# Generate the compilation database on first use (configure only, no build).
if [[ ! -f "${compile_db}" ]]; then
    cmake --preset tidy >/dev/null
fi

exec "${clang_tidy}" --config-file=clang-tidy -p "${build_dir}" "$@"
