#!/bin/bash

if [ -d "$ARDUINO_ESP32_PATH/tools/esp32-arduino-libs" ]; then
    SDKCONFIG_DIR="$ARDUINO_ESP32_PATH/tools/esp32-arduino-libs"
elif [ -d "$GITHUB_WORKSPACE/tools/esp32-arduino-libs" ]; then
    SDKCONFIG_DIR="$GITHUB_WORKSPACE/tools/esp32-arduino-libs"
else
    SDKCONFIG_DIR="tools/esp32-arduino-libs"
fi

function check_requirements { # check_requirements <sketchdir> <sdkconfig_path>
    local sketchdir=$1
    local sdkconfig_path=$2
    local has_requirements=1
    local requirements
    local requirements_or

    if [ ! -f "$sdkconfig_path" ] || [ ! -f "$sketchdir/ci.yml" ]; then
        echo "WARNING: sdkconfig or ci.yml not found. Assuming requirements are met." 1>&2
        # Return 1 on error to force the sketch to be built and fail. This way the
        # CI will fail and the user will know that the sketch has a problem.
    else
        # Check if the sketch requires any configuration options (AND)
        requirements=$(yq eval '.requires[]' "$sketchdir/ci.yml" 2>/dev/null)
        if [[ "$requirements" != "null" && "$requirements" != "" ]]; then
            for requirement in $requirements; do
                requirement=$(echo "$requirement" | xargs)
                found_line=$(grep -E "^$requirement" "$sdkconfig_path")
                if [[ "$found_line" == "" ]]; then
                    has_requirements=0
                fi
            done
        fi

        # Check if the sketch requires any configuration options (OR)
        requirements_or=$(yq eval '.requires_any[]' "$sketchdir/ci.yml" 2>/dev/null)
        if [[ "$requirements_or" != "null" && "$requirements_or" != "" ]]; then
            local found=false
            for requirement in $requirements_or; do
                requirement=$(echo "$requirement" | xargs)
                found_line=$(grep -E "^$requirement" "$sdkconfig_path")
                if [[ "$found_line" != "" ]]; then
                    found=true
                    break
                fi
            done
            if [[ "$found" == "false" ]]; then
                has_requirements=0
            fi
        fi
    fi

    echo $has_requirements
}

