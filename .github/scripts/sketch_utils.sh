#!/bin/bash

SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPTS_DIR}/env.sh"

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
            while IFS= read -r requirement; do
                # Trim whitespace and newlines (use sed instead of xargs for compatibility)
                requirement=$(echo "$requirement" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//;s/[\r\n]//g')
                # Skip empty lines
                [[ -z "$requirement" ]] && continue
                found_line=$(grep -E "^$requirement" "$sdkconfig_path")
                if [[ "$found_line" == "" ]]; then
                    # C5 publishes one Wi-Fi sdkconfig. Matter-over-Thread is
                    # Tools → Matter Network → Thread (overlay + .thread.a).
                    if [[ "$requirement" == "CONFIG_ENABLE_MATTER_OVER_THREAD=y" && "$sdkconfig_path" == *"/esp32c5/"* ]]; then
                        continue
                    fi
                    has_requirements=0
                fi
            done <<< "$requirements"
        fi

        # Check if the sketch requires any configuration options (OR)
        requirements_or=$(yq eval '.requires_any[]' "$sketchdir/ci.yml" 2>/dev/null)
        if [[ "$requirements_or" != "null" && "$requirements_or" != "" ]]; then
            local found=false
            while IFS= read -r requirement; do
                # Trim whitespace and newlines (use sed instead of xargs for compatibility)
                requirement=$(echo "$requirement" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//;s/[\r\n]//g')
                # Skip empty lines
                [[ -z "$requirement" ]] && continue
                found_line=$(grep -E "^$requirement" "$sdkconfig_path")
                if [[ "$found_line" != "" ]]; then
                    found=true
                    break
                fi
            done <<< "$requirements_or"
            if [[ "$found" == "false" ]]; then
                has_requirements=0
            fi
        fi
    fi

    echo "$has_requirements"
}

# Return success unless ci.yml explicitly disables the selected platform or
# platform/target pair. Missing files, malformed YAML, and all values other
# than boolean false are intentionally treated as allowed, matching the test
# runner and report generator semantics.
function platform_build_allowed { # platform_build_allowed <sketchdir> <platform> <target>
    local sketchdir=$1
    local platform=$2
    local target=$3
    local ci_yml="$sketchdir/ci.yml"
    local platform_value
    local platform_target_value

    if [ -z "$platform" ] || [ ! -f "$ci_yml" ]; then
        return 0
    fi

    platform_value=$(yq eval ".platforms.\"${platform}\"" "$ci_yml" 2>/dev/null)
    if [ "$platform_value" = "false" ]; then
        return 1
    fi

    platform_target_value=$(yq eval ".platforms.\"${platform}\".\"${target}\"" "$ci_yml" 2>/dev/null)
    if [ "$platform_target_value" = "false" ]; then
        return 1
    fi

    return 0
}

# Join FQBN menu options, dropping empty entries and keeping only the last value
# given for each menu key. Callers list options lowest priority first, so a test
# can override a per-target default instead of emitting the same key twice, which
# arduino-cli does not resolve predictably. Each key keeps the position of its
# first appearance, so the resulting FQBN stays stable.
function _normalize_fqbn_opts {
    echo "$1" | tr ',' '\n' | awk '
        {
            gsub(/^[[:space:]]+|[[:space:]]+$/, "")
            if ($0 == "") {
                next
            }
            eq = index($0, "=")
            key = (eq ? substr($0, 1, eq - 1) : $0)
            if (!(key in value)) {
                order[++count] = key
            }
            value[key] = $0
        }
        END {
            for (i = 1; i <= count; i++) {
                printf "%s%s", (i > 1 ? "," : ""), value[order[i]]
            }
            printf "\n"
        }
    '
}

# Force ESP32-P4 Serial mapping for the environment. Applied last so it wins
# over target defaults, fqbn_append, full FQBNs from ci.yml, and -fqbn.
# CI hardware runners talk to UART0, so CDC on boot must be Disabled
# (CDCOnBoot=default). Local and Wokwi use USB Serial/JTAG, so Enabled
# (CDCOnBoot=cdc).
function apply_cdc_on_boot_opt {
    local fqbn="$1"
    local board
    board=$(echo "$fqbn" | cut -d':' -f3)
    if [ "$board" != "esp32p4" ]; then
        echo "$fqbn"
        return 0
    fi

    local base options cdc_opt
    base=$(echo "$fqbn" | cut -d':' -f1-3)
    options=$(echo "$fqbn" | cut -d':' -f4-)
    if [ "${CI:-false}" = "true" ]; then
        cdc_opt="CDCOnBoot=default"
    else
        cdc_opt="CDCOnBoot=cdc"
    fi
    echo "${base}:$(_normalize_fqbn_opts "${options},${cdc_opt}")"
}

function default_fqbn_for_target {
    local target="$1"
    local options_override="${2:-}"
    local debug_level="${3:-}"
    local extra_opts="${4:-}"
    local pkg="${5:-espressif:esp32}"
    local fqbn_append="${6:-}"

    local opt=""

    # Lowest priority first: per-target defaults, then the test's fqbn_append,
    # then options the caller asked for, then the debug level from the command
    # line. Duplicate menu keys are resolved by _normalize_fqbn_opts.
    local overrides="${fqbn_append},${extra_opts},${debug_level}"

    local esp32_opts esp32s2_opts esp32s3_opts esp32c3_opts esp32c6_opts esp32h2_opts esp32p4_opts esp32c5_opts
    esp32_opts=$(_normalize_fqbn_opts "PSRAM=enabled,${overrides}")
    esp32s2_opts=$(_normalize_fqbn_opts "PSRAM=enabled,${overrides}")
    esp32s3_opts=$(_normalize_fqbn_opts "PSRAM=opi,USBMode=default,${overrides}")
    esp32c3_opts=$(_normalize_fqbn_opts "${overrides}")
    esp32c6_opts=$(_normalize_fqbn_opts "${overrides}")
    esp32h2_opts=$(_normalize_fqbn_opts "${overrides}")
    esp32p4_opts=$(_normalize_fqbn_opts "PSRAM=enabled,USBMode=hwcdc,ChipVariant=postv3,${overrides}")
    esp32c5_opts=$(_normalize_fqbn_opts "PSRAM=enabled,${overrides}")

    local result=""
    case "$target" in
        esp32)
            [ -n "${options_override:-$esp32_opts}" ] && opt=":${options_override:-$esp32_opts}"
            result="${pkg}:esp32${opt}"
            ;;
        esp32s2)
            [ -n "${options_override:-$esp32s2_opts}" ] && opt=":${options_override:-$esp32s2_opts}"
            result="${pkg}:esp32s2${opt}"
            ;;
        esp32c3)
            [ -n "${options_override:-$esp32c3_opts}" ] && opt=":${options_override:-$esp32c3_opts}"
            result="${pkg}:esp32c3${opt}"
            ;;
        esp32s3)
            [ -n "${options_override:-$esp32s3_opts}" ] && opt=":${options_override:-$esp32s3_opts}"
            result="${pkg}:esp32s3${opt}"
            ;;
        esp32c6)
            [ -n "${options_override:-$esp32c6_opts}" ] && opt=":${options_override:-$esp32c6_opts}"
            result="${pkg}:esp32c6${opt}"
            ;;
        esp32h2)
            [ -n "${options_override:-$esp32h2_opts}" ] && opt=":${options_override:-$esp32h2_opts}"
            result="${pkg}:esp32h2${opt}"
            ;;
        esp32p4)
            [ -n "${options_override:-$esp32p4_opts}" ] && opt=":${options_override:-$esp32p4_opts}"
            result="${pkg}:esp32p4${opt}"
            ;;
        esp32c5)
            [ -n "${options_override:-$esp32c5_opts}" ] && opt=":${options_override:-$esp32c5_opts}"
            result="${pkg}:esp32c5${opt}"
            ;;
        *)
            echo "ERROR: Invalid chip: $target" >&2
            return 1
            ;;
    esac
    apply_cdc_on_boot_opt "$result"
}

