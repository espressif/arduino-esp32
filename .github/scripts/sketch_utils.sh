#!/bin/bash

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

        if [ -z $options ] && [ -f $sketchdir/cfg.json ]; then
            # The config file could contain multiple FQBNs for one chip.  If
            # that's the case we build one time for every FQBN.

            len=`jq -r --arg chip $target '.targets[] | select(.name==$chip) | .fqbn | length' $sketchdir/cfg.json`
            fqbn=`jq -r --arg chip $target '.targets[] | select(.name==$chip) | .fqbn' $sketchdir/cfg.json`
        else
            # Since we are passing options, we will end up with only one FQBN to
            # build.

            len=1

            # Default FQBN options if none were passed in the command line.

            esp32_opts="PSRAM=enabled,PartitionScheme=huge_app"
            esp32s2_opts="PSRAM=enabled,PartitionScheme=huge_app"
            esp32s3_opts="PSRAM=opi,USBMode=default,PartitionScheme=huge_app"
            esp32c3_opts="PartitionScheme=huge_app"
            esp32c6_opts="PartitionScheme=huge_app"
            esp32h2_opts="PartitionScheme=huge_app"

            # Select the common part of the FQBN based on the target.  The rest will be
            # appended depending on the passed options.

            case "$target" in
                "esp32")
                    fqbn="espressif:esp32:esp32:${options:-$esp32_opts}"
                ;;
                "esp32s2")
                    fqbn="espressif:esp32:esp32s2:${options:-$esp32s2_opts}"
                ;;
                "esp32c3")
                    fqbn="espressif:esp32:esp32c3:${options:-$esp32c3_opts}"
                ;;
                "esp32s3")
                    fqbn="espressif:esp32:esp32s3:${options:-$esp32s3_opts}"
                ;;
                "esp32c6")
                    fqbn="espressif:esp32:esp32c6:${options:-$esp32c6_opts}"
                ;;
                "esp32h2")
                    fqbn="espressif:esp32:esp32h2:${options:-$esp32h2_opts}"
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
        echo "No FQBN passed or unvalid chip: $target"
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

    if [[ -n $target ]] && [[ -f "$sketchdir/.skip.$target" ]]; then
        echo "Skipping $sketchname for target $target"
        exit 0
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
                > $output_file
            
            exit_status=$?
            if [ $exit_status -ne 0 ]; then
                echo ""ERROR: Compilation failed with error code $exit_status""
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
                echo ""ERROR: Compilation failed with error code $exit_status""
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

function count_sketches(){ # count_sketches <path> [target]
    local path=$1
    local target=$2

    if [ $# -lt 1 ]; then
      echo "ERROR: Illegal number of parameters"
      echo "USAGE: ${0} count <path> [target]"
    fi

    rm -rf sketches.txt
    if [ ! -d "$path" ]; then
        touch sketches.txt
        return 0
    fi

    local sketches=$(find $path -name *.ino | sort)
    local sketchnum=0
    for sketch in $sketches; do
        local sketchdir=$(dirname $sketch)
        local sketchdirname=$(basename $sketchdir)
        local sketchname=$(basename $sketch)
        if [[ "$sketchdirname.ino" != "$sketchname" ]]; then
            continue
        elif [[ -n $target ]] && [[ -f "$sketchdir/.skip.$target" ]]; then
            continue
        else
            echo $sketch >> sketches.txt
            sketchnum=$(($sketchnum + 1))
        fi
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
        * )
            break
            ;;
        esac
        shift
    done

    local xtra_opts=$*

    if [ -z $chunk_index ] || [ -z $chunk_max ]; then
        echo "ERROR: Invalid chunk paramters"
        echo "$USAGE"
        exit 1
    fi

    if [ "$chunk_max" -le 0 ]; then
        echo "ERROR: Chunks count must be positive number"
        return 1
    fi

    if [ "$chunk_index" -gt "$chunk_max" ] &&  [ "$chunk_max" -ge 2 ]; then
        chunk_index=$chunk_max
    fi

    set +e
    count_sketches "$path" "$target"
    local sketchcount=$?
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
            echo "Skipping job"
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

    sizes_file="$GITHUB_WORKSPACE/cli_compile_$chunk_index.json"
    if [ $log_compilation ]; then
        #echo board,target and start of sketches to sizes_file json
        echo "{ \"board\": \"$fqbn\",
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
        echo "Building Sketch Index $(($sketchnum - 1)) - $sketchdirname"
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