function build_sketch { # build_sketch <ide_path> <user_path> <path-to-ino> [extra-options]
    while [ -n "$1" ]; do
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
        -d )
            shift
            debug_level="DebugLevel=$1"
            ;;
        * )
            break
            ;;
        esac
        shift
    done

    xtra_opts=("$@")
    len=0

    if [ -z "$sketchdir" ]; then
        echo "ERROR: Sketch directory not provided"
        echo "$USAGE"
        exit 1
    fi

    # No FQBN was passed, try to get it from other options

    if [ -z "$fqbn" ]; then
        if [ -z "$target" ]; then
            echo "ERROR: Unspecified chip"
            echo "$USAGE"
            exit 1
        fi

        # The options are either stored in the test directory, for a per test
        # customization or passed as parameters.  Command line options take
        # precedence.  Note that the following logic also falls to the default
        # parameters if no arguments were passed and no file was found.

        if [ -z "$options" ] && [ -f "$sketchdir"/ci.yml ]; then
            # The config file could contain multiple FQBNs for one chip.  If
            # that's the case we build one time for every FQBN.

            len=$(yq eval ".fqbn.${target} | length" "$sketchdir"/ci.yml 2>/dev/null || echo 0)
            if [ "$len" -gt 0 ]; then
                fqbn=$(yq eval ".fqbn.${target} | sort | @json" "$sketchdir"/ci.yml)
            fi
        fi

        if [ -n "$options" ] || [ "$len" -eq 0 ]; then
            # Since we are passing options, we will end up with only one FQBN to
            # build.

            len=1

            if [ -f "$sketchdir"/ci.yml ]; then
                fqbn_append=$(yq eval '.fqbn_append' "$sketchdir"/ci.yml 2>/dev/null)
                if [ "$fqbn_append" == "null" ]; then
                    fqbn_append=""
                fi
            fi

            # Default FQBN options if none were passed in the command line.
            # Replace any double commas with a single one and strip leading and
            # trailing commas.

            esp32_opts=$(echo "PSRAM=enabled,$debug_level,$fqbn_append" | sed 's/^,*//;s/,*$//;s/,\{2,\}/,/g')
            esp32s2_opts=$(echo "PSRAM=enabled,$debug_level,$fqbn_append" | sed 's/^,*//;s/,*$//;s/,\{2,\}/,/g')
            esp32s3_opts=$(echo "PSRAM=opi,USBMode=default,$debug_level,$fqbn_append" | sed 's/^,*//;s/,*$//;s/,\{2,\}/,/g')
            esp32c3_opts=$(echo "$debug_level,$fqbn_append" | sed 's/^,*//;s/,*$//;s/,\{2,\}/,/g')
            esp32c6_opts=$(echo "$debug_level,$fqbn_append" | sed 's/^,*//;s/,*$//;s/,\{2,\}/,/g')
            esp32h2_opts=$(echo "$debug_level,$fqbn_append" | sed 's/^,*//;s/,*$//;s/,\{2,\}/,/g')
            esp32p4_opts=$(echo "PSRAM=enabled,USBMode=default,$debug_level,$fqbn_append" | sed 's/^,*//;s/,*$//;s/,\{2,\}/,/g')
            esp32c5_opts=$(echo "$debug_level,$fqbn_append" | sed 's/^,*//;s/,*$//;s/,\{2,\}/,/g')

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
                "esp32p4")
                    [ -n "${options:-$esp32p4_opts}" ] && opt=":${options:-$esp32p4_opts}"
                    fqbn="espressif:esp32:esp32p4$opt"
                ;;
                "esp32c5")
                    [ -n "${options:-$esp32c5_opts}" ] && opt=":${options:-$esp32c5_opts}"
                    fqbn="espressif:esp32:esp32c5$opt"
                ;;
                *)
                    echo "ERROR: Invalid chip: $target"
                    exit 1
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

    # The directory that will hold all the artifacts (the build directory) is
    # provided through:
    #  1. An env variable called ARDUINO_BUILD_DIR.
    #  2. Created at the sketch level as "build" in the case of a single
    #     configuration test.
    #  3. Created at the sketch level as "buildX" where X is the number
    #     of configuration built in case of a multiconfiguration test.

    sketchname=$(basename "$sketchdir")
    local has_requirements

    if [ -f "$sketchdir"/ci.yml ]; then
        # If the target is listed as false, skip the sketch. Otherwise, include it.
        is_target=$(yq eval ".targets.${target}" "$sketchdir"/ci.yml 2>/dev/null)
        if [[ "$is_target" == "false" ]]; then
            echo "Skipping $sketchname for target $target"
            exit 0
        fi

        has_requirements=$(check_requirements "$sketchdir" "$SDKCONFIG_DIR/$target/sdkconfig")
        if [ "$has_requirements" == "0" ]; then
            echo "Target $target does not meet the requirements for $sketchname. Skipping."
            exit 0
        fi
    fi

    # Install libraries from ci.yml if they exist
    install_libs -ai "$ide_path" -s "$sketchdir"
    install_result=$?
    if [ $install_result -ne 0 ]; then
        echo "ERROR: Library installation failed for $sketchname" >&2
        exit $install_result
    fi

    ARDUINO_CACHE_DIR="$HOME/.arduino/cache.tmp"
    if [ -n "$ARDUINO_BUILD_DIR" ]; then
        build_dir="$ARDUINO_BUILD_DIR"
    elif [ "$len" -eq 1 ]; then
        # build_dir="$sketchdir/build"
        build_dir="$HOME/.arduino/tests/$target/$sketchname/build.tmp"
    fi

    output_file="$HOME/.arduino/cli_compile_output.txt"
    sizes_file="$GITHUB_WORKSPACE/cli_compile_$chunk_index.json"

    mkdir -p "$ARDUINO_CACHE_DIR"
    for i in $(seq 0 $((len - 1))); do
        if [ "$len" -ne 1 ]; then
            # build_dir="$sketchdir/build$i"
            build_dir="$HOME/.arduino/tests/$target/$sketchname/build$i.tmp"
        fi
        rm -rf "$build_dir"
        mkdir -p "$build_dir"

        currfqbn=$(echo "$fqbn" | jq -r --argjson i "$i" '.[$i]')

        if [ -f "$ide_path/arduino-cli" ]; then
            echo "Building $sketchname with arduino-cli and FQBN=$currfqbn"

            curroptions=$(echo "$currfqbn" | cut -d':' -f4)
            currfqbn=$(echo "$currfqbn" | cut -d':' -f1-3)
            "$ide_path"/arduino-cli compile \
                --fqbn "$currfqbn" \
                --board-options "$curroptions" \
                --warnings "all" \
                --build-property "compiler.warning_flags.all=-Wall -Werror=all -Wextra" \
                --build-path "$build_dir" \
                "${xtra_opts[@]}" "${sketchdir}" \
                2>&1 | tee "$output_file"

            exit_status=${PIPESTATUS[0]}
            if [ "$exit_status" -ne 0 ]; then
                echo "ERROR: Compilation failed with error code $exit_status"
                exit "$exit_status"
            fi

            # Copy ci.yml alongside compiled binaries for later consumption by reporting tools
            if [ -f "$sketchdir/ci.yml" ]; then
                cp -f "$sketchdir/ci.yml" "$build_dir/ci.yml" 2>/dev/null || true
            fi

            if [ -n "$log_compilation" ]; then
                #Extract the program storage space and dynamic memory usage in bytes and percentage in separate variables from the output, just the value without the string
                flash_bytes=$(grep -oE 'Sketch uses ([0-9]+) bytes' "$output_file" | awk '{print $3}')
                flash_percentage=$(grep -oE 'Sketch uses ([0-9]+) bytes \(([0-9]+)%\)' "$output_file" | awk '{print $5}' | tr -d '(%)')
                ram_bytes=$(grep -oE 'Global variables use ([0-9]+) bytes' "$output_file" | awk '{print $4}')
                ram_percentage=$(grep -oE 'Global variables use ([0-9]+) bytes \(([0-9]+)%\)' "$output_file" | awk '{print $6}' | tr -d '(%)')

                # Extract the directory path excluding the filename
                directory_path=$(dirname "$sketch")
                # Define the constant part
                constant_part="/home/runner/Arduino/hardware/espressif/esp32/libraries/"
                # Extract the desired substring
                lib_sketch_name="${directory_path#"$constant_part"}"
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

            "$ide_path"/arduino-builder -compile -logger=human -core-api-version=10810 \
                -fqbn=\""$currfqbn"\" \
                -warnings="all" \
                -tools "$ide_path/tools-builder" \
                -hardware "$user_path/hardware" \
                -libraries "$user_path/libraries" \
                -build-cache "$ARDUINO_CACHE_DIR" \
                -build-path "$build_dir" \
                "${xtra_opts[@]}" "${sketchdir}/${sketchname}.ino"

            exit_status=$?
            if [ $exit_status -ne 0 ]; then
                echo "ERROR: Compilation failed with error code $exit_status"
                exit $exit_status
            fi
            # Copy ci.yml alongside compiled binaries for later consumption by reporting tools
            if [ -f "$sketchdir/ci.yml" ]; then
                cp -f "$sketchdir/ci.yml" "$build_dir/ci.yml" 2>/dev/null || true
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

