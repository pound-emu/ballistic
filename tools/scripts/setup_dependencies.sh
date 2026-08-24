#!/usr/bin/env bash
set -euo pipefail

PACKAGE_MANAGER="${1:?Usage: setup_deps.sh <apt|brew>}"

case "$PACKAGE_MANAGER" in
    apt)
        sudo apt-get update -qq
        sudo apt-get install -y --no-install-recommends libclang-18-dev ccache ninja-build libwayland-dev libxkbcommon-dev xorg-dev mold
        ;;
    brew)
        brew install ccache ninja
        ;;
    *)
        echo "Unsupported package manager: $PACKAGE_MANAGER" >&2
        exit 1
        ;;
esac