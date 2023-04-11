#!/bin/bash

# Get inputs from command
owner_repository=$1
pr_number=$2

url="https://api.github.com/repos/$owner_repository/pulls/$pr_number/files"
echo $url

# Get changes in boards.txt file from PR
Patch=$(curl $url | jq -r '.[] | select(.filename == "boards.txt") | .patch ')

# Extract only changed lines number and count
substring_patch=$(echo "$Patch" | grep -o '@@[^@]*@@')

params_array=()

IFS=$'\n' read -d '' -ra params <<< $(echo "$substring_patch" | grep -oE '[-+][0-9]+,[0-9]+')

for param in "${params[@]}"
do
    echo "The parameter is $param"
    params_array+=("$param")
done

boards_array=()
previous_board=""
file="boards.txt"

# Loop through boards.txt file and extract all boards that were added
for (( c=0; c<${#params_array[@]}; c+=2 ))
do
    deletion_count=$( echo "${params_array[c]}" | cut -d',' -f2 | cut -d' ' -f1 )
    addition_line=$( echo "${params_array[c+1]}" | cut -d'+' -f2 | cut -d',' -f1 )
    addition_count=$( echo "${params_array[c+1]}" | cut -d'+' -f2 | cut -d',' -f2 | cut -d' ' -f1 )
    addition_end=$(($addition_line+$addition_count))
    
    addition_line=$(($addition_line + 3))
    addition_end=$(($addition_end - $deletion_count))

    echo $addition_line
    echo $addition_end

    i=0

    while read -r line
    do
    i=$((i+1))
    if [ $i -lt $addition_line ]
    then
    continue
    elif [ $i -gt $addition_end ]
    then
    break
    fi
    board_name=$(echo "$line" | cut -d '.' -f1 | cut -d '#' -f1)
    if [ "$board_name" != "" ]
    then
        if [ "$board_name" != "$previous_board" ]
        then
            boards_array+=("espressif:esp32:$board_name")
            previous_board="$board_name"
            echo "Added 'espressif:esp32:$board_name' to array"
        fi
    fi
    done < "$file"
done

# Create JSON like string with all boards found and pass it to env variable
board_count=${#boards_array[@]}

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
