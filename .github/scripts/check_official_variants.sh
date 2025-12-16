#!/bin/bash

#
# Check if Official Variants Changed
# This script determines if a workflow should run based on whether official variants were modified.
#
# Usage:
#   check_official_variants.sh <event_name> <target_list> <changed_files...>
#
# Arguments:
#   event_name: GitHub event name (pull_request, push, etc.)
#   target_list: Which target list to use (BUILD_TEST_TARGETS or IDF_COMPONENT_TARGETS)
#   changed_files: Space-separated list of changed files (all remaining arguments)
#
# Output:
#   Sets should_run=true or should_run=false to $GITHUB_OUTPUT
#

set -e

# Source centralized SoC configuration
SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPTS_DIR}/socs_config.sh"

# Parse arguments
EVENT_NAME="$1"
TARGET_LIST="${2:-BUILD_TEST_TARGETS}"
shift 2
CHANGED_FILES="$*"

# Validate arguments
if [ -z "$EVENT_NAME" ]; then
    echo "ERROR: event_name is required"
    exit 1
fi

if [ -z "$CHANGED_FILES" ]; then
    echo "ERROR: changed_files is required"
    exit 1
fi

# Select the appropriate target list
case "$TARGET_LIST" in
    "BUILD_TEST_TARGETS")
        OFFICIAL_TARGETS=("${BUILD_TEST_TARGETS[@]}")
        ;;
    "IDF_COMPONENT_TARGETS")
        OFFICIAL_TARGETS=("${IDF_COMPONENT_TARGETS[@]}")
        ;;
    *)
        echo "ERROR: Invalid target_list: $TARGET_LIST (must be BUILD_TEST_TARGETS or IDF_COMPONENT_TARGETS)"
        exit 1
        ;;
esac

# Initialize result
should_run="false"

# If not a PR, always run
if [ "$EVENT_NAME" != "pull_request" ]; then
    should_run="true"
    echo "Not a PR, will run"
else
    # Check each changed file
    for file in $CHANGED_FILES; do
        # Check if file is in variants/ directory
        if [[ "$file" == variants/* ]]; then
            # Extract variant name (first directory after variants/)
            variant=$(echo "$file" | cut -d'/' -f2)

            # Check if this variant is in official targets
            for target in "${OFFICIAL_TARGETS[@]}"; do
                if [ "$variant" == "$target" ]; then
                    should_run="true"
                    echo "Official variant changed: $variant"
                    break 2
                fi
            done
        fi
    done
fi

# If no official variants changed, check if non-variant files changed
if [ "$should_run" == "false" ]; then
    for file in $CHANGED_FILES; do
        if [[ "$file" != variants/* ]]; then
            should_run="true"
            echo "Non-variant file changed: $file"
            break
        fi
    done
fi

# Output result
if [ -n "$GITHUB_OUTPUT" ]; then
    echo "should_run=$should_run" >> "$GITHUB_OUTPUT"
fi

if [ "$should_run" == "false" ]; then
    echo "Only non-official variants changed, skipping build"
else
    echo "Will run workflow"
fi

# Also output to stdout for easy testing
echo "$should_run"

