#!/usr/bin/env bash

set -euo pipefail

export DEBIAN_FRONTEND=noninteractive

echo "[deps] Updating apt indexes"
apt-get update -y

echo "[deps] Installing base packages"
apt-get install -y jq unzip curl wget

echo "[deps] Installing Python packages"
pip3 install PyYAML

echo "[deps] Installing yq (mikefarah/yq) for current architecture"
YQ_VERSION="v4.48.1"
ARCH="$(uname -m)"
case "$ARCH" in
    x86_64) YQ_BINARY=yq_linux_amd64 ;;
    aarch64|arm64) YQ_BINARY=yq_linux_arm64 ;;
    armv7l|armv6l|armhf|arm) YQ_BINARY=yq_linux_arm ;;
    i386|i686) YQ_BINARY=yq_linux_386 ;;
    *) echo "Unsupported architecture: $ARCH"; exit 1 ;;
esac

echo "[deps] Downloading mikefarah/yq $YQ_VERSION ($YQ_BINARY)"
wget -q https://github.com/mikefarah/yq/releases/download/${YQ_VERSION}/${YQ_BINARY} -O /usr/bin/yq
chmod +x /usr/bin/yq
yq --version

echo "[deps] Dependencies installed successfully"
