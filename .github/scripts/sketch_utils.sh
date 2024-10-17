#!/bin/bash

if [ -d "$ARDUINO_ESP32_PATH/tools/esp32-arduino-libs" ]; then
    SDKCONFIG_DIR="$ARDUINO_ESP32_PATH/tools/esp32-arduino-libs"
elif [ -d "$GITHUB_WORKSPACE/tools/esp32-arduino-libs" ]; then
    SDKCONFIG_DIR="$GITHUB_WORKSPACE/tools/esp32-arduino-libs"
else
    SDKCONFIG_DIR="tools/esp32-arduino-libs"
fi

function build_sketch(){ # build_sketch <ide_path> <user_path> <path-to-ino> [extra-options]
    while [ ! -z "$1" ]; do
        case "$1" in
        -ai )
            shift
            ide_path=$1
            ;;
        -au )
            shift
            user_path=$1
            ;;
        -t )
            shift
            target=$1
            ;;
        -fqbn )
            shift
            fqbn=$1
            ;;
        -o )
            shift
            options=$1
            ;;
        -s )
            shift
            sketchdir=$1
            ;;
        -i )
            shift
            chunk_index=$1
            ;;
        -l )
            shift
            log_compilation=$1
            ;;
        * )
            break
            ;;
        esac
        shift
    done

    xtra_opts=$*
    len=0

    if [ -z $sketchdir ]; then
        echo "ERROR: Sketch directory not provided"
        echo "$USAGE"
        exit 1
    fi

    # No FQBN was passed, try to get it from other options

    if [ -z $fqbn ]; then
        if [ -z $target ]; then
            echo "ERROR: Unspecified chip"
            echo "$USAGE"
            exit 1
        fi

        # The options are either stored in the test directory, for a per test
        # customization or passed as parameters.  Command line options take
        # precedence.  Note that the following logic also falls to the default
        # parameters if no arguments were passed and no file was found.

        if [ -z $options ] && [ -f $sketchdir/ci.json ]; then
            # The config file could contain multiple FQBNs for one chip.  If
            # that's the case we build one time for every FQBN.

            len=`jq -r --arg target $target '.fqbn[$target] | length' $sketchdir/ci.json`
            if [ $len -gt 0 ]; then
                fqbn=`jq -r --arg target $target '.fqbn[$target] | sort' $sketchdir/ci.json`
            fi
        fi

        if [ ! -z $options ] || [ $len -eq 0 ]; then
            # Since we are passing options, we will end up with only one FQBN to
            # build.

            len=1

            if [ -f $sketchdir/ci.json ]; then
                fqbn_append=`jq -r '.fqbn_append' $sketchdir/ci.json`
                if [ $fqbn_append == "null" ]; then
                    fqbn_append=""
                fi
            fi

            # Default FQBN options if none were passed in the command line.

            esp32_opts="PSRAM=enabled${fqbn_append:+,$fqbn_append}"
            esp32s2_opts="PSRAM=enabled${fqbn_append:+,$fqbn_append}"
            esp32s3_opts="PSRAM=opi,USBMode=default${fqbn_append:+,$fqbn_append}"
            esp32c3_opts="$fqbn_append"
            esp32c6_opts="$fqbn_append"
            esp32h2_opts="$fqbn_append"

            # Select the common part of the FQBN based on the target.  The rest will be
            # appended depending on the passed options.

            opt=""

            case "$target" in
                "esp32")
                    [ -n "${options:-$esp32_opts}" ] && opt=":${options:-$esp32_opts}"
                    fqbn="espressif:esp32:esp32$opt"
                ;;
                "esp32s2")
                    [ -n "${options:-$esp32s2_opts}" ] && opt=":${options:-$esp32s2_opts}"
                    fqbn="espressif:esp32:esp32s2$opt"
                ;;
                "esp32c3")
                    [ -n "${options:-$esp32c3_opts}" ] && opt=":${options:-$esp32c3_opts}"
                    fqbn="espressif:esp32:esp32c3$opt"
                ;;
                "esp32s3")
                    [ -n "${options:-$esp32s3_opts}" ] && opt=":${options:-$esp32s3_opts}"
                    fqbn="espressif:esp32:esp32s3$opt"
                ;;
                "esp32c6")
                    [ -n "${options:-$esp32c6_opts}" ] && opt=":${options:-$esp32c6_opts}"
                    fqbn="espressif:esp32:esp32c6$opt"
                ;;
                "esp32h2")
                    [ -n "${options:-$esp32h2_opts}" ] && opt=":${options:-$esp32h2_opts}"
                    fqbn="espressif:esp32:esp32h2$opt"
                ;;
            esac

            # Make it look like a JSON array.

            fqbn="[\"$fqbn\"]"
        fi
    else
        # An FQBN was passed.  Make it look like a JSON array.

        len=1
        fqbn="[\"$fqbn\"]"
    fi

    if [ -z "$fqbn" ]; then
        echo "No FQBN passed or invalid chip: $target"
        exit 1
    fi

    # The directory that will hold all the artifcats (the build directory) is
    # provided through:
    #  1. An env variable called ARDUINO_BUILD_DIR.
    #  2. Created at the sketch level as "build" in the case of a single
    #     configuration test.
    #  3. Created at the sketch level as "buildX" where X is the number
    #     of configuration built in case of a multiconfiguration test.

    sketchname=$(basename $sketchdir)

    if [ -f $sketchdir/ci.json ]; then
        # If the target is listed as false, skip the sketch. Otherwise, include it.
        is_target=$(jq -r --arg target $target '.targets[$target]' $sketchdir/ci.json)
        if [[ "$is_target" == "false" ]]; then
            echo "Skipping $sketchname for target $target"
            exit 0
        fi

        # Check if the sketch requires any configuration options (AND)
        requirements=$(jq -r '.requires[]? // empty' $sketchdir/ci.json)
        if [[ "$requirements" != "null" && "$requirements" != "" ]]; then
            for requirement in $requirements; do
                requirement=$(echo $requirement | xargs)
                found_line=$(grep -E "^$requirement" "$SDKCONFIG_DIR/$target/sdkconfig")
                if [[ "$found_line" == "" ]]; then
                    echo "Target $target does not meet the requirement $requirement for $sketchname. Skipping."
                    exit 0
                fi
            done
        fi

        # Check if the sketch excludes any configuration options (OR)
        requirements_or=$(jq -r '.requires_any[]? // empty' $sketchdir/ci.json)
        if [[ "$requirements_or" != "null" && "$requirements_or" != "" ]]; then
            found=false
            for requirement in $requirements_or; do
                requirement=$(echo $requirement | xargs)
                found_line=$(grep -E "^$requirement" "$SDKCONFIG_DIR/$target/sdkconfig")
                if [[ "$found_line" != "" ]]; then
                    found=true
                    break
                fi
            done
            if [[ "$found" == "false" ]]; then
                echo "Target $target meets none of the requirements in requires_any for $sketchname. Skipping."
                exit 0
            fi
        fi
    fi

    ARDUINO_CACHE_DIR="$HOME/.arduino/cache.tmp"
    if [ -n "$ARDUINO_BUILD_DIR" ]; then
        build_dir="$ARDUINO_BUILD_DIR"
    elif [ $len -eq 1 ]; then
        # build_dir="$sketchdir/build"
        build_dir="$HOME/.arduino/tests/$sketchname/build.tmp"
    fi

    output_file="$HOME/.arduino/cli_compile_output.txt"
    sizes_file="$GITHUB_WORKSPACE/cli_compile_$chunk_index.json"

    mkdir -p "$ARDUINO_CACHE_DIR"
    for i in `seq 0 $(($len - 1))`
    do
        if [ $len -ne 1 ]; then
          # build_dir="$sketchdir/build$i"
          build_dir="$HOME/.arduino/tests/$sketchname/build$i.tmp"
        fi
        rm -rf $build_dir
        mkdir -p $build_dir

        currfqbn=`echo $fqbn | jq -r --argjson i $i '.[$i]'`

        if [ -f "$ide_path/arduino-cli" ]; then
            echo "Building $sketchname with arduino-cli and FQBN=$currfqbn"

            curroptions=`echo "$currfqbn" | cut -d':' -f4`
            currfqbn=`echo "$currfqbn" | cut -d':' -f1-3`
            $ide_path/arduino-cli compile \
                --fqbn "$currfqbn" \
                --board-options "$curroptions" \
                --warnings "all" \
                --build-property "compiler.warning_flags.all=-Wall -Werror=all -Wextra" \
                --build-cache-path "$ARDUINO_CACHE_DIR" \
                --build-path "$build_dir" \
                $xtra_opts "${sketchdir}" \
                2>&1 | tee $output_file

            exit_status=${PIPESTATUS[0]}
            if [ $exit_status -ne 0 ]; then
                echo "ERROR: Compilation failed with error code $exit_status"
                exit $exit_status
            fi

            if [ $log_compilation ]; then
                #Extract the program storage space and dynamic memory usage in bytes and percentage in separate variables from the output, just the value without the string
                flash_bytes=$(grep -oE 'Sketch uses ([0-9]+) bytes' $output_file | awk '{print $3}')
                flash_percentage=$(grep -oE 'Sketch uses ([0-9]+) bytes \(([0-9]+)%\)' $output_file | awk '{print $5}' | tr -d '(%)')
                ram_bytes=$(grep -oE 'Global variables use ([0-9]+) bytes' $output_file | awk '{print $4}')
                ram_percentage=$(grep -oE 'Global variables use ([0-9]+) bytes \(([0-9]+)%\)' $output_file | awk '{print $6}' | tr -d '(%)')

                # Extract the directory path excluding the filename
                directory_path=$(dirname "$sketch")
                # Define the constant part
                constant_part="/home/runner/Arduino/hardware/espressif/esp32/libraries/"
                # Extract the desired substring using sed
                lib_sketch_name=$(echo "$directory_path" | sed "s|$constant_part||")
                #append json file where key is fqbn, sketch name, sizes -> extracted values
                echo "{\"name\": \"$lib_sketch_name\",
                    \"sizes\": [{
                            \"flash_bytes\": $flash_bytes,
                            \"flash_percentage\": $flash_percentage,
                            \"ram_bytes\": $ram_bytes,
                            \"ram_percentage\": $ram_percentage
                            }]
                    }," >> "$sizes_file"
            fi

        elif [ -f "$ide_path/arduino-builder" ]; then
            echo "Building $sketchname with arduino-builder and FQBN=$currfqbn"
            echo "Build path = $build_dir"

            $ide_path/arduino-builder -compile -logger=human -core-api-version=10810 \
                -fqbn=\"$currfqbn\" \
                -warnings="all" \
                -tools "$ide_path/tools-builder" \
                -hardware "$user_path/hardware" \
                -libraries "$user_path/libraries" \
                -build-cache "$ARDUINO_CACHE_DIR" \
                -build-path "$build_dir" \
                $xtra_opts "${sketchdir}/${sketchname}.ino"

            exit_status=$?
            if [ $exit_status -ne 0 ]; then
                echo "ERROR: Compilation failed with error code $exit_status"
                exit $exit_status
            fi
            # $ide_path/arduino-builder -compile -logger=human -core-api-version=10810 \
            #     -fqbn=\"$currfqbn\" \
            #     -warnings="all" \
            #     -tools "$ide_path/tools-builder" \
            #     -tools "$ide_path/tools" \
            #     -built-in-libraries "$ide_path/libraries" \
            #     -hardware "$ide_path/hardware" \
            #     -hardware "$user_path/hardware" \
            #     -libraries "$user_path/libraries" \
            #     -build-cache "$ARDUINO_CACHE_DIR" \
            #     -build-path "$build_dir" \
            #     $xtra_opts "${sketchdir}/${sketchname}.ino"
        fi
    done

    unset fqbn
    unset xtra_opts
    unset options
}

