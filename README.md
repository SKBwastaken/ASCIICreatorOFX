# ASCII Art OFX Plugin

High-performance OpenFX (OFX) image effect plugin for DaVinci Resolve and other OFX-compliant hosts. Converts video tracks and clips into customizable ASCII art in real time on Windows and Linux.

---

## Features

- Real-Time Multi-Core Rendering: CPU-accelerated rendering utilizing OpenMP across all available cores.
- Pre-compiled & Standalone: Ready-to-use binaries for Windows (x64) and Linux (x86_64). Zero external runtime dependencies.
- Font Engines:
  - Bundled Goliath Encrypted Font (automatically detected from bundle resources).
  - Custom font loading: Browse and load any external TrueType (`.ttf`) or OpenType (`.otf`) font file.
  - Built-in embedded fallback bitmap font.
- Threshold & Alpha Cutoff:
  - Skip Black Areas: Suppresses glyph rendering over dark and transparent backgrounds to prevent stray characters in shadows.
  - Independent Black Cutoff and Alpha Cutoff threshold sliders.
- Grid & Spacing Control:
  - Single uniform Character Spacing slider (2px to 200px+) maintaining square, uniform cell layouts.
  - Adjustable character aspect ratio and font scaling.
- Color Palettes:
  - Full Color (per-pixel original sampling).
  - Average Solid Color per cell.
  - Monochrome / Tint with custom foreground and background colors.
  - Cyber Lime (#C2FD04 on Black) cyber aesthetic preset.
- Frame Hold / Performance Multiplier:
  - Update every N frames (1 to 12): Renders at 1/2, 1/3, or lower frame rates for classic retro/terminal cadence while reducing GPU/CPU render time by 50-70%.
- Edge-to-Edge Sampling: 100% border coverage without edge cropping or grid misalignments.

---

## Installation

### Windows (DaVinci Resolve)

#### Option 1: 1-Click Installer
1. Close DaVinci Resolve.
2. Double-click `Install.bat` (requests administrative privileges to copy files into the system OFX folder).
3. Start DaVinci Resolve.

#### Option 2: Manual Copy
Copy `AsciiArt.ofx.bundle` into:
```text
C:\Program Files\Common Files\OFX\Plugins\
```

To uninstall, run `Uninstall.bat` or delete `AsciiArt.ofx.bundle` from the directory above.

---

### Linux (DaVinci Resolve)

#### Option 1: Installer Script
1. Close DaVinci Resolve.
2. Run in terminal:
```bash
chmod +x install.sh
./install.sh
```
The script installs system-wide to `/usr/OFX/Plugins/` (if sudo is available) or user-locally to `~/.ofx/Plugins/`.

#### Option 2: Manual Copy
Copy `AsciiArt.ofx.bundle` to either:
- System-wide: `/usr/OFX/Plugins/`
- User-only: `~/.ofx/Plugins/` (no root permissions required)

To uninstall, run `./uninstall.sh` or remove `AsciiArt.ofx.bundle` from the target plugins directory.

---

## How to Use in DaVinci Resolve

1. Launch DaVinci Resolve and open a project.
2. In the **Edit** or **Color** page, open the **Effects** library.
3. Navigate to **OpenFX -> Stylize -> ASCII Art**.
4. Drag the effect onto your video clip or node.
5. Adjust parameters in the Inspector panel.

---

## Parameters

| Parameter | Type | Description |
| :--- | :--- | :--- |
| **Character Spacing** | Slider | Size of each ASCII cell in pixels (2 - 200px). |
| **Character Aspect** | Slider | Width-to-height ratio of characters. |
| **Font Source** | Dropdown | Choose between Goliath (bundled), Custom Font File (.ttf/.otf), or Fallback Bitmap. |
| **Font File** | File Browser | Path to custom TrueType or OpenType font. |
| **Character Set** | Dropdown | Presets: Standard, Goliath, Detailed, Blocks, Dense, Minimal, Binary, or Custom. |
| **Custom Characters** | String | User-defined ramp string when Character Set is set to Custom. |
| **Color Mode** | Dropdown | Original Full Color, Cell Average Color, Monochrome, Cyber Lime, or Invert. |
| **Background Color** | RGB | Background fill color behind characters. |
| **Contrast / Brightness** | Sliders | Pre-render input luminance adjustments. |
| **Skip Black Areas** | Toggle | Disables rendering glyphs in black or shadow areas. |
| **Black Cutoff** | Slider | Luminance threshold below which characters are omitted. |
| **Alpha Cutoff** | Slider | Transparency threshold below which characters are omitted. |
| **Frame Hold** | Integer | Updates ASCII generation every N frames (1 = full rate, 2 = half rate, etc.). |

---

## Building from Source

### Windows (MSVC)
Requirements: Visual Studio 2022 (Community, Professional, or Enterprise) with C++ Desktop Development tools.

Run:
```cmd
build.bat
```
The compiled binary is written directly to `AsciiArt.ofx.bundle\Contents\Win64\AsciiArt.ofx`.

### Linux (GCC / Clang)
Requirements: `g++` or `clang++`, `make`, OpenMP (`libgomp`).

Run:
```bash
make
# Or install directly:
sudo make install
```
The compiled binary is written to `AsciiArt.ofx.bundle/Contents/Linux-x86-64/AsciiArt.ofx`.

---

## Repository Structure

```text
ASCIICreatorOFX/
├── AsciiArt.ofx.bundle/     Pre-compiled multi-platform bundle (Win64 & Linux-x86-64)
├── include/                 OpenFX C API headers and stb_truetype
├── src/                     ascii_art_ofx.cpp and glyph_atlas.h source code
├── releases/                Pre-packaged ZIP archives for Windows and Linux
├── build.bat                Windows MSVC build script
├── Makefile                 Linux build file
├── Install.bat              Windows 1-click installer
├── Uninstall.bat            Windows uninstaller
├── install.sh               Linux installer script
├── uninstall.sh             Linux uninstaller script
├── Goliath.ttf              Default retro cyber font asset
└── README.md
```

---

## Credits
- Author: SKB (https://discord.com/users/289503943409664000)
- Portfolio: https://therealskb.carrd.co/
