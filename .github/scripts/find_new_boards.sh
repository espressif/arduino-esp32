#!/bin/bash

# Get inputs from command
owner_repository=$1
base_ref=$2

# Download the boards.txt file from the base branch
curl -L -o boards_base.txt https://raw.githubusercontent.com/$owner_repository/$base_ref/boards.txt

# Compare boards.txt file in the repo with the modified file from PR
diff=$(diff -u boards_base.txt boards.txt)

# Check if the diff is empty
if [ -z "$diff" ]
then
    echo "No changes in boards.txt file"
    echo "FQBNS="
    exit 0
fi

# Extract added or modified lines (lines starting with '+' or '-')
modified_lines=$(echo "$diff" | grep -E '^[+-][^+-]')

# Print the modified lines for debugging
echo "Modified lines:"
echo "$modified_lines"

boards_array=()
previous_board=""

# Extract board names from the modified lines, and add them to the boards_array
while read -r line
do
    board_name=$(echo "$line" | cut -d '.' -f1 | cut -d '#' -f1)
    # remove + or - from the board name at the beginning
    board_name=$(echo "$board_name" | sed 's/^[+-]//')
    if [ "$board_name" != "" ] && [ "$board_name" != "+" ] && [ "$board_name" != "-" ] && [ "$board_name" != "esp32_family" ]
    then
        if [ "$board_name" != "$previous_board" ]
        then
            boards_array+=("espressif:esp32:$board_name")
            previous_board="$board_name"
            echo "Added 'espressif:esp32:$board_name' to array"
        fi
    fi
done <<< "$modified_lines"

# Create JSON like string with all boards found and pass it to env variable
board_count=${#boards_array[@]}

if [ $board_count -gt 0 ]
then
    json_matrix='{"fqbn": ['
    for board in ${boards_array[@]}
    do
        json_matrix+='"'$board'"'
        if [ $board_count -gt 1 ]
        then
            json_matrix+=","
        fi
        board_count=$(($board_count - 1))
    done
    json_matrix+=']}'

    echo $json_matrix
    echo "FQBNS=${json_matrix}" >> $GITHUB_ENV
else
    echo "FQBNS=" >> $GITHUB_ENV
fi