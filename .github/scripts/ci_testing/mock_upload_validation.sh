#!/bin/bash
# Local convenience wrapper for mock upload CI (not used by GHA).
# Usage: bash .github/scripts/ci_testing/mock_upload_validation.sh [cli|builder|all] [soc]
#
# Until bundled esptool supports socket://, use pip esptool for local runs:
#   MOCK_ESPTOOL_OVERRIDE=1 bash .github/scripts/ci_testing/mock_upload_validation.sh cli esp32

set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec bash "${SCRIPT_DIR}/../test-mock-upload.sh" "$@"
