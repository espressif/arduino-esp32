#!/bin/bash

# Check if a test is a multi-device test
function is_multi_device_test {
    local test_dir=$1
    local has_multi_device
    if [ -f "$test_dir/ci.yml" ]; then
        has_multi_device=$(yq eval '.multi_device' "$test_dir/ci.yml" 2>/dev/null)
        if [[ "$has_multi_device" != "null" && "$has_multi_device" != "" ]]; then
            echo "1"
            return
        fi
    fi
    echo "0"
}

function run_multi_device_test {
    local target=$1
    local test_dir=$2
    local options=$3
    local erase_flash=$4
    local test_name
    local test_type
    local result=0
    local error=0
    local devices

    test_name=$(basename "$test_dir")
    test_type=$(basename "$(dirname "$test_dir")")

    printf "\033[95mRunning multi-device test: %s\033[0m\n" "$test_name"

    # Get the list of devices from ci.yml
    devices=$(yq eval '.multi_device | keys | .[]' "$test_dir/ci.yml" 2>/dev/null | sort)

    if [[ -z "$devices" ]]; then
        echo "ERROR: No devices found in multi_device configuration for $test_name"
        return 1
    fi

    # Build the build-dir argument for pytest-embedded
    local build_dirs=""
    local device_count=0
    local sketch_name
    local build_dir
    local sdkconfig_path
    local compiled_target
    for device in $devices; do
        sketch_name=$(yq eval ".multi_device.$device" "$test_dir/ci.yml" 2>/dev/null)
        build_dir="$HOME/.arduino/tests/$target/${test_name}_${sketch_name}/build.tmp"

        if [ ! -d "$build_dir" ]; then
            printf "\033[93mSkipping multi-device test %s: build not found for device %s in %s\033[0m\n" "$test_name" "$device" "$build_dir"
            return 0
        fi

        # Check if the build is for the correct target
        sdkconfig_path="$build_dir/sdkconfig"
        if [ -f "$sdkconfig_path" ]; then
            compiled_target=$(grep -E "CONFIG_IDF_TARGET=" "$sdkconfig_path" | cut -d'"' -f2)
            if [ "$compiled_target" != "$target" ]; then
                printf "\033[91mError: Device %s compiled for %s, expected %s\033[0m\n" "$device" "$compiled_target" "$target"
                return 1
            fi
        fi

        if [ $device_count -gt 0 ]; then
            build_dirs="$build_dirs|$build_dir"
        else
            build_dirs="$build_dir"
        fi
        device_count=$((device_count + 1))
    done

    if [ $device_count -lt 2 ]; then
        echo "ERROR: Multi-device test $test_name requires at least 2 devices, found $device_count"
        return 1
    fi

    # Check platform support
    if [ -f "$test_dir/ci.yml" ]; then
        is_target=$(yq eval ".targets.${target}" "$test_dir/ci.yml" 2>/dev/null)
        selected_platform=$(yq eval ".platforms.${platform}" "$test_dir/ci.yml" 2>/dev/null)

        if [[ $is_target == "false" ]] || [[ $selected_platform == "false" ]]; then
            printf "\033[93mSkipping %s test for %s, platform: %s\033[0m\n" "$test_name" "$target" "$platform"
            printf "\n\n\n"
            return 0
        fi
    fi

    # Build embedded-services argument
    if [ $platform == "wokwi" ]; then
        echo "ERROR: Wokwi platform not supported for multi-device tests"
        return 1
    elif [ $platform == "qemu" ]; then
        echo "ERROR: QEMU platform not supported for multi-device tests"
        return 1
    else
        # For hardware platform, use esp,arduino for each device
        local services="esp,arduino"
        for ((i=1; i<device_count; i++)); do
            services="$services|esp,arduino"
        done
        extra_args=("--embedded-services" "$services")
    fi

    # Verify ports are set for multi-device tests
    if [ $device_count -eq 2 ]; then
        if [ -z "${ESPPORT1}" ] || [ -z "${ESPPORT2}" ]; then
            echo "ERROR: ESPPORT1 and ESPPORT2 must be set for 2-device tests"
            return 1
        fi
    else
        echo "ERROR: Only 2-device tests are currently supported"
        return 1
    fi

    local wifi_args=""
    if [ -n "$wifi_ssid" ]; then
        wifi_args="--wifi-ssid \"$wifi_ssid\""
    fi
    if [ -n "$wifi_password" ]; then
        wifi_args="$wifi_args --wifi-password \"$wifi_password\""
    fi

    local report_file="$test_dir/$target/$test_name.xml"

    if [ "$erase_flash" -eq 1 ]; then
        esptool -c "$target" erase-flash
    fi

    result=0

    # Build pytest command as an array to properly handle arguments
    local pytest_cmd=(
        pytest -s "$test_dir/test_$test_name.py"
        --count "$device_count"
        --build-dir "$build_dirs"
        --port "${ESPPORT1}|${ESPPORT2}"
        --target "$target"
        --junit-xml="$report_file"
        -o "junit_suite_name=${test_type}_${platform}_${target}_${test_name}"
        "${extra_args[@]}"
    )

    if [ -n "$wifi_ssid" ]; then
        pytest_cmd+=(--wifi-ssid "$wifi_ssid")
    fi
    if [ -n "$wifi_password" ]; then
        pytest_cmd+=(--wifi-password "$wifi_password")
    fi

    printf "\033[95m%s\033[0m\n" "${pytest_cmd[*]}"

    set +e
    "${pytest_cmd[@]}"
    result=$?
    set -e
    printf "\n"

    if [ $result -ne 0 ]; then
        result=0
        printf "\033[95mRetrying test: %s\033[0m\n" "$test_name"
        printf "\033[95m%s\033[0m\n" "${pytest_cmd[*]}"

        set +e
        "${pytest_cmd[@]}"
        result=$?
        set -e
        printf "\n"

        if [ $result -ne 0 ]; then
            printf "\033[91mFailed test: %s\033[0m\n\n" "$test_name"
            error=$result
        fi
    fi

    return $error
}

