#!/bin/bash

function build_sketch(){ # build_sketch <ide_path> <user_path> <fqbn> <path-to-ino> [extra-options]
    if [ "$#" -lt 4 ]; then
        echo "ERROR: Illegal number of parameters"
        echo "USAGE: ${0} build <ide_path> <user_path> <fqbn> <path-to-ino> [extra-options]"
        return 1
    fi

    local ide_path=$1
    local usr_path=$2
    local fqbn=$3
    local sketch=$4
    local xtra_opts=$5
    local win_opts=$6

    ARDUINO_CACHE_DIR="$HOME/.arduino/cache.tmp"
    if [ -z "$ARDUINO_BUILD_DIR" ]; then
        build_dir="$(dirname $sketch)/build"
    else
        build_dir="$ARDUINO_BUILD_DIR"
    fi

    echo $sketch

    rm -rf "$build_dir"
    mkdir -p "$build_dir"
    mkdir -p "$ARDUINO_CACHE_DIR"
    $ide_path/arduino-builder -compile -logger=human -core-api-version=10810 \
        -fqbn=$fqbn \
        -warnings="all" \
        -tools "$ide_path/tools-builder" \
        -tools "$ide_path/tools" \
        -built-in-libraries "$ide_path/libraries" \
        -hardware "$ide_path/hardware" \
        -hardware "$usr_path/hardware" \
        -libraries "$usr_path/libraries" \
        -build-cache "$ARDUINO_CACHE_DIR" \
        -build-path "$build_dir" \
        $win_opts $xtra_opts "$sketch"
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

function build_sketches(){ # build_sketches <ide_path> <user_path> <fqbn> <target> <path> <chunk> <total-chunks> [extra-options]
    local ide_path=$1
    local usr_path=$2
    local fqbn=$3
    local target=$4
    local path=$5
    local chunk_idex=$6
    local chunks_num=$7
    local xtra_opts=$8

    if [ "$#" -lt 7 ]; then
        echo "ERROR: Illegal number of parameters"
        echo "USAGE: ${0} chunk_build <ide_path> <user_path> <fqbn> <target> <path> [<chunk> <total-chunks>] [extra-options]"
        return 1
    fi

    if [ "$chunks_num" -le 0 ]; then
        echo "ERROR: Chunks count must be positive number"
        return 1
    fi
    if [ "$chunk_idex" -ge "$chunks_num" ] && [ "$chunks_num" -ge 2 ]; then
        echo "ERROR: Chunk index must be less than chunks count"
        return 1
    fi

    set +e
    count_sketches "$path" "$target"
    local sketchcount=$?
    set -e
    local sketches=$(cat sketches.txt)
    rm -rf sketches.txt

    local chunk_size=$(( $sketchcount / $chunks_num ))
    local all_chunks=$(( $chunks_num * $chunk_size ))
    if [ "$all_chunks" -lt "$sketchcount" ]; then
        chunk_size=$(( $chunk_size + 1 ))
    fi

    local start_index=0
    local end_index=0
    if [ "$chunk_idex" -ge "$chunks_num" ]; then
        start_index=$chunk_idex
        end_index=$sketchcount
    else
        start_index=$(( $chunk_idex * $chunk_size ))
        if [ "$sketchcount" -le "$start_index" ]; then
            echo "Skipping job"
            return 0
        fi

        end_index=$(( $(( $chunk_idex + 1 )) * $chunk_size ))
        if [ "$end_index" -gt "$sketchcount" ]; then
            end_index=$sketchcount
        fi
    fi

    local start_num=$(( $start_index + 1 ))
    echo "Found $sketchcount Sketches for target '$target'";
    echo "Chunk Index : $chunk_idex"
    echo "Chunk Count : $chunks_num"
    echo "Chunk Size  : $chunk_size"
    echo "Start Sketch: $start_num"
    echo "End Sketch  : $end_index"

    local sketchnum=0
    for sketch in $sketches; do
        local sketchdir=$(dirname $sketch)
        local sketchdirname=$(basename $sketchdir)
        local sketchname=$(basename $sketch)
        sketchnum=$(($sketchnum + 1))
        if [ "$sketchnum" -le "$start_index" ] \
        || [ "$sketchnum" -gt "$end_index" ]; then
            continue
        fi
        echo ""
        echo "Building Sketch Index $(($sketchnum - 1)) - $sketchdirname"
        build_sketch "$ide_path" "$usr_path" "$fqbn" "$sketch" "$xtra_opts"
        local result=$?
        if [ $result -ne 0 ]; then
            return $result
        fi
    done
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
  "count")
    count_sketches $*
    ;;
  "build")
    build_sketch $*
    ;;
  "chunk_build")
    build_sketches $*
    ;;
  *)
    echo "ERROR: Unrecognized command"
    echo "$USAGE"
    exit 2
esac