function count_sketches { # count_sketches <path> [target] [ignore-requirements] [file]
    local path=$1
    local target=$2
    local ignore_requirements=$3
    local file=$4
    local sketches

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
        sketches=$(cat "$file" | sort)
    else
        sketches=$(find "$path" -name '*.ino' | sort)
    fi

    local sketchnum=0
    for sketch in $sketches; do
        local sketchdir
        local sketchdirname
        local sketchname
        local has_requirements

        sketchdir=$(dirname "$sketch")
        sketchdirname=$(basename "$sketchdir")
        sketchname=$(basename "$sketch")

        if [[ "$sketchdirname.ino" != "$sketchname" ]]; then
            continue
        elif [[ -n $target ]] && [[ -f $sketchdir/ci.yml ]]; then
            # If the target is listed as false, skip the sketch. Otherwise, include it.
            is_target=$(yq eval ".targets.${target}" "$sketchdir"/ci.yml 2>/dev/null)
            if [[ "$is_target" == "false" ]]; then
                continue
            fi

            if [ "$ignore_requirements" != "1" ]; then
                has_requirements=$(check_requirements "$sketchdir" "$SDKCONFIG_DIR/$target/sdkconfig")
                if [ "$has_requirements" == "0" ]; then
                    continue
                fi
            fi
        fi
        echo "$sketch" >> sketches.txt
        sketchnum=$((sketchnum + 1))
    done
    return $sketchnum
}