# Read the fqbn_append options a ci.yml requests for one target. The field is
# either a string applied to every target, or a map of per-target entries with an
# optional "default" entry. Map entries are merged with "default", the per-target
# entry winning, so a test only has to spell out what differs on that target.
# The field defaults to the top level fqbn_append, but any path can be given so
# that the per-device fields of a multi-device test resolve the same way.
function fqbn_append_for_target { # fqbn_append_for_target <ci_yml> <target> [yq_path]
    local ci_yml="$1"
    local target="$2"
    local path="${3:-.fqbn_append}"

    if [ ! -f "$ci_yml" ]; then
        return 0
    fi

    local value
    if [ "$(yq eval "${path} | type" "$ci_yml" 2>/dev/null)" == "!!map" ]; then
        local common specific
        common=$(yq eval "${path}.default // \"\"" "$ci_yml" 2>/dev/null)
        specific=$(yq eval "${path}.\"${target}\" // \"\"" "$ci_yml" 2>/dev/null)
        value="${common},${specific}"
    else
        value=$(yq eval "${path} // \"\"" "$ci_yml" 2>/dev/null)
    fi

    _normalize_fqbn_opts "$value"
}

function default_upload_test_fqbn {
    default_fqbn_for_target "$1" "" "" "UploadSpeed=115200" "${2:-espressif:esp32}"
}

function split_fqbn_for_cli {
    local fqbn="$1"
    CLI_FQBN_OPTIONS=$(echo "$fqbn" | cut -d':' -f4-)
    CLI_FQBN_BASE=$(echo "$fqbn" | cut -d':' -f1-3)
}

