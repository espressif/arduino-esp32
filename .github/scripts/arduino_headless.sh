#!/bin/bash
# Headless Arduino IDE 1.x / arduino-nightly builder for CI (upload, board install).

[ -n "$ARDUINO_HEADLESS_SOURCED" ] && return 0
ARDUINO_HEADLESS_SOURCED=1

_HEADLESS_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPTS_DIR="${SCRIPTS_DIR:-$_HEADLESS_LIB_DIR}"

function run_with_timeout {
    local seconds="$1" pid elapsed rc
    shift
    "$@" &
    pid=$!
    elapsed=0
    while kill -0 "$pid" 2>/dev/null; do
        if [ "$elapsed" -ge "$seconds" ]; then
            echo "ERROR: timed out after ${seconds}s" >&2
            kill "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
            return 124
        fi
        sleep 1
        elapsed=$((elapsed + 1))
    done
    wait "$pid"
    rc=$?
    return "$rc"
}

function _arduino_headless_log_filter_enabled {
    case "${ARDUINO_HEADLESS_LOG_FILTER:-1}" in
        0 | n | N | no | NO | false | FALSE | off | OFF) return 1 ;;
        *) return 0 ;;
    esac
}

function _filter_arduino_headless_log_noise {
    # arduino-nightly mDNS (jmdns) spams WARN + hex dumps on cloud VM hostnames; not actionable in CI.
    LC_ALL=C grep -aEv \
        -e '^(DEBUG|TRACE|INFO) StatusLogger' \
        -e '^Picked up JAVA_TOOL_OPTIONS:' \
        -e 'Arduino\[[0-9]+:[0-9]+\] ' \
        -e '\[JRSAppKitAWT markAppIsDaemon\]' \
        -e '^[[:space:]]+"-' \
        -e '^[[:space:]]*[{}][[:space:]]*$' \
        -e '^[[:space:]]*\)' \
        -e '^JVM(Runtime|Options|Arguments|Classpath|DefaultOptions|MainClassName)' \
        -e '^(CFBundleName|SearchSystemJVM|Command line passed|WorkingDirectory)=' \
        -e 'javax\.jmdns' \
        -e 'dns\[(query|response),' \
        -e '^questions:$' \
        -e 'questions=' \
        -e '[[:space:]]*question:' \
        -e 'DNSQuestion@' \
        -e 'TYPE_IGNORE' \
        -e 'CLASS_UNKNOWN' \
        -e 'flags=0x[0-9a-fA-F]+:' \
        -e 'bx-internal-cloud' \
        -e '\.local' \
        -e '^[[:space:]]*\[[[:space:]]*DNSQuestion@' \
        -e '^[[:space:]]*[0-9a-fA-F]+: [0-9a-fA-F ]' \
        -e '\]\]' \
    | LC_ALL=C grep -a '^[[:print:][:space:]]*$'
}

function _run_arduino_headless_filtered {
    local rc
    if _arduino_headless_log_filter_enabled; then
        set -o pipefail
        "$@" 2>&1 | _filter_arduino_headless_log_noise
        rc=${PIPESTATUS[0]}
    else
        "$@"
        rc=$?
    fi
    return "$rc"
}

function _arduino_headless_java_bin {
    local appdir="$1"
    if [ -x "$appdir/java/bin/java" ]; then
        echo "$appdir/java/bin/java"
        return 0
    fi
    command -v java
}

function _arduino_headless_classpath {
    local appdir="$1" cp
    cp=$(printf '%s:' "$appdir"/lib/*.jar)
    echo "${cp%:}"
}

function _arduino_headless_jul_config {
    local props="$_HEADLESS_LIB_DIR/arduino-headless-logging.properties"
    [ -f "$props" ] || return 1
    printf '%s' "$props"
}

function _run_arduino_headless {
    local appdir="$1"
    shift
    local java_bin classpath jul_config
    local -a java_props=() java_cmd=()
    [ -d "$appdir" ] || { echo "ERROR: Arduino tree not found at $appdir" >&2; return 1; }

    jul_config="$(_arduino_headless_jul_config || true)"
    java_props=(-Djava.awt.headless=true)
    [ -n "$jul_config" ] && java_props+=("-Djava.util.logging.config.file=$jul_config")
    export JAVA_TOOL_OPTIONS="${java_props[*]}"

    if [[ "${OS_IS_WINDOWS:-0}" == "1" ]]; then
        MSYS2_ARG_CONV_EXCL="*" MSYS_NO_PATHCONV=1 \
            _run_arduino_headless_filtered "$appdir/arduino_debug.exe" "$@"
        return
    fi

    if [[ "${OS_IS_MACOS:-0}" == "1" ]]; then
        local app_bundle launcher
        app_bundle="$(cd "$appdir/../.." && pwd)"
        launcher="$app_bundle/Contents/MacOS/Arduino"
        [ -x "$launcher" ] || { echo "ERROR: Arduino launcher not found at $launcher" >&2; return 1; }
        java_props+=(-Dapple.awt.UIElement=true)
        export JAVA_TOOL_OPTIONS="${java_props[*]}"
        if [[ "$(uname -m)" == "arm64" ]]; then
            _run_arduino_headless_filtered arch -x86_64 "$launcher" "$@"
        else
            _run_arduino_headless_filtered "$launcher" "$@"
        fi
        return
    fi

    java_bin="$(_arduino_headless_java_bin "$appdir")"
    [ -n "$java_bin" ] || { echo "ERROR: java not found for $appdir" >&2; return 1; }
    classpath="$(_arduino_headless_classpath "$appdir")"
    java_cmd=("$java_bin" "${java_props[@]}" -DAPP_DIR="$appdir" -cp "$classpath" processing.app.Base "$@")
    if [[ "${OS_IS_LINUX:-0}" == "1" ]] && command -v xvfb-run >/dev/null 2>&1; then
        _run_arduino_headless_filtered xvfb-run -a "${java_cmd[@]}"
    else
        _run_arduino_headless_filtered "${java_cmd[@]}"
    fi
}

function run_arduino_ide_v1 {
    _run_arduino_headless "${ARDUINO_IDE_PATH:?ARDUINO_IDE_PATH required — source install-arduino-ide.sh first}" "$@"
}

function run_arduino_builder {
    _run_arduino_headless "${ARDUINO_BUILDER_PATH:?ARDUINO_BUILDER_PATH required — source install-arduino-builder.sh first}" "$@"
}
