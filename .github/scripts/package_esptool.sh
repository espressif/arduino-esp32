#!/bin/bash

set -euo pipefail

# Check version argument
if [[ $# -ne 3 ]]; then
  echo "Usage: $0 <version> <base_folder> <json_path>"
  echo "Example: $0 5.0.dev1 /tmp/esptool /tmp/esptool-5.0.dev1.json"
  exit 1
fi

VERSION=$1
BASE_FOLDER=$2
JSON_PATH=$3

export COPYFILE_DISABLE=1

shopt -s nullglob  # So for loop doesn't run if no matches

# Function to update JSON for a given host
function update_json_for_host {
  local host=$1
  local archive=$2

  # Extract the old url from the JSON for this host, then replace only the filename
  old_url=$(jq -r --arg host "$host" '
    .packages[].tools[] | select(.name == "esptool_py") | .systems[] | select(.host == $host) | .url // empty
  ' "$tmp_json")
  if [[ -n "$old_url" ]]; then
    base_url="${old_url%/*}"
    url="$base_url/$archive"
  else
    echo "No old url found for $host"
    exit 1
  fi

  archiveFileName="$archive"
  checksum="SHA-256:$(shasum -a 256 "$archive" | awk '{print $1}')"
  size=$(stat -f%z "$archive")

  # Use jq to update the JSON
  jq --arg host "$host" \
     --arg url "$url" \
     --arg archiveFileName "$archiveFileName" \
     --arg checksum "$checksum" \
     --arg size "$size" \
     '
     .packages[].tools[]
     |= if .name == "esptool_py" then
          .systems = (
            ((.systems // []) | map(select(.host != $host))) + [{
              host: $host,
              url: $url,
              archiveFileName: $archiveFileName,
              checksum: $checksum,
              size: $size
            }]
          )
        else
          .
        end
     ' "$tmp_json" > "$tmp_json.new" && mv "$tmp_json.new" "$tmp_json"
}

cd "$BASE_FOLDER"

# Delete all archives before starting
rm -f esptool-*.tar.gz esptool-*.zip

for dir in esptool-*; do
  # Check if directory exists and is a directory
  if [[ ! -d "$dir" ]]; then
    continue
  fi

  base="${dir#esptool-}"

  # Add 'linux-' prefix if base doesn't contain linux/macos/win64
  if [[ "$base" != *linux* && "$base" != *macos* && "$base" != *win64* ]]; then
    base="linux-${base}"
  fi

  if [[ "$dir" == esptool-win* ]]; then
    # Windows zip archive
    zipfile="esptool-v${VERSION}-${base}.zip"
    echo "Creating $zipfile from $dir ..."
    zip -r "$zipfile" "$dir"
  else
    # Non-Windows: set permissions and tar.gz archive
    tarfile="esptool-v${VERSION}-${base}.tar.gz"
    echo "Setting permissions and creating $tarfile from $dir ..."
    chmod -R u=rwx,g=rx,o=rx "$dir"
    tar -cvzf "$tarfile" "$dir"
  fi
done

# After the for loop, update the JSON for each archive
# Create a temporary JSON file to accumulate changes
tmp_json="${JSON_PATH}.tmp"
cp "$JSON_PATH" "$tmp_json"

for archive in esptool-v"${VERSION}"-*.tar.gz esptool-v"${VERSION}"-*.zip; do
  [ -f "$archive" ] || continue

  echo "Updating JSON for $archive"

  # Determine host from archive name
  case "$archive" in
    *linux-amd64*)   host="x86_64-pc-linux-gnu" ;;
    *linux-armv7*)   host="arm-linux-gnueabihf" ;;
    *linux-aarch64*) host="aarch64-linux-gnu" ;;
    *macos-amd64*)   host="x86_64-apple-darwin" ;;
    *macos-arm64*)   host="arm64-apple-darwin" ;;
    *win64*)         hosts=("x86_64-mingw32" "i686-mingw32") ;;
    *) echo "Unknown host for $archive"; continue ;;
  esac

  # For win64, loop over both hosts; otherwise, use a single host
  if [[ "$archive" == *win64* ]]; then
    for host in "${hosts[@]}"; do
      update_json_for_host "$host" "$archive"
    done
  else
    update_json_for_host "$host" "$archive"
  fi
done

# After all archives are processed, move the temporary JSON to the final file
mv "$tmp_json" "$JSON_PATH"