function build_sketch { # build_sketch <ide_path> <user_path> <path-to-ino> [extra-options]
    local skip_install=0
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
        -td )
            shift
            ci_yml_dir=$1
            ;;
        -bn )
            shift
            build_name=$1
            ;;
        -fa )
            shift
            fqbn_append_override=$1
            ;;
        --arduino-cli )
            use_arduino_cli=1
            ;;
        --skip-install )
            skip_install=1
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

        # Resolve which ci.yml to use for FQBN / fqbn_append lookups.
        # For multi-device tests (-td), the sub-sketch directory does not
        # contain its own ci.yml; the parent test directory does.
        local ci_yml_for_build=""
        if [ -f "$sketchdir"/ci.yml ]; then
            ci_yml_for_build="$sketchdir/ci.yml"
        elif [ -n "$ci_yml_dir" ] && [ -f "$ci_yml_dir/ci.yml" ]; then
            ci_yml_for_build="$ci_yml_dir/ci.yml"
        fi

        if [ -z "$options" ] && [ -n "$ci_yml_for_build" ]; then
            # The config file could contain multiple FQBNs for one chip.  If
            # that's the case we build one time for every FQBN.

            len=$(yq eval ".fqbn.${target} | length" "$ci_yml_for_build" 2>/dev/null || echo 0)
            if [ "$len" -gt 0 ]; then
                fqbn=$(yq eval ".fqbn.${target} | sort | @json" "$ci_yml_for_build")
            fi
        fi

        if [ -n "$options" ] || [ "$len" -eq 0 ]; then
            # Since we are passing options, we will end up with only one FQBN to
            # build.

            len=1

            # Options passed with -fa are merged on top of the ones the ci.yml
            # asks for, so a single device of a multi-device test can add to or
            # override them without losing the ones shared by the whole test.
            if [ -n "$ci_yml_for_build" ]; then
                fqbn_append=$(fqbn_append_for_target "$ci_yml_for_build" "$target")
            fi
            if [ -n "${fqbn_append_override:-}" ]; then
                fqbn_append=$(_normalize_fqbn_opts "${fqbn_append},${fqbn_append_override}")
            fi

            fqbn=$(default_fqbn_for_target "$target" "$options" "${debug_level:-}" "" "espressif:esp32" "$fqbn_append") || exit 1
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
    local ci_yml_lib_dir="${ci_yml_dir:-$sketchdir}"
    if [ "$skip_install" -eq 0 ]; then
        install_libs -ai "$ide_path" -s "$ci_yml_lib_dir"
        install_result=$?
        if [ $install_result -ne 0 ]; then
            echo "ERROR: Library installation failed for $sketchname" >&2
            exit $install_result
        fi
    fi

    # Build directory path:
    #   ARDUINO_BUILD_DIR env var → full override (used by on-push.sh for non-test builds)
    #   Otherwise → ~/.arduino/tests/$target/$build_output_name/build[N].tmp
    #     -bn sets a parent directory (test name) for multi-device builds:
    #       e.g. -bn wifi_ap + sketchname=ap → wifi_ap/ap/build.tmp
    #     Without -bn, uses $sketchname directly:
    #       e.g. sketchname=gpio → gpio/build.tmp
    local build_output_name="${build_name:+$build_name/}$sketchname"

    ARDUINO_CACHE_DIR="$HOME/.arduino/cache.tmp"
    sizes_file="${ARDUINO_BUILD_SIZES_FILE:-$GITHUB_WORKSPACE/cli_compile_$chunk_index.json}"

    mkdir -p "$ARDUINO_CACHE_DIR"
    for i in $(seq 0 $((len - 1))); do
        if [ -n "$ARDUINO_BUILD_DIR" ]; then
            # Full override from env var (non-test builds like on-push.sh)
            build_dir="$ARDUINO_BUILD_DIR"
        elif [ "$len" -eq 1 ]; then
            build_dir="$HOME/.arduino/tests/$target/$build_output_name/build.tmp"
        else
            build_dir="$HOME/.arduino/tests/$target/$build_output_name/build$i.tmp"
        fi
        mkdir -p "$build_dir"
        output_file="$build_dir/cli_compile_output.txt"

        currfqbn=$(echo "$fqbn" | jq -r --argjson i "$i" '.[$i]')
        currfqbn=$(apply_cdc_on_boot_opt "$currfqbn")

        if [ "${use_arduino_cli:-0}" -eq 1 ] && [ -f "$ide_path/arduino-cli" ]; then
            echo "Building $sketchname with arduino-cli and FQBN=$currfqbn"
            split_fqbn_for_cli "$currfqbn"
            local curroptions="$CLI_FQBN_OPTIONS"
            local currcli_fqbn="$CLI_FQBN_BASE"
            "$ide_path"/arduino-cli compile \
                --fqbn "$currcli_fqbn" \
                --board-options "$curroptions" \
                --warnings "all" \
                --build-property "compiler.warning_flags.all=-Wall -Werror=all -Wextra" \
                --build-path "$build_dir" \
                "${xtra_opts[@]}" "${sketchdir}" \
                2>&1 | tee "$output_file"
        elif [ "${use_arduino_cmake:-0}" -eq 0 ] && [ -f "$REPO_ROOT/tools/arduino_cmake.py" ]; then
            echo "Building $sketchname with arduino_cmake.py and FQBN=$currfqbn"
            python3 "$REPO_ROOT/tools/arduino_cmake.py" compile \
                --fqbn "$currfqbn" \
                --warnings "all" \
                --build-property "compiler.warning_flags.all=-Wall -Werror=all -Wextra" \
                --build-path "$build_dir" \
                "${xtra_opts[@]}" --sketch "${sketchdir}" \
                2>&1 | tee "$output_file"
        else
            echo "ERROR: Requested build tool not found (arduino-cli or arduino_cmake.py)"
            exit 1
        fi

        exit_status=${PIPESTATUS[0]}
        if [ "$exit_status" -ne 0 ]; then
            echo "ERROR: Compilation failed with error code $exit_status"
            exit "$exit_status"
        fi

        # Copy ci.yml alongside compiled binaries for later consumption by reporting tools.
        # For multi-device tests, ci.yml lives in the parent test directory (-td),
        # not in individual sketch directories.
        local ci_yml_source="${ci_yml_dir:-$sketchdir}"
        if [ -f "$ci_yml_source/ci.yml" ]; then
            cp -f "$ci_yml_source/ci.yml" "$build_dir/ci.yml" 2>/dev/null || true
        fi

        if [ -n "$COMPILE_COMMANDS_DIR" ] && [ -f "$build_dir/compile_commands.json" ]; then
            mkdir -p "$COMPILE_COMMANDS_DIR"
            # Use a path-based unique name to avoid collisions when multiple sketches share
            # the same basename (e.g. two sketches both named LeaderNode in different subdirs).
            # Strip the prefix up to and including the first occurrence of "/libraries/" so
            # the result is relative and readable; fall back to the full path if not found.
            local sketch_relpath="${sketchdir#*/libraries/}"
            local sketch_safe="${sketch_relpath//\//_}"  # replace all '/' with '_'
            sketch_safe="${sketch_safe#_}"               # strip any leading underscore
            jq --arg t "$target" '[.[] | . + {_target: $t}]' \
                "$build_dir/compile_commands.json" > "$COMPILE_COMMANDS_DIR/${target}_${sketch_safe}.json"
        fi

        if [ -n "$log_compilation" ]; then
            flash_bytes=$(grep -oE 'Sketch uses ([0-9]+) bytes' "$output_file" | awk '{print $3}')
            flash_percentage=$(grep -oE 'Sketch uses ([0-9]+) bytes \(([0-9]+)%\)' "$output_file" | awk '{print $5}' | tr -d '(%)')
            ram_bytes=$(grep -oE 'Global variables use ([0-9]+) bytes' "$output_file" | awk '{print $4}')
            ram_percentage=$(grep -oE 'Global variables use ([0-9]+) bytes \(([0-9]+)%\)' "$output_file" | awk '{print $6}' | tr -d '(%)')

            constant_part="/home/runner/Arduino/hardware/espressif/esp32/libraries/"
            lib_sketch_name="${sketchdir#"$constant_part"}"
            echo "{\"name\": \"$lib_sketch_name\",
                \"sizes\": [{
                        \"flash_bytes\": $flash_bytes,
                        \"flash_percentage\": $flash_percentage,
                        \"ram_bytes\": $ram_bytes,
                        \"ram_percentage\": $ram_percentage
                        }]
                }," >> "$sizes_file"
        fi
    done

    unset fqbn
    unset xtra_opts
    unset options
}

