#!/bin/bash

# Source centralized SoC configuration
SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPTS_DIR}/socs_config.sh"

# Get all boards
boards_array=()

boards_list=$(grep '.tarch=' boards.txt)

while read -r line; do
    board_name=$(echo "$line" | cut -d '.' -f1 | cut -d '#' -f1)
    # Skip SoCs that don't have pre-built libs
    if should_skip_lib_build "$board_name"; then
        echo "Skipping 'espressif:esp32:$board_name'"
        continue
    fi
    boards_array+=("espressif:esp32:$board_name")
    echo "Added 'espressif:esp32:$board_name' to array"
done <<< "$boards_list"

# Create JSON like string with all boards found and pass it to env variable
board_count=${#boards_array[@]}
echo "Boards found: $board_count"
echo "BOARD-COUNT=$board_count" >> "$GITHUB_ENV"

if [ "$board_count" -gt 0 ]; then
    json_matrix='['
    for board in "${boards_array[@]}"; do
        json_matrix+='"'$board'"'
        if [ "$board_count" -gt 1 ]; then
            json_matrix+=","
        fi
        board_count=$((board_count - 1))
    done
    json_matrix+=']'

    echo "$json_matrix"
    echo "FQBNS=${json_matrix}" >> "$GITHUB_ENV"
else
    echo "FQBNS=" >> "$GITHUB_ENV"
fi
