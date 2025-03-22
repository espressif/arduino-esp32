#!/bin/bash

build_all=false
chunks_count=0
last_check_files=""
last_check_result=""
gh_output=""

# Define the file patterns
core_files=(
    '\.github/.*'
    'cores/.*'
    'package/.*'
    'tools/.*'
    'platform\.txt'
    'programmers\.txt'
    'variants/esp32/.*'
    'variants/esp32c3/.*'
    'variants/esp32c6/.*'
    'variants/esp32h2/.*'
    'variants/esp32p4/.*'
    'variants/esp32s2/.*'
    'variants/esp32s3/.*'
)
library_files=(
    'libraries/.*/examples/.*'
    'libraries/.*/src/.*'
)
networking_files=(
    'libraries/Network/src/.*'
)
fs_files=(
    'libraries/FS/src/.*'
)
static_sketches_files=(
    'libraries/NetworkClientSecure/examples/WiFiClientSecure/WiFiClientSecure\.ino'
    'libraries/BLE/examples/Server/Server\.ino'
    'libraries/ESP32/examples/Camera/CameraWebServer/CameraWebServer\.ino'
    'libraries/Insights/examples/MinimalDiagnostics/MinimalDiagnostics\.ino'
    'libraries/NetworkClientSecure/src/.*'
    'libraries/BLE/src/.*'
    'libraries/Insights/src/.*'
)
idf_files=(
    'idf_component\.yml'
    'Kconfig\.projbuild'
    'CMakeLists\.txt'
    'variants/esp32c2/.*'
)

# Function to check if any files match the patterns
check_files() {
    local patterns=("$@")
    local files_found=""
    for pattern in "${patterns[@]}"; do
        echo "Checking pattern: $pattern"
        matched_files=$(echo "$gh_output" | grep -E "$pattern")
        echo "matched_files: $matched_files"
        files_found+="$matched_files "
    done

    last_check_files=$(echo "$files_found" | xargs)
    if [[ -n $last_check_files ]]; then
        last_check_result="true"
    else
        last_check_result="false"
    fi
    echo "last_check_result: $last_check_result"
}

if [[ $IS_PR != 'true' ]]; then
    gh_output=$(gh api repos/espressif/arduino-esp32/commits/"$GITHUB_SHA" --jq '.files[].filename')
else
    gh_output=$(gh pr diff "$PR_NUM" --name-only)
fi
echo "gh_output: $gh_output"

# Output the results
check_files "${core_files[@]}"
CORE_CHANGED=$last_check_result
check_files "${library_files[@]}"
LIB_CHANGED=$last_check_result
LIB_FILES=$last_check_files
check_files "${networking_files[@]}"
NETWORKING_CHANGED=$last_check_result
check_files "${fs_files[@]}"
FS_CHANGED=$last_check_result
check_files "${static_sketches_files[@]}"
STATIC_SKETCHES_CHANGED=$last_check_result
check_files "${idf_files[@]}"
IDF_CHANGED=$last_check_result

if [[ $CORE_CHANGED == 'true' ]] || [[ $IS_PR != 'true' ]]; then
    echo "Core files changed or not a PR. Building all."
    build_all=true
    chunks_count=$MAX_CHUNKS
elif [[ $LIB_CHANGED == 'true' ]]; then
    echo "Libraries changed. Building only affected sketches."
    if [[ $NETWORKING_CHANGED == 'true' ]]; then
        echo "Networking libraries changed. Building networking related sketches."
        networking_sketches="$(find libraries/WiFi -name '*.ino') "
        networking_sketches+="$(find libraries/Ethernet -name '*.ino') "
        networking_sketches+="$(find libraries/PPP -name '*.ino') "
        networking_sketches+="$(find libraries/NetworkClientSecure -name '*.ino') "
        networking_sketches+="$(find libraries/WebServer -name '*.ino') "
    fi
    if [[ $FS_CHANGED == 'true' ]]; then
        echo "FS libraries changed. Building FS related sketches."
        fs_sketches="$(find libraries/SD -name '*.ino') "
        fs_sketches+="$(find libraries/SD_MMC -name '*.ino') "
        fs_sketches+="$(find libraries/SPIFFS -name '*.ino') "
        fs_sketches+="$(find libraries/LittleFS -name '*.ino') "
        fs_sketches+="$(find libraries/FFat -name '*.ino') "
    fi
    sketches="$networking_sketches $fs_sketches"
    for file in $LIB_FILES; do
        lib=$(echo "$file" | awk -F "/" '{print $1"/"$2}')
        if [[ "$file" == *.ino ]]; then
            # If file ends with .ino, add it to the list of sketches
            echo "Sketch found: $file"
            sketches+="$file "
        elif [[ "$file" == "$lib/src/"* ]]; then
            # If file is inside the src directory, find all sketches in the lib/examples directory
            echo "Library src file found: $file"
            if [[ -d $lib/examples ]]; then
                lib_sketches=$(find "$lib"/examples -name '*.ino')
                sketches+="$lib_sketches "
                echo "Library sketches: $lib_sketches"
            fi
        else
            # If file is in a example folder but it is not a sketch, find all sketches in the current directory
            echo "File in example folder found: $file"
            sketch=$(find "$(dirname "$file")" -name '*.ino')
            sketches+="$sketch "
            echo "Sketch in example folder: $sketch"
        fi
        echo ""
    done
fi

if [[ -n $sketches ]]; then
    # Remove duplicates
    sketches=$(echo "$sketches" | tr ' ' '\n' | sort | uniq)
    for sketch in $sketches; do
        echo "$sketch" >> sketches_found.txt
        chunks_count=$((chunks_count+1))
    done
    echo "Number of sketches found: $chunks_count"
    echo "Sketches:"
    echo "$sketches"

    if [[ $chunks_count -gt $MAX_CHUNKS ]]; then
        echo "More sketches than the allowed number of chunks found. Limiting to $MAX_CHUNKS chunks."
        chunks_count=$MAX_CHUNKS
    fi
fi

chunks='["0"'
for i in $(seq 1 $(( chunks_count - 1 )) ); do
    chunks+=",\"$i\""
done
chunks+="]"

{
    echo "build_all=$build_all"
    echo "build_libraries=$LIB_CHANGED"
    echo "build_static_sketches=$STATIC_SKETCHES_CHANGED"
    echo "build_idf=$IDF_CHANGED"
    echo "chunk_count=$chunks_count"
    echo "chunks=$chunks"
} >> "$GITHUB_OUTPUT"