function count_sketches { # count_sketches <path> [target] [ignore-requirements] [file] [platform] [output-file]
    local path=$1
    local target=$2
    local ignore_requirements=$3
    local file=$4
    local platform=$5
    local output_file="${6:-$PWD/sketches.txt}"
    local candidates_file

    if [ $# -lt 1 ]; then
        echo "ERROR: Illegal number of parameters"
        echo "USAGE: ${0} count <path> [target] [ignore-requirements] [file] [platform] [output-file]"
        return 2
    fi

    : > "$output_file"
    if [ ! -d "$path" ]; then
        echo "0"
        return 0
    fi

    candidates_file=$(mktemp "${TMPDIR:-/tmp}/arduino-sketch-candidates.XXXXXX") || return 1
    if [ -f "$file" ]; then
        sort "$file" > "$candidates_file"
    else
        find "$path" -name '*.ino' | sort > "$candidates_file"
    fi

    local sketchnum=0
    while IFS= read -r sketch || [ -n "$sketch" ]; do
        local sketchdir
        local sketchdirname
        local sketchname
        local has_requirements
        local parent_dir

        sketchdir=$(dirname "$sketch")
        sketchdirname=$(basename "$sketchdir")
        sketchname=$(basename "$sketch")
        parent_dir=$(dirname "$sketchdir")

        if [[ "$sketchdirname.ino" != "$sketchname" ]]; then
            continue
        # Skip sketches that are part of multi-device tests (they are built separately)
        elif [[ -f "$parent_dir/ci.yml" ]]; then
            local has_multi_device
            has_multi_device=$(yq eval '.multi_device' "$parent_dir/ci.yml" 2>/dev/null)
            if [[ "$has_multi_device" != "null" && "$has_multi_device" != "" ]]; then
                continue
            fi
        fi

        if [[ -n $platform ]] && ! platform_build_allowed "$sketchdir" "$platform" "$target"; then
            continue
        fi

        if [[ -n $target ]] && [[ -f $sketchdir/ci.yml ]]; then
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
        echo "$sketch" >> "$output_file"
        sketchnum=$((sketchnum + 1))
    done < "$candidates_file"
    rm -f "$candidates_file"
    # Echo count to stdout instead of using return code (which is limited to 0-255)
    echo "$sketchnum"
}

function logical_cpu_count {
    local cpus=""
    if command -v getconf >/dev/null 2>&1; then
        cpus=$(getconf _NPROCESSORS_ONLN 2>/dev/null)
    fi
    if ! [[ "$cpus" =~ ^[1-9][0-9]*$ ]] && command -v sysctl >/dev/null 2>&1; then
        cpus=$(sysctl -n hw.logicalcpu 2>/dev/null)
    fi
    if ! [[ "$cpus" =~ ^[1-9][0-9]*$ ]] && command -v nproc >/dev/null 2>&1; then
        cpus=$(nproc 2>/dev/null)
    fi
    if ! [[ "$cpus" =~ ^[1-9][0-9]*$ ]] && [[ "${NUMBER_OF_PROCESSORS:-}" =~ ^[1-9][0-9]*$ ]]; then
        cpus=$NUMBER_OF_PROCESSORS
    fi
    if ! [[ "$cpus" =~ ^[1-9][0-9]*$ ]]; then
        cpus=1
    fi
    echo "$cpus"
}

function auto_sketch_jobs {
    # Two-wide (or more on bigger machines) pipelines independent links
    # after a one-sketch warmup at full Ninja width.
    local cpus
    cpus=$(logical_cpu_count)
    local jobs=$((cpus / 2))
    if [ "$jobs" -lt 1 ]; then
        jobs=1
    fi
    echo "$jobs"
}

function remove_trailing_comma {
    local file=$1
    local temp_file
    temp_file=$(mktemp "${file}.XXXXXX") || return 1
    if ! awk '
        NR > 1 { print previous }
        { previous = $0 }
        END {
            sub(/,$/, "", previous)
            print previous
        }
    ' "$file" > "$temp_file"; then
        rm -f "$temp_file"
        return 1
    fi
    mv -f "$temp_file" "$file"
}

function _reap_parallel_worker {
    local slot=$1
    local running_index="${slot_indices[$slot]:-}"
    local pid="${pids[$slot]:-}"
    local wait_result=1

    if [ -z "$running_index" ] || [ -z "$pid" ]; then
        return 0
    fi
    if wait "$pid"; then
        wait_result=0
    else
        wait_result=$?
    fi
    if [ ! -f "$work_dir/status.$running_index" ]; then
        if [ "$wait_result" -eq 0 ]; then
            wait_result=1
        fi
        printf '%s\n' "$wait_result" > "$work_dir/status.$running_index"
        printf 'Worker exited without status for %s (exit %s)\n' \
            "$(cat "$work_dir/name.$running_index")" "$wait_result" >> "$work_dir/log.$running_index"
    fi
    local result_status=1
    if [ -f "$work_dir/status.$running_index" ]; then
        result_status=$(cat "$work_dir/status.$running_index")
    fi
    printf '[parallel] done  %s/%s %s (exit %s, active %s)\n' \
        "$running_index" "${sketch_count:-?}" \
        "$(cat "$work_dir/name.$running_index")" "$result_status" "$((active - 1))"
    unset "pids[$slot]"
    unset "slot_indices[$slot]"
    active=$((active - 1))
}

function _cleanup_parallel_workers {
    if [ "${cleanup_done:-0}" -eq 1 ]; then
        return 0
    fi
    cleanup_done=1

    local slot
    for ((slot = 0; slot < jobs; slot++)); do
        local pid="${pids[$slot]:-}"
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            kill -TERM "$pid" 2>/dev/null || true
        fi
    done
    for ((slot = 0; slot < jobs; slot++)); do
        local pid="${pids[$slot]:-}"
        if [ -n "$pid" ]; then
            wait "$pid" 2>/dev/null || true
        fi
    done
    if [ -n "${work_dir:-}" ]; then
        rm -rf "$work_dir"
    fi
}

function _restore_trap {
    local signal=$1
    local saved=$2
    if [ -n "$saved" ]; then
        eval "$saved"
    else
        trap - "$signal"
    fi
}

function validate_unique_sketch_basenames {
    local list_file=$1
    local seen_names=()
    local seen_paths=()
    local preflight_sketch
    while IFS= read -r preflight_sketch || [ -n "$preflight_sketch" ]; do
        [ -z "$preflight_sketch" ] && continue
        local preflight_name
        preflight_name=$(basename "$preflight_sketch")
        local seen_index
        for ((seen_index = 0; seen_index < ${#seen_names[@]}; seen_index++)); do
            if [ "${seen_names[$seen_index]}" = "$preflight_name" ]; then
                echo "ERROR: Duplicate sketch basename '$preflight_name' would share an artifact directory:" >&2
                echo "  ${seen_paths[$seen_index]}" >&2
                echo "  $preflight_sketch" >&2
                return 2
            fi
        done
        seen_names+=("$preflight_name")
        seen_paths+=("$preflight_sketch")
    done < "$list_file"
    return 0
}

# Build every sketch directory listed in a newline-delimited file. For jobs=1,
# call build_sketch directly so output and failure behavior remain unchanged.
# Parallel workers use separate processes, logs, statuses, and size fragments.
function build_sketch_list {
    local list_file=""
    local jobs=1
    local isolated_build_dirs=0
    local ide_path=""
    local target=""
    local chunk_index=""
    local log_compilation=""
    local build_args=()

    while [ -n "$1" ]; do
        case "$1" in
        -F )
            shift
            list_file=$1
            ;;
        -j|--jobs )
            shift
            jobs=$1
            ;;
        --isolated-build-dirs )
            isolated_build_dirs=1
            ;;
        -ai )
            shift
            ide_path=$1
            build_args+=("-ai" "$ide_path")
            ;;
        -au )
            shift
            build_args+=("-au" "$1")
            ;;
        -t )
            shift
            target=$1
            build_args+=("-t" "$target")
            ;;
        -i )
            shift
            chunk_index=$1
            build_args+=("-i" "$chunk_index")
            ;;
        -l )
            shift
            log_compilation=$1
            build_args+=("-l" "$log_compilation")
            ;;
        -d )
            shift
            build_args+=("-d" "$1")
            ;;
        -fqbn )
            shift
            build_args+=("-fqbn" "$1")
            ;;
        -o )
            shift
            build_args+=("-o" "$1")
            ;;
        -m|-f )
            shift
            ;;
        --arduino-cli )
            build_args+=("--arduino-cli")
            ;;
        * )
            break
            ;;
        esac
        shift
    done
    local xtra_opts=("$@")

    if [ ! -f "$list_file" ]; then
        echo "ERROR: Sketch list file not found: $list_file" >&2
        return 2
    fi
    if ! [[ "$jobs" =~ ^[1-9][0-9]*$ ]]; then
        echo "ERROR: -j/--jobs must be a positive integer" >&2
        return 2
    fi
    if [ "$jobs" -gt 1 ] && [ "$isolated_build_dirs" -eq 0 ] && [ -n "${ARDUINO_BUILD_DIR:-}" ]; then
        echo "ERROR: -j/--jobs greater than 1 cannot be used with ARDUINO_BUILD_DIR; parallel sketches require separate artifact directories" >&2
        return 2
    fi

    # Every sketch basename maps to the same artifact directory for a target.
    # Reject collisions before installs or compiles rather than racing/corrupting.
    if [ "$jobs" -gt 1 ] && [ "$isolated_build_dirs" -eq 0 ]; then
        validate_unique_sketch_basenames "$list_file" || return $?
    fi

    if [ "$jobs" -eq 1 ]; then
        local serial_sketch
        while IFS= read -r serial_sketch || [ -n "$serial_sketch" ]; do
            [ -z "$serial_sketch" ] && continue
            build_sketch "${build_args[@]}" -s "$serial_sketch" "${xtra_opts[@]}"
            local serial_result=$?
            if [ "$serial_result" -ne 0 ]; then
                return "$serial_result"
            fi
        done < "$list_file"
        return 0
    fi

    # Arduino library/config mutation is not safe from concurrent workers.
    local library_sketch
    while IFS= read -r library_sketch || [ -n "$library_sketch" ]; do
        [ -z "$library_sketch" ] && continue
        install_libs -ai "$ide_path" -s "$library_sketch"
        local install_result=$?
        if [ "$install_result" -ne 0 ]; then
            echo "ERROR: Library preinstall failed for $(basename "$library_sketch")" >&2
            return "$install_result"
        fi
    done < "$list_file"

    local work_dir
    work_dir=$(mktemp -d "${TMPDIR:-/tmp}/arduino-build-workers.XXXXXX") || return 1
    local cleanup_done=0
    local automatic_inner_jobs=0
    local warmup_inner_jobs="${CMAKE_BUILD_PARALLEL_LEVEL:-}"
    local split_inner_jobs="${CMAKE_BUILD_PARALLEL_LEVEL:-}"
    if [ -z "$warmup_inner_jobs" ]; then
        # First sketch of each batch compiles the shared cache at full -jN.
        # Later sketches split that budget so two Ninja jobs cannot oversubscribe.
        local logical_cpus
        logical_cpus=$(logical_cpu_count)
        warmup_inner_jobs=$logical_cpus
        split_inner_jobs=$((logical_cpus / jobs))
        if [ "$split_inner_jobs" -lt 1 ]; then
            split_inner_jobs=1
        fi
        automatic_inner_jobs=1
    fi

    local script_path="${SCRIPTS_DIR}/sketch_utils.sh"
    local pids=()
    local slot_indices=()
    local active=0
    local max_active=0
    local index=0
    local warmed_up=0
    local sketch_count=0
    local count_sketch
    while IFS= read -r count_sketch || [ -n "$count_sketch" ]; do
        [ -z "$count_sketch" ] && continue
        sketch_count=$((sketch_count + 1))
    done < "$list_file"
    local inner_banner="inner ninja ${warmup_inner_jobs}"
    if [ "$warmup_inner_jobs" != "$split_inner_jobs" ]; then
        inner_banner="inner ninja ${warmup_inner_jobs} then ${split_inner_jobs}"
    fi
    echo "Parallel sketch builds: ${sketch_count} sketches, ${jobs} jobs, ${inner_banner}, isolated-build-dirs=${isolated_build_dirs}"
    local saved_int_trap
    local saved_term_trap
    local saved_exit_trap
    saved_int_trap=$(trap -p INT)
    saved_term_trap=$(trap -p TERM)
    saved_exit_trap=$(trap -p EXIT)
    trap '_cleanup_parallel_workers; exit 130' INT
    trap '_cleanup_parallel_workers; exit 143' TERM
    trap '_cleanup_parallel_workers' EXIT
    local sketchdir
    while IFS= read -r sketchdir || [ -n "$sketchdir" ]; do
        [ -z "$sketchdir" ] && continue

        local concurrency=$jobs
        if [ "$warmed_up" -eq 0 ]; then
            concurrency=1
        fi
        while [ "$active" -ge "$concurrency" ]; do
            local completed_slot=-1
            local slot
            for ((slot = 0; slot < jobs; slot++)); do
                local running_index="${slot_indices[$slot]:-}"
                local running_pid="${pids[$slot]:-}"
                if [ -n "$running_index" ] \
                && { [ -f "$work_dir/status.$running_index" ] || ! kill -0 "$running_pid" 2>/dev/null; }; then
                    _reap_parallel_worker "$slot"
                    completed_slot=$slot
                    warmed_up=1
                    concurrency=$jobs
                    break
                fi
            done
            if [ "$completed_slot" -lt 0 ]; then
                sleep 0.05
            fi
        done

        local free_slot=0
        while [ -n "${slot_indices[$free_slot]:-}" ]; do
            free_slot=$((free_slot + 1))
        done
        index=$((index + 1))
        local worker_inner_jobs=$split_inner_jobs
        if [ "$warmed_up" -eq 0 ]; then
            worker_inner_jobs=$warmup_inner_jobs
        fi
        printf '%s\n' "$sketchdir" > "$work_dir/name.$index"
        printf '[parallel] start %s/%s %s (active %s/%s)\n' \
            "$index" "$sketch_count" "$sketchdir" "$((active + 1))" "$concurrency"
        (
            set +e
            worker_child_pid=""
            trap 'if [ -n "$worker_child_pid" ]; then kill -INT "$worker_child_pid" 2>/dev/null || true; wait "$worker_child_pid" 2>/dev/null || true; fi; exit 130' INT
            trap 'if [ -n "$worker_child_pid" ]; then kill -TERM "$worker_child_pid" 2>/dev/null || true; wait "$worker_child_pid" 2>/dev/null || true; fi; exit 143' TERM
            worker_env=("ARDUINO_BUILD_SIZES_FILE=$work_dir/sizes.$index")
            if [ "$automatic_inner_jobs" -eq 1 ]; then
                worker_env+=("CMAKE_BUILD_PARALLEL_LEVEL=$worker_inner_jobs")
            fi
            if [ "$isolated_build_dirs" -eq 1 ]; then
                # Reuse one CMake dir per worker slot so later sketches keep a
                # warm configure/cache, matching master's shared ARDUINO_BUILD_DIR.
                worker_env+=("ARDUINO_BUILD_DIR=$work_dir/build.slot.$free_slot")
            fi
            env "${worker_env[@]}" \
                "$script_path" build "${build_args[@]}" --skip-install -s "$sketchdir" "${xtra_opts[@]}" &
            worker_child_pid=$!
            if wait "$worker_child_pid"; then
                worker_result=0
            else
                worker_result=$?
            fi
            trap - INT TERM
            printf '%s\n' "$worker_result" > "$work_dir/status.$index"
            exit "$worker_result"
        ) > "$work_dir/log.$index" 2>&1 &
        pids[free_slot]=$!
        slot_indices[free_slot]=$index
        printf '%s\n' "${pids[$free_slot]}" > "$work_dir/pid.$index"
        active=$((active + 1))
        if [ "$active" -gt "$max_active" ]; then
            max_active=$active
        fi
    done < "$list_file"

    local slot
    for ((slot = 0; slot < jobs; slot++)); do
        if [ -n "${slot_indices[$slot]:-}" ]; then
            _reap_parallel_worker "$slot"
        fi
    done

    local aggregate_sizes_file="${ARDUINO_BUILD_SIZES_FILE:-$GITHUB_WORKSPACE/cli_compile_$chunk_index.json}"
    local succeeded=0
    local failed=0
    local first_failure=0
    local success_indices=()
    local failure_indices=()
    local result_index
    for ((result_index = 1; result_index <= index; result_index++)); do
        local result=1
        if [ -f "$work_dir/status.$result_index" ]; then
            result=$(cat "$work_dir/status.$result_index")
        fi
        if [ -n "$log_compilation" ] && [ -f "$work_dir/sizes.$result_index" ]; then
            cat "$work_dir/sizes.$result_index" >> "$aggregate_sizes_file"
        fi
        if [ "$result" -eq 0 ]; then
            succeeded=$((succeeded + 1))
            success_indices+=("$result_index")
        else
            failed=$((failed + 1))
            failure_indices+=("$result_index")
            if [ "$first_failure" -eq 0 ]; then
                first_failure=$result
            fi
        fi
    done

    local idx
    if [ "${#success_indices[@]}" -gt 0 ]; then
        for idx in "${success_indices[@]}"; do
            printf '\n===== Build log: %s =====\n' "$(cat "$work_dir/name.$idx")"
            cat "$work_dir/log.$idx"
        done
    fi
    if [ "${#failure_indices[@]}" -gt 0 ]; then
        for idx in "${failure_indices[@]}"; do
            printf '\n===== Build log: %s =====\n' "$(cat "$work_dir/name.$idx")"
            cat "$work_dir/log.$idx"
        done
    fi
    printf '\nParallel build summary: %s succeeded, %s failed (max active %s/%s)\n' \
        "$succeeded" "$failed" "$max_active" "$jobs"
    if [ "${#failure_indices[@]}" -gt 0 ]; then
        for idx in "${failure_indices[@]}"; do
            local result=1
            if [ -f "$work_dir/status.$idx" ]; then
                result=$(cat "$work_dir/status.$idx")
            fi
            printf 'FAILED: %s (exit %s)\n' "$(cat "$work_dir/name.$idx")" "$result"
        done
    fi
    _cleanup_parallel_workers
    _restore_trap INT "$saved_int_trap"
    _restore_trap TERM "$saved_term_trap"
    _restore_trap EXIT "$saved_exit_trap"
    return "$first_failure"
}

