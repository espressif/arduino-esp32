#!/bin/bash

#
# Find Arduino board FQBNs for CI.
#
# Usage:
#   find_boards.sh all --output-format=<array|matrix>
#   find_boards.sh new <owner/repo> <base_ref> --output-format=<array|matrix>
#   find_boards.sh official <owner/repo> <base_ref> --output-format=<array|matrix>
#
# --format is accepted as an alias for --output-format.
#
# Subcommands:
#   all       - All boards with .tarch= (skips SKIP_LIB_BUILD_SOCS)
#   new       - Boards changed vs base branch boards.txt
#   official  - Same as new, filtered to CORE_SOCS board IDs
#   help      - Show usage
#
# Outputs:
#   Writes FQBNS and BOARD-COUNT (including 0) to $GITHUB_ENV when set.
#   Prints the JSON payload to stdout when boards are found.
#
# Local testing:
#   Set FIND_BOARDS_BASE_FILE to a local boards.txt to skip curl for new/official.
#   python3 .github/scripts/ci_testing/test_find_boards.py
#

set -euo pipefail

SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPTS_DIR}/socs_config.sh"

PKG_PREFIX="espressif:esp32"
EXCLUDED_ENTRIES=("" "+" "-" "esp32_family" "menu")

OUTPUT_FORMAT=""
COMMAND=""
OWNER_REPOSITORY=""
BASE_REF=""
declare -a BOARD_IDS=()

usage() {
    cat <<'EOF'
Usage:
  find_boards.sh all --output-format=<array|matrix>
  find_boards.sh new <owner/repo> <base_ref> --output-format=<array|matrix>
  find_boards.sh official <owner/repo> <base_ref> --output-format=<array|matrix>

--format is accepted as an alias for --output-format.
EOF
}

is_excluded_entry() {
    local board_name="$1"
    local excluded
    for excluded in "${EXCLUDED_ENTRIES[@]}"; do
        if [ "$board_name" = "$excluded" ]; then
            return 0
        fi
    done
    return 1
}

