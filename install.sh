#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

echo ""
echo "========================================================"
echo "  ASCII Art OFX Plugin - Build 10"
echo "========================================================"
echo ""
echo "  What's New in Build 10:"
echo "  * Direct Website Button: Click opens https://therealskb.carrd.co/"
echo "  * Pre-compiled Zero-Dependency Linux Bundle"
echo "  * Minimalist Installer & Clean UI"
echo "  * Multi-Core Hardware Acceleration (OpenMP)"
echo "  * Frame Hold (Update Every N Frames: 1-12)"
echo "  * Skip Black & Transparent Areas"
echo "  * Uniform Character Spacing slider (up to 200px)"
echo "  * Goliath Encrypted Font included"
echo "========================================================"
echo ""

if [ ! -d "AsciiArt.ofx.bundle" ]; then
    echo "[FAILED] AsciiArt.ofx.bundle not found. Please extract the ZIP before running."
    exit 1
fi

DEST="/usr/OFX/Plugins"
if [ "$EUID" -ne 0 ]; then
    if sudo -n true 2>/dev/null; then
        sudo mkdir -p "$DEST" && sudo cp -r "AsciiArt.ofx.bundle" "$DEST/" 2>/dev/null
    else
        DEST="$HOME/.ofx/Plugins"
        mkdir -p "$DEST" && cp -r "AsciiArt.ofx.bundle" "$DEST/" 2>/dev/null
    fi
else
    mkdir -p "$DEST" && cp -r "AsciiArt.ofx.bundle" "$DEST/" 2>/dev/null
fi

if [ $? -eq 0 ]; then
    echo "[SUCCESS] ASCII Art OFX Plugin (Build 10) installed to $DEST!"
    echo ""
    exit 0
else
    echo "[FAILED] Could not copy bundle to $DEST."
    exit 1
fi