function build_sketches { # build_sketches <ide_path> <user_path> <target> <path> <chunk> <total-chunks> [extra-options]
    local args=()
    local jobs=1
    local isolated_build_dirs=0
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
        --arduino-cli )
            args+=("--arduino-cli")
            ;;
        --platform )
            shift
            platform=$1
            ;;
        -j|--jobs )
            shift
            jobs=$1
            ;;
        --isolated-build-dirs )
            isolated_build_dirs=1
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
    if ! [[ "$jobs" =~ ^[1-9][0-9]*$ ]]; then
        echo "ERROR: -j/--jobs must be a positive integer"
        return 2
    fi

    if [ "$chunk_max" -le 0 ]; then
        echo "ERROR: Chunks count must be positive number"
        return 1
    fi

    if [ "$chunk_index" -gt "$chunk_max" ] && [ "$chunk_max" -ge 2 ]; then
        chunk_index=$chunk_max
    fi

    set +e
    local sketchcount
    local sketches_file_for_invocation
    sketches_file_for_invocation=$(mktemp "${TMPDIR:-/tmp}/arduino-sketches.XXXXXX") || return 1
    if [ -n "$sketches_file" ]; then
        sketchcount=$(count_sketches "$path" "$target" "0" "$sketches_file" "$platform" "$sketches_file_for_invocation")
    else
        sketchcount=$(count_sketches "$path" "$target" "" "" "$platform" "$sketches_file_for_invocation")
    fi
    set -e

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
            rm -f "$sketches_file_for_invocation"
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
    local chunk_sketches_file
    chunk_sketches_file=$(mktemp "${TMPDIR:-/tmp}/arduino-chunk-sketches.XXXXXX") || return 1
    while IFS= read -r sketch || [ -n "$sketch" ]; do
        local sketchdir

        sketchdir=$(dirname "$sketch")
        sketchnum=$((sketchnum + 1))

        if [ "$sketchnum" -le "$start_index" ] \
        || [ "$sketchnum" -gt "$end_index" ]; then
            continue
        fi
        printf '%s\n' "$sketchdir" >> "$chunk_sketches_file"
    done < "$sketches_file_for_invocation"
    rm -f "$sketches_file_for_invocation"

    if [ "$jobs" -gt 1 ] && [ "$isolated_build_dirs" -eq 0 ] \
    && ! validate_unique_sketch_basenames "$chunk_sketches_file"; then
        rm -f "$chunk_sketches_file"
        return 2
    fi

    if [ "$jobs" -eq 1 ]; then
        local serial_index=$start_index
        local serial_result=0
        while IFS= read -r sketchdir || [ -n "$sketchdir" ]; do
            [ -z "$sketchdir" ] && continue
            serial_index=$((serial_index + 1))
            echo ""
            echo "Building Sketch Index $serial_index - $(basename "$sketchdir")"
            build_sketch "${args[@]}" -s "$sketchdir" "${xtra_opts[@]}"
            serial_result=$?
            if [ "$serial_result" -ne 0 ]; then
                break
            fi
        done < "$chunk_sketches_file"
        if [ "$serial_result" -ne 0 ]; then
            rm -f "$chunk_sketches_file"
            return "$serial_result"
        fi
    else
        local parallel_index=$start_index
        while IFS= read -r sketchdir || [ -n "$sketchdir" ]; do
            [ -z "$sketchdir" ] && continue
            parallel_index=$((parallel_index + 1))
            echo ""
            echo "Building Sketch Index $parallel_index - $(basename "$sketchdir")"
        done < "$chunk_sketches_file"
        set +e
        local parallel_args=()
        if [ "$isolated_build_dirs" -eq 1 ]; then
            parallel_args+=("--isolated-build-dirs")
        fi
        build_sketch_list -F "$chunk_sketches_file" -j "$jobs" "${parallel_args[@]}" \
            "${args[@]}" "${xtra_opts[@]}"
        local parallel_result=$?
        set -e
        if [ "$parallel_result" -ne 0 ]; then
            rm -f "$chunk_sketches_file"
            return "$parallel_result"
        fi
    fi
    rm -f "$chunk_sketches_file"

    if [ -n "$log_compilation" ]; then
        #remove last comma from json
        if [ "$jobs" -gt 1 ] || [ "$i" -eq $((len - 1)) ]; then
            remove_trailing_comma "$sizes_file" || return 1
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