parse_board_id_from_line() {
    local line="$1"
    local board_name
    board_name=$(echo "$line" | cut -d '.' -f1 | cut -d '#' -f1)
    board_name=${board_name#[-+]}
    board_name=${board_name//[[:space:]]/}
    echo "$board_name"
}

board_id_already_listed() {
    local board_name="$1"
    local existing
    for existing in "${BOARD_IDS[@]+"${BOARD_IDS[@]}"}"; do
        if [ "$existing" = "$board_name" ]; then
            return 0
        fi
    done
    return 1
}

add_board_id() {
    local board_name="$1"
    if is_excluded_entry "$board_name"; then
        return 0
    fi
    if board_id_already_listed "$board_name"; then
        return 0
    fi
    BOARD_IDS+=("$board_name")
    echo "Added '${PKG_PREFIX}:${board_name}' to array"
}

is_core_soc() {
    local board_name="$1"
    local soc
    for soc in "${CORE_SOCS[@]}"; do
        if [ "$soc" = "$board_name" ]; then
            return 0
        fi
    done
    return 1
}

filter_board_ids_to_core_socs() {
    local -a filtered=()
    local board_name
    for board_name in "${BOARD_IDS[@]+"${BOARD_IDS[@]}"}"; do
        if is_core_soc "$board_name"; then
            filtered+=("$board_name")
        else
            echo "Skipping non-official board '${PKG_PREFIX}:${board_name}'"
        fi
    done
    BOARD_IDS=("${filtered[@]+"${filtered[@]}"}")
}

collect_all_boards() {
    local boards_list line board_name
    boards_list=$(grep '.tarch=' boards.txt || true)
    if [ -z "$boards_list" ]; then
        return 0
    fi
    while read -r line; do
        [ -z "$line" ] && continue
        board_name=$(parse_board_id_from_line "$line")
        if should_skip_lib_build "$board_name"; then
            echo "Skipping '${PKG_PREFIX}:${board_name}'"
            continue
        fi
        add_board_id "$board_name"
    done <<< "$boards_list"
}

collect_changed_boards() {
    local owner_repository="$1"
    local base_ref="$2"
    local base_file="boards_base.txt"
    local diff modified_lines line board_name

    # Local/tests: set FIND_BOARDS_BASE_FILE to skip the network fetch.
    if [ -n "${FIND_BOARDS_BASE_FILE:-}" ]; then
        if [ ! -f "$FIND_BOARDS_BASE_FILE" ]; then
            echo "ERROR: FIND_BOARDS_BASE_FILE not found: ${FIND_BOARDS_BASE_FILE}" >&2
            exit 1
        fi
        base_file="$FIND_BOARDS_BASE_FILE"
    else
        curl -fsSL -o "$base_file" \
            "https://raw.githubusercontent.com/${owner_repository}/${base_ref}/boards.txt"
    fi

    diff=$(diff -u "$base_file" boards.txt || true)
    if [ -z "$diff" ]; then
        echo "No changes in boards.txt file"
        return 0
    fi

    modified_lines=$(echo "$diff" | grep -E '^[+-][^+-]' || true)
    echo "Modified lines:"
    echo "$modified_lines"

    if [ -z "$modified_lines" ]; then
        return 0
    fi

    while read -r line; do
        [ -z "$line" ] && continue
        board_name=$(parse_board_id_from_line "$line")
        add_board_id "$board_name"
    done <<< "$modified_lines"
}

emit_json_array() {
    local -a fqbns=("$@")
    local count=${#fqbns[@]}
    local i
    local json='['
    for ((i = 0; i < count; i++)); do
        json+="\"${fqbns[$i]}\""
        if [ "$i" -lt $((count - 1)) ]; then
            json+=','
        fi
    done
    json+=']'
    echo "$json"
}

emit_json_matrix() {
    local array_json
    array_json=$(emit_json_array "$@")
    echo "{\"fqbn\": ${array_json}}"
}

emit_output() {
    local board_count=${#BOARD_IDS[@]}
    local -a fqbns=()
    local board_name payload

    echo "Boards found: $board_count"

    if [ -n "${GITHUB_ENV:-}" ]; then
        echo "BOARD-COUNT=$board_count" >> "$GITHUB_ENV"
    fi

    if [ "$board_count" -eq 0 ]; then
        if [ -n "${GITHUB_ENV:-}" ]; then
            echo "FQBNS=" >> "$GITHUB_ENV"
        fi
        echo "FQBNS="
        return 0
    fi

    for board_name in "${BOARD_IDS[@]}"; do
        fqbns+=("${PKG_PREFIX}:${board_name}")
    done

    case "$OUTPUT_FORMAT" in
        array)
            payload=$(emit_json_array "${fqbns[@]}")
            ;;
        matrix)
            payload=$(emit_json_matrix "${fqbns[@]}")
            ;;
        *)
            echo "ERROR: unsupported --output-format: ${OUTPUT_FORMAT}" >&2
            usage >&2
            exit 1
            ;;
    esac

    echo "$payload"
    if [ -n "${GITHUB_ENV:-}" ]; then
        echo "FQBNS=${payload}" >> "$GITHUB_ENV"
    fi
}

parse_args() {
    local arg
    if [ "$#" -lt 1 ]; then
        usage >&2
        exit 1
    fi

    COMMAND="$1"
    shift

    case "$COMMAND" in
        all) ;;
        new|official)
            if [ "$#" -lt 2 ]; then
                echo "ERROR: ${COMMAND} requires <owner/repo> <base_ref>" >&2
                usage >&2
                exit 1
            fi
            OWNER_REPOSITORY="$1"
            BASE_REF="$2"
            shift 2
            ;;
        -h|--help|help)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: unknown subcommand: ${COMMAND}" >&2
            usage >&2
            exit 1
            ;;
    esac

    while [ "$#" -gt 0 ]; do
        arg="$1"
        case "$arg" in
            --output-format=*|--format=*)
                OUTPUT_FORMAT="${arg#*=}"
                ;;
            --output-format|--format)
                if [ "$#" -lt 2 ]; then
                    echo "ERROR: $arg requires a value (array|matrix)" >&2
                    exit 1
                fi
                OUTPUT_FORMAT="$2"
                shift
                ;;
            *)
                echo "ERROR: unknown argument: $arg" >&2
                usage >&2
                exit 1
                ;;
        esac
        shift
    done

    if [ -z "$OUTPUT_FORMAT" ]; then
        echo "ERROR: --output-format=<array|matrix> is required" >&2
        usage >&2
        exit 1
    fi

    case "$OUTPUT_FORMAT" in
        array|matrix) ;;
        *)
            echo "ERROR: unsupported --output-format: ${OUTPUT_FORMAT}" >&2
            usage >&2
            exit 1
            ;;
    esac
}

parse_args "$@"

case "$COMMAND" in
    all)
        collect_all_boards
        ;;
    new)
        collect_changed_boards "$OWNER_REPOSITORY" "$BASE_REF"
        ;;
    official)
        collect_changed_boards "$OWNER_REPOSITORY" "$BASE_REF"
        filter_board_ids_to_core_socs
        ;;
    help)
        usage
        exit 0
        ;;
    *)
        echo "ERROR: unknown subcommand: ${COMMAND}" >&2
        usage >&2
        exit 1
        ;;
esac

emit_output