function build_sketches { # build_sketches <ide_path> <user_path> <target> <path> <chunk> <total-chunks> [extra-options]
    local args=()
    while [ -n "$1" ]; do
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
            args+=("-t" "$target")
            ;;
        -fqbn )
            shift
            fqbn=$1
            args+=("-fqbn" "$fqbn")
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
        -d )
            shift
            debug_level="$1"
            args+=("-d" "$debug_level")
            ;;
        * )
            break
            ;;
        esac
        shift
    done

    local xtra_opts=("$@")

    if [ -z "$chunk_index" ] || [ -z "$chunk_max" ]; then
        echo "ERROR: Invalid chunk parameters"
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
    local sketches
    sketches=$(cat sketches.txt)
    rm -rf sketches.txt

    local chunk_size
    local all_chunks
    chunk_size=$(( sketchcount / chunk_max ))
    all_chunks=$(( chunk_max * chunk_size ))
    if [ "$all_chunks" -lt "$sketchcount" ]; then
        chunk_size=$(( chunk_size + 1 ))
    fi

    local start_index=0
    local end_index=0
    if [ "$chunk_index" -ge "$chunk_max" ]; then
        start_index=$chunk_index
        end_index=$sketchcount
    else
        start_index=$(( chunk_index * chunk_size ))
        if [ "$sketchcount" -le "$start_index" ]; then
            echo "No sketches to build for $target in this chunk"
            return 0
        fi

        end_index=$(( $(( chunk_index + 1 )) * chunk_size ))
        if [ "$end_index" -gt "$sketchcount" ]; then
            end_index=$sketchcount
        fi
    fi

    local start_num
    start_num=$(( start_index + 1 ))
    echo "Found $sketchcount Sketches for target '$target'";
    echo "Chunk Index : $chunk_index"
    echo "Chunk Count : $chunk_max"
    echo "Chunk Size  : $chunk_size"
    echo "Start Sketch: $start_num"
    echo "End Sketch  : $end_index"

    #if fqbn is not passed then set it to default for compilation log
    if [ -z "$fqbn" ]; then
        log_fqbn="espressif:esp32:$target"
    else
        log_fqbn=$fqbn
    fi

    sizes_file="$GITHUB_WORKSPACE/cli_compile_$chunk_index.json"
    if [ -n "$log_compilation" ]; then
        #echo board,target and start of sketches to sizes_file json
        echo "{ \"board\": \"$log_fqbn\",
                \"target\": \"$target\",
                \"sketches\": [" >> "$sizes_file"
    fi

    local sketchnum=0
    args+=("-ai" "$ide_path" "-au" "$user_path" "-i" "$chunk_index")
    if [ -n "$log_compilation" ]; then
        args+=("-l" "$log_compilation")
    fi
    for sketch in $sketches; do
        local sketchdir
        local sketchdirname

        sketchdir=$(dirname "$sketch")
        sketchdirname=$(basename "$sketchdir")
        sketchnum=$((sketchnum + 1))

        if [ "$sketchnum" -le "$start_index" ] \
        || [ "$sketchnum" -gt "$end_index" ]; then
            continue
        fi
        echo ""
        echo "Building Sketch Index $sketchnum - $sketchdirname"
        build_sketch "${args[@]}" -s "$sketchdir" "${xtra_opts[@]}"
        local result=$?
        if [ $result -ne 0 ]; then
            return $result
        fi
    done

    if [ -n "$log_compilation" ]; then
        #remove last comma from json
        if [ "$i" -eq $((len - 1)) ]; then
            sed -i '$ s/.$//' "$sizes_file"
        fi
        #echo end of sketches sizes_file json
        echo "]" >> "$sizes_file"
        #echo end of board sizes_file json
        echo "}," >> "$sizes_file"
    fi

    return 0
}

