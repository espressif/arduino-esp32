#!/bin/bash

set -e

CHECK_REQUIREMENTS="./components/arduino-esp32/.github/scripts/sketch_utils.sh check_requirements"

# Export IDF environment
. ${IDF_PATH}/export.sh

# Find all examples in ./components/arduino-esp32/idf_component_examples
idf_component_examples=$(find ./components/arduino-esp32/idf_component_examples -mindepth 1 -maxdepth 1 -type d)

for example in $idf_component_examples; do
    if [ -f "$example"/ci.json ]; then
        # If the target is listed as false, skip the sketch. Otherwise, include it.
        is_target=$(jq -r --arg target "$IDF_TARGET" '.targets[$target]' "$example"/ci.json)
        if [[ "$is_target" == "false" ]]; then
            printf "\n\033[93mSkipping %s for target %s\033[0m\n\n" "$example" "$IDF_TARGET"
            continue
        fi
    fi

    idf.py -C "$example" set-target "$IDF_TARGET"

    has_requirements=$(${CHECK_REQUIREMENTS} "$example" "$example/sdkconfig")
    if [ "$has_requirements" -eq 0 ]; then
        printf "\n\033[93m%s does not meet the requirements for %s. Skipping...\033[0m\n\n" "$example" "$IDF_TARGET"
        continue
    fi

    printf "\n\033[95mBuilding %s\033[0m\n\n" "$example"
    idf.py -C "$example" -DEXTRA_COMPONENT_DIRS="$PWD/components" build
done