function run_test {
    local target=$1
    local sketch=$2
    local options=$3
    local erase_flash=$4
    local sketchdir
    local sketchname
    local result=0
    local error=0
    local sdkconfig_path
    local extra_args
    local test_type

    sketchdir=$(dirname "$sketch")
    sketchname=$(basename "$sketchdir")
    test_type=$(basename "$(dirname "$sketchdir")")

    if [ "$options" -eq 0 ] && [ -f "$sketchdir"/ci.yml ]; then
        len=$(yq eval ".fqbn.${target} | length" "$sketchdir"/ci.yml 2>/dev/null || echo 0)
        if [ "$len" -eq 0 ]; then
            len=1
        fi
    else
        len=1
    fi

    if [ "$len" -eq 1 ]; then
        sdkconfig_path="$HOME/.arduino/tests/$target/$sketchname/build.tmp/sdkconfig"
    else
        sdkconfig_path="$HOME/.arduino/tests/$target/$sketchname/build0.tmp/sdkconfig"
    fi

    if [ -f "$sketchdir"/ci.yml ]; then
        # If the target or platform is listed as false, skip the sketch. Otherwise, include it.
        is_target=$(yq eval ".targets.${target}" "$sketchdir"/ci.yml 2>/dev/null)
        selected_platform=$(yq eval ".platforms.${platform}" "$sketchdir"/ci.yml 2>/dev/null)

        if [[ $is_target == "false" ]] || [[ $selected_platform == "false" ]]; then
            printf "\033[93mSkipping %s test for %s, platform: %s\033[0m\n" "$sketchname" "$target" "$platform"
            printf "\n\n\n"
            return 0
        fi
    fi

    if [ ! -f "$sdkconfig_path" ]; then
        printf "\033[93mSketch %s build not found in %s\nMight be due to missing target requirements or build failure\033[0m\n" "$(dirname "$sdkconfig_path")" "$sketchname"
        printf "\n\n\n"
        return 0
    fi

    local compiled_target
    compiled_target=$(grep -E "CONFIG_IDF_TARGET=" "$sdkconfig_path" | cut -d'"' -f2)
    if [ "$compiled_target" != "$target" ]; then
        printf "\033[91mError: Sketch %s compiled for %s, expected %s\033[0m\n" "$sketchname" "$compiled_target" "$target"
        printf "\n\n\n"
        return 1
    fi

    if [ "$len" -eq 1 ]; then
        # build_dir="$sketchdir/build"
        build_dir="$HOME/.arduino/tests/$target/$sketchname/build.tmp"
        report_file="$sketchdir/$target/$sketchname.xml"
    fi

    for i in $(seq 0 $((len - 1))); do
        fqbn="Default"

        if [ "$len" -ne 1 ]; then
            fqbn=$(yq eval ".fqbn.${target} | sort | .[${i}]" "$sketchdir"/ci.yml 2>/dev/null)
        elif [ -f "$sketchdir"/ci.yml ]; then
            has_fqbn=$(yq eval ".fqbn.${target}" "$sketchdir"/ci.yml 2>/dev/null)
            if [ "$has_fqbn" != "null" ]; then
                fqbn=$(yq eval ".fqbn.${target} | .[0]" "$sketchdir"/ci.yml 2>/dev/null)
            fi
        fi

        printf "\033[95mRunning test: %s -- Config: %s\033[0m\n" "$sketchname" "$fqbn"
        if [ "$erase_flash" -eq 1 ]; then
            esptool -c "$target" erase-flash
        fi

        if [ "$len" -ne 1 ]; then
            # build_dir="$sketchdir/build$i"
            build_dir="$HOME/.arduino/tests/$target/$sketchname/build$i.tmp"
            report_file="$sketchdir/$target/$sketchname$i.xml"
        fi

        if [ $platform == "wokwi" ]; then
            extra_args=("--target" "$target" "--embedded-services" "arduino,wokwi")
            if [[ -f "$sketchdir/diagram.$target.json" ]]; then
                extra_args+=("--wokwi-diagram" "$sketchdir/diagram.$target.json")
            fi
        elif [ $platform == "qemu" ]; then
            PATH=$HOME/qemu/bin:$PATH
            extra_args=("--embedded-services" "qemu" "--qemu-image-path" "$build_dir/$sketchname.ino.merged.bin")

            if [ "$target" == "esp32" ] || [ "$target" == "esp32s3" ]; then
                extra_args+=("--qemu-prog-path" "qemu-system-xtensa" "--qemu-cli-args=\"-machine $target -m 4M -nographic\"")
            elif [ "$target" == "esp32c3" ]; then
                extra_args+=("--qemu-prog-path" "qemu-system-riscv32" "--qemu-cli-args=\"-machine $target -icount 3 -nographic\"")
            else
                printf "\033[91mUnsupported QEMU target: %s\033[0m\n" "$target"
                exit 1
            fi
        else
            extra_args=("--embedded-services" "esp,arduino")
        fi

        rm "$sketchdir"/diagram.json 2>/dev/null || true

        # Build pytest command as an array to properly handle arguments
        local pytest_cmd=(
            pytest -s "$sketchdir/test_$sketchname.py"
            --build-dir "$build_dir"
            --junit-xml="$report_file"
            -o "junit_suite_name=${test_type}_${platform}_${target}_${sketchname}${i}"
            "${extra_args[@]}"
        )

        if [ -n "$wifi_ssid" ]; then
            pytest_cmd+=(--wifi-ssid "$wifi_ssid")
        fi
        if [ -n "$wifi_password" ]; then
            pytest_cmd+=(--wifi-password "$wifi_password")
        fi

        result=0
        printf "\033[95m%s\033[0m\n" "${pytest_cmd[*]}"
        set +e
        "${pytest_cmd[@]}"
        result=$?
        set -e
        printf "\n"
        if [ $result -ne 0 ]; then
            result=0
            printf "\033[95mRetrying test: %s -- Config: %s\033[0m\n" "$sketchname" "$i"
            printf "\033[95m%s\033[0m\n" "${pytest_cmd[*]}"
            set +e
            "${pytest_cmd[@]}"
            result=$?
            set -e
            printf "\n"
            if [ $result -ne 0 ]; then
                printf "\033[91mFailed test: %s -- Config: %s\033[0m\n\n" "$sketchname" "$i"
                error=$result
            fi
        fi
    done
    return $error
}

