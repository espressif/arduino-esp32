#!/bin/bash

function run_test() {
    local target=$1
    local sketch=$2
    local options=$3
    local erase_flash=$4
    local sketchdir=$(dirname $sketch)
    local sketchname=$(basename $sketchdir)

    if [[ -f "$sketchdir/.skip.$platform" ]]; then
      echo "Skipping $sketchname test for $target, platform: $platform"
      skipfile="$sketchdir/.test_skipped"
      touch $skipfile
      exit 0
    fi

    if [ $options -eq 0 ] && [ -f $sketchdir/cfg.json ]; then
        len=`jq -r --arg chip $target '.targets[] | select(.name==$chip) | .fqbn | length' $sketchdir/cfg.json`
    else
        len=1
    fi

    if [ $len -eq 1 ]; then
      # build_dir="$sketchdir/build"
      build_dir="$HOME/.arduino/tests/$sketchname/build.tmp"
      report_file="$sketchdir/$sketchname.xml"
    fi

    for i in `seq 0 $(($len - 1))`
    do
        echo "Running test: $sketchname -- Config: $i"
        if [ $erase_flash -eq 1 ]; then
            esptool.py -c $target erase_flash
        fi

        if [ $len -ne 1 ]; then
            # build_dir="$sketchdir/build$i"
            build_dir="$HOME/.arduino/tests/$sketchname/build$i.tmp"
            report_file="$sketchdir/$sketchname$i.xml"
        fi

        if [ $platform == "wokwi" ]; then
            extra_args="--target $target --embedded-services arduino,wokwi --wokwi-timeout=$wokwi_timeout"
            if [[ -f "$sketchdir/scenario.yaml" ]]; then
                extra_args+=" --wokwi-scenario $sketchdir/scenario.yaml"
            fi
        else
            extra_args="--embedded-services esp,arduino"
        fi

        pytest tests --build-dir $build_dir -k test_$sketchname --junit-xml=$report_file $extra_args
        result=$?
        if [ $result -ne 0 ]; then
            return $result
        fi
    done
}

SCRIPTS_DIR="./.github/scripts"
COUNT_SKETCHES="${SCRIPTS_DIR}/sketch_utils.sh count"

platform="hardware"
wokwi_timeout=60000
chunk_run=0
options=0
erase=0

while [ ! -z "$1" ]; do
    case $1 in
    -c )
        chunk_run=1
        ;;
    -w )
        shift
        wokwi_timeout=$1
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
    * )
      break
      ;;
    esac
    shift
done

source ${SCRIPTS_DIR}/install-arduino-ide.sh

# If sketch is provided and test type is not, test type is inferred from the sketch path
if [[ $test_type == "all" ]] || [[ -z $test_type ]]; then
    if [ -n "$sketch" ]; then
        tmp_sketch_path=$(find tests -name $sketch.ino)
        test_type=$(basename $(dirname $(dirname "$tmp_sketch_path")))
        echo "Sketch $sketch test type: $test_type"
        test_folder="$PWD/tests/$test_type"
    else
      test_folder="$PWD/tests"
    fi
else
    test_folder="$PWD/tests/$test_type"
fi

if [ $chunk_run -eq 0 ]; then
    if [ -z $sketch ]; then
        echo "ERROR: Sketch name is required for single test run"
        exit 1
    fi
    run_test $target $test_folder/$sketch/$sketch.ino $options $erase
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
  ${COUNT_SKETCHES} $test_folder $target
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
          touch $PWD/tests/.test_skipped
          exit 0
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