function count_sketches(){ # count_sketches <path> [target] [file] [ignore-requirements]
    local path=$1
    local target=$2
    local ignore_requirements=$3
    local file=$4

    if [ $# -lt 1 ]; then
      echo "ERROR: Illegal number of parameters"
      echo "USAGE: ${0} count <path> [target]"
    fi

    rm -rf sketches.txt
    touch sketches.txt
    if [ ! -d "$path" ]; then
        return 0
    fi

    if [ -f "$file" ]; then
        local sketches=$(cat $file)
    else
        local sketches=$(find $path -name *.ino | sort)
    fi

    local sketchnum=0
    for sketch in $sketches; do
        local sketchdir=$(dirname $sketch)
        local sketchdirname=$(basename $sketchdir)
        local sketchname=$(basename $sketch)
        if [[ "$sketchdirname.ino" != "$sketchname" ]]; then
            continue
        elif [[ -n $target ]] && [[ -f $sketchdir/ci.json ]]; then
            # If the target is listed as false, skip the sketch. Otherwise, include it.
            is_target=$(jq -r --arg target $target '.targets[$target]' $sketchdir/ci.json)
            if [[ "$is_target" == "false" ]]; then
                continue
            fi

            if [ "$ignore_requirements" != "1" ]; then
                # Check if the sketch requires any configuration options (AND)
                requirements=$(jq -r '.requires[]? // empty' $sketchdir/ci.json)
                if [[ "$requirements" != "null" && "$requirements" != "" ]]; then
                    for requirement in $requirements; do
                        requirement=$(echo $requirement | xargs)
                        found_line=$(grep -E "^$requirement" $SDKCONFIG_DIR/$target/sdkconfig)
                        if [[ "$found_line" == "" ]]; then
                            continue 2
                        fi
                    done
                fi

                # Check if the sketch excludes any configuration options (OR)
                requirements_or=$(jq -r '.requires_any[]? // empty' $sketchdir/ci.json)
                if [[ "$requirements_or" != "null" && "$requirements_or" != "" ]]; then
                    found=false
                    for requirement in $requirements_or; do
                        requirement=$(echo $requirement | xargs)
                        found_line=$(grep -E "^$requirement" $SDKCONFIG_DIR/$target/sdkconfig)
                        if [[ "$found_line" != "" ]]; then
                            found=true
                            break
                        fi
                    done
                    if [[ "$found" == "false" ]]; then
                        continue 2
                    fi
                fi
            fi
        fi
        echo $sketch >> sketches.txt
        sketchnum=$(($sketchnum + 1))
    done
    return $sketchnum
}

function build_sketches(){ # build_sketches <ide_path> <user_path> <target> <path> <chunk> <total-chunks> [extra-options]

    local args=""
    while [ ! -z "$1" ]; do
        case $1 in
        -ai )
            shift
            ide_path=$1
            ;;
        -au )
            shift
            user_path=$1
            ;;
        -t )
            shift
            target=$1
            args+=" -t $target"
            ;;
        -fqbn )
            shift
            fqbn=$1
            args+=" -fqbn $fqbn"
            ;;
        -p )
            shift
            path=$1
            ;;
        -i )
            shift
            chunk_index=$1
            ;;
        -m )
            shift
            chunk_max=$1
            ;;
        -l )
            shift
            log_compilation=$1
            ;;
        -f )
            shift
            sketches_file=$1
            ;;
        * )
            break
            ;;
        esac
        shift
    done

    local xtra_opts=$*

    if [ -z "$chunk_index" ] || [ -z "$chunk_max" ]; then
        echo "ERROR: Invalid chunk paramters"
        echo "$USAGE"
        exit 1
    fi

    if [ "$chunk_max" -le 0 ]; then
        echo "ERROR: Chunks count must be positive number"
        return 1
    fi

    if [ "$chunk_index" -gt "$chunk_max" ] && [ "$chunk_max" -ge 2 ]; then
        chunk_index=$chunk_max
    fi

    set +e
    if [ -n "$sketches_file" ]; then
        count_sketches "$path" "$target" "0" "$sketches_file"
        local sketchcount=$?
    else
        count_sketches "$path" "$target"
        local sketchcount=$?
    fi
    set -e
    local sketches=$(cat sketches.txt)
    rm -rf sketches.txt

    local chunk_size=$(( $sketchcount / $chunk_max ))
    local all_chunks=$(( $chunk_max * $chunk_size ))
    if [ "$all_chunks" -lt "$sketchcount" ]; then
        chunk_size=$(( $chunk_size + 1 ))
    fi

    local start_index=0
    local end_index=0
    if [ "$chunk_index" -ge "$chunk_max" ]; then
        start_index=$chunk_index
        end_index=$sketchcount
    else
        start_index=$(( $chunk_index * $chunk_size ))
        if [ "$sketchcount" -le "$start_index" ]; then
            echo "No sketches to build for $target in this chunk"
            return 0
        fi

        end_index=$(( $(( $chunk_index + 1 )) * $chunk_size ))
        if [ "$end_index" -gt "$sketchcount" ]; then
            end_index=$sketchcount
        fi
    fi

    local start_num=$(( $start_index + 1 ))
    echo "Found $sketchcount Sketches for target '$target'";
    echo "Chunk Index : $chunk_index"
    echo "Chunk Count : $chunk_max"
    echo "Chunk Size  : $chunk_size"
    echo "Start Sketch: $start_num"
    echo "End Sketch  : $end_index"

    #if fqbn is not passed then set it to default for compilation log
    if [ -z $fqbn ]; then
        log_fqbn="espressif:esp32:$target"
    else
        log_fqbn=$fqbn
    fi

    sizes_file="$GITHUB_WORKSPACE/cli_compile_$chunk_index.json"
    if [ $log_compilation ]; then
        #echo board,target and start of sketches to sizes_file json
        echo "{ \"board\": \"$log_fqbn\",
                \"target\": \"$target\",
                \"sketches\": [" >> "$sizes_file"
    fi

    local sketchnum=0
    args+=" -ai $ide_path -au $user_path -i $chunk_index"
    if [ $log_compilation ]; then
        args+=" -l $log_compilation"
    fi
    for sketch in $sketches; do
        local sketchdir=$(dirname $sketch)
        local sketchdirname=$(basename $sketchdir)
        sketchnum=$(($sketchnum + 1))
        if [ "$sketchnum" -le "$start_index" ] \
        || [ "$sketchnum" -gt "$end_index" ]; then
            continue
        fi
        echo ""
        echo "Building Sketch Index $sketchnum - $sketchdirname"
        build_sketch $args -s $sketchdir $xtra_opts
        local result=$?
        if [ $result -ne 0 ]; then
            return $result
        fi
    done

    if [ $log_compilation ]; then
        #remove last comma from json
        if [ $i -eq $(($len - 1)) ]; then
            sed -i '$ s/.$//' "$sizes_file"
        fi
        #echo end of sketches sizes_file json
        echo "]" >> "$sizes_file"
        #echo end of board sizes_file json
        echo "}," >> "$sizes_file"
    fi

    return 0
}

USAGE="
USAGE: ${0} [command] [options]
Available commands:
  count: Count sketches.
  build: Build a sketch.
  chunk_build: Build a chunk of sketches.
"

cmd=$1
shift
if [ -z $cmd ]; then
    echo "ERROR: No command supplied"
    echo "$USAGE"
    exit 2
fi

case "$cmd" in
    "count") count_sketches $*
    ;;
    "build") build_sketch $*
    ;;
    "chunk_build") build_sketches $*
    ;;
    *)
        echo "ERROR: Unrecognized command"
        echo "$USAGE"
        exit 2
esac