SCRIPTS_DIR="./.github/scripts"
COUNT_SKETCHES="${SCRIPTS_DIR}/sketch_utils.sh count"

platform="hardware"
chunk_run=0
options=0
erase=0
wifi_ssid=""
wifi_password=""

# Check for supplied port. Single dut tests will use ESPPORT.
if [ -n "${ESPPORT}" ]; then
  echo "Supplied port [ESPPORT]: ${ESPPORT}"
elif [ -n "${ESPPORT1}" ]; then
  echo "Supplied port [ESPPORT1]: ${ESPPORT1}"
  if [ -n "${ESPPORT2}" ]; then
    echo "Supplied port [ESPPORT2]: ${ESPPORT2}"
  fi
else
  echo "No port supplied. Using ESPPORT=/dev/ttyUSB0, ESPPORT1=/dev/ttyUSB0, ESPPORT2=/dev/ttyUSB1"
  ESPPORT="/dev/ttyUSB0"
  ESPPORT1="/dev/ttyUSB0"
  ESPPORT2="/dev/ttyUSB1"
fi

while [ -n "$1" ]; do
    case $1 in
    -c )
        chunk_run=1
        ;;
    -Q )
        if [ ! -d "$QEMU_PATH" ]; then
            echo "QEMU path $QEMU_PATH does not exist"
            exit 1
        fi
        platform="qemu"
        ;;
    -W )
        if [[ -z $WOKWI_CLI_TOKEN ]]; then
            echo "Wokwi CLI token is not set"
            exit 1
        fi
        platform="wokwi"
        ;;
    -o )
        options=1
        ;;
    -s )
        shift
        sketch=$1
        ;;
    -t )
        shift
        target=$1
        ;;
    -i )
        shift
        chunk_index=$1
        ;;
    -m )
        shift
        chunk_max=$1
        ;;
    -e )
        erase=1
        ;;
    -h )
        echo "$USAGE"
        exit 0
        ;;
    -type )
        shift
        test_type=$1
        ;;
    -wifi-ssid )
        shift
        wifi_ssid=$1
        ;;
    -wifi-password )
        shift
        wifi_password=$1
        ;;
    * )
        break
        ;;
    esac
    shift
