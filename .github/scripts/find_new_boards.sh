#!/bin/bash

# Get inputs from command
owner_repository=$1
pr_number=$2

url="https://api.github.com/repos/$owner_repository/pulls/$pr_number/files"
echo $url

# Get changes in boards.txt file from PR
Boards_modified_url=$(curl -s $url | jq -r '.[] | select(.filename == "boards.txt") | .raw_url')

# Echo the modified boards.txt file URL
echo "Modified boards.txt file URL:"
echo $Boards_modified_url

# Download the modified boards.txt file
curl -L -o boards_pr.txt $Boards_modified_url

# Compare boards.txt file in the repo with the modified file
diff=$(diff -u boards.txt boards_pr.txt)

# Extract added or modified lines (lines starting with '+' or '-')
modified_lines=$(echo "$diff" | grep -E '^[+-][^+-]')

boards_array=()
previous_board=""
file="boards.txt"

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