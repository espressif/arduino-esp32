#!/usr/bin/env bash

function print_size_info()
{
    elf_file=$1

    if [ -z "$elf_file" ]; then
        printf "sketch                       iram0.text flash.text flash.rodata dram0.data dram0.bss    dram     flash\n"
        return 0
    fi

    elf_name=$(basename $elf_file)
    sketch_name="${elf_name%.*}"
    # echo $sketch_name
    declare -A segments
    while read -a tokens; do
        seg=${tokens[0]}
        seg=${seg//./}
        size=${tokens[1]}
        addr=${tokens[2]}
        if [ "$addr" -eq "$addr" -a "$addr" -ne "0" ] 2>/dev/null; then
            segments[$seg]=$size
        fi
    done < <(xtensa-esp32-elf-size --format=sysv $elf_file)

    total_ram=$((${segments[dram0data]} + ${segments[dram0bss]}))
    total_flash=$((${segments[iram0text]} + ${segments[flashtext]} + ${segments[dram0data]} + ${segments[flashrodata]}))
    printf "%-28s %-8d   %-8d   %-8d     %-8d   %-8d     %-8d %-8d\n" $sketch_name ${segments[iram0text]} ${segments[flashtext]} ${segments[flashrodata]} ${segments[dram0data]} ${segments[dram0bss]} $total_ram $total_flash
    return 0
}

function build_sketches()
{
    #set +e
    local arduino=$1
    local srcpath=$2
    local build_arg=$3
    local build_dir=build.tmp
    mkdir -p $build_dir
    local build_cmd="python tools/build.py -b esp32 -v -k -p $PWD/$build_dir $build_arg "
    local sketches=$(find $srcpath -name *.ino)
    print_size_info >size.log
    export ARDUINO_IDE_PATH=$arduino
    for sketch in $sketches; do
        rm -rf $build_dir/*
        local sketchdir=$(dirname $sketch)
        local sketchdirname=$(basename $sketchdir)
        local sketchname=$(basename $sketch)
        if [[ "${sketchdirname}.ino" != "$sketchname" ]]; then
            echo "Skipping $sketch, beacause it is not the main sketch file";
            continue
        fi;
        if [[ -f "$sketchdir/.test.skip" ]]; then
            echo -e "\n ------------ Skipping $sketch ------------ \n";
            continue
        fi
        echo -e "\n ------------ Building $sketch ------------ \n";
        # $arduino --verify $sketch;
        #echo "$build_cmd $sketch"
        time ($build_cmd $sketch >build.log)
        local result=$?
        if [ $result -ne 0 ]; then
            echo "Build failed ($1)"
            echo "Build log:"
            cat build.log
            return $result
        fi
        rm build.log
        print_size_info $build_dir/*.elf >>size.log
    done
    #set -e
}