done

if [ ! $platform == "qemu" ]; then
    source "${SCRIPTS_DIR}/install-arduino-ide.sh"
fi

source "${SCRIPTS_DIR}/tests_utils.sh"

# If sketch is provided and test type is not, test type is inferred from the sketch path
if [[ $test_type == "all" ]] || [[ -z $test_type ]]; then
    if [ -n "$sketch" ]; then
        detect_test_type_and_folder "$sketch"
    else
        test_folder="$PWD/tests"
    fi
else
    test_folder="$PWD/tests/$test_type"
fi

if [ $chunk_run -eq 0 ]; then
    if [ -z "$sketch" ]; then
        echo "ERROR: Sketch name is required for single test run"
        exit 1
    fi

    # Check if this is a multi-device test
    test_dir="$test_folder/$sketch"
    if [ -d "$test_dir" ] && [ "$(is_multi_device_test "$test_dir")" -eq 1 ]; then
        run_multi_device_test "$target" "$test_dir" $options $erase
        exit $?
    else
        run_test "$target" "$test_folder"/"$sketch"/"$sketch".ino $options $erase
        exit $?
    fi
else
    if [ "$chunk_max" -le 0 ]; then
        echo "ERROR: Chunks count must be positive number"
        exit 1
    fi

    if [ "$chunk_index" -ge "$chunk_max" ] && [ "$chunk_max" -ge 2 ]; then
        echo "ERROR: Chunk index must be less than chunks count"
        exit 1
    fi

    set +e
    # Ignore requirements as we don't have the libs. The requirements will be checked in the run_test function
    ${COUNT_SKETCHES} "$test_folder" "$target" "1"
    sketchcount=$?
    set -e
    sketches=$(cat sketches.txt)
    rm -rf sketches.txt

    chunk_size=$(( sketchcount / chunk_max ))
    all_chunks=$(( chunk_max * chunk_size ))
    if [ "$all_chunks" -lt "$sketchcount" ]; then
        chunk_size=$(( chunk_size + 1 ))
    fi

    start_index=0
    end_index=0
    if [ "$chunk_index" -ge "$chunk_max" ]; then
        start_index=$chunk_index
        end_index=$sketchcount
    else
        start_index=$(( chunk_index * chunk_size ))
        if [ "$sketchcount" -le "$start_index" ]; then
            exit 0
        fi

        end_index=$(( $(( chunk_index + 1 )) * chunk_size ))
        if [ "$end_index" -gt "$sketchcount" ]; then
            end_index=$sketchcount
        fi
    fi

    sketchnum=0
    error=0

    # First, run all multi-device tests in the test folder
    if [ -d "$test_folder" ]; then
        for test_dir in "$test_folder"/*; do
            if [ -d "$test_dir" ] && [ "$(is_multi_device_test "$test_dir")" -eq 1 ]; then
                exit_code=0
                run_multi_device_test "$target" "$test_dir" $options $erase || exit_code=$?
                if [ $exit_code -ne 0 ]; then
                    error=$exit_code
                fi
            fi
        done
    fi

    # Then run regular tests
    for sketch in $sketches; do

        sketchnum=$((sketchnum + 1))
        if [ "$sketchnum" -le "$start_index" ] \
        || [ "$sketchnum" -gt "$end_index" ]; then
            continue
        fi

        printf "\033[95mSketch Index %s\033[0m\n" "$((sketchnum - 1))"

        exit_code=0
        run_test "$target" "$sketch" $options $erase || exit_code=$?
        if [ $exit_code -ne 0 ]; then
            error=$exit_code
        fi
    done
    exit $error
fi
