// ASCII Art OFX Plugin for DaVinci Resolve
// Copyright (c) 2026 SKB. All rights reserved.
//
// A filter effect that converts video frames into ASCII art.
// Supports custom fonts (stb_truetype), Goliath font auto-detection,
// and embedded fallback bitmap fonts.

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <ctime>
#include <new>
#include <algorithm>
#include <vector>
#include <string>
#include <cstdint>
#ifdef _OPENMP
#include <omp.h>
#endif

// OpenFX headers
#include "ofxCore.h"
#include "ofxImageEffect.h"
#include "ofxParam.h"
#include "ofxProperty.h"
#include "ofxMemory.h"
#include "ofxMultiThread.h"
#include "ofxMessage.h"
#include "ofxPixels.h"

// stb_truetype for custom font rendering
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

// Fallback bitmap font
#include "glyph_atlas.h"

// ============================================================================
// Plugin identity
// ============================================================================
#define PLUGIN_ID "com.skb.asciiart"
#define PLUGIN_NAME "ASCII"
#define PLUGIN_GROUP "Stylize"
#define PLUGIN_VERSION_MAJOR 1
#define PLUGIN_VERSION_MINOR 3
#define PLUGIN_BUILD_NUMBER  11
#define PLUGIN_VERSION_STR   "v1.3 (Build 11)"

// ============================================================================
// Parameter IDs
// ============================================================================
#define PARAM_PRESET          "preset"
#define PARAM_CHAR_SPACING    "charSpacing"
#define PARAM_CHAR_ASPECT     "charAspect"
#define PARAM_FONT_SOURCE     "fontSource"
#define PARAM_FONT_FILE       "fontFile"
#define PARAM_CHAR_SET        "charSet"
#define PARAM_CUSTOM_CHARS    "customChars"
#define PARAM_COLOR_MODE      "colorMode"
#define PARAM_BG_R            "bgR"
#define PARAM_BG_G            "bgG"
#define PARAM_BG_B            "bgB"
#define PARAM_CONTRAST        "contrast"
#define PARAM_BRIGHTNESS      "brightness"
#define PARAM_INVERT          "invert"
#define PARAM_RANDOM_CHARS    "randomChars"
#define PARAM_FONT_SCALE      "fontScale"
#define PARAM_FLIP_V          "flipVertical"
#define PARAM_SKIP_BLACK      "skipBlack"
#define PARAM_BLACK_CUTOFF    "blackCutoff"
#define PARAM_ALPHA_CUTOFF    "alphaCutoff"
#define PARAM_ENABLE_GLOW     "enableGlow"
#define PARAM_GLOW_RADIUS     "glowRadius"
#define PARAM_GLOW_INTENSITY  "glowIntensity"
#define PARAM_GLOW_BLEND_MODE "glowBlendMode"
#define PARAM_FRAME_HOLD      "frameHold"
#define PARAM_BUILD_INFO      "buildInfo"
#define PARAM_INFO_BUTTON     "infoButton"

// ============================================================================
// Character set presets
// ============================================================================
static const char* RAMP_STANDARD        = " .:-=+*#%@";
static const char* RAMP_GOLIATH         = " 01234dDeE5aAbB679fFcC8";
static const char* RAMP_DETAILED        = " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";
static const char* RAMP_BLOCKS_ASCII    = " .-+#@";
static const char* RAMP_DENSE           = " .,:;i1tfLCG08@";
static const char* RAMP_MINIMAL         = " .:#";
static const char* RAMP_BINARY          = " @";

// ============================================================================
// Export macro
// ============================================================================
#if defined(_WIN32)
  #define EXPORT extern "C" __declspec(dllexport)
#else
  #define EXPORT extern "C" __attribute__((visibility("default")))
#endif

// ============================================================================
// Host suite pointers
// ============================================================================
static OfxHost*                 gHost = nullptr;
static OfxImageEffectSuiteV1*   gEffectSuite = nullptr;
static OfxPropertySuiteV1*      gPropSuite = nullptr;
static OfxParameterSuiteV1*     gParamSuite = nullptr;
static OfxMemorySuiteV1*        gMemorySuite = nullptr;
static OfxMultiThreadSuiteV1*   gThreadSuite = nullptr;
static OfxMessageSuiteV1*       gMessageSuite = nullptr;

// ============================================================================
// Helper: file exists & bundle resource path
// ============================================================================
static bool fileExists(const char* path) {
    if (!path || !path[0]) return false;
    FILE* f = fopen(path, "rb");
    if (f) { fclose(f); return true; }
    return false;
}

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
static std::string getBundleResourcePath(const char* filename) {
    char path[MAX_PATH];
    HMODULE hm = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | 
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)&fileExists, &hm)) {
        if (GetModuleFileNameA(hm, path, sizeof(path))) {
            std::string p(path);
            size_t pos = p.find_last_of("\\/");
            if (pos != std::string::npos) {
                p = p.substr(0, pos); // ...\Contents\Win64
                pos = p.find_last_of("\\/");
                if (pos != std::string::npos) {
                    p = p.substr(0, pos); // ...\Contents
                    std::string resPath = p + "\\Resources\\" + filename;
                    if (fileExists(resPath.c_str())) return resPath;
                }
            }
        }
    }
    return "";
}
#else
#include <dlfcn.h>
#include <unistd.h>
static std::string getBundleResourcePath(const char* filename) {
    Dl_info info;
    if (dladdr((void*)&fileExists, &info) && info.dli_fname) {
        std::string p(info.dli_fname);
        size_t pos = p.find_last_of('/');
        if (pos != std::string::npos) {
            p = p.substr(0, pos); // .../Contents/Linux-x86-64
            pos = p.find_last_of('/');
            if (pos != std::string::npos) {
                p = p.substr(0, pos); // .../Contents
                std::string resPath = p + "/Resources/" + filename;
                if (fileExists(resPath.c_str())) return resPath;
            }
        }
    }
    return "";
}
#endif

// ============================================================================
// Glyph cache for custom fonts (rendered via stb_truetype)
// ============================================================================
struct GlyphBitmap {
    unsigned char* pixels;
    int w, h;
    int xoff, yoff;
    int advance;
};

struct FontCache {
    stbtt_fontinfo fontInfo;
    unsigned char* fontData;
    size_t fontDataSize;
    float scale;
    int cellW, cellH;
    GlyphBitmap glyphs[256];
    bool valid;
    char loadedPath[1024];
    int loadedCellH;
    int ascent, descent, lineGap;

    FontCache() : fontData(nullptr), fontDataSize(0), scale(0), cellW(0), cellH(0),
                  valid(false), loadedCellH(0), ascent(0), descent(0), lineGap(0) {
        memset(glyphs, 0, sizeof(glyphs));
        loadedPath[0] = '\0';
    }

    void clear() {
        for (int i = 0; i < 256; i++) {
            if (glyphs[i].pixels) {
                free(glyphs[i].pixels);
                glyphs[i].pixels = nullptr;
            }
        }
        if (fontData) { free(fontData); fontData = nullptr; }
        valid = false;
        loadedPath[0] = '\0';
        loadedCellH = 0;
    }

    bool loadFont(const char* path, int desiredCellH) {
        if (valid && strcmp(loadedPath, path) == 0 && loadedCellH == desiredCellH)
            return true;

        clear();

        FILE* f = fopen(path, "rb");
        if (!f) return false;

        fseek(f, 0, SEEK_END);
        fontDataSize = (size_t)ftell(f);
        fseek(f, 0, SEEK_SET);

        fontData = (unsigned char*)malloc(fontDataSize);
        if (!fontData) { fclose(f); return false; }

        if (fread(fontData, 1, fontDataSize, f) != fontDataSize) {
            fclose(f); clear(); return false;
        }
        fclose(f);

        if (!stbtt_InitFont(&fontInfo, fontData, 0)) {
            clear(); return false;
        }

        scale = stbtt_ScaleForPixelHeight(&fontInfo, (float)desiredCellH);
        stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);

        int advW = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&fontInfo, '0', &advW, &lsb);
        cellW = (int)ceil(advW * scale);
        if (cellW < 1) cellW = desiredCellH / 2;
        cellH = desiredCellH;

        for (int code = 32; code <= 126; code++) {
            int w = 0, h = 0, xoff = 0, yoff = 0;
            unsigned char* bmp = stbtt_GetCodepointBitmap(
                &fontInfo, 0, scale, code, &w, &h, &xoff, &yoff);
            glyphs[code].pixels = bmp;
            glyphs[code].w = w;
            glyphs[code].h = h;
            glyphs[code].xoff = xoff;
            glyphs[code].yoff = yoff;
            int adv = 0, lb = 0;
            stbtt_GetCodepointHMetrics(&fontInfo, code, &adv, &lb);
            glyphs[code].advance = (int)(adv * scale);
        }

        strncpy(loadedPath, path, sizeof(loadedPath) - 1);
        loadedPath[sizeof(loadedPath) - 1] = '\0';
        loadedCellH = desiredCellH;
        valid = true;
        return true;
    }
};

