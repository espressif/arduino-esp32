#!/bin/sh
# Source: https://github.com/arduino/tooling-project-assets/blob/main/other/installation-script/install.sh

# The original version of this script (https://github.com/Masterminds/glide.sh/blob/master/get) is licensed under the
# MIT license. See https://github.com/Masterminds/glide/blob/master/LICENSE for more details and copyright notice.

PROJECT_OWNER="arduino"
PROJECT_NAME="arduino-cli"

# BINDIR represents the local bin location, defaults to ./bin.
EFFECTIVE_BINDIR=""
DEFAULT_BINDIR="$PWD/bin"

fail() {
  echo "$1"
  exit 1
}

initDestination() {
  if [ -n "$BINDIR" ]; then
    if [ ! -d "$BINDIR" ]; then
      # The second instance of $BINDIR is intentionally a literal in this message.
      # shellcheck disable=SC2016
      fail "$BINDIR "'($BINDIR)'" folder not found. Please create it before continuing."
    fi
    EFFECTIVE_BINDIR="$BINDIR"
  else
    if [ ! -d "$DEFAULT_BINDIR" ]; then
      mkdir "$DEFAULT_BINDIR"
    fi
    EFFECTIVE_BINDIR="$DEFAULT_BINDIR"
  fi
  echo "Installing in $EFFECTIVE_BINDIR"
}

initArch() {
  ARCH=$(uname -m)
  case $ARCH in
  armv5*) ARCH="armv5" ;;
  armv6*) ARCH="ARMv6" ;;
  armv7*) ARCH="ARMv7" ;;
  aarch64) ARCH="ARM64" ;;
  arm64) ARCH="ARM64" ;;
  x86) ARCH="32bit" ;;
  x86_64) ARCH="64bit" ;;
  i686) ARCH="32bit" ;;
  i386) ARCH="32bit" ;;
  esac
  echo "ARCH=$ARCH"
}

initFallbackArch() {
  case "${OS}_${ARCH}" in
  macOS_ARM64)
    # Rosetta 2 allows applications built for x86-64 hosts to run on the ARM 64-bit M1 processor
    FALLBACK_ARCH='64bit'
    ;;
  esac
}

initOS() {
  OS=$(uname -s)
  case "$OS" in
  Linux*) OS='Linux' ;;
  Darwin*) OS='macOS' ;;
  MINGW*) OS='Windows' ;;
  MSYS*) OS='Windows' ;;
  esac
  echo "OS=$OS"
}

initDownloadTool() {
  if command -v "curl" >/dev/null 2>&1; then
    DOWNLOAD_TOOL="curl"
  elif command -v "wget" >/dev/null 2>&1; then
    DOWNLOAD_TOOL="wget"
  else
    fail "You need curl or wget as download tool. Please install it first before continuing"
  fi
  echo "Using $DOWNLOAD_TOOL as download tool"
}

# checkLatestVersion() sets the CHECKLATESTVERSION_TAG variable to the latest version
checkLatestVersion() {
  # Use the GitHub releases webpage to find the latest version for this project
  # so we don't get rate-limited.
  CHECKLATESTVERSION_REGEX="v\?[0-9][A-Za-z0-9\.-]*"
  CHECKLATESTVERSION_LATEST_URL="https://github.com/${PROJECT_OWNER}/${PROJECT_NAME}/releases/tag/v0.36.0-rc.1"
  if [ "$DOWNLOAD_TOOL" = "curl" ]; then
    CHECKLATESTVERSION_TAG=$(curl -SsL $CHECKLATESTVERSION_LATEST_URL | grep -o "<title>Release $CHECKLATESTVERSION_REGEX · ${PROJECT_OWNER}/${PROJECT_NAME}" | grep -o "$CHECKLATESTVERSION_REGEX")
  elif [ "$DOWNLOAD_TOOL" = "wget" ]; then
    CHECKLATESTVERSION_TAG=$(wget -q -O - $CHECKLATESTVERSION_LATEST_URL | grep -o "<title>Release $CHECKLATESTVERSION_REGEX · ${PROJECT_OWNER}/${PROJECT_NAME}" | grep -o "$CHECKLATESTVERSION_REGEX")
  fi
  if [ "$CHECKLATESTVERSION_TAG" = "" ]; then
    echo "Cannot determine latest tag."
    exit 1
  fi
}

