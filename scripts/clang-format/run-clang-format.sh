#!/usr/bin/env bash
#
# Run clang-format over every tracked C/C++ source and header in the
# repository (*.c, *.h, *.cpp, *.hpp).
#
# The style is loaded from the repo-root `clang-format` file (it lacks the
# leading dot, so it is not auto-discovered and must be given explicitly),
# exactly like the `clang-format` pre-commit hook.
#
# Only files tracked by git are formatted, so build directories and fetched
# third-party code (googletest) are never touched.
#
# Usage:
#   scripts/clang-format/run-clang-format.sh           # reformat files in place
#   scripts/clang-format/run-clang-format.sh --check   # report violations only,
#                                                      # exit non-zero if any
#
# Environment overrides:
#   CLANG_FORMAT  explicit clang-format binary (default: resolved clang-format)
#
# Requires: git and clang-format (22, to match the pre-commit hook).
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

mode="fix"
case "${1:-}" in
    "") ;;
    --check) mode="check" ;;
    -h|--help)
        sed -n '2,/^set -euo/p' "$0" | sed '$d; s/^# \{0,1\}//'
        exit 0
        ;;
    *)
        echo "error: unknown argument '$1' (expected --check or nothing)" >&2
        exit 2
        ;;
esac

# Resolve a clang-format binary: honour $CLANG_FORMAT, otherwise prefer the
# unversioned name and fall back to a known versioned one.
clang_format="${CLANG_FORMAT:-}"
if [[ -z "${clang_format}" ]]; then
    for candidate in clang-format clang-format-22; do
        if command -v "${candidate}" >/dev/null 2>&1; then
            clang_format="${candidate}"
            break
        fi
    done
fi
if [[ -z "${clang_format}" ]]; then
    echo "error: clang-format not found on PATH (set \$CLANG_FORMAT to override)" >&2
    exit 1
fi

mapfile -t files < <(git ls-files '*.c' '*.h' '*.cpp' '*.hpp')
if [[ ${#files[@]} -eq 0 ]]; then
    echo "==> No C/C++ files found."
    exit 0
fi

style_args=(-style=file:clang-format --fallback-style=none)

echo "==> Using $("${clang_format}" --version)"

if [[ "${mode}" == "check" ]]; then
    echo "==> Checking formatting of ${#files[@]} files"
    "${clang_format}" "${style_args[@]}" --dry-run --Werror "${files[@]}"
    echo "==> All files are correctly formatted."
else
    echo "==> Formatting ${#files[@]} files in place"
    "${clang_format}" "${style_args[@]}" -i "${files[@]}"
    echo "==> Done. Review the changes with: git diff"
fi