function install_libs { # install_libs [-ai <cli_path>] -s <sketchdir> [-v]
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
            echo "USAGE: install_libs [-ai <cli_path>] -s <sketchdir> [-v]" >&2
            return 1
            ;;
        esac
        shift
    done

    if [ -z "$sketchdir" ]; then
        echo "ERROR: Sketch directory not provided" >&2
        echo "USAGE: install_libs [-ai <cli_path>] -s <sketchdir> [-v]" >&2
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

    if [ -z "$ide_path" ] || [ ! -f "$ide_path/arduino-cli" ]; then
        echo "arduino-cli not found, installing for library support..."
        source "${SCRIPTS_DIR}/install-arduino-cli.sh"
        ide_path="$ARDUINO_IDE_PATH"
        if [ ! -f "$ide_path/arduino-cli" ]; then
            echo "ERROR: Failed to install arduino-cli" >&2
            return 1
        fi
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
    build_list: Build newline-listed sketch directories (-F file [-j jobs] [--isolated-build-dirs] plus build options).
    chunk_build: Build a chunk of sketches (supports -j jobs and --isolated-build-dirs).
    check_requirements: Check if target meets sketch requirements.
    platform_build_allowed: Check whether ci.yml allows a platform/target build.
    logical_cpu_count: Print the available logical CPU count.
    auto_sketch_jobs: Print the default sketch-job count (half the logical CPUs, at least 1).
    remove_trailing_comma: Portably remove a trailing comma from a file's last line.
    install_libs: Install libraries from ci.yml file.
    default_upload_test_fqbn: Print default mock-upload FQBN for a SoC (target [pkg_prefix]).
    fqbn_append: Print the fqbn_append options a ci.yml sets for a target (ci_yml target [yq_path]).
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
    "build_list") build_sketch_list "$@"
    ;;
    "check_requirements") check_requirements "$@"
    ;;
    "platform_build_allowed") platform_build_allowed "$@"
    ;;
    "logical_cpu_count") logical_cpu_count
    ;;
    "auto_sketch_jobs") auto_sketch_jobs
    ;;
    "remove_trailing_comma") remove_trailing_comma "$@"
    ;;
    "install_libs") install_libs "$@"
    ;;
    "default_upload_test_fqbn") default_upload_test_fqbn "$@"
    ;;
    "fqbn_append") fqbn_append_for_target "$@"
    ;;
    *)
        echo "ERROR: Unrecognized command"
        echo "$USAGE"
        exit 2
    esac
