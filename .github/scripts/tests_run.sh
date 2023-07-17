#!/bin/bash

function run_test() {
    local target=$1
    local sketch=$2
    local options=$3
    local erase_flash=$4
    local sketchdir=$(dirname $sketch)
    local sketchname=$(basename $sketchdir)

    if [ $options -eq 0 ] && [ -f $sketchdir/cfg.json ]; then
        len=`jq -r --arg chip $target '.targets[] | select(.name==$chip) | .fqbn | length' $sketchdir/cfg.json`
    else
        len=1
    fi

    if [ $len -eq 1 ]; then
      # build_dir="tests/$sketchname/build"
      build_dir="$HOME/.arduino/tests/$sketchname/build.tmp"
      report_file="tests/$sketchname/$sketchname.xml"
    fi

    for i in `seq 0 $(($len - 1))`
    do
        echo "Running test: $sketchname -- Config: $i"
        if [ $erase_flash -eq 1 ]; then
            esptool.py -c $target erase_flash
        fi

        if [ $len -ne 1 ]; then
            # build_dir="tests/$sketchname/build$i"
            build_dir="$HOME/.arduino/tests/$sketchname/build$i.tmp"
            report_file="tests/$sketchname/$sketchname$i.xml"
        fi

        pytest tests --build-dir $build_dir -k test_$sketchname --junit-xml=$report_file
        result=$?
        if [ $result -ne 0 ]; then
            return $result
        fi
    done
}

SCRIPTS_DIR="./.github/scripts"
COUNT_SKETCHES="${SCRIPTS_DIR}/sketch_utils.sh count"

chunk_run=0
options=0
erase=0

while [ ! -z "$1" ]; do
    case $1 in
    -c )
        chunk_run=1
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
    * )
      break
      ;;
    esac
    shift
done

source ${SCRIPTS_DIR}/install-arduino-ide.sh

if [ $chunk_run -eq 0 ]; then
    run_test $target $PWD/tests/$sketch/$sketch.ino $options $erase
else
  if [ "$chunk_max" -le 0 ]; then
      echo "ERROR: Chunks count must be positive number"
      return 1
  fi

  if [ "$chunk_index" -ge "$chunk_max" ] && [ "$chunk_max" -ge 2 ]; then
      echo "ERROR: Chunk index must be less than chunks count"
      return 1
  fi

  set +e
  ${COUNT_SKETCHES} $PWD/tests $target
  sketchcount=$?
  set -e
  sketches=$(cat sketches.txt)
  rm -rf sketches.txt

  chunk_size=$(( $sketchcount / $chunk_max ))
  all_chunks=$(( $chunk_max * $chunk_size ))
  if [ "$all_chunks" -lt "$sketchcount" ]; then
      chunk_size=$(( $chunk_size + 1 ))
  fi

  start_index=0
  end_index=0
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

  start_num=$(( $start_index + 1 ))
  sketchnum=0

  for sketch in $sketches; do

      sketchnum=$(($sketchnum + 1))
      if [ "$sketchnum" -le "$start_index" ] \
      || [ "$sketchnum" -gt "$end_index" ]; then
          continue
      fi
      echo ""
      echo "Sketch Index $(($sketchnum - 1))"

      run_test $target $sketch $options $erase
  done
fi
