#!/bin/bash

# Source centralized SoC configuration
SCRIPTS_DIR_CONFIG="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPTS_DIR_CONFIG}/socs_config.sh"

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

            # Check if target is supported by QEMU
            if ! is_qemu_supported "$target"; then
                printf "\033[91mUnsupported QEMU target: %s\033[0m\n" "$target"
                exit 1
            fi

            # Get QEMU architecture for target
            qemu_arch=$(get_arch "$target")
            if [ "$qemu_arch" == "xtensa" ]; then
                extra_args+=("--qemu-prog-path" "qemu-system-xtensa" "--qemu-cli-args=\"-machine $target -m 4M -nographic\"")
            elif [ "$qemu_arch" == "riscv32" ]; then
                extra_args+=("--qemu-prog-path" "qemu-system-riscv32" "--qemu-cli-args=\"-machine $target -icount 3 -nographic\"")
            else
                printf "\033[91mUnknown QEMU architecture for target: %s\033[0m\n" "$target"
                exit 1
            fi
        else
            extra_args=("--embedded-services" "esp,arduino")
        fi

        rm "$sketchdir"/diagram.json 2>/dev/null || true

        local wifi_args=""
        if [ -n "$wifi_ssid" ]; then
            wifi_args="--wifi-ssid \"$wifi_ssid\""
        fi
        if [ -n "$wifi_password" ]; then
            wifi_args="$wifi_args --wifi-password \"$wifi_password\""
        fi

        result=0
        printf "\033[95mpytest -s \"%s/test_%s.py\" --build-dir \"%s\" --junit-xml=\"%s\" -o junit_suite_name=%s_%s_%s_%s%s %s %s\033[0m\n" "$sketchdir" "$sketchname" "$build_dir" "$report_file" "$test_type" "$platform" "$target" "$sketchname" "$i" "${extra_args[*]@Q}" "$wifi_args"
        bash -c "set +e; pytest -s \"$sketchdir/test_$sketchname.py\" --build-dir \"$build_dir\" --junit-xml=\"$report_file\" -o junit_suite_name=${test_type}_${platform}_${target}_${sketchname}${i} ${extra_args[*]@Q} $wifi_args; exit \$?" || result=$?
        printf "\n"
        if [ $result -ne 0 ]; then
            result=0
            printf "\033[95mRetrying test: %s -- Config: %s\033[0m\n" "$sketchname" "$i"
            printf "\033[95mpytest -s \"%s/test_%s.py\" --build-dir \"%s\" --junit-xml=\"%s\" -o junit_suite_name=%s_%s_%s_%s%s %s %s\033[0m\n" "$sketchdir" "$sketchname" "$build_dir" "$report_file" "$test_type" "$platform" "$target" "$sketchname" "$i" "${extra_args[*]@Q}" "$wifi_args"
            bash -c "set +e; pytest -s \"$sketchdir/test_$sketchname.py\" --build-dir \"$build_dir\" --junit-xml=\"$report_file\" -o junit_suite_name=${test_type}_${platform}_${target}_${sketchname}${i} ${extra_args[*]@Q} $wifi_args; exit \$?" || result=$?
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

# Parse comma-separated targets
if [ -n "$target" ]; then
    IFS=',' read -ra targets_to_run <<< "$target"
fi

# Parse comma-separated sketches
if [ -n "$sketch" ]; then
    IFS=',' read -ra sketches_to_run <<< "$sketch"
fi

# Handle default target and sketch logic
if [ -z "$target" ] && [ -z "$sketch" ]; then
    # No target or sketch specified - run all sketches for all targets
    echo "No target or sketch specified, running all sketches for all targets"
    chunk_run=1
    # Set defaults for chunk mode when auto-enabled
    if [ -z "$chunk_index" ]; then
        chunk_index=0
    fi
    if [ -z "$chunk_max" ]; then
        chunk_max=1
    fi
    targets_to_run=("${BUILD_TEST_TARGETS[@]}")
    sketches_to_run=()
elif [ -z "$target" ]; then
    # No target specified, but sketch(es) specified - run sketches for all targets
    echo "No target specified, running sketch(es) '${sketches_to_run[*]}' for all targets"
    targets_to_run=("${BUILD_TEST_TARGETS[@]}")
elif [ -z "$sketch" ]; then
    # No sketch specified, but target(s) specified - run all sketches for targets
    echo "No sketch specified, running all sketches for target(s) '${targets_to_run[*]}'"
    chunk_run=1
    # Set defaults for chunk mode when auto-enabled
    if [ -z "$chunk_index" ]; then
        chunk_index=0
    fi
    if [ -z "$chunk_max" ]; then
        chunk_max=1
    fi
    sketches_to_run=()
fi

if [ ! $platform == "qemu" ]; then
    source "${SCRIPTS_DIR}/install-arduino-ide.sh"
fi

# If sketch is provided and test type is not, test type is inferred from the sketch path
if [[ $test_type == "all" ]] || [[ -z $test_type ]]; then
    if [ ${#sketches_to_run[@]} -eq 1 ]; then
        tmp_sketch_path=$(find tests -name "${sketches_to_run[0]}".ino)
        test_type=$(basename "$(dirname "$(dirname "$tmp_sketch_path")")")
        echo "Sketch ${sketches_to_run[0]} test type: $test_type"
        test_folder="$PWD/tests/$test_type"
    else
        test_folder="$PWD/tests"
    fi
else
    test_folder="$PWD/tests/$test_type"
fi

# Loop through all targets to run
error_code=0
for current_target in "${targets_to_run[@]}"; do
    echo "Running for target: $current_target"

    if [ $chunk_run -eq 0 ]; then
        # Run specific sketches
        for current_sketch in "${sketches_to_run[@]}"; do
            echo "  Running sketch: $current_sketch"

            # Find test folder for this sketch if needed
            if [[ $test_type == "all" ]] || [[ -z $test_type ]]; then
                tmp_sketch_path=$(find tests -name "$current_sketch".ino)
                if [ -z "$tmp_sketch_path" ]; then
                    echo "ERROR: Sketch $current_sketch not found"
                    continue
                fi
                sketch_test_type=$(basename "$(dirname "$(dirname "$tmp_sketch_path")")")
                sketch_test_folder="$PWD/tests/$sketch_test_type"
            else
                sketch_test_folder="$test_folder"
            fi

            run_test "$current_target" "$sketch_test_folder"/"$current_sketch"/"$current_sketch".ino $options $erase
            current_exit_code=$?
            if [ $current_exit_code -ne 0 ]; then
                error_code=$current_exit_code
            fi
        done
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
        ${COUNT_SKETCHES} "$test_folder" "$current_target" "1"
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
                continue
            fi

            end_index=$(( $(( chunk_index + 1 )) * chunk_size ))
            if [ "$end_index" -gt "$sketchcount" ]; then
                end_index=$sketchcount
            fi
        fi

        sketchnum=0
        target_error=0

        for sketch in $sketches; do

            sketchnum=$((sketchnum + 1))
            if [ "$sketchnum" -le "$start_index" ] \
            || [ "$sketchnum" -gt "$end_index" ]; then
                continue
            fi

            printf "\033[95mSketch Index %s\033[0m\n" "$((sketchnum - 1))"

            exit_code=0
            run_test "$current_target" "$sketch" $options $erase || exit_code=$?
            if [ $exit_code -ne 0 ]; then
                target_error=$exit_code
            fi
        done
        if [ $target_error -ne 0 ]; then
            error_code=$target_error
        fi
    fi
done
exit $error_code