// ============================================================================
// Instance data
// ============================================================================
struct InstanceData {
    OfxImageClipHandle sourceClip;
    OfxImageClipHandle outputClip;
    OfxParamHandle presetParam;
    OfxParamHandle charSpacingParam;
    OfxParamHandle charAspectParam;
    OfxParamHandle fontSourceParam;
    OfxParamHandle fontFileParam;
    OfxParamHandle charSetParam;
    OfxParamHandle customCharsParam;
    OfxParamHandle colorModeParam;
    OfxParamHandle bgRParam, bgGParam, bgBParam;
    OfxParamHandle contrastParam;
    OfxParamHandle brightnessParam;
    OfxParamHandle invertParam;
    OfxParamHandle randomCharsParam;
    OfxParamHandle fontScaleParam;
    OfxParamHandle flipVParam;
    OfxParamHandle skipBlackParam;
    OfxParamHandle blackCutoffParam;
    OfxParamHandle alphaCutoffParam;
    OfxParamHandle enableGlowParam;
    OfxParamHandle glowRadiusParam;
    OfxParamHandle glowIntensityParam;
    OfxParamHandle glowBlendModeParam;
    OfxParamHandle frameHoldParam;
    FontCache fontCache;
    unsigned char* renderBuffer;
    size_t renderBufferSize;
    unsigned char* glowBuffer;
    size_t glowBufferSize;
    unsigned char* blurBuffer;
    size_t blurBufferSize;
    unsigned char* blurTemp;
    size_t blurTempSize;
    long long lastRenderedFrame;
    bool hasCachedOutput;
    int lastRenderW, lastRenderH, lastRenderNcomp;
};

// ============================================================================
// Helpers & Palettes
// ============================================================================
static InstanceData* getInstanceData(OfxImageEffectHandle effect) {
    OfxPropertySetHandle effectProps;
    gEffectSuite->getPropertySet(effect, &effectProps);
    InstanceData* data = nullptr;
    gPropSuite->propGetPointer(effectProps, kOfxPropInstanceData, 0, (void**)&data);
    return data;
}