# Return 0 if the string looks like a git URL we should pass to --git-url
is_git_like_url() {
    local u=$1
    [[ "$u" =~ ^https?://.+ ]] || [[ "$u" =~ ^git@[^:]+:.+ ]]
}

# If status!=0, print errors/warnings from captured output (fallback to full output)
print_err_warnings() {
    local status=$1; shift
    local out=$*
    if [ "$status" -ne 0 ]; then
        printf '%s\n' "$out" | grep -Ei "error|warning|warn" >&2 || printf '%s\n' "$out" >&2
    fi
}

function install_libs { # install_libs <ide_path> <sketchdir> [-v]
    local ide_path=""
    local sketchdir=""
    local verbose=false

    while [ -n "$1" ]; do
        case "$1" in
        -ai ) shift; ide_path=$1 ;;
        -s  ) shift; sketchdir=$1 ;;
        -v  ) verbose=true ;;
        * )
            echo "ERROR: Unknown argument: $1" >&2
            echo "USAGE: install_libs -ai <ide_path> -s <sketchdir> [-v]" >&2
            return 1
            ;;
        esac
        shift
    done

    if [ -z "$ide_path" ]; then
        echo "ERROR: IDE path not provided" >&2
        echo "USAGE: install_libs -ai <ide_path> -s <sketchdir> [-v]" >&2
        return 1
    fi
    if [ -z "$sketchdir" ]; then
        echo "ERROR: Sketch directory not provided" >&2
        echo "USAGE: install_libs -ai <ide_path> -s <sketchdir> [-v]" >&2
        return 1
    fi
    if [ ! -f "$ide_path/arduino-cli" ]; then
        echo "ERROR: arduino-cli not found at $ide_path/arduino-cli" >&2
        return 1
    fi

    if [ ! -f "$sketchdir/ci.yml" ]; then
        [ "$verbose" = true ] && echo "No ci.yml found in $sketchdir, skipping library installation"
        return 0
    fi
    if ! yq eval '.' "$sketchdir/ci.yml" >/dev/null 2>&1; then
        echo "ERROR: $sketchdir/ci.yml is not valid YAML" >&2
        return 1
    fi

    local libs_type
    libs_type=$(yq eval '.libs | type' "$sketchdir/ci.yml" 2>/dev/null)
    if [ -z "$libs_type" ] || [ "$libs_type" = "null" ] || [ "$libs_type" = "!!null" ]; then
        [ "$verbose" = true ] && echo "No libs field found in ci.yml, skipping library installation"
        return 0
    elif [ "$libs_type" != "!!seq" ]; then
        echo "ERROR: libs field in ci.yml must be an array, found: $libs_type" >&2
        return 1
    fi

    local libs_count
    libs_count=$(yq eval '.libs | length' "$sketchdir/ci.yml" 2>/dev/null)
    if [ "$libs_count" -eq 0 ]; then
        [ "$verbose" = true ] && echo "libs array is empty in ci.yml, skipping library installation"
        return 0
    fi

    echo "Installing $libs_count libraries from $sketchdir/ci.yml"

    local needs_unsafe=false
    local original_unsafe_setting=""
    local libs
    libs=$(yq eval '.libs[]' "$sketchdir/ci.yml" 2>/dev/null)

    # Detect any git-like URL (GitHub/GitLab/Bitbucket/self-hosted/ssh)
    for lib in $libs; do
        if is_git_like_url "$lib"; then
            needs_unsafe=true
            break
        fi
    done

    if [ "$needs_unsafe" = true ]; then
        [ "$verbose" = true ] && echo "Checking current unsafe install setting..."
        original_unsafe_setting=$("$ide_path/arduino-cli" config get library.enable_unsafe_install 2>/dev/null || echo "false")
        if [ "$original_unsafe_setting" = "false" ]; then
            [ "$verbose" = true ] && echo "Enabling unsafe installs for Git URLs..."
            "$ide_path/arduino-cli" config set library.enable_unsafe_install true >/dev/null 2>&1 || \
                echo "WARNING: Failed to enable unsafe installs, Git URL installs may fail" >&2
        else
            [ "$verbose" = true ] && echo "Unsafe installs already enabled"
        fi
    fi

    local rc=0 install_status=0 output=""
    for lib in $libs; do
        [ "$verbose" = true ] && echo "Processing library: $lib"

        if is_git_like_url "$lib"; then
            [ "$verbose" = true ] && echo "Installing library from git URL: $lib"
            if [ "$verbose" = true ]; then
                "$ide_path/arduino-cli" lib install --git-url "$lib"
                install_status=$?
            else
                output=$("$ide_path/arduino-cli" lib install --git-url "$lib" 2>&1)
                install_status=$?
            fi
        else
            [ "$verbose" = true ] && echo "Installing library by name: $lib"
            if [ "$verbose" = true ]; then
                "$ide_path/arduino-cli" lib install "$lib"
                install_status=$?
            else
                output=$("$ide_path/arduino-cli" lib install "$lib" 2>&1)
                install_status=$?
            fi
        fi

        # Treat "already installed"/"up to date" as success (idempotent)
        if [ $install_status -ne 0 ] && echo "$output" | grep -qiE 'already installed|up to date'; then
            install_status=0
        fi

        if [ "$verbose" != true ]; then
            print_err_warnings "$install_status" "$output"
        fi

        if [ $install_status -ne 0 ]; then
            echo "ERROR: Failed to install library: $lib" >&2
            rc=$install_status
            break
        else
            [ "$verbose" = true ] && echo "Successfully installed library: $lib"
        fi
    done

    if [ "$needs_unsafe" = true ] && [ "$original_unsafe_setting" = "false" ]; then
        [ "$verbose" = true ] && echo "Restoring original unsafe install setting..."
        "$ide_path/arduino-cli" config set library.enable_unsafe_install false >/dev/null 2>&1 || true
    fi

    [ $rc -eq 0 ] && echo "Library installation completed"
    return $rc
}


USAGE="
USAGE: ${0} [command] [options]
Available commands:
    count: Count sketches.
    build: Build a sketch.
    chunk_build: Build a chunk of sketches.
    check_requirements: Check if target meets sketch requirements.
    install_libs: Install libraries from ci.yml file.
"

cmd=$1
shift
if [ -z "$cmd" ]; then
    echo "ERROR: No command supplied"
    echo "$USAGE"
    exit 2
fi

case "$cmd" in
    "count") count_sketches "$@"
    ;;
    "build") build_sketch "$@"
    ;;
    "chunk_build") build_sketches "$@"
    ;;
    "check_requirements") check_requirements "$@"
    ;;
    "install_libs") install_libs "$@"
    ;;
    *)
        echo "ERROR: Unrecognized command"
        echo "$USAGE"
        exit 2
esac
