#!/bin/bash

export PLATFORMIO_ESP32_PATH="$HOME/.platformio/packages/framework-arduinoespressif32"
PLATFORMIO_ESP32_URL="https://github.com/platformio/platform-espressif32.git"

TOOLCHAIN_VERSION="12.2.0+20230208"
ESPTOOLPY_VERSION="~1.40501.0"
ESPRESSIF_ORGANIZATION_NAME="espressif"
SDKCONFIG_DIR="$PLATFORMIO_ESP32_PATH/tools/esp32-arduino-libs"

echo "Installing Python Wheel ..."
pip install wheel > /dev/null 2>&1

echo "Installing PlatformIO ..."
pip install -U https://github.com/platformio/platformio/archive/master.zip > /dev/null 2>&1

echo "Installing Platform ESP32 ..."
python -m platformio platform install $PLATFORMIO_ESP32_URL  > /dev/null 2>&1

echo "Replacing the package versions ..."
replace_script="import json; import os;"
replace_script+="fp=open(os.path.expanduser('~/.platformio/platforms/espressif32/platform.json'), 'r+');"
replace_script+="data=json.load(fp);"
# Use framework sources from the repository
replace_script+="data['packages']['framework-arduinoespressif32']['version'] = '*';"
replace_script+="del data['packages']['framework-arduinoespressif32']['owner'];"
# Use toolchain packages from the "espressif" organization
replace_script+="data['packages']['toolchain-xtensa-esp32']['owner']='$ESPRESSIF_ORGANIZATION_NAME';"
replace_script+="data['packages']['toolchain-xtensa-esp32s2']['owner']='$ESPRESSIF_ORGANIZATION_NAME';"
replace_script+="data['packages']['toolchain-riscv32-esp']['owner']='$ESPRESSIF_ORGANIZATION_NAME';"
# Update versions to use the upstream
replace_script+="data['packages']['toolchain-xtensa-esp32']['version']='$TOOLCHAIN_VERSION';"
replace_script+="data['packages']['toolchain-xtensa-esp32s2']['version']='$TOOLCHAIN_VERSION';"
replace_script+="data['packages']['toolchain-xtensa-esp32s3']['version']='$TOOLCHAIN_VERSION';"
replace_script+="data['packages']['toolchain-riscv32-esp']['version']='$TOOLCHAIN_VERSION';"
# Add new "framework-arduinoespressif32-libs" package
# Read "package_esp32_index.template.json" to extract a url to a zip package for "esp32-arduino-libs"
replace_script+="fpackage=open(os.path.join('package', 'package_esp32_index.template.json'), 'r+');"
replace_script+="package_data=json.load(fpackage);"
replace_script+="fpackage.close();"
replace_script+="libs_package_archive_url=next(next(system['url'] for system in tool['systems'] if system['host'] == 'x86_64-pc-linux-gnu') for tool in package_data['packages'][0]['tools'] if tool['name'] == 'esp32-arduino-libs');"
replace_script+="data['packages'].update({'framework-arduinoespressif32-libs':{'type':'framework','optional':False,'version':libs_package_archive_url}});"
replace_script+="data['packages']['toolchain-xtensa-esp32'].update({'optional':False});"
# esptool.py may require an upstream version (for now platformio is the owner)
replace_script+="data['packages']['tool-esptoolpy']['version']='$ESPTOOLPY_VERSION';"
# Save results
replace_script+="fp.seek(0);fp.truncate();json.dump(data, fp, indent=2);fp.close()"
python -c "$replace_script"

if [ "$GITHUB_REPOSITORY" == "espressif/arduino-esp32" ];  then
    echo "Linking Core..."
    ln -s $GITHUB_WORKSPACE "$PLATFORMIO_ESP32_PATH"
else
    echo "Cloning Core Repository ..."
    git clone --recursive https://github.com/espressif/arduino-esp32.git "$PLATFORMIO_ESP32_PATH" > /dev/null 2>&1
fi

echo "PlatformIO for ESP32 has been installed"
echo ""

function build_pio_sketch(){ # build_pio_sketch <board> <options> <path-to-ino>
    if [ "$#" -lt 3 ]; then
        echo "ERROR: Illegal number of parameters"
        echo "USAGE: build_pio_sketch <board> <options> <path-to-ino>"
        return 1
    fi

    local board="$1"
    local options="$2"
    local sketch="$3"
    local sketch_dir=$(dirname "$sketch")
    echo ""
    echo "Compiling '"$(basename "$sketch")"' ..."
    python -m platformio ci --board "$board" "$sketch_dir" --project-option="$options"
}

