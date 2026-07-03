#!/bin/bash
# Local convenience wrapper for mock upload CI (not used by GHA).
# Usage: bash .github/scripts/ci_testing/mock_upload_validation.sh [cli|builder|all] [soc]

set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec bash "${SCRIPT_DIR}/../test-mock-upload.sh" "$@"
