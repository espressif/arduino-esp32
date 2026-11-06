#!/bin/bash
# One-script local release validation (build + JSON + pre-release tests).
# Usage: bash .github/scripts/ci_testing/release_validation.sh [--commit-version] [version]
# Default version: next patch after latest git tag.
#
# Version files are bumped for the build, then reverted by default so the working
# tree is unchanged. Generated hosted/*.bin copies are removed on revert as well.
# Pass --commit-version to keep the bump as a local commit when validation passes
# (version files only; no push).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RELEASE_DIR="$(cd "$SCRIPT_DIR/../release" && pwd)"
source "$RELEASE_DIR/common.sh"

REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
COMMIT_VERSION_BUMP=false
VERSION_ARG=""
VERSION_BUMP_SNAPSHOT_DIR=""

for arg in "$@"; do
    case "$arg" in
        --commit-version) COMMIT_VERSION_BUMP=true ;;
        -h|--help)
            echo "Usage: bash $0 [--commit-version] [version]"
            exit 0
            ;;
        -*)
            echo "ERROR: unknown option: $arg" >&2
            exit 1
            ;;
        *)
            if [ -n "$VERSION_ARG" ]; then
                echo "ERROR: multiple version arguments" >&2
                exit 1
            fi
            VERSION_ARG="$arg"
            ;;
    esac
done

if [ -n "$VERSION_ARG" ]; then
    RELEASE_TAG=$(normalize_release_tag "$VERSION_ARG")
elif ! RELEASE_TAG=$(next_patch_version); then
    echo "ERROR: Could not determine version. Pass version as argument." >&2
    exit 1
fi

finish_release_validation() {
    local rc=$?
    if [ -n "$VERSION_BUMP_SNAPSHOT_DIR" ] && [ -d "$VERSION_BUMP_SNAPSHOT_DIR" ]; then
        if [ "$COMMIT_VERSION_BUMP" = true ] && [ "$rc" -eq 0 ]; then
            echo "Committing version bump for $RELEASE_TAG ..."
            commit_version_bump_only "$RELEASE_TAG" "$REPO_ROOT" || rc=1
        else
            echo "Reverting local version bump files ..."
            restore_version_bump_from_snapshot "$REPO_ROOT" "$VERSION_BUMP_SNAPSHOT_DIR" || true
            restore_hosted_from_snapshot "$REPO_ROOT" "$VERSION_BUMP_SNAPSHOT_DIR" || true
        fi
        rm -rf "$VERSION_BUMP_SNAPSHOT_DIR"
    fi
    exit "$rc"
}

export GITHUB_WORKSPACE="$REPO_ROOT"
export RELEASE_TAG
export RELEASE_PRE="${RELEASE_PRE:-false}"
export DRY_RUN=true JSON_MODE=test

cd "$REPO_ROOT"
echo "Local release validation for $RELEASE_TAG"
echo "Generated files: $RELEASE_DIR/tmp"
if [ "$COMMIT_VERSION_BUMP" = true ]; then
    echo "Version bump: will commit on success (--commit-version)"
else
    echo "Version bump: will revert on exit (pass --commit-version to keep); hosted/ cleaned up on revert"
fi
echo "Note: xz archives are skipped by default locally (SKIP_XZ_GENERATION=true). Set SKIP_XZ_GENERATION=false to build them."

VERSION_BUMP_SNAPSHOT_DIR="$(mktemp -d)"
snapshot_version_bump_files "$REPO_ROOT" "$VERSION_BUMP_SNAPSHOT_DIR"
snapshot_hosted_dir "$REPO_ROOT" "$VERSION_BUMP_SNAPSHOT_DIR"
trap finish_release_validation EXIT

bash "$RELEASE_DIR/build-packages.sh"
bash "$RELEASE_DIR/generate-package-json.sh"
set +e
bash "$RELEASE_DIR/test-package-install.sh" pre
test_rc=$?
set -e
# test-package-install.sh prints the detailed summary to stderr before exiting.
[ "$test_rc" -eq 0 ] || exit "$test_rc"
echo ""
echo "Release validation PASSED for $RELEASE_TAG"
