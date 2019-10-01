#!/bin/bash

export PLATFORMIO_ESP32_PATH="$HOME/.platformio/packages/framework-arduinoespressif32"

echo "Installing Python Wheel..."
pip install wheel > /dev/null 2>&1
if [ $? -ne 0 ]; then echo "ERROR: Install failed"; exit 1; fi

echo "Installing PlatformIO..."
pip install -U https://github.com/platformio/platformio/archive/develop.zip > /dev/null 2>&1
if [ $? -ne 0 ]; then echo "ERROR: Install failed"; exit 1; fi

echo "Installing Platform ESP32..."
python -m platformio platform install https://github.com/platformio/platform-espressif32.git#feature/stage > /dev/null 2>&1
if [ $? -ne 0 ]; then echo "ERROR: Install failed"; exit 1; fi

echo "Replacing the framework version..."
if [[ "$OSTYPE" == "darwin"* ]]; then
	sed 's/https:\/\/github\.com\/espressif\/arduino-esp32\.git/*/' "$HOME/.platformio/platforms/espressif32/platform.json" > "platform.json" && \
	mv -f "platform.json" "$HOME/.platformio/platforms/espressif32/platform.json"
else
	sed -i 's/https:\/\/github\.com\/espressif\/arduino-esp32\.git/*/' "$HOME/.platformio/platforms/espressif32/platform.json"
fi
if [ $? -ne 0 ]; then echo "ERROR: Replace failed"; exit 1; fi

if [ "$GITHUB_REPOSITORY" == "espressif/arduino-esp32" ];  then
	echo "Linking Core..." && \
	ln -s $GITHUB_WORKSPACE "$PLATFORMIO_ESP32_PATH"
else
	echo "Cloning Core Repository..." && \
	git clone https://github.com/espressif/arduino-esp32.git "$PLATFORMIO_ESP32_PATH" > /dev/null 2>&1
	if [ $? -ne 0 ]; then echo "ERROR: GIT clone failed"; exit 1; fi
fi

echo "PlatformIO for ESP32 has been installed"
echo ""


function build_pio_sketch(){ # build_pio_sketch <board> <path-to-ino>
    if [ "$#" -lt 2 ]; then
        echo "ERROR: Illegal number of parameters"
        echo "USAGE: build_pio_sketch <board> <path-to-ino>"
        return 1
    fi

	local board="$1"
	local sketch="$2"
	local sketch_dir=$(dirname "$sketch")
	echo ""
	echo "Compiling '"$(basename "$sketch")"'..."
	python -m platformio ci  --board "$board" "$sketch_dir" --project-option="board_build.partitions = huge_app.csv"
}

function count_sketches() # count_sketches <examples-path>
{
	local examples="$1"
    local sketches=$(find $examples -name *.ino)
    local sketchnum=0
    rm -rf sketches.txt
    for sketch in $sketches; do
        local sketchdir=$(dirname $sketch)
        local sketchdirname=$(basename $sketchdir)
        local sketchname=$(basename $sketch)
        if [[ "${sketchdirname}.ino" != "$sketchname" ]]; then
            continue
        fi;
        if [[ -f "$sketchdir/.test.skip" ]]; then
            continue
        fi
        echo $sketch >> sketches.txt
        sketchnum=$(($sketchnum + 1))
    done
    return $sketchnum
}

function build_pio_sketches() # build_pio_sketches <board> <examples-path> <chunk> <total-chunks>
{
    if [ "$#" -lt 2 ]; then
        echo "ERROR: Illegal number of parameters"
        echo "USAGE: build_pio_sketches <board> <examples-path> [<chunk> <total-chunks>]"
        return 1
    fi

    local board=$1
    local examples=$2
    local chunk_idex=$3
    local chunks_num=$4

    if [ "$#" -lt 4 ]; then
        chunk_idex="0"
        chunks_num="1"
    fi

	if [ "$chunks_num" -le 0 ]; then
		echo "ERROR: Chunks count must be positive number"
		return 1
	fi
	if [ "$chunk_idex" -ge "$chunks_num" ]; then
		echo "ERROR: Chunk index must be less than chunks count"
		return 1
	fi

    count_sketches "$examples"
    local sketchcount=$?
    local sketches=$(cat sketches.txt)
    rm -rf sketches.txt

    local chunk_size=$(( $sketchcount / $chunks_num ))
    local all_chunks=$(( $chunks_num * $chunk_size ))
    if [ "$all_chunks" -lt "$sketchcount" ]; then
    	chunk_size=$(( $chunk_size + 1 ))
    fi

    local start_index=$(( $chunk_idex * $chunk_size ))
    if [ "$sketchcount" -le "$start_index" ]; then
    	echo "Skipping job"
    	return 0
    fi

    local end_index=$(( $(( $chunk_idex + 1 )) * $chunk_size ))
    if [ "$end_index" -gt "$sketchcount" ]; then
    	end_index=$sketchcount
    fi

    local start_num=$(( $start_index + 1 ))
    echo "Found $sketchcount Sketches";
    echo "Chunk Count : $chunks_num"
    echo "Chunk Size  : $chunk_size"
    echo "Start Sketch: $start_num"
    echo "End Sketch  : $end_index"

    local sketchnum=0
    for sketch in $sketches; do
        local sketchdir=$(dirname $sketch)
        local sketchdirname=$(basename $sketchdir)
        local sketchname=$(basename $sketch)
        if [ "${sketchdirname}.ino" != "$sketchname" ] \
        || [ -f "$sketchdir/.test.skip" ]; then
            continue
        fi
        sketchnum=$(($sketchnum + 1))
        if [ "$sketchnum" -le "$start_index" ] \
        || [ "$sketchnum" -gt "$end_index" ]; then
        	continue
        fi
        build_pio_sketch "$board" "$sketch"
        local result=$?
        if [ $result -ne 0 ]; then
            return $result
        fi
    done
    return 0
}
