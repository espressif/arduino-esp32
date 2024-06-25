#!/bin/bash

build_all=false
chunks_count=0

if [[ $CORE_CHANGED == 'true' ]] || [[ $IS_PR != 'true' ]]; then
  echo "Core files changed or not a PR. Building all."
  build_all=true
  chunks_count=$MAX_CHUNKS
elif [[ $LIB_CHANGED == 'true' ]]; then
  echo "Libraries changed. Building only affected sketches."
  if [[ $NETWORKING_CHANGED == 'true' ]]; then
    echo "Networking libraries changed. Building networking related sketches."
    networking_sketches="$(find libraries/WiFi -name *.ino) "
    networking_sketches+="$(find libraries/Ethernet -name *.ino) "
    networking_sketches+="$(find libraries/PPP -name *.ino) "
    networking_sketches+="$(find libraries/NetworkClientSecure -name *.ino) "
    networking_sketches+="$(find libraries/WebServer -name *.ino) "
  fi
  if [[ $FS_CHANGED == 'true' ]]; then
    echo "FS libraries changed. Building FS related sketches."
    fs_sketches="$(find libraries/SD -name *.ino) "
    fs_sketches+="$(find libraries/SD_MMC -name *.ino) "
    fs_sketches+="$(find libraries/SPIFFS -name *.ino) "
    fs_sketches+="$(find libraries/LittleFS -name *.ino) "
    fs_sketches+="$(find libraries/FFat -name *.ino) "
  fi
  sketches="$networking_sketches $fs_sketches"
  for file in $LIB_FILES; do
      if [[ $file == *.ino ]]; then
          # If file ends with .ino, add it to the list of sketches
          echo "Sketch found: $file"
          sketches+="$file "
      elif [[ $(basename $(dirname $file)) == "src" ]]; then
          # If file is in a src directory, find all sketches in the parent/examples directory
          echo "Library src file found: $file"
          lib=$(dirname $(dirname $file))
          if [[ -d $lib/examples ]]; then
              lib_sketches=$(find $lib/examples -name *.ino)
              sketches+="$lib_sketches "
              echo "Library sketches: $lib_sketches"
          fi
      else
          # If file is in a example folder but it is not a sketch, find all sketches in the current directory
          echo "File in example folder found: $file"
          sketch=$(find $(dirname $file) -name *.ino)
          sketches+="$sketch "
          echo "Sketch in example folder: $sketch"
      fi
      echo ""
  done
fi

if [[ -n $sketches ]]; then
  # Remove duplicates
  sketches=$(echo $sketches | tr ' ' '\n' | sort | uniq)
  for sketch in $sketches; do
      echo $sketch >> sketches_found.txt
      chunks_count=$((chunks_count+1))
  done
  echo "Number of sketches found: $chunks_count"
  echo "Sketches:"
  echo "$sketches"

  if [[ $chunks_count -gt $MAX_CHUNKS ]]; then
    echo "More sketches than the allowed number of chunks found. Limiting to $MAX_CHUNKS chunks."
    chunks_count=$MAX_CHUNKS
  fi
fi

chunks='["0"'
for i in $(seq 1 $(( $chunks_count - 1 )) ); do
  chunks+=",\"$i\""
done
chunks+="]"

echo "build_all=$build_all" >> $GITHUB_OUTPUT
echo "build_libraries=$BUILD_LIBRARIES" >> $GITHUB_OUTPUT
echo "build_static_sketches=$BUILD_STATIC_SKETCHES" >> $GITHUB_OUTPUT
echo "build_idf=$BUILD_IDF" >> $GITHUB_OUTPUT
echo "build_platformio=$BUILD_PLATFORMIO" >> $GITHUB_OUTPUT
echo "chunk_count=$chunks_count" >> $GITHUB_OUTPUT
echo "chunks=$chunks" >> $GITHUB_OUTPUT
