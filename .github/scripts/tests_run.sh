#!/bin/bash

target=$1
chunk_idex=$2
chunks_num=$3

SCRIPTS_DIR="./.github/scripts"
COUNT_SKETCHES="${SCRIPTS_DIR}/sketch_utils.sh count"

source ${SCRIPTS_DIR}/install-arduino-ide.sh

if [ "$chunks_num" -le 0 ]; then
    echo "ERROR: Chunks count must be positive number"
    return 1
fi
if [ "$chunk_idex" -ge "$chunks_num" ] && [ "$chunks_num" -ge 2 ]; then
    echo "ERROR: Chunk index must be less than chunks count"
    return 1
fi

set +e
${COUNT_SKETCHES} $PWD/tests $target
sketchcount=$?
set -e
sketches=$(cat sketches.txt)
rm -rf sketches.txt

chunk_size=$(( $sketchcount / $chunks_num ))
all_chunks=$(( $chunks_num * $chunk_size ))
if [ "$all_chunks" -lt "$sketchcount" ]; then
    chunk_size=$(( $chunk_size + 1 ))
fi

start_index=0
end_index=0
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

start_num=$(( $start_index + 1 ))
sketchnum=0

for sketch in $sketches; do
    sketchdir=$(dirname $sketch)
    sketchdirname=$(basename $sketchdir)
    sketchname=$(basename $sketch)
    sketchnum=$(($sketchnum + 1))
    if [ "$sketchnum" -le "$start_index" ] \
    || [ "$sketchnum" -gt "$end_index" ]; then
        continue
    fi
    echo ""
    echo "Test for Sketch Index $(($sketchnum - 1)) - $sketchdirname"
    pytest tests -k test_$sketchdirname --junit-xml=tests/$sketchdirname/$sketchdirname.xml
    result=$?
    if [ $result -ne 0 ]; then
        return $result
    fi
done
