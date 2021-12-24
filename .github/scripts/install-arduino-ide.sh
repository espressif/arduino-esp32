#!/bin/bash

#OSTYPE: 'linux-gnu', ARCH: 'x86_64' => linux64
#OSTYPE: 'msys', ARCH: 'x86_64' => win32
#OSTYPE: 'darwin18', ARCH: 'i386' => macos

OSBITS=`arch`
if [[ "$OSTYPE" == "linux"* ]]; then
	export OS_IS_LINUX="1"
	ARCHIVE_FORMAT="tar.xz"
	if [[ "$OSBITS" == "i686" ]]; then
		OS_NAME="linux32"
	elif [[ "$OSBITS" == "x86_64" ]]; then
		OS_NAME="linux64"
	elif [[ "$OSBITS" == "armv7l" || "$OSBITS" == "aarch64" ]]; then
		OS_NAME="linuxarm"
	else
		OS_NAME="$OSTYPE-$OSBITS"
		echo "Unknown OS '$OS_NAME'"
		exit 1
	fi
elif [[ "$OSTYPE" == "darwin"* ]]; then
	export OS_IS_MACOS="1"
	ARCHIVE_FORMAT="zip"
	OS_NAME="macosx"
elif [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
	export OS_IS_WINDOWS="1"
	ARCHIVE_FORMAT="zip"
	OS_NAME="windows"
else
	OS_NAME="$OSTYPE-$OSBITS"
	echo "Unknown OS '$OS_NAME'"
	exit 1
fi
export OS_NAME

ARDUINO_BUILD_DIR="$HOME/.arduino/build.tmp"
ARDUINO_CACHE_DIR="$HOME/.arduino/cache.tmp"

if [ "$OS_IS_MACOS" == "1" ]; then
	export ARDUINO_IDE_PATH="/Applications/Arduino.app/Contents/Java"
	export ARDUINO_USR_PATH="$HOME/Documents/Arduino"
elif [ "$OS_IS_WINDOWS" == "1" ]; then
	export ARDUINO_IDE_PATH="$HOME/arduino_ide"
	export ARDUINO_USR_PATH="$HOME/Documents/Arduino"
else
	export ARDUINO_IDE_PATH="$HOME/arduino_ide"
	export ARDUINO_USR_PATH="$HOME/Arduino"
fi

# Updated as of Nov 3rd 2020
ARDUINO_IDE_URL="https://github.com/espressif/arduino-esp32/releases/download/1.0.4/arduino-nightly-"

# Currently not working
#ARDUINO_IDE_URL="https://www.arduino.cc/download.php?f=/arduino-nightly-"

if [ ! -d "$ARDUINO_IDE_PATH" ]; then
	echo "Installing Arduino IDE on $OS_NAME ..."
	echo "Downloading '$ARDUINO_IDE_URL$OS_NAME.$ARCHIVE_FORMAT' to 'arduino.$ARCHIVE_FORMAT' ..."
	if [ "$OS_IS_LINUX" == "1" ]; then
		wget -O "arduino.$ARCHIVE_FORMAT" "$ARDUINO_IDE_URL$OS_NAME.$ARCHIVE_FORMAT" > /dev/null 2>&1
		echo "Extracting 'arduino.$ARCHIVE_FORMAT' ..."
		tar xf "arduino.$ARCHIVE_FORMAT" > /dev/null
		mv arduino-nightly "$ARDUINO_IDE_PATH"
	else
		curl -o "arduino.$ARCHIVE_FORMAT" -L "$ARDUINO_IDE_URL$OS_NAME.$ARCHIVE_FORMAT" > /dev/null 2>&1
		echo "Extracting 'arduino.$ARCHIVE_FORMAT' ..."
		unzip "arduino.$ARCHIVE_FORMAT" > /dev/null
		if [ "$OS_IS_MACOS" == "1" ]; then
			mv "Arduino.app" "/Applications/Arduino.app"
		else
			mv arduino-nightly "$ARDUINO_IDE_PATH"
		fi
	fi
	rm -rf "arduino.$ARCHIVE_FORMAT"

	mkdir -p "$ARDUINO_USR_PATH/libraries"
	mkdir -p "$ARDUINO_USR_PATH/hardware"

	echo "Arduino IDE Installed in '$ARDUINO_IDE_PATH'"
	echo ""
fi

function build_sketch(){ # build_sketch <fqbn> <path-to-ino> [extra-options]
    if [ "$#" -lt 2 ]; then
		echo "ERROR: Illegal number of parameters"
		echo "USAGE: build_sketch <fqbn> <path-to-ino> [extra-options]"
		return 1
	fi

	local fqbn="$1"
	local sketch="$2"
	local xtra_opts="$3"
	local win_opts=""
	if [ "$OS_IS_WINDOWS" == "1" ]; then
		local ctags_version=`ls "$ARDUINO_IDE_PATH/tools-builder/ctags/"`
		local preprocessor_version=`ls "$ARDUINO_IDE_PATH/tools-builder/arduino-preprocessor/"`
		win_opts="-prefs=runtime.tools.ctags.path=$ARDUINO_IDE_PATH/tools-builder/ctags/$ctags_version -prefs=runtime.tools.arduino-preprocessor.path=$ARDUINO_IDE_PATH/tools-builder/arduino-preprocessor/$preprocessor_version"
	fi

	echo ""
	echo "Compiling '"$(basename "$sketch")"' ..."
	mkdir -p "$ARDUINO_BUILD_DIR"
	mkdir -p "$ARDUINO_CACHE_DIR"
	$ARDUINO_IDE_PATH/arduino-builder -compile -logger=human -core-api-version=10810 \
		-fqbn=$fqbn \
		-warnings="all" \
		-tools "$ARDUINO_IDE_PATH/tools-builder" \
		-tools "$ARDUINO_IDE_PATH/tools" \
		-built-in-libraries "$ARDUINO_IDE_PATH/libraries" \
		-hardware "$ARDUINO_IDE_PATH/hardware" \
		-hardware "$ARDUINO_USR_PATH/hardware" \
		-libraries "$ARDUINO_USR_PATH/libraries" \
		-build-cache "$ARDUINO_CACHE_DIR" \
		-build-path "$ARDUINO_BUILD_DIR" \
		$win_opts $xtra_opts "$sketch"
}

function count_sketches() # count_sketches <examples-path>
{
	local examples="$1"
    rm -rf sketches.txt
	if [ ! -d "$examples" ]; then
		touch sketches.txt
		return 0
	fi
    local sketches=$(find $examples -name *.ino)
    local sketchnum=0
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

function build_sketches() # build_sketches <fqbn> <examples-path> <chunk> <total-chunks> [extra-options]
{
    local fqbn=$1
    local examples=$2
    local chunk_idex=$3
    local chunks_num=$4
    local xtra_opts=$5

    if [ "$#" -lt 2 ]; then
		echo "ERROR: Illegal number of parameters"
		echo "USAGE: build_sketches <fqbn> <examples-path> [<chunk> <total-chunks>] [extra-options]"
		return 1
	fi

    if [ "$#" -lt 4 ]; then
		chunk_idex="0"
		chunks_num="1"
		xtra_opts=$3
	fi

	if [ "$chunks_num" -le 0 ]; then
		echo "ERROR: Chunks count must be positive number"
		return 1
	fi
	if [ "$chunk_idex" -ge "$chunks_num" ]; then
		echo "ERROR: Chunk index must be less than chunks count"
		return 1
	fi

	set +e
    count_sketches "$examples"
    local sketchcount=$?
	set -e
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
        build_sketch "$fqbn" "$sketch" "$xtra_opts"
        local result=$?
        if [ $result -ne 0 ]; then
            return $result
        fi
    done
    return 0
}
