# ASCII OFX Plugin

High-performance OpenFX (OFX) image effect plugin for DaVinci Resolve and other OFX-compliant hosts. Converts video tracks and clips into customizable ASCII art in real time on Windows and Linux. Appears simply as **ASCII** under the Stylize category.

---

## Features

- Real-Time Multi-Core Rendering: CPU-accelerated rendering utilizing OpenMP across all available cores.
- Curated Style Presets: One-click presets to instantly dial in signature aesthetics (Goliath Cyber Lime, Classic Matrix CRT, Full Color Hi-Fi, Pure 1-Bit Terminal, Cyberpunk Neon, CGA Retro PC, ZX Spectrum Vintage, and Lo-Fi 12fps Anime Hold).
- Authentic CRT Phosphor Glow / Bloom: Fast separable box-blurred phosphor bloom pass with Screen and Additive blending modes.
- Expanded Retro Color Palettes: 12 color modes including Original Colors, Cyber Lime, Solid Neon Cyber Lime, Matrix Green (#00FF66), Cyberpunk Neon (Cyan/Pink), CGA 4-Color, ZX Spectrum 16-Color, Monochrome Green/Amber/White, Pure 1-Bit Binary B&W, and Luminance Grayscale.
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
- Frame Hold / Performance Multiplier:
  - Update every N frames (1 to 12): Renders at 1/2, 1/3, or lower frame rates for classic retro/terminal cadence while reducing GPU/CPU render time by 50-70%.
- Edge-to-Edge Sampling: 100% border coverage without edge cropping or grid misalignments.

---

## Downloads

Download the latest pre-compiled binaries:
- [AsciiArt_OFX_Windows.zip](https://github.com/SKBwastaken/ASCIICreatorOFX/releases/download/v1.3.0/AsciiArt_OFX_Windows.zip) (Windows x64 - Includes 1-Click `Install.bat` and `Update.bat`)
- [AsciiArt_OFX_Linux.zip](https://github.com/SKBwastaken/ASCIICreatorOFX/releases/download/v1.2.0/AsciiArt_OFX_Linux.zip) (Linux x86_64 - Includes `install.sh`)

View all releases: [Releases Page](https://github.com/SKBwastaken/ASCIICreatorOFX/releases)

---

## Installation & Updates

### Windows (DaVinci Resolve)

#### Option 1: 1-Click Installer
1. Close DaVinci Resolve.
2. Double-click `Install.bat` (requests administrative privileges to install into the system OFX folder).
3. Start DaVinci Resolve.

#### Option 2: 1-Click Updater with Automatic Backup
Run `Update.bat`. It automatically renames your previous installation to a timestamped backup folder (`AsciiArt.ofx.bundle.backup_YYYYMMDD_HHMMSS`) and installs the latest version cleanly.

#### Option 3: Manual Copy
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
3. Navigate to **OpenFX -> Stylize -> ASCII**.
4. Drag the effect onto your video clip or node.
5. Adjust parameters in the Inspector panel, or choose a preset from the **Preset** dropdown.

---

## Parameters

| Parameter | Type | Description |
| :--- | :--- | :--- |
| **Preset** | Dropdown | Curated instant style configurations (Goliath Cyber Lime, Classic Matrix CRT, Full Color Hi-Fi, Pure 1-Bit Terminal, Cyberpunk Neon, CGA Retro PC, ZX Spectrum Vintage, Lo-Fi 12fps Anime Hold). |
| **Character Spacing** | Slider | Size of each ASCII cell in pixels (2 - 200px). |
| **Char Aspect (H/W)** | Slider | Width-to-height ratio of characters (default 1.0 = equal spacing). |
| **Font Source** | Dropdown | Choose between Goliath (bundled), Built-in Monospace, or Custom Font File (.ttf/.otf). |
| **Custom Font File** | File Browser | Path to custom TrueType or OpenType font. |
| **Character Set** | Dropdown | Presets: Goliath (22 visible), Standard, Detailed (70+), Blocks, Dense, Minimal, Binary, or Custom Ramp. |
| **Custom Ramp** | String | User-defined ramp string when Character Set is set to Custom Ramp. |
| **Color Mode** | Dropdown | 12 modes: Original, Cyber Lime, Solid Cyber Lime, Matrix Green (#00FF66), Cyberpunk Neon, CGA Mode (4-Color), ZX Spectrum (16-Color), Mono Green, Mono Amber, Mono White, Pure 1-Bit B&W, and Luminance Grayscale. |
| **CRT Phosphor Glow** | Toggle | Enables authentic CRT phosphor bloom and bleed. |
| **Glow Radius** | Slider | Spread radius of phosphor bloom in pixels (1 - 30). |
| **Glow Intensity** | Slider | Brightness and prominence of phosphor glow. |
| **Glow Blend Mode** | Dropdown | Screen (Soft Bloom) or Additive (Vibrant/Hot). |
| **BG Red / Green / Blue** | Sliders | Background fill color behind characters. |
| **Contrast / Brightness** | Sliders | Pre-render input luminance adjustments. |
| **Invert** | Toggle | Inverts luminance ramp mapping. |
| **Random Characters** | Toggle | Randomizes character glyphs per cell with consistent seed. |
| **Font Scale** | Slider | Scale factor of glyphs inside their grid cells. |
| **Flip Vertical** | Toggle | Inverts vertical orientation if source is flipped in node graph. |
| **Skip Black Areas** | Toggle | Suppresses glyph rendering over shadow/black pixels. |
| **Black Threshold** | Slider | Luminance cutoff below which pixels are treated as transparent background. |
| **Alpha Threshold** | Slider | Transparency cutoff below which pixels are omitted. |
| **Update Every N Frames** | Integer | Frame hold (1 = full rate, 2 = 12fps anime hold, etc.) with RAM caching. |
| **About: ASCII** | Button | Direct link to repository: https://github.com/SKBwastaken/ASCIICreatorOFX |

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
├── Update.bat               Windows 1-click updater with automatic backup
├── Uninstall.bat            Windows uninstaller
├── install.sh               Linux installer script
├── uninstall.sh             Linux uninstaller script
├── Goliath.ttf              Default retro cyber font asset
└── README.md
```

---

## Credits
- Author: SKB (https://discord.com/users/289503943409664000)
