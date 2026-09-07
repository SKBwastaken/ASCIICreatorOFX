#!/usr/bin/env bash
set -e

echo "Uninstalling ASCII Art OFX Plugin..."

REMOVED=0

if [ -d "/usr/OFX/Plugins/AsciiArt.ofx.bundle" ]; then
    if [ "$EUID" -ne 0 ]; then
        sudo rm -rf "/usr/OFX/Plugins/AsciiArt.ofx.bundle" 2>/dev/null && REMOVED=1
    else
        rm -rf "/usr/OFX/Plugins/AsciiArt.ofx.bundle" 2>/dev/null && REMOVED=1
    fi
fi

if [ -d "$HOME/.ofx/Plugins/AsciiArt.ofx.bundle" ]; then
    rm -rf "$HOME/.ofx/Plugins/AsciiArt.ofx.bundle" 2>/dev/null && REMOVED=1
fi

if [ $REMOVED -eq 1 ]; then
    echo "[SUCCESS] ASCII Art OFX Plugin uninstalled."
else
    echo "No plugin installation found."
fi