getFile() {
  GETFILE_URL="$1"
  GETFILE_FILE_PATH="$2"
  if [ "$DOWNLOAD_TOOL" = "curl" ]; then
    GETFILE_HTTP_STATUS_CODE=$(curl -s -w '%{http_code}' -L "$GETFILE_URL" -o "$GETFILE_FILE_PATH")
  elif [ "$DOWNLOAD_TOOL" = "wget" ]; then
    wget --server-response --content-on-error -q -O "$GETFILE_FILE_PATH" "$GETFILE_URL"
    GETFILE_HTTP_STATUS_CODE=$(awk '/^  HTTP/{print $2}' "$TMP_FILE")
  fi
  echo "$GETFILE_HTTP_STATUS_CODE"
}

downloadFile() {
  if [ -z "$1" ]; then
    checkLatestVersion
    TAG="$CHECKLATESTVERSION_TAG"
  else
    TAG=$1
  fi
  #  arduino-lint_0.4.0-rc1_Linux_64bit.[tar.gz, zip]
  APPLICATION_DIST_PREFIX="${PROJECT_NAME}_${TAG#"v"}_"
  if [ "$OS" = "Windows" ]; then
    APPLICATION_DIST_EXTENSION=".zip"
  else
    APPLICATION_DIST_EXTENSION=".tar.gz"
  fi
  APPLICATION_DIST="${APPLICATION_DIST_PREFIX}${OS}_${ARCH}${APPLICATION_DIST_EXTENSION}"

  # Support specifying nightly build versions (e.g., "nightly-latest") via the script argument.
  case "$TAG" in
  nightly*)
    DOWNLOAD_URL_PREFIX="https://downloads.arduino.cc/${PROJECT_NAME}/nightly/"
    ;;
  *)
    DOWNLOAD_URL_PREFIX="https://downloads.arduino.cc/${PROJECT_NAME}/"
    ;;
  esac
  DOWNLOAD_URL="${DOWNLOAD_URL_PREFIX}${APPLICATION_DIST}"

  INSTALLATION_TMP_FILE="/tmp/$APPLICATION_DIST"
  echo "Downloading $DOWNLOAD_URL"
  httpStatusCode=$(getFile "$DOWNLOAD_URL" "$INSTALLATION_TMP_FILE")
  if [ "$httpStatusCode" -ne 200 ]; then
    if [ -n "$FALLBACK_ARCH" ]; then
      echo "$OS $ARCH release not currently available. Checking for alternative $OS $FALLBACK_ARCH release for your system."
      FALLBACK_APPLICATION_DIST="${APPLICATION_DIST_PREFIX}${OS}_${FALLBACK_ARCH}${APPLICATION_DIST_EXTENSION}"
      DOWNLOAD_URL="${DOWNLOAD_URL_PREFIX}${FALLBACK_APPLICATION_DIST}"
      echo "Downloading $DOWNLOAD_URL"
      httpStatusCode=$(getFile "$DOWNLOAD_URL" "$INSTALLATION_TMP_FILE")
    fi

    if [ "$httpStatusCode" -ne 200 ]; then
      echo "Did not find a release for your system: $OS $ARCH"
      echo "Trying to find a release using the GitHub API."

      LATEST_RELEASE_URL="https://api.github.com/repos/${PROJECT_OWNER}/$PROJECT_NAME/releases/tags/$TAG"
      if [ "$DOWNLOAD_TOOL" = "curl" ]; then
        HTTP_RESPONSE=$(curl -sL --write-out 'HTTPSTATUS:%{http_code}' "$LATEST_RELEASE_URL")
        HTTP_STATUS_CODE=$(echo "$HTTP_RESPONSE" | tr -d '\n' | sed -e 's/.*HTTPSTATUS://')
        BODY=$(echo "$HTTP_RESPONSE" | sed -e 's/HTTPSTATUS\:.*//g')
      elif [ "$DOWNLOAD_TOOL" = "wget" ]; then
        TMP_FILE=$(mktemp)
        BODY=$(wget --server-response --content-on-error -q -O - "$LATEST_RELEASE_URL" 2>"$TMP_FILE" || true)
        HTTP_STATUS_CODE=$(awk '/^  HTTP/{print $2}' "$TMP_FILE")
      fi
      if [ "$HTTP_STATUS_CODE" != 200 ]; then
        echo "Request failed with HTTP status code $HTTP_STATUS_CODE"
        fail "Body: $BODY"
      fi

      # || true forces this command to not catch error if grep does not find anything
      DOWNLOAD_URL=$(echo "$BODY" | grep 'browser_' | cut -d\" -f4 | grep "$APPLICATION_DIST") || true
      if [ -z "$DOWNLOAD_URL" ]; then
        DOWNLOAD_URL=$(echo "$BODY" | grep 'browser_' | cut -d\" -f4 | grep "$FALLBACK_APPLICATION_DIST") || true
      fi

      if [ -z "$DOWNLOAD_URL" ]; then
        echo "Sorry, we dont have a dist for your system: $OS $ARCH"
        fail "You can request one here: https://github.com/${PROJECT_OWNER}/$PROJECT_NAME/issues"
      else
        echo "Downloading $DOWNLOAD_URL"
        getFile "$DOWNLOAD_URL" "$INSTALLATION_TMP_FILE"
      fi
    fi
  fi
}

installFile() {
  INSTALLATION_TMP_DIR="/tmp/$PROJECT_NAME"
  mkdir -p "$INSTALLATION_TMP_DIR"
  if [ "$OS" = "Windows" ]; then
    unzip -d "$INSTALLATION_TMP_DIR" "$INSTALLATION_TMP_FILE"
  else
    tar xf "$INSTALLATION_TMP_FILE" -C "$INSTALLATION_TMP_DIR"
  fi
  INSTALLATION_TMP_BIN="$INSTALLATION_TMP_DIR/$PROJECT_NAME"
  cp "$INSTALLATION_TMP_BIN" "$EFFECTIVE_BINDIR"
  rm -rf "$INSTALLATION_TMP_DIR"
  rm -f "$INSTALLATION_TMP_FILE"
}

bye() {
  BYE_RESULT=$?
  if [ "$BYE_RESULT" != "0" ]; then
    echo "Failed to install $PROJECT_NAME"
  fi
  exit $BYE_RESULT
}

testVersion() {
  set +e
  if EXECUTABLE_PATH="$(command -v $PROJECT_NAME)"; then
    # Convert to resolved, absolute paths before comparison
    EXECUTABLE_REALPATH="$(cd -- "$(dirname -- "$EXECUTABLE_PATH")" && pwd -P)"
    EFFECTIVE_BINDIR_REALPATH="$(cd -- "$EFFECTIVE_BINDIR" && pwd -P)"
    if [ "$EXECUTABLE_REALPATH" != "$EFFECTIVE_BINDIR_REALPATH" ]; then
      # $PATH is intentionally a literal in this message.
      # shellcheck disable=SC2016
      echo "An existing $PROJECT_NAME was found at $EXECUTABLE_PATH. Please prepend \"$EFFECTIVE_BINDIR\" to your "'$PATH'" or remove the existing one."
    fi
  else
    # $PATH is intentionally a literal in this message.
    # shellcheck disable=SC2016
    echo "$PROJECT_NAME not found. You might want to add \"$EFFECTIVE_BINDIR\" to your "'$PATH'
  fi

  set -e
  APPLICATION_VERSION="$("$EFFECTIVE_BINDIR/$PROJECT_NAME" version)"
  echo "$APPLICATION_VERSION installed successfully in $EFFECTIVE_BINDIR"
}

# Execution

#Stop execution on any error
trap "bye" EXIT
initDestination
set -e
initArch
initOS
initFallbackArch
initDownloadTool
downloadFile "$1"
installFile
testVersion