function count_sketches(){ # count_sketches <examples-path>
    local examples="$1"
    rm -rf sketches.txt
    if [ ! -d "$examples" ]; then
        touch sketches.txt
        return 0
    fi
    local sketches=$(find $examples -name *.ino)
    local sketchnum=0
    for sketch in $sketches; do
        local sketchdir=$(dirname $sketch)
        local sketchdirname=$(basename $sketchdir)
        local sketchname=$(basename $sketch)
        if [[ "${sketchdirname}.ino" != "$sketchname" ]]; then
            continue
        elif [ -f $sketchdir/ci.json ]; then
            # If the target is listed as false, skip the sketch. Otherwise, include it.
            is_target=$(jq -r '.targets[esp32]' $sketchdir/ci.json)
            if [[ "$is_target" == "false" ]]; then
                continue
            fi

            # Check if the sketch requires any configuration options (AND)
            requirements=$(jq -r '.requires[]? // empty' $sketchdir/ci.json)
            if [[ "$requirements" != "null" && "$requirements" != "" ]]; then
                for requirement in $requirements; do
                    requirement=$(echo $requirement | xargs)
                    found_line=$(grep -E "^$requirement" "$SDKCONFIG_DIR/esp32/sdkconfig")
                    if [[ "$found_line" == "" ]]; then
                        continue 2
                    fi
                done
            fi

            # Check if the sketch requires any configuration options (OR)
            requirements_or=$(jq -r '.requires_any[]? // empty' $sketchdir/ci.json)
            if [[ "$requirements_or" != "null" && "$requirements_or" != "" ]]; then
                found=false
                for requirement in $requirements_or; do
                    requirement=$(echo $requirement | xargs)
                    found_line=$(grep -E "^$requirement" "$SDKCONFIG_DIR/esp32/sdkconfig")
                    if [[ "$found_line" != "" ]]; then
                        found=true
                        break
                    fi
                done
                if [[ "$found" == "false" ]]; then
                    continue
                fi
            fi
        fi

        echo $sketch >> sketches.txt
        sketchnum=$(($sketchnum + 1))
    done
    return $sketchnum
}

function build_pio_sketches(){ # build_pio_sketches <board> <options> <examples-path> <chunk> <total-chunks>
    if [ "$#" -lt 3 ]; then
        echo "ERROR: Illegal number of parameters"
        echo "USAGE: build_pio_sketches <board> <options> <examples-path> [<chunk> <total-chunks>]"
        return 1
    fi

    local board=$1
    local options="$2"
    local examples=$3
    local chunk_idex=$4
    local chunks_num=$5

    if [ "$#" -lt 5 ]; then
        chunk_idex="0"
        chunks_num="1"
    fi

    if [ "$chunks_num" -le 0 ]; then
        echo "ERROR: Chunks count must be positive number"
        return 1
    fi
    if [ "$chunk_idex" -ge "$chunks_num" ]; then
        echo "ERROR: Chunk index must be less than chunks count"
        return 1
    fi

    set +e
    count_sketches "$examples"
    local sketchcount=$?
    set -e
    local sketches=$(cat sketches.txt)
    rm -rf sketches.txt

    local chunk_size=$(( $sketchcount / $chunks_num ))
    local all_chunks=$(( $chunks_num * $chunk_size ))
    if [ "$all_chunks" -lt "$sketchcount" ]; then
        chunk_size=$(( $chunk_size + 1 ))
    fi

    local start_index=$(( $chunk_idex * $chunk_size ))
    if [ "$sketchcount" -le "$start_index" ]; then
        echo "Skipping job"
        return 0
    fi

    local end_index=$(( $(( $chunk_idex + 1 )) * $chunk_size ))
    if [ "$end_index" -gt "$sketchcount" ]; then
        end_index=$sketchcount
    fi

    local start_num=$(( $start_index + 1 ))
    echo "Found $sketchcount Sketches";
    echo "Chunk Count : $chunks_num"
    echo "Chunk Size  : $chunk_size"
    echo "Start Sketch: $start_num"
    echo "End Sketch  : $end_index"

    local sketchnum=0
    for sketch in $sketches; do
        local sketchdir=$(dirname $sketch)
        local sketchdirname=$(basename $sketchdir)
        local sketchname=$(basename $sketch)
        if [[ "$sketchdirname.ino" != "$sketchname" ]]; then
            continue
        elif [ -f $sketchdir/ci.json ]; then
            # If the target is listed as false, skip the sketch. Otherwise, include it.
            is_target=$(jq -r '.targets[esp32]' $sketchdir/ci.json)
            if [[ "$is_target" == "false" ]]; then
                continue
            fi

            # Check if the sketch requires any configuration options (AND)
            requirements=$(jq -r '.requires[]? // empty' $sketchdir/ci.json)
            if [[ "$requirements" != "null" && "$requirements" != "" ]]; then
                for requirement in $requirements; do
                    requirement=$(echo $requirement | xargs)
                    found_line=$(grep -E "^$requirement" "$SDKCONFIG_DIR/esp32/sdkconfig")
                    if [[ "$found_line" == "" ]]; then
                        continue 2
                    fi
                done
            fi

            # Check if the sketch requires any configuration options (OR)
            requirements_or=$(jq -r '.requires_any[]? // empty' $sketchdir/ci.json)
            if [[ "$requirements_or" != "null" && "$requirements_or" != "" ]]; then
                found=false
                for requirement in $requirements_or; do
                    requirement=$(echo $requirement | xargs)
                    found_line=$(grep -E "^$requirement" "$SDKCONFIG_DIR/esp32/sdkconfig")
                    if [[ "$found_line" != "" ]]; then
                        found=true
                        break
                    fi
                done
                if [[ "$found" == "false" ]]; then
                    continue
                fi
            fi
        fi

        sketchnum=$(($sketchnum + 1))
        if [ "$sketchnum" -le "$start_index" ] \
        || [ "$sketchnum" -gt "$end_index" ]; then
            continue
        fi
        build_pio_sketch "$board" "$options" "$sketch"
        local result=$?
        if [ $result -ne 0 ]; then
            return $result
        fi
    done
    return 0
}