static inline int clampI(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline double clampD(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Retro Palettes
static const unsigned char CGA_PALETTE[4][3] = {
    {0, 0, 0},
    {85, 255, 255},
    {255, 85, 255},
    {255, 255, 255}
};

static const unsigned char ZX_SPECTRUM_PALETTE[16][3] = {
    {0,   0,   0},   {0,   0,   192}, {192, 0,   0},   {192, 0,   192},
    {0,   192, 0},   {0,   192, 192}, {192, 192, 0},   {192, 192, 192},
    {0,   0,   0},   {0,   0,   255}, {255, 0,   0},   {255, 0,   255},
    {0,   255, 0},   {0,   255, 255}, {255, 255, 0},   {255, 255, 255}
};

static inline void matchNearestPalette(int r, int g, int b, const unsigned char pal[][3], int count,
                                       unsigned char& outR, unsigned char& outG, unsigned char& outB) {
    int bestDist = 99999999;
    int bestIdx = 0;
    for (int i = 0; i < count; i++) {
        int dr = r - pal[i][0];
        int dg = g - pal[i][1];
        int db = b - pal[i][2];
        int dist = dr * dr + dg * dg + db * db;
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }
    outR = pal[bestIdx][0];
    outG = pal[bestIdx][1];
    outB = pal[bestIdx][2];
}

// Fast separable box blur for real-time phosphor bloom
static void fastBoxBlur(const unsigned char* src, unsigned char* dst, unsigned char* temp,
                        int w, int h, int ncomp, int radius) {
    if (radius <= 0) {
        memcpy(dst, src, (size_t)w * h * ncomp);
        return;
    }
    int div = 2 * radius + 1;

    // Horizontal pass: src -> temp
#if defined(_OPENMP)
    #pragma omp parallel for schedule(static)
#endif
    for (int y = 0; y < h; y++) {
        const unsigned char* srow = src + y * w * ncomp;
        unsigned char* trow = temp + y * w * ncomp;
        int sum[4] = {0, 0, 0, 0};

        for (int i = -radius; i <= radius; i++) {
            int cx = clampI(i, 0, w - 1);
            for (int c = 0; c < ncomp; c++) {
                sum[c] += srow[cx * ncomp + c];
            }
        }

        for (int x = 0; x < w; x++) {
            for (int c = 0; c < ncomp; c++) {
                trow[x * ncomp + c] = (unsigned char)(sum[c] / div);
            }
            int xOut = clampI(x - radius, 0, w - 1);
            int xIn  = clampI(x + radius + 1, 0, w - 1);
            for (int c = 0; c < ncomp; c++) {
                sum[c] += srow[xIn * ncomp + c] - srow[xOut * ncomp + c];
            }
        }
    }

    // Vertical pass: temp -> dst
#if defined(_OPENMP)
    #pragma omp parallel for schedule(static)
#endif
    for (int x = 0; x < w; x++) {
        int sum[4] = {0, 0, 0, 0};

        for (int i = -radius; i <= radius; i++) {
            int cy = clampI(i, 0, h - 1);
            const unsigned char* p = temp + (cy * w + x) * ncomp;
            for (int c = 0; c < ncomp; c++) {
                sum[c] += p[c];
            }
        }

        for (int y = 0; y < h; y++) {
            unsigned char* d = dst + (y * w + x) * ncomp;
            for (int c = 0; c < ncomp; c++) {
                d[c] = (unsigned char)(sum[c] / div);
            }
            int yOut = clampI(y - radius, 0, h - 1);
            int yIn  = clampI(y + radius + 1, 0, h - 1);
            const unsigned char* pIn  = temp + (yIn * w + x) * ncomp;
            const unsigned char* pOut = temp + (yOut * w + x) * ncomp;
            for (int c = 0; c < ncomp; c++) {
                sum[c] += pIn[c] - pOut[c];
            }
        }
    }
}

// ============================================================================
// Draw fallback bitmap glyph (Consolas 8x16)
// ============================================================================
static void drawFallbackGlyph(unsigned char* buf, int bufW, int bufH, int ncomp,
                              int cx, int cy, int cw, int ch_,
                              int charCode, double fscale,
                              unsigned char fR, unsigned char fG, unsigned char fB,
                              unsigned char* glowBuf = nullptr) {
    if (charCode < GLYPH_FIRST || charCode > GLYPH_LAST) return;
    const unsigned char* glyph = FALLBACK_FONT[charCode - GLYPH_FIRST];

    double sx = (double)cw / GLYPH_W * fscale;
    double sy = (double)ch_ / GLYPH_H * fscale;
    int dw = (int)(GLYPH_W * sx);
    int dh = (int)(GLYPH_H * sy);
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    int ox = (cw - dw) / 2;
    int oy = (ch_ - dh) / 2;

    for (int py = 0; py < dh; py++) {
        int srcY = (int)(py / sy);
        if (srcY >= GLYPH_H) srcY = GLYPH_H - 1;
        int outY = cy + oy + py;
        if (outY < cy || outY >= cy + ch_ || outY >= bufH) continue;
        for (int px = 0; px < dw; px++) {
            int srcX = (int)(px / sx);
            if (srcX >= GLYPH_W) srcX = GLYPH_W - 1;
            if (glyph[srcY] & (1 << (7 - srcX))) {
                int outX = cx + ox + px;
                if (outX < cx || outX >= cx + cw || outX >= bufW) continue;
                int idx = (outY * bufW + outX) * ncomp;
                buf[idx + 0] = fR;
                buf[idx + 1] = fG;
                buf[idx + 2] = fB;
                if (ncomp >= 4) buf[idx + 3] = 255;
                if (glowBuf) {
                    glowBuf[idx + 0] = fR;
                    glowBuf[idx + 1] = fG;
                    glowBuf[idx + 2] = fB;
                    if (ncomp >= 4) glowBuf[idx + 3] = 255;
                }
            }
        }
    }
}

// ============================================================================
// Draw stb_truetype glyph
// ============================================================================
static void drawStbGlyph(unsigned char* buf, int bufW, int bufH, int ncomp,
                         int cx, int cy, int cw, int ch_,
                         FontCache& fc, int charCode, double fscale,
                         unsigned char fR, unsigned char fG, unsigned char fB,
                         unsigned char* glowBuf = nullptr) {
    if (charCode < 32 || charCode > 126) return;
    GlyphBitmap& gb = fc.glyphs[charCode];
    if (!gb.pixels || gb.w == 0 || gb.h == 0) return;

    // Center glyph in cell
    int glyphX = cx + (cw - gb.w) / 2;
    int glyphY = cy + (ch_ - gb.h) / 2;

    for (int py = 0; py < gb.h; py++) {
        int outY = glyphY + py;
        if (outY < cy || outY >= cy + ch_ || outY >= bufH) continue;
        for (int px = 0; px < gb.w; px++) {
            int outX = glyphX + px;
            if (outX < cx || outX >= cx + cw || outX >= bufW) continue;
            unsigned char alpha = gb.pixels[py * gb.w + px];
            if (alpha == 0) continue;
            unsigned char* dst = buf + (outY * bufW + outX) * ncomp;
            if (alpha >= 250) {
                dst[0] = fR;
                dst[1] = fG;
                dst[2] = fB;
            } else {
                unsigned int a = alpha;
                unsigned int invA = 255 - a;
                dst[0] = (unsigned char)((fR * a + dst[0] * invA + 127) / 255);
                dst[1] = (unsigned char)((fG * a + dst[1] * invA + 127) / 255);
                dst[2] = (unsigned char)((fB * a + dst[2] * invA + 127) / 255);
            }
            if (ncomp >= 4) dst[3] = 255;

            if (glowBuf) {
                unsigned char* gdst = glowBuf + (outY * bufW + outX) * ncomp;
                if (alpha >= 250) {
                    gdst[0] = fR;
                    gdst[1] = fG;
                    gdst[2] = fB;
                } else {
                    unsigned int a = alpha;
                    gdst[0] = (unsigned char)((fR * a + 127) / 255);
                    gdst[1] = (unsigned char)((fG * a + 127) / 255);
                    gdst[2] = (unsigned char)((fB * a + 127) / 255);
                }
                if (ncomp >= 4) gdst[3] = 255;
            }
        }
    }
}

// ============================================================================
// ACTION: Load
// ============================================================================
static OfxStatus actionLoad() {
    if (!gHost) return kOfxStatErrMissingHostFeature;
    gEffectSuite = (OfxImageEffectSuiteV1*)gHost->fetchSuite(gHost->host, kOfxImageEffectSuite, 1);
    gPropSuite   = (OfxPropertySuiteV1*)gHost->fetchSuite(gHost->host, kOfxPropertySuite, 1);
    gParamSuite  = (OfxParameterSuiteV1*)gHost->fetchSuite(gHost->host, kOfxParameterSuite, 1);
    gMemorySuite = (OfxMemorySuiteV1*)gHost->fetchSuite(gHost->host, kOfxMemorySuite, 1);
    gThreadSuite = (OfxMultiThreadSuiteV1*)gHost->fetchSuite(gHost->host, kOfxMultiThreadSuite, 1);
    gMessageSuite = (OfxMessageSuiteV1*)gHost->fetchSuite(gHost->host, kOfxMessageSuite, 1);
    if (!gEffectSuite || !gPropSuite || !gParamSuite) return kOfxStatErrMissingHostFeature;
    return kOfxStatOK;
}

// ============================================================================
// ACTION: Describe
// ============================================================================
static OfxStatus actionDescribe(OfxImageEffectHandle effect) {
    OfxPropertySetHandle p;
    gEffectSuite->getPropertySet(effect, &p);
    gPropSuite->propSetString(p, kOfxPropLabel, 0, PLUGIN_NAME);
    gPropSuite->propSetString(p, kOfxPropVersionLabel, 0, PLUGIN_VERSION_STR);
    gPropSuite->propSetString(p, kOfxImageEffectPluginPropGrouping, 0, PLUGIN_GROUP);
    gPropSuite->propSetString(p, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);
    gPropSuite->propSetString(p, kOfxImageEffectPropSupportedContexts, 1, kOfxImageEffectContextGeneral);
    gPropSuite->propSetString(p, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthByte);
    gPropSuite->propSetString(p, kOfxImageEffectPluginRenderThreadSafety, 0, kOfxImageEffectRenderFullySafe);
    gPropSuite->propSetInt(p, kOfxImageEffectPropTemporalClipAccess, 0, 0);
    gPropSuite->propSetInt(p, kOfxImageEffectPropSupportsTiles, 0, 0);
    return kOfxStatOK;
}

// ============================================================================
// ACTION: Describe in context
// ============================================================================
static OfxStatus actionDescribeInContext(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs) {
    OfxPropertySetHandle cp;
    gEffectSuite->clipDefine(effect, kOfxImageEffectSimpleSourceClipName, &cp);
    gPropSuite->propSetString(cp, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
    gPropSuite->propSetString(cp, kOfxImageEffectPropSupportedComponents, 1, kOfxImageComponentRGB);

    gEffectSuite->clipDefine(effect, kOfxImageEffectOutputClipName, &cp);
    gPropSuite->propSetString(cp, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
    gPropSuite->propSetString(cp, kOfxImageEffectPropSupportedComponents, 1, kOfxImageComponentRGB);

    OfxParamSetHandle ps;
    gEffectSuite->getParamSet(effect, &ps);
    OfxPropertySetHandle pp;

    // Preset Selection (Quick Curated Looks)
    gParamSuite->paramDefine(ps, kOfxParamTypeChoice, PARAM_PRESET, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Preset");
    gPropSuite->propSetString(pp, kOfxParamPropHint, 0, "Select curated style preset to instantly configure all parameters");
    gPropSuite->propSetInt(pp, kOfxParamPropDefault, 0, 1);
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 0, "Custom / Manual");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 1, "Classic Matrix CRT (Default)");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 2, "Goliath Cyber Lime");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 3, "Full Color Hi-Fi");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 4, "Pure 1-Bit Terminal");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 5, "Cyberpunk Neon");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 6, "CGA Retro PC");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 7, "ZX Spectrum Vintage");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 8, "Lo-Fi 12fps Anime Hold");

    // Character Spacing (the single unified slider for spacing and density!)
    gParamSuite->paramDefine(ps, kOfxParamTypeInteger, PARAM_CHAR_SPACING, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Character Spacing");
    gPropSuite->propSetString(pp, kOfxParamPropHint, 0, "Consistent spacing between characters in pixels (lower = higher resolution, higher = larger characters)");
    gPropSuite->propSetInt(pp, kOfxParamPropDefault, 0, 25);
    gPropSuite->propSetInt(pp, kOfxParamPropMin, 0, 2);
    gPropSuite->propSetInt(pp, kOfxParamPropMax, 0, 300);
    gPropSuite->propSetInt(pp, kOfxParamPropDisplayMin, 0, 2);
    gPropSuite->propSetInt(pp, kOfxParamPropDisplayMax, 0, 200);

    // Char Aspect (default 1.0 = equal horizontal and vertical spacing!)
    gParamSuite->paramDefine(ps, kOfxParamTypeDouble, PARAM_CHAR_ASPECT, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Char Aspect (H/W)");
    gPropSuite->propSetString(pp, kOfxParamPropHint, 0, "1.0 = equal spacing in all directions. Adjust only if you want stretched or tall cells.");
    gPropSuite->propSetDouble(pp, kOfxParamPropDefault, 0, 1.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropMin, 0, 0.2);
    gPropSuite->propSetDouble(pp, kOfxParamPropMax, 0, 3.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropDisplayMin, 0, 0.5);
    gPropSuite->propSetDouble(pp, kOfxParamPropDisplayMax, 0, 2.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropIncrement, 0, 0.05);

    // Font Source Dropdown
    gParamSuite->paramDefine(ps, kOfxParamTypeChoice, PARAM_FONT_SOURCE, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Font Source");
    gPropSuite->propSetInt(pp, kOfxParamPropDefault, 0, 3);
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 0, "Goliath Encrypted (Auto-detect)");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 1, "Consolas Monospace (Bundled)");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 2, "Built-in Monospace");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 3, "Custom Font File (Browse...)");

    // Font File Path (with native file picker!)
    gParamSuite->paramDefine(ps, kOfxParamTypeString, PARAM_FONT_FILE, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Custom Font File");
    gPropSuite->propSetString(pp, kOfxParamPropHint, 0, "Select a .ttf or .otf font file");
    gPropSuite->propSetString(pp, kOfxParamPropStringMode, 0, kOfxParamStringIsFilePath);
    gPropSuite->propSetInt(pp, kOfxParamPropStringFilePathExists, 0, 1);
    gPropSuite->propSetString(pp, kOfxParamPropDefault, 0, "C:\\Users\\SKB\\Downloads\\Consolas-Regular.ttf");

    // Character Set
    gParamSuite->paramDefine(ps, kOfxParamTypeChoice, PARAM_CHAR_SET, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Character Set");
    gPropSuite->propSetInt(pp, kOfxParamPropDefault, 0, 2);
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 0, "Goliath (22 Visible Glyphs)");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 1, "Standard (.:-=+*#%@)");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 2, "Detailed (70+ Chars)");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 3, "Blocks (.-+#@)");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 4, "Dense");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 5, "Minimal (.:#)");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 6, "Binary (@)");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 7, "Custom Ramp");

    // Custom Characters
    gParamSuite->paramDefine(ps, kOfxParamTypeString, PARAM_CUSTOM_CHARS, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Custom Ramp (if set to Custom)");
    gPropSuite->propSetString(pp, kOfxParamPropHint, 0, "Only used when Character Set is set to 'Custom Ramp'");
    gPropSuite->propSetString(pp, kOfxParamPropDefault, 0, " .:-=+*#%@");

    // Color Mode (Expanded retro palettes)
    gParamSuite->paramDefine(ps, kOfxParamTypeChoice, PARAM_COLOR_MODE, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Color Mode");
    gPropSuite->propSetInt(pp, kOfxParamPropDefault, 0, 0);
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 0, "Original Colors");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 1, "Cyber Lime (#C2FD04) on Black");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 2, "Solid Cyber Lime (Punchy Neon)");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 3, "Matrix Green (#00FF66)");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 4, "Cyberpunk Neon (Cyan/Pink)");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 5, "CGA Mode (4-Color Retro)");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 6, "ZX Spectrum 16-Color");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 7, "Mono Green (Matrix)");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 8, "Mono Amber");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 9, "Mono White");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 10, "Pure 1-Bit (Strict B&W Binary)");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 11, "Luminance Grayscale");

    // CRT Phosphor Glow
    gParamSuite->paramDefine(ps, kOfxParamTypeBoolean, PARAM_ENABLE_GLOW, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "CRT Phosphor Glow");
    gPropSuite->propSetString(pp, kOfxParamPropHint, 0, "Enable authentic CRT phosphor bloom and soft bleeding");
    gPropSuite->propSetInt(pp, kOfxParamPropDefault, 0, 1);

    gParamSuite->paramDefine(ps, kOfxParamTypeInteger, PARAM_GLOW_RADIUS, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Glow Radius");
    gPropSuite->propSetString(pp, kOfxParamPropHint, 0, "Spread radius of phosphor glow in pixels");
    gPropSuite->propSetInt(pp, kOfxParamPropDefault, 0, 8);
    gPropSuite->propSetInt(pp, kOfxParamPropMin, 0, 1);
    gPropSuite->propSetInt(pp, kOfxParamPropMax, 0, 30);
    gPropSuite->propSetInt(pp, kOfxParamPropDisplayMin, 0, 1);
    gPropSuite->propSetInt(pp, kOfxParamPropDisplayMax, 0, 20);

    gParamSuite->paramDefine(ps, kOfxParamTypeDouble, PARAM_GLOW_INTENSITY, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Glow Intensity");
    gPropSuite->propSetString(pp, kOfxParamPropHint, 0, "Brightness and strength of phosphor glow");
    gPropSuite->propSetDouble(pp, kOfxParamPropDefault, 0, 0.85);
    gPropSuite->propSetDouble(pp, kOfxParamPropMin, 0, 0.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropMax, 0, 2.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropDisplayMin, 0, 0.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropDisplayMax, 0, 1.5);

    gParamSuite->paramDefine(ps, kOfxParamTypeChoice, PARAM_GLOW_BLEND_MODE, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Glow Blend Mode");
    gPropSuite->propSetString(pp, kOfxParamPropHint, 0, "Screen for soft phosphor bloom, Additive for intense neon saturation");
    gPropSuite->propSetInt(pp, kOfxParamPropDefault, 0, 0);
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 0, "Screen (Soft Bloom)");
    gPropSuite->propSetString(pp, kOfxParamPropChoiceOption, 1, "Additive (Vibrant/Hot)");

    // Background R/G/B
    gParamSuite->paramDefine(ps, kOfxParamTypeDouble, PARAM_BG_R, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "BG Red");
    gPropSuite->propSetDouble(pp, kOfxParamPropDefault, 0, 0.01);
    gPropSuite->propSetDouble(pp, kOfxParamPropMin, 0, 0.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropMax, 0, 1.0);

    gParamSuite->paramDefine(ps, kOfxParamTypeDouble, PARAM_BG_G, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "BG Green");
    gPropSuite->propSetDouble(pp, kOfxParamPropDefault, 0, 0.03);
    gPropSuite->propSetDouble(pp, kOfxParamPropMin, 0, 0.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropMax, 0, 1.0);

    gParamSuite->paramDefine(ps, kOfxParamTypeDouble, PARAM_BG_B, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "BG Blue");
    gPropSuite->propSetDouble(pp, kOfxParamPropDefault, 0, 0.01);
    gPropSuite->propSetDouble(pp, kOfxParamPropMin, 0, 0.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropMax, 0, 1.0);

    // Contrast
    gParamSuite->paramDefine(ps, kOfxParamTypeDouble, PARAM_CONTRAST, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Contrast");
    gPropSuite->propSetDouble(pp, kOfxParamPropDefault, 0, 30.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropMin, 0, -100.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropMax, 0, 100.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropDisplayMin, 0, -100.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropDisplayMax, 0, 100.0);

    // Brightness
    gParamSuite->paramDefine(ps, kOfxParamTypeDouble, PARAM_BRIGHTNESS, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Brightness");
    gPropSuite->propSetDouble(pp, kOfxParamPropDefault, 0, 10.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropMin, 0, -100.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropMax, 0, 100.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropDisplayMin, 0, -100.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropDisplayMax, 0, 100.0);

    // Invert
    gParamSuite->paramDefine(ps, kOfxParamTypeBoolean, PARAM_INVERT, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Invert");
    gPropSuite->propSetInt(pp, kOfxParamPropDefault, 0, 0);

    // Random Characters
    gParamSuite->paramDefine(ps, kOfxParamTypeBoolean, PARAM_RANDOM_CHARS, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Random Characters");
    gPropSuite->propSetInt(pp, kOfxParamPropDefault, 0, 0);

    // Font Scale
    gParamSuite->paramDefine(ps, kOfxParamTypeDouble, PARAM_FONT_SCALE, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Font Scale");
    gPropSuite->propSetString(pp, kOfxParamPropHint, 0, "Scale of glyph within cell");
    gPropSuite->propSetDouble(pp, kOfxParamPropDefault, 0, 1.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropMin, 0, 0.3);
    gPropSuite->propSetDouble(pp, kOfxParamPropMax, 0, 2.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropDisplayMin, 0, 0.3);
    gPropSuite->propSetDouble(pp, kOfxParamPropDisplayMax, 0, 2.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropIncrement, 0, 0.01);

    // Flip Vertical
    gParamSuite->paramDefine(ps, kOfxParamTypeBoolean, PARAM_FLIP_V, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Flip Vertical");
    gPropSuite->propSetString(pp, kOfxParamPropHint, 0, "Flip image vertically if upside-down in node graph");
    gPropSuite->propSetInt(pp, kOfxParamPropDefault, 0, 0);

    // Skip Black Areas (default ON!)
    gParamSuite->paramDefine(ps, kOfxParamTypeBoolean, PARAM_SKIP_BLACK, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Skip Black Areas");
    gPropSuite->propSetString(pp, kOfxParamPropHint, 0, "Completely skip black / shadow areas so no faint symbols appear");
    gPropSuite->propSetInt(pp, kOfxParamPropDefault, 0, 1);

    // Black Cutoff Threshold
    gParamSuite->paramDefine(ps, kOfxParamTypeDouble, PARAM_BLACK_CUTOFF, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Black Threshold (0-100)");
    gPropSuite->propSetString(pp, kOfxParamPropHint, 0, "Luminance threshold below which pixels are treated as empty background");
    gPropSuite->propSetDouble(pp, kOfxParamPropDefault, 0, 15.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropMin, 0, 0.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropMax, 0, 100.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropDisplayMin, 0, 0.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropDisplayMax, 0, 60.0);

    // Alpha Cutoff Threshold
    gParamSuite->paramDefine(ps, kOfxParamTypeDouble, PARAM_ALPHA_CUTOFF, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Alpha Threshold (0-100%)");
    gPropSuite->propSetString(pp, kOfxParamPropHint, 0, "Skip pixels with transparency below this percentage");
    gPropSuite->propSetDouble(pp, kOfxParamPropDefault, 0, 10.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropMin, 0, 0.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropMax, 0, 100.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropDisplayMin, 0, 0.0);
    gPropSuite->propSetDouble(pp, kOfxParamPropDisplayMax, 0, 100.0);

    // Frame Hold (Update Every N Frames)
    gParamSuite->paramDefine(ps, kOfxParamTypeInteger, PARAM_FRAME_HOLD, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "Update Every N Frames");
    gPropSuite->propSetString(pp, kOfxParamPropHint, 0, "1 = every frame (full speed), 2 = hold 2 frames, 3 = 1/3 rate. Massively boosts playback speed and gives clean retro look!");
    gPropSuite->propSetInt(pp, kOfxParamPropDefault, 0, 1);
    gPropSuite->propSetInt(pp, kOfxParamPropMin, 0, 1);
    gPropSuite->propSetInt(pp, kOfxParamPropMax, 0, 30);
    gPropSuite->propSetInt(pp, kOfxParamPropDisplayMin, 0, 1);
    gPropSuite->propSetInt(pp, kOfxParamPropDisplayMax, 0, 12);

    // About / Info Push Button (Opens GitHub)
    gParamSuite->paramDefine(ps, kOfxParamTypePushButton, PARAM_INFO_BUTTON, &pp);
    gPropSuite->propSetString(pp, kOfxPropLabel, 0, "ASCII " PLUGIN_VERSION_STR " - GitHub");
    gPropSuite->propSetString(pp, kOfxParamPropHint, 0, "Visit https://github.com/SKBwastaken/ASCIICreatorOFX");

    return kOfxStatOK;
}

// ============================================================================
// ACTION: Create / Destroy instance
// ============================================================================
static OfxStatus actionCreateInstance(OfxImageEffectHandle effect) {
    InstanceData* d = new(std::nothrow) InstanceData();
    if (!d) return kOfxStatFailed;
    memset(d, 0, sizeof(InstanceData));
    new (&d->fontCache) FontCache();

    gEffectSuite->clipGetHandle(effect, kOfxImageEffectSimpleSourceClipName, &d->sourceClip, nullptr);
    gEffectSuite->clipGetHandle(effect, kOfxImageEffectOutputClipName, &d->outputClip, nullptr);

    OfxParamSetHandle ps;
    gEffectSuite->getParamSet(effect, &ps);
    gParamSuite->paramGetHandle(ps, PARAM_PRESET, &d->presetParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_CHAR_SPACING, &d->charSpacingParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_CHAR_ASPECT, &d->charAspectParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_FONT_SOURCE, &d->fontSourceParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_FONT_FILE, &d->fontFileParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_CHAR_SET, &d->charSetParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_CUSTOM_CHARS, &d->customCharsParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_COLOR_MODE, &d->colorModeParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_BG_R, &d->bgRParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_BG_G, &d->bgGParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_BG_B, &d->bgBParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_CONTRAST, &d->contrastParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_BRIGHTNESS, &d->brightnessParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_INVERT, &d->invertParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_RANDOM_CHARS, &d->randomCharsParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_FONT_SCALE, &d->fontScaleParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_FLIP_V, &d->flipVParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_SKIP_BLACK, &d->skipBlackParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_BLACK_CUTOFF, &d->blackCutoffParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_ALPHA_CUTOFF, &d->alphaCutoffParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_ENABLE_GLOW, &d->enableGlowParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_GLOW_RADIUS, &d->glowRadiusParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_GLOW_INTENSITY, &d->glowIntensityParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_GLOW_BLEND_MODE, &d->glowBlendModeParam, nullptr);
    gParamSuite->paramGetHandle(ps, PARAM_FRAME_HOLD, &d->frameHoldParam, nullptr);
    d->lastRenderedFrame = -999999;
    d->hasCachedOutput = false;
    d->lastRenderW = 0;
    d->lastRenderH = 0;
    d->lastRenderNcomp = 0;

    OfxPropertySetHandle ep;
    gEffectSuite->getPropertySet(effect, &ep);
    gPropSuite->propSetPointer(ep, kOfxPropInstanceData, 0, (void*)d);
    return kOfxStatOK;
}

static OfxStatus actionDestroyInstance(OfxImageEffectHandle effect) {
    InstanceData* d = getInstanceData(effect);
    if (d) {
        d->fontCache.clear();
        if (d->renderBuffer) { free(d->renderBuffer); d->renderBuffer = nullptr; }
        if (d->glowBuffer) { free(d->glowBuffer); d->glowBuffer = nullptr; }
        if (d->blurBuffer) { free(d->blurBuffer); d->blurBuffer = nullptr; }
        if (d->blurTemp) { free(d->blurTemp); d->blurTemp = nullptr; }
        delete d;
    }
    return kOfxStatOK;
}

// ============================================================================
// ACTION: Render
// ============================================================================
static OfxStatus actionRender(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs) {
    InstanceData* d = getInstanceData(effect);
    if (!d) return kOfxStatFailed;

    OfxTime time;
    gPropSuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);

    // Read parameters
    int charSpacing; gParamSuite->paramGetValueAtTime(d->charSpacingParam, time, &charSpacing);
    double charAspect; gParamSuite->paramGetValueAtTime(d->charAspectParam, time, &charAspect);
    int fontSource; gParamSuite->paramGetValueAtTime(d->fontSourceParam, time, &fontSource);
    char* fontPath = nullptr; gParamSuite->paramGetValueAtTime(d->fontFileParam, time, &fontPath);
    int charSetIdx; gParamSuite->paramGetValueAtTime(d->charSetParam, time, &charSetIdx);
    char* customChars = nullptr; gParamSuite->paramGetValueAtTime(d->customCharsParam, time, &customChars);
    int colorMode; gParamSuite->paramGetValueAtTime(d->colorModeParam, time, &colorMode);
    double bgR, bgG, bgB;
    gParamSuite->paramGetValueAtTime(d->bgRParam, time, &bgR);
    gParamSuite->paramGetValueAtTime(d->bgGParam, time, &bgG);
    gParamSuite->paramGetValueAtTime(d->bgBParam, time, &bgB);
    double contrast; gParamSuite->paramGetValueAtTime(d->contrastParam, time, &contrast);
    double brightness; gParamSuite->paramGetValueAtTime(d->brightnessParam, time, &brightness);
    int invertLum; gParamSuite->paramGetValueAtTime(d->invertParam, time, &invertLum);
    int randomCh; gParamSuite->paramGetValueAtTime(d->randomCharsParam, time, &randomCh);
    double fontScale; gParamSuite->paramGetValueAtTime(d->fontScaleParam, time, &fontScale);
    int flipV; gParamSuite->paramGetValueAtTime(d->flipVParam, time, &flipV);
    int skipBlack = 1; if (d->skipBlackParam) gParamSuite->paramGetValueAtTime(d->skipBlackParam, time, &skipBlack);
    double blackCutoff = 15.0; if (d->blackCutoffParam) gParamSuite->paramGetValueAtTime(d->blackCutoffParam, time, &blackCutoff);
    double alphaCutoff = 10.0; if (d->alphaCutoffParam) gParamSuite->paramGetValueAtTime(d->alphaCutoffParam, time, &alphaCutoff);
    int enableGlow = 0; if (d->enableGlowParam) gParamSuite->paramGetValueAtTime(d->enableGlowParam, time, &enableGlow);
    int glowRadius = 6; if (d->glowRadiusParam) gParamSuite->paramGetValueAtTime(d->glowRadiusParam, time, &glowRadius);
    double glowIntensity = 0.60; if (d->glowIntensityParam) gParamSuite->paramGetValueAtTime(d->glowIntensityParam, time, &glowIntensity);
    int glowBlendMode = 0; if (d->glowBlendModeParam) gParamSuite->paramGetValueAtTime(d->glowBlendModeParam, time, &glowBlendMode);
    int frameHold = 1; if (d->frameHoldParam) gParamSuite->paramGetValueAtTime(d->frameHoldParam, time, &frameHold);
    if (frameHold < 1) frameHold = 1;

    long long currentFrame = (long long)floor(time + 0.0001);
    long long heldFrame = (frameHold > 1) ? ((currentFrame / frameHold) * frameHold) : currentFrame;

    // Fetch clips
    OfxPropertySetHandle srcImg = nullptr, dstImg = nullptr;
    if (gEffectSuite->clipGetImage(d->sourceClip, time, nullptr, &srcImg) != kOfxStatOK || !srcImg)
        return kOfxStatFailed;
    if (gEffectSuite->clipGetImage(d->outputClip, time, nullptr, &dstImg) != kOfxStatOK || !dstImg) {
        gEffectSuite->clipReleaseImage(srcImg);
        return kOfxStatFailed;
    }

    void* srcData = nullptr; void* dstData = nullptr;
    gPropSuite->propGetPointer(srcImg, kOfxImagePropData, 0, &srcData);
    gPropSuite->propGetPointer(dstImg, kOfxImagePropData, 0, &dstData);
    int srcStride, dstStride;
    gPropSuite->propGetInt(srcImg, kOfxImagePropRowBytes, 0, &srcStride);
    gPropSuite->propGetInt(dstImg, kOfxImagePropRowBytes, 0, &dstStride);
    OfxRectI srcB, dstB;
    gPropSuite->propGetIntN(srcImg, kOfxImagePropBounds, 4, &srcB.x1);
    gPropSuite->propGetIntN(dstImg, kOfxImagePropBounds, 4, &dstB.x1);

    char* compStr = nullptr;
    gPropSuite->propGetString(srcImg, kOfxImageEffectPropComponents, 0, &compStr);
    int ncomp = 4;
    if (compStr && strcmp(compStr, kOfxImageComponentRGB) == 0) ncomp = 3;

    int imgW = srcB.x2 - srcB.x1, imgH = srcB.y2 - srcB.y1;
    int dstW = dstB.x2 - dstB.x1, dstH = dstB.y2 - dstB.y1;

    if (imgW <= 0 || imgH <= 0 || dstW <= 0 || dstH <= 0) {
        gEffectSuite->clipReleaseImage(srcImg); gEffectSuite->clipReleaseImage(dstImg);
        return kOfxStatOK;
    }

    // Fast Cache Bypass: if holding frames and we already have this held frame rendered, copy instantly!
    bool canUseCached = (frameHold > 1) && d->hasCachedOutput && (d->lastRenderedFrame == heldFrame)
                        && (d->lastRenderW == dstW) && (d->lastRenderH == dstH)
                        && (d->lastRenderNcomp == ncomp) && d->renderBuffer;

    if (canUseCached) {
        unsigned char* dstBase = (unsigned char*)dstData;
#if defined(_OPENMP)
        #pragma omp parallel for schedule(static)
#endif
        for (int y = 0; y < dstH; y++) {
            memcpy(dstBase + y * dstStride, d->renderBuffer + y * dstW * ncomp, dstW * ncomp);
        }
        gEffectSuite->clipReleaseImage(srcImg);
        gEffectSuite->clipReleaseImage(dstImg);
        return kOfxStatOK;
    }

    // Determine character ramp
    const char* ramp = RAMP_DETAILED;
    switch (charSetIdx) {
        case 0: ramp = RAMP_GOLIATH; break;
        case 1: ramp = RAMP_STANDARD; break;
        case 2: ramp = RAMP_DETAILED; break;
        case 3: ramp = RAMP_BLOCKS_ASCII; break;
        case 4: ramp = RAMP_DENSE; break;
        case 5: ramp = RAMP_MINIMAL; break;
        case 6: ramp = RAMP_BINARY; break;
        case 7: ramp = (customChars && strlen(customChars) > 0) ? customChars : RAMP_STANDARD; break;
        default: ramp = RAMP_DETAILED; break;
    }
    int rampLen = (int)strlen(ramp);
    if (rampLen < 1) { ramp = RAMP_DETAILED; rampLen = (int)strlen(ramp); }

    // Non-space pool for random mode
    char nsPool[256]; int nsCount = 0;
    for (int i = 0; i < rampLen && nsCount < 255; i++)
        if (ramp[i] != ' ') nsPool[nsCount++] = ramp[i];
    if (nsCount == 0) { nsPool[0] = '@'; nsCount = 1; }

    // Calculate grid columns and rows from Character Spacing (uniform pixel spacing!)
    int spacing = clampI(charSpacing, 2, 300);
    int cols = dstW / spacing;
    if (cols < 4) cols = 4;

    double asp = (charAspect < 0.1) ? 1.0 : charAspect;
    int rows = (int)(0.5 + dstH / (spacing * asp));
    if (rows < 4) rows = 4;

    // Font selection logic
    bool useCustom = false;
    std::string fontToLoad = "";

    if (fontSource == 3 && fontPath && strlen(fontPath) > 0) {
        // User explicitly specified custom font path
        std::string cp = fontPath;
        if (cp.size() >= 2 && cp.front() == '"' && cp.back() == '"') {
            cp = cp.substr(1, cp.size() - 2);
        }
        if (fileExists(cp.c_str())) {
            fontToLoad = cp;
        }
    }

    if (fontSource == 1 || (fontSource == 3 && fontToLoad.empty())) {
        // Consolas (Bundled resource, Downloads, or Windows Fonts)
        std::string resFont = getBundleResourcePath("Consolas-Regular.ttf");
        if (!resFont.empty()) {
            fontToLoad = resFont;
        } else if (fileExists("C:\\Users\\SKB\\Downloads\\Consolas-Regular.ttf")) {
            fontToLoad = "C:\\Users\\SKB\\Downloads\\Consolas-Regular.ttf";
        } else if (fileExists("C:\\Windows\\Fonts\\consola.ttf")) {
            fontToLoad = "C:\\Windows\\Fonts\\consola.ttf";
        }
    } else if (fontSource == 0) {
        // Goliath Auto-load (checks Bundle Resources, then Desktop, then Windows Fonts)
#ifdef _WIN32
        std::string resFont = getBundleResourcePath("Goliath.ttf");
        if (!resFont.empty()) {
            fontToLoad = resFont;
        } else
#endif
        if (fileExists("C:\\Users\\SKB\\Desktop\\Goliath.ttf")) {
            fontToLoad = "C:\\Users\\SKB\\Desktop\\Goliath.ttf";
        } else if (fileExists("C:\\Users\\SKB\\AppData\\Local\\Microsoft\\Windows\\Fonts\\Cypher.ttf")) {
            fontToLoad = "C:\\Users\\SKB\\AppData\\Local\\Microsoft\\Windows\\Fonts\\Cypher.ttf";
        }
    }

    int avgCellH = dstH / rows;
    if (avgCellH < 4) avgCellH = 4;
    int desH = (int)(avgCellH * fontScale);
    if (desH < 4) desH = 4;

    if (!fontToLoad.empty()) {
        useCustom = d->fontCache.loadFont(fontToLoad.c_str(), desH);
    }

    // Contrast factor
    double factor = (259.0 * (contrast + 255.0)) / (255.0 * (259.0 - contrast));
    if (randomCh) srand((unsigned int)(time * 1000.0));

    unsigned char bgRb = (unsigned char)clampI((int)(bgR * 255.0), 0, 255);
    unsigned char bgGb = (unsigned char)clampI((int)(bgG * 255.0), 0, 255);
    unsigned char bgBb = (unsigned char)clampI((int)(bgB * 255.0), 0, 255);

    // Contiguous reusable render buffer (zero allocations during playback)
    size_t reqSize = (size_t)dstW * dstH * ncomp;
    if (!d->renderBuffer || d->renderBufferSize < reqSize) {
        if (d->renderBuffer) free(d->renderBuffer);
        d->renderBufferSize = reqSize;
        d->renderBuffer = (unsigned char*)malloc(reqSize);
        if (!d->renderBuffer) {
            gEffectSuite->clipReleaseImage(srcImg);
            gEffectSuite->clipReleaseImage(dstImg);
            return kOfxStatFailed;
        }
    }
    unsigned char* rb = d->renderBuffer;

    // Fast parallel background fill
    if (ncomp >= 4) {
        uint32_t bgPixel = ((uint32_t)255 << 24) | ((uint32_t)bgBb << 16) | ((uint32_t)bgGb << 8) | (uint32_t)bgRb;
        uint32_t* rb32 = (uint32_t*)rb;
        int totalPix = dstW * dstH;
#if defined(_OPENMP)
        #pragma omp parallel for schedule(static)
#endif
        for (int i = 0; i < totalPix; i++) {
            rb32[i] = bgPixel;
        }
    } else {
#if defined(_OPENMP)
        #pragma omp parallel for schedule(static)
#endif
        for (int y = 0; y < dstH; y++) {
            unsigned char* row = rb + y * dstW * ncomp;
            for (int x = 0; x < dstW; x++) {
                row[x * 3 + 0] = bgRb;
                row[x * 3 + 1] = bgGb;
                row[x * 3 + 2] = bgBb;
            }
        }
    }

    // Ensure glow emission buffer is allocated and cleared to 0 (isolated from background)
    if (enableGlow) {
        if (!d->glowBuffer || d->glowBufferSize < reqSize) {
            if (d->glowBuffer) free(d->glowBuffer);
            d->glowBufferSize = reqSize;
            d->glowBuffer = (unsigned char*)malloc(reqSize);
        }
        if (d->glowBuffer) {
            memset(d->glowBuffer, 0, reqSize);
        }
    }

    // Process grid in parallel across CPU cores
    unsigned char* srcBase = (unsigned char*)srcData;

#if defined(_OPENMP)
    #pragma omp parallel for schedule(dynamic, 4)
#endif
    for (int gr = 0; gr < rows; gr++) {
        int y0 = (gr * dstH) / rows;
        int y1 = ((gr + 1) * dstH) / rows;
        int cellH = y1 - y0;

        int sampleGr = flipV ? (rows - 1 - gr) : gr;
        int sY = clampI((int)(((sampleGr + 0.5) * imgH) / rows), 0, imgH - 1);
        unsigned char* srcRow = srcBase + sY * srcStride;

        for (int gc = 0; gc < cols; gc++) {
            int x0 = (gc * dstW) / cols;
            int x1 = ((gc + 1) * dstW) / cols;
            int cellW = x1 - x0;

            int sX = clampI((int)(((gc + 0.5) * imgW) / cols), 0, imgW - 1);
            unsigned char* sp = srcRow + sX * ncomp;

            int red = sp[0], green = sp[1], blue = sp[2];
            int alpha = (ncomp >= 4) ? sp[3] : 255;
            int alphaThreshold = (int)(alphaCutoff * 2.55);
            if (alpha <= alphaThreshold) continue;

            double aR = clampD(factor * (red - 128.0) + 128.0 + brightness, 0.0, 255.0);
            double aG = clampD(factor * (green - 128.0) + 128.0 + brightness, 0.0, 255.0);
            double aB = clampD(factor * (blue - 128.0) + 128.0 + brightness, 0.0, 255.0);

            double lum = 0.299 * aR + 0.587 * aG + 0.114 * aB;
            if (invertLum) lum = 255.0 - lum;
            if (skipBlack && lum <= blackCutoff) continue;

            // Re-normalize luminance above blackCutoff so characters ramp smoothly from cutoff to 255
            double effectiveMin = skipBlack ? blackCutoff : 0.0;
            double range = 255.0 - effectiveMin;
            if (range < 1.0) range = 1.0;
            double nL = clampD((lum - effectiveMin) / range, 0.0, 1.0);

            char ch;
            if (randomCh) {
                unsigned int h = (unsigned int)(heldFrame * 1000) ^ ((unsigned int)gr * 7919) ^ ((unsigned int)gc * 313);
                h = (h ^ 61) ^ (h >> 16);
                h = h + (h << 3);
                h = h ^ (h >> 4);
                h = h * 0x27d4eb2d;
                h = h ^ (h >> 15);
                ch = nsPool[h % nsCount];
            } else {
                int ci = (int)(nL * rampLen);
                if (ci >= rampLen) ci = rampLen - 1;
                ch = ramp[ci];
            }
            if (ch == ' ') continue;

            unsigned char fR, fG, fB;
            switch (colorMode) {
                case 0: // Original Colors
                    fR = (unsigned char)red; fG = (unsigned char)green; fB = (unsigned char)blue;
                    break;
                case 1: // Cyber Lime (#C2FD04 -> RGB 194, 253, 4 with luminance brightness)
                    fR = (unsigned char)clampI((int)(194.0 * (lum / 255.0)), 0, 255);
                    fG = (unsigned char)clampI((int)(253.0 * (lum / 255.0)), 0, 255);
                    fB = (unsigned char)clampI((int)(4.0   * (lum / 255.0)), 0, 255);
                    break;
                case 2: // Solid Cyber Lime (punchy neon glow without dimming)
                    fR = 194; fG = 253; fB = 4;
                    break;
                case 3: // Matrix Green (#00FF66 -> RGB 0, 255, 102)
                    fR = 0;
                    fG = (unsigned char)clampI((int)(255.0 * (lum / 255.0)), 0, 255);
                    fB = (unsigned char)clampI((int)(102.0 * (lum / 255.0)), 0, 255);
                    break;
                case 4: // Cyberpunk Neon (Cyan/Pink gradient based on luminance)
                    {
                        double t = lum / 255.0;
                        fR = (unsigned char)clampI((int)(255.0 * t), 0, 255);
                        fG = (unsigned char)clampI((int)(240.0 * (1.0 - t * 0.7)), 0, 255);
                        fB = (unsigned char)clampI((int)(255.0 * (0.8 + 0.2 * t)), 0, 255);
                    }
                    break;
                case 5: // CGA Mode (4-Color Retro)
                    matchNearestPalette(red, green, blue, CGA_PALETTE, 4, fR, fG, fB);
                    break;
                case 6: // ZX Spectrum 16-Color
                    matchNearestPalette(red, green, blue, ZX_SPECTRUM_PALETTE, 16, fR, fG, fB);
                    break;
                case 7: // Mono Green (Matrix)
                    fR = 0;
                    fG = (unsigned char)clampI((int)(lum * 0.9), 0, 230);
                    fB = 0;
                    break;
                case 8: // Mono Amber
                    fR = (unsigned char)clampI((int)lum, 0, 255);
                    fG = (unsigned char)clampI((int)(lum * 0.6), 0, 153);
                    fB = 0;
                    break;
                case 9: // Mono White
                    fR = fG = fB = (unsigned char)clampI((int)lum, 0, 255);
                    break;
                case 10: // Pure 1-Bit (Strict B&W Binary)
                    {
                        unsigned char val = (lum > 127.0) ? 255 : 0;
                        fR = fG = fB = val;
                    }
                    break;
                case 11: // Luminance Grayscale
                default:
                    fR = fG = fB = (unsigned char)clampI((int)lum, 0, 255);
                    break;
            }

            if (useCustom) {
                drawStbGlyph(rb, dstW, dstH, ncomp, x0, y0, cellW, cellH,
                             d->fontCache, (int)ch, fontScale, fR, fG, fB,
                             enableGlow ? d->glowBuffer : nullptr);
            } else {
                drawFallbackGlyph(rb, dstW, dstH, ncomp, x0, y0, cellW, cellH,
                                  (int)ch, fontScale, fR, fG, fB,
                                  enableGlow ? d->glowBuffer : nullptr);
            }
        }
    }

    // CRT Phosphor Glow Bloom pass (emanates ONLY from characters, NOT the entire background!)
    if (enableGlow && glowRadius > 0 && glowIntensity > 0.01 && d->glowBuffer) {
        if (!d->blurBuffer || d->blurBufferSize < reqSize) {
            if (d->blurBuffer) free(d->blurBuffer);
            d->blurBufferSize = reqSize;
            d->blurBuffer = (unsigned char*)malloc(reqSize);
        }
        if (!d->blurTemp || d->blurTempSize < reqSize) {
            if (d->blurTemp) free(d->blurTemp);
            d->blurTempSize = reqSize;
            d->blurTemp = (unsigned char*)malloc(reqSize);
        }
        if (d->blurBuffer && d->blurTemp) {
            fastBoxBlur(d->glowBuffer, d->blurBuffer, d->blurTemp, dstW, dstH, ncomp, glowRadius);

            double intensity = glowIntensity;
            int totalPixels = dstW * dstH;

#if defined(_OPENMP)
            #pragma omp parallel for schedule(static)
#endif
            for (int i = 0; i < totalPixels; i++) {
                int pxOffset = i * ncomp;
                int b0 = d->blurBuffer[pxOffset + 0];
                int b1 = d->blurBuffer[pxOffset + 1];
                int b2 = d->blurBuffer[pxOffset + 2];
                if ((b0 | b1 | b2) == 0) continue;

                for (int c = 0; c < 3; c++) {
                    int base = rb[pxOffset + c];
                    int bloom = (int)(d->blurBuffer[pxOffset + c] * intensity);
                    if (bloom <= 0) continue;

                    if (glowBlendMode == 1) {
                        // Additive
                        int res = base + bloom;
                        rb[pxOffset + c] = (unsigned char)(res > 255 ? 255 : res);
                    } else {
                        // Screen (Soft CRT Bloom)
                        int invB = 255 - (bloom > 255 ? 255 : bloom);
                        int res = 255 - ((255 - base) * invB) / 255;
                        rb[pxOffset + c] = (unsigned char)(res > 255 ? 255 : (res < 0 ? 0 : res));
                    }
                }
            }
        }
    }

    // Copy to OFX destination in parallel (respecting stride)
    unsigned char* dstBase = (unsigned char*)dstData;
#if defined(_OPENMP)
    #pragma omp parallel for schedule(static)
#endif
    for (int y = 0; y < dstH; y++) {
        memcpy(dstBase + y * dstStride, rb + y * dstW * ncomp, dstW * ncomp);
    }

    d->lastRenderedFrame = heldFrame;
    d->hasCachedOutput = true;
    d->lastRenderW = dstW;
    d->lastRenderH = dstH;
    d->lastRenderNcomp = ncomp;

    gEffectSuite->clipReleaseImage(srcImg);
    gEffectSuite->clipReleaseImage(dstImg);
    return kOfxStatOK;
}

// ============================================================================
// ACTION: Instance Changed (handles Presets and GitHub link button)
// ============================================================================
static OfxStatus actionInstanceChanged(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs) {
    InstanceData* d = getInstanceData(effect);
    if (!d) return kOfxStatOK;

    char* paramName = nullptr;
    gPropSuite->propGetString(inArgs, kOfxPropName, 0, &paramName);
    if (!paramName) return kOfxStatOK;

    if (strcmp(paramName, PARAM_INFO_BUTTON) == 0) {
#if defined(_WIN32)
        ShellExecuteA(NULL, "open", "https://github.com/SKBwastaken/ASCIICreatorOFX", NULL, NULL, SW_SHOWNORMAL);
#else
        int ret = system("xdg-open https://github.com/SKBwastaken/ASCIICreatorOFX 2>/dev/null &");
        (void)ret;
#endif
    } else if (strcmp(paramName, PARAM_PRESET) == 0) {
        int presetIdx = 0;
        if (d->presetParam && gParamSuite->paramGetValue(d->presetParam, &presetIdx) == kOfxStatOK) {
            if (presetIdx == 1) {
                // Classic Matrix CRT (Default)
                gParamSuite->paramSetValue(d->charSpacingParam, 25);
                gParamSuite->paramSetValue(d->charAspectParam, 1.0);
                gParamSuite->paramSetValue(d->fontSourceParam, 3);
                gParamSuite->paramSetValue(d->fontFileParam, "C:\\Users\\SKB\\Downloads\\Consolas-Regular.ttf");
                gParamSuite->paramSetValue(d->charSetParam, 2);
                gParamSuite->paramSetValue(d->colorModeParam, 0);
                gParamSuite->paramSetValue(d->bgRParam, 0.01);
                gParamSuite->paramSetValue(d->bgGParam, 0.03);
                gParamSuite->paramSetValue(d->bgBParam, 0.01);
                gParamSuite->paramSetValue(d->contrastParam, 30.0);
                gParamSuite->paramSetValue(d->brightnessParam, 10.0);
                gParamSuite->paramSetValue(d->invertParam, 0);
                gParamSuite->paramSetValue(d->randomCharsParam, 0);
                gParamSuite->paramSetValue(d->fontScaleParam, 1.0);
                gParamSuite->paramSetValue(d->skipBlackParam, 1);
                gParamSuite->paramSetValue(d->blackCutoffParam, 15.0);
                gParamSuite->paramSetValue(d->alphaCutoffParam, 10.0);
                if (d->enableGlowParam) gParamSuite->paramSetValue(d->enableGlowParam, 1);
                if (d->glowRadiusParam) gParamSuite->paramSetValue(d->glowRadiusParam, 8);
                if (d->glowIntensityParam) gParamSuite->paramSetValue(d->glowIntensityParam, 0.85);
                if (d->glowBlendModeParam) gParamSuite->paramSetValue(d->glowBlendModeParam, 0);
                gParamSuite->paramSetValue(d->frameHoldParam, 1);
            } else if (presetIdx == 2) {
                // Goliath Cyber Lime
                gParamSuite->paramSetValue(d->charSpacingParam, 25);
                gParamSuite->paramSetValue(d->charAspectParam, 1.0);
                gParamSuite->paramSetValue(d->fontSourceParam, 0);
                gParamSuite->paramSetValue(d->charSetParam, 0);
                gParamSuite->paramSetValue(d->colorModeParam, 1);
                gParamSuite->paramSetValue(d->bgRParam, 0.0);
                gParamSuite->paramSetValue(d->bgGParam, 0.0);
                gParamSuite->paramSetValue(d->bgBParam, 0.0);
                gParamSuite->paramSetValue(d->contrastParam, 20.0);
                gParamSuite->paramSetValue(d->brightnessParam, 5.0);
                gParamSuite->paramSetValue(d->invertParam, 0);
                gParamSuite->paramSetValue(d->randomCharsParam, 0);
                gParamSuite->paramSetValue(d->fontScaleParam, 1.0);
                gParamSuite->paramSetValue(d->skipBlackParam, 1);
                gParamSuite->paramSetValue(d->blackCutoffParam, 15.0);
                gParamSuite->paramSetValue(d->alphaCutoffParam, 10.0);
                if (d->enableGlowParam) gParamSuite->paramSetValue(d->enableGlowParam, 1);
                if (d->glowRadiusParam) gParamSuite->paramSetValue(d->glowRadiusParam, 6);
                if (d->glowIntensityParam) gParamSuite->paramSetValue(d->glowIntensityParam, 0.60);
                if (d->glowBlendModeParam) gParamSuite->paramSetValue(d->glowBlendModeParam, 0);
                gParamSuite->paramSetValue(d->frameHoldParam, 1);
            } else if (presetIdx == 3) {
                // Full Color Hi-Fi
                gParamSuite->paramSetValue(d->charSpacingParam, 8);
                gParamSuite->paramSetValue(d->charAspectParam, 1.0);
                gParamSuite->paramSetValue(d->fontSourceParam, 1);
                gParamSuite->paramSetValue(d->charSetParam, 2);
                gParamSuite->paramSetValue(d->colorModeParam, 0);
                gParamSuite->paramSetValue(d->bgRParam, 0.0);
                gParamSuite->paramSetValue(d->bgGParam, 0.0);
                gParamSuite->paramSetValue(d->bgBParam, 0.0);
                gParamSuite->paramSetValue(d->contrastParam, 10.0);
                gParamSuite->paramSetValue(d->brightnessParam, 0.0);
                gParamSuite->paramSetValue(d->invertParam, 0);
                gParamSuite->paramSetValue(d->randomCharsParam, 0);
                gParamSuite->paramSetValue(d->fontScaleParam, 1.0);
                gParamSuite->paramSetValue(d->skipBlackParam, 0);
                gParamSuite->paramSetValue(d->blackCutoffParam, 5.0);
                gParamSuite->paramSetValue(d->alphaCutoffParam, 10.0);
                if (d->enableGlowParam) gParamSuite->paramSetValue(d->enableGlowParam, 0);
                gParamSuite->paramSetValue(d->frameHoldParam, 1);
            } else if (presetIdx == 4) {
                // Pure 1-Bit Terminal
                gParamSuite->paramSetValue(d->charSpacingParam, 16);
                gParamSuite->paramSetValue(d->charAspectParam, 1.0);
                gParamSuite->paramSetValue(d->fontSourceParam, 0);
                gParamSuite->paramSetValue(d->charSetParam, 6);
                gParamSuite->paramSetValue(d->colorModeParam, 10);
                gParamSuite->paramSetValue(d->bgRParam, 0.0);
                gParamSuite->paramSetValue(d->bgGParam, 0.0);
                gParamSuite->paramSetValue(d->bgBParam, 0.0);
                gParamSuite->paramSetValue(d->contrastParam, 40.0);
                gParamSuite->paramSetValue(d->brightnessParam, 0.0);
                gParamSuite->paramSetValue(d->invertParam, 0);
                gParamSuite->paramSetValue(d->randomCharsParam, 0);
                gParamSuite->paramSetValue(d->fontScaleParam, 1.0);
                gParamSuite->paramSetValue(d->skipBlackParam, 1);
                gParamSuite->paramSetValue(d->blackCutoffParam, 30.0);
                gParamSuite->paramSetValue(d->alphaCutoffParam, 10.0);
                if (d->enableGlowParam) gParamSuite->paramSetValue(d->enableGlowParam, 0);
                gParamSuite->paramSetValue(d->frameHoldParam, 1);
            } else if (presetIdx == 5) {
                // Cyberpunk Neon
                gParamSuite->paramSetValue(d->charSpacingParam, 25);
                gParamSuite->paramSetValue(d->charAspectParam, 1.0);
                gParamSuite->paramSetValue(d->fontSourceParam, 0);
                gParamSuite->paramSetValue(d->charSetParam, 0);
                gParamSuite->paramSetValue(d->colorModeParam, 4);
                gParamSuite->paramSetValue(d->bgRParam, 0.04);
                gParamSuite->paramSetValue(d->bgGParam, 0.0);
                gParamSuite->paramSetValue(d->bgBParam, 0.08);
                gParamSuite->paramSetValue(d->contrastParam, 25.0);
                gParamSuite->paramSetValue(d->brightnessParam, 15.0);
                gParamSuite->paramSetValue(d->invertParam, 0);
                gParamSuite->paramSetValue(d->randomCharsParam, 0);
                gParamSuite->paramSetValue(d->fontScaleParam, 1.0);
                gParamSuite->paramSetValue(d->skipBlackParam, 1);
                gParamSuite->paramSetValue(d->blackCutoffParam, 15.0);
                gParamSuite->paramSetValue(d->alphaCutoffParam, 10.0);
                if (d->enableGlowParam) gParamSuite->paramSetValue(d->enableGlowParam, 1);
                if (d->glowRadiusParam) gParamSuite->paramSetValue(d->glowRadiusParam, 7);
                if (d->glowIntensityParam) gParamSuite->paramSetValue(d->glowIntensityParam, 0.75);
                if (d->glowBlendModeParam) gParamSuite->paramSetValue(d->glowBlendModeParam, 1);
                gParamSuite->paramSetValue(d->frameHoldParam, 1);
            } else if (presetIdx == 6) {
                // CGA Retro PC
                gParamSuite->paramSetValue(d->charSpacingParam, 16);
                gParamSuite->paramSetValue(d->charAspectParam, 1.0);
                gParamSuite->paramSetValue(d->fontSourceParam, 1);
                gParamSuite->paramSetValue(d->charSetParam, 1);
                gParamSuite->paramSetValue(d->colorModeParam, 5);
                gParamSuite->paramSetValue(d->bgRParam, 0.0);
                gParamSuite->paramSetValue(d->bgGParam, 0.0);
                gParamSuite->paramSetValue(d->bgBParam, 0.0);
                gParamSuite->paramSetValue(d->contrastParam, 20.0);
                gParamSuite->paramSetValue(d->brightnessParam, 0.0);
                gParamSuite->paramSetValue(d->invertParam, 0);
                gParamSuite->paramSetValue(d->randomCharsParam, 0);
                gParamSuite->paramSetValue(d->fontScaleParam, 1.0);
                gParamSuite->paramSetValue(d->skipBlackParam, 1);
                gParamSuite->paramSetValue(d->blackCutoffParam, 15.0);
                gParamSuite->paramSetValue(d->alphaCutoffParam, 10.0);
                if (d->enableGlowParam) gParamSuite->paramSetValue(d->enableGlowParam, 0);
                gParamSuite->paramSetValue(d->frameHoldParam, 1);
            } else if (presetIdx == 7) {
                // ZX Spectrum Vintage
                gParamSuite->paramSetValue(d->charSpacingParam, 14);
                gParamSuite->paramSetValue(d->charAspectParam, 1.0);
                gParamSuite->paramSetValue(d->fontSourceParam, 1);
                gParamSuite->paramSetValue(d->charSetParam, 1);
                gParamSuite->paramSetValue(d->colorModeParam, 6);
                gParamSuite->paramSetValue(d->bgRParam, 0.0);
                gParamSuite->paramSetValue(d->bgGParam, 0.0);
                gParamSuite->paramSetValue(d->bgBParam, 0.0);
                gParamSuite->paramSetValue(d->contrastParam, 15.0);
                gParamSuite->paramSetValue(d->brightnessParam, 0.0);
                gParamSuite->paramSetValue(d->invertParam, 0);
                gParamSuite->paramSetValue(d->randomCharsParam, 0);
                gParamSuite->paramSetValue(d->fontScaleParam, 1.0);
                gParamSuite->paramSetValue(d->skipBlackParam, 1);
                gParamSuite->paramSetValue(d->blackCutoffParam, 15.0);
                gParamSuite->paramSetValue(d->alphaCutoffParam, 10.0);
                if (d->enableGlowParam) gParamSuite->paramSetValue(d->enableGlowParam, 0);
                gParamSuite->paramSetValue(d->frameHoldParam, 1);
            } else if (presetIdx == 8) {
                // Lo-Fi 12fps Anime Hold
                gParamSuite->paramSetValue(d->charSpacingParam, 25);
                gParamSuite->paramSetValue(d->charAspectParam, 1.0);
                gParamSuite->paramSetValue(d->fontSourceParam, 0);
                gParamSuite->paramSetValue(d->charSetParam, 0);
                gParamSuite->paramSetValue(d->colorModeParam, 1);
                gParamSuite->paramSetValue(d->bgRParam, 0.0);
                gParamSuite->paramSetValue(d->bgGParam, 0.0);
                gParamSuite->paramSetValue(d->bgBParam, 0.0);
                gParamSuite->paramSetValue(d->contrastParam, 20.0);
                gParamSuite->paramSetValue(d->brightnessParam, 5.0);
                gParamSuite->paramSetValue(d->invertParam, 0);
                gParamSuite->paramSetValue(d->randomCharsParam, 0);
                gParamSuite->paramSetValue(d->fontScaleParam, 1.0);
                gParamSuite->paramSetValue(d->skipBlackParam, 1);
                gParamSuite->paramSetValue(d->blackCutoffParam, 15.0);
                gParamSuite->paramSetValue(d->alphaCutoffParam, 10.0);
                if (d->enableGlowParam) gParamSuite->paramSetValue(d->enableGlowParam, 1);
                if (d->glowRadiusParam) gParamSuite->paramSetValue(d->glowRadiusParam, 6);
                if (d->glowIntensityParam) gParamSuite->paramSetValue(d->glowIntensityParam, 0.60);
                if (d->glowBlendModeParam) gParamSuite->paramSetValue(d->glowBlendModeParam, 0);
                gParamSuite->paramSetValue(d->frameHoldParam, 2);
            }
        }
    }
    return kOfxStatOK;
}

// ============================================================================
// Main entry & plugin struct
// ============================================================================
static OfxStatus pluginMain(const char* action, const void* handle,
                             OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs) {
    if (!strcmp(action, kOfxActionLoad)) return actionLoad();
    if (!strcmp(action, kOfxActionUnload)) return kOfxStatOK;
    if (!strcmp(action, kOfxActionDescribe)) return actionDescribe((OfxImageEffectHandle)handle);
    if (!strcmp(action, kOfxImageEffectActionDescribeInContext))
        return actionDescribeInContext((OfxImageEffectHandle)handle, inArgs);
    if (!strcmp(action, kOfxActionCreateInstance)) return actionCreateInstance((OfxImageEffectHandle)handle);
    if (!strcmp(action, kOfxActionDestroyInstance)) return actionDestroyInstance((OfxImageEffectHandle)handle);
    if (!strcmp(action, kOfxImageEffectActionRender)) return actionRender((OfxImageEffectHandle)handle, inArgs);
    if (!strcmp(action, kOfxActionInstanceChanged)) return actionInstanceChanged((OfxImageEffectHandle)handle, inArgs);
    return kOfxStatReplyDefault;
}

static void setHost(OfxHost* host) { gHost = host; }

static OfxPlugin plugin = {
    kOfxImageEffectPluginApi, kOfxImageEffectPluginApiVersion,
    PLUGIN_ID, PLUGIN_VERSION_MAJOR, PLUGIN_VERSION_MINOR,
    setHost, pluginMain
};

EXPORT int OfxGetNumberOfPlugins(void) { return 1; }
EXPORT OfxPlugin* OfxGetPlugin(int nth) { return nth == 0 ? &plugin : nullptr; }
