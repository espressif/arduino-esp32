#!/bin/bash

# Get all boards
boards_array=()

for line in `grep '.tarch=' boards.txt`; do
    board_name=$(echo "$line" | cut -d '.' -f1 | cut -d '#' -f1)
    # skip esp32c2 as we dont build libs for it
    if [ "$board_name" == "esp32c2" ]; then
        echo "Skipping 'espressif:esp32:$board_name'"
        continue
    fi
    boards_array+=("espressif:esp32:$board_name")
    echo "Added 'espressif:esp32:$board_name' to array"
done

# Create JSON like string with all boards found and pass it to env variable
board_count=${#boards_array[@]}
echo "Boards found: $board_count"
echo "BOARD-COUNT=$board_count" >> $GITHUB_ENV

if [ $board_count -gt 0 ]
then
    json_matrix='['
    for board in ${boards_array[@]}
    do
        json_matrix+='"'$board'"'
        if [ $board_count -gt 1 ]
        then
            json_matrix+=","
        fi
        board_count=$(($board_count - 1))
    done
    json_matrix+=']'

    echo $json_matrix
    echo "FQBNS=${json_matrix}" >> $GITHUB_ENV
else
    echo "FQBNS=" >> $GITHUB_ENV
fi
