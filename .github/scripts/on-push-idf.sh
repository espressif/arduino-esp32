#!/bin/bash

set -e

CHECK_REQUIREMENTS="./components/arduino-esp32/.github/scripts/sketch_utils.sh check_requirements"

# Export IDF environment
. ${IDF_PATH}/export.sh

if [ -n "$1" ]; then
    # If a file is provided, use it to get the affected examples.
    affected_examples=$(cat "$1" | sort)
else
    # Otherwise, find all examples in ./components/arduino-esp32/idf_component_examples
    affected_examples=$(find ./components/arduino-esp32/idf_component_examples -mindepth 1 -maxdepth 1 -type d | sed 's|^\./components/arduino-esp32/||' | sort)
fi

for example in $affected_examples; do
    example_path="$PWD/components/arduino-esp32/$example"
    if [ -f "$example_path/ci.json" ]; then
        # If the target is listed as false, skip the sketch. Otherwise, include it.
        is_target=$(jq -r --arg target "$IDF_TARGET" '.targets[$target]' "$example_path/ci.json")
        if [[ "$is_target" == "false" ]]; then
            printf "\n\033[93mSkipping %s for target %s\033[0m\n\n" "$example" "$IDF_TARGET"
            continue
        fi
    fi

    idf.py --preview -C "$example_path" set-target "$IDF_TARGET"

    has_requirements=$(${CHECK_REQUIREMENTS} "$example_path" "$example_path/sdkconfig")
    if [ "$has_requirements" -eq 0 ]; then
        printf "\n\033[93m%s does not meet the requirements for %s. Skipping...\033[0m\n\n" "$example" "$IDF_TARGET"
        continue
    fi

    printf "\n\033[95mBuilding %s\033[0m\n\n" "$example"
    idf.py --preview -C "$example_path" -DEXTRA_COMPONENT_DIRS="$PWD/components" build
done
