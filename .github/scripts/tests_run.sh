#!/bin/bash

function run_test() {
    local target=$1
    local sketch=$2
    local options=$3
    local erase_flash=$4
    local sketchdir=$(dirname $sketch)
    local sketchname=$(basename $sketchdir)
    local result=0
    local error=0

    if [ $options -eq 0 ] && [ -f $sketchdir/ci.json ]; then
        len=`jq -r --arg target $target '.fqbn[$target] | length' $sketchdir/ci.json`
        if [ $len -eq 0 ]; then
            len=1
        fi
    else
        len=1
    fi

    if [ $len -eq 1 ]; then
        SDKCONFIG_PATH="$HOME/.arduino/tests/$sketchname/build.tmp/sdkconfig"
    else
        SDKCONFIG_PATH="$HOME/.arduino/tests/$sketchname/build0.tmp/sdkconfig"
    fi

    if [ -f $sketchdir/ci.json ]; then
        # If the target or platform is listed as false, skip the sketch. Otherwise, include it.
        is_target=$(jq -r --arg target $target '.targets[$target]' $sketchdir/ci.json)
        selected_platform=$(jq -r --arg platform $platform '.platforms[$platform]' $sketchdir/ci.json)

        if [[ $is_target == "false" ]] || [[ $selected_platform == "false" ]]; then
            printf "\033[93mSkipping $sketchname test for $target, platform: $platform\033[0m\n"
            printf "\n\n\n"
            return 0
        fi

        # Check if the sketch requires any configuration options (AND)
        requirements=$(jq -r '.requires[]? // empty' $sketchdir/ci.json)
        if [[ "$requirements" != "null" && "$requirements" != "" ]]; then
            for requirement in $requirements; do
                requirement=$(echo $requirement | xargs)
                found_line=$(grep -E "^$requirement" "$SDKCONFIG_PATH")
                if [[ "$found_line" == "" ]]; then
                    printf "\033[93mTarget $target does not meet the requirement $requirement for $sketchname. Skipping.\033[0m\n"
                    printf "\n\n\n"
                    return 0
                fi
            done
        fi

        # Check if the sketch requires any configuration options (OR)
        requirements_or=$(jq -r '.requires_any[]? // empty' $sketchdir/ci.json)
        if [[ "$requirements_or" != "null" && "$requirements_or" != "" ]]; then
            found=false
            for requirement in $requirements_or; do
                requirement=$(echo $requirement | xargs)
                found_line=$(grep -E "^$requirement" "$SDKCONFIG_PATH")
                if [[ "$found_line" != "" ]]; then
                    found=true
                    break
                fi
            done
            if [[ "$found" == "false" ]]; then
                printf "\033[93mTarget $target meets none of the requirements in requires_any for $sketchname. Skipping.\033[0m\n"
                printf "\n\n\n"
                return 0
            fi
        fi
    fi

    if [ $len -eq 1 ]; then
      # build_dir="$sketchdir/build"
      build_dir="$HOME/.arduino/tests/$sketchname/build.tmp"
      report_file="$sketchdir/$target/$sketchname.xml"
    fi

    for i in `seq 0 $(($len - 1))`
    do
        fqbn="Default"

        if [ $len -ne 1 ]; then
            fqbn=`jq -r --arg target $target --argjson i $i '.fqbn[$target] | sort | .[$i]' $sketchdir/ci.json`
        elif [ -f $sketchdir/ci.json ]; then
            has_fqbn=`jq -r --arg target $target '.fqbn[$target]' $sketchdir/ci.json`
            if [ "$has_fqbn" != "null" ]; then
                fqbn=`jq -r --arg target $target '.fqbn[$target] | .[0]' $sketchdir/ci.json`
            fi
        fi

        printf "\033[95mRunning test: $sketchname -- Config: $fqbn\033[0m\n"
        if [ $erase_flash -eq 1 ]; then
            esptool.py -c $target erase_flash
        fi

        if [ $len -ne 1 ]; then
            # build_dir="$sketchdir/build$i"
            build_dir="$HOME/.arduino/tests/$sketchname/build$i.tmp"
            report_file="$sketchdir/$target/$sketchname$i.xml"
        fi

        if [ $platform == "wokwi" ]; then
            extra_args="--target $target --embedded-services arduino,wokwi --wokwi-timeout=$wokwi_timeout"
            if [[ -f "$sketchdir/scenario.yaml" ]]; then
                extra_args+=" --wokwi-scenario $sketchdir/scenario.yaml"
            fi
            if [[ -f "$sketchdir/diagram.$target.json" ]]; then
                extra_args+=" --wokwi-diagram $sketchdir/diagram.$target.json"
            fi

        elif [ $platform == "qemu" ]; then
            PATH=$HOME/qemu/bin:$PATH
            extra_args="--embedded-services qemu --qemu-image-path $build_dir/$sketchname.ino.merged.bin"

            if [ $target == "esp32" ] || [ $target == "esp32s3" ]; then
                extra_args+=" --qemu-prog-path qemu-system-xtensa --qemu-cli-args=\"-machine $target -m 4M -nographic\""
            elif [ $target == "esp32c3" ]; then
                extra_args+=" --qemu-prog-path qemu-system-riscv32 --qemu-cli-args=\"-machine $target -icount 3 -nographic\""
            else
                printf "\033[91mUnsupported QEMU target: $target\033[0m\n"
                exit 1
            fi
        else
            extra_args="--embedded-services esp,arduino"
        fi

        result=0
        printf "\033[95mpytest tests --build-dir $build_dir -k test_$sketchname --junit-xml=$report_file $extra_args\033[0m\n"
        bash -c "set +e; pytest tests --build-dir $build_dir -k test_$sketchname --junit-xml=$report_file $extra_args; exit \$?" || result=$?
        printf "\n"
        if [ $result -ne 0 ]; then
            result=0
            printf "\033[95mRetrying test: $sketchname -- Config: $i\033[0m\n"
            printf "\033[95mpytest tests --build-dir $build_dir -k test_$sketchname --junit-xml=$report_file $extra_args\033[0m\n"
            bash -c "set +e; pytest tests --build-dir $build_dir -k test_$sketchname --junit-xml=$report_file $extra_args; exit \$?" || result=$?
            printf "\n"
            if [ $result -ne 0 ]; then
              printf "\033[91mFailed test: $sketchname -- Config: $i\033[0m\n\n"
              error=$result
            fi
        fi
    done
    return $error
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
    -Q )
        if [ ! -d $QEMU_PATH ]; then
            echo "QEMU path $QEMU_PATH does not exist"
            exit 1
        fi
        platform="qemu"
        ;;
    -W )
        shift
        wokwi_timeout=$1
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
    * )
      break
      ;;
    esac
    shift
done

if [ ! $platform == "qemu" ]; then
    source ${SCRIPTS_DIR}/install-arduino-ide.sh
fi

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
    exit $?
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
  ${COUNT_SKETCHES} "$test_folder" "$target" "1"
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
          exit 0
      fi

      end_index=$(( $(( $chunk_index + 1 )) * $chunk_size ))
      if [ "$end_index" -gt "$sketchcount" ]; then
          end_index=$sketchcount
      fi
  fi

  start_num=$(( $start_index + 1 ))
  sketchnum=0
  error=0

  for sketch in $sketches; do

      sketchnum=$(($sketchnum + 1))
      if [ "$sketchnum" -le "$start_index" ] \
      || [ "$sketchnum" -gt "$end_index" ]; then
          continue
      fi

      printf "\033[95mSketch Index $(($sketchnum - 1))\033[0m\n"

      exit_code=0
      run_test $target $sketch $options $erase || exit_code=$?
      if [ $exit_code -ne 0 ]; then
          error=$exit_code
      fi
  done
  exit $error
fi
