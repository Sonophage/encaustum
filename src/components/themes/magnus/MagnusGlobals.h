#pragma once

// Magnus design-system globals — the shared visual vocabulary every Magnus screen
// (theme primitives + bespoke activities) draws with. Header-only inline helpers so
// any translation unit can include and reuse them without a build entry.
//
// Aesthetic: ink-on-paper, 1-bit. Tone via dither, emphasis via inversion, structure
// via rules + tracked small-caps. Keep all Magnus drawing flowing through these so the
// look stays consistent across the homepage, reader, stats, sleep and boot screens.

#include <GfxRenderer.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

#include "components/themes/BaseTheme.h"  // Rect
#include "fontIds.h"

namespace magnus {

// ── Archive lore — shared so every screen labels a book identically ──────────
inline uint32_t pathHash(const std::string& path) {
  uint32_t h = 2166136261u;  // FNV-1a
  for (unsigned char c : path) {
    h ^= c;
    h *= 16777619u;
  }
  return h;
}

// Per-book case number, MAG- prefixed (e.g. MAG-0122104).
inline std::string bookCode(const std::string& path) {
  char buf[16];
  snprintf(buf, sizeof(buf), "MAG-%07u", (unsigned)(pathHash(path) % 10000000u));
  return buf;
}

// The fourteen fears — each book is deterministically filed under one fonds.
inline const char* fondsFor(const std::string& path) {
  static const char* kFonds[] = {"THE BURIED",  "THE CORRUPTION", "THE DARK",     "THE DESOLATION",
                                 "THE END",     "THE EYE",        "THE FLESH",    "THE HUNT",
                                 "THE LONELY",  "THE SLAUGHTER",  "THE SPIRAL",   "THE STRANGER",
                                 "THE VAST",    "THE WEB"};
  return kFonds[pathHash(path) % (sizeof(kFonds) / sizeof(kFonds[0]))];
}

// ── Spacing tokens ──────────────────────────────────────────────────────────
constexpr int SIDE_PAD = 22;   // standard content side padding
constexpr int RULE = 2;        // section / header rule thickness
constexpr int HAIR = 1;        // hairline divider thickness
constexpr int GAP = 12;        // standard vertical gap
constexpr int READER_TOP = 30; // reader page top-strip height (clock · case no. · battery)
constexpr int READER_GAP = 22; // breathing room between the strip/footer rules and the body text

// ── Type roles (all UNCOMPRESSED tables — compressed garbles on this build) ──
constexpr int FONT_DISPLAY = EBGARAMOND_18_FONT_ID;  // hero numerals / splash title (24pt too costly in flash)
constexpr int FONT_TITLE = EBGARAMOND_18_FONT_ID;   // screen / dialog titles
constexpr int FONT_BODY = EBGARAMOND_12_FONT_ID;    // list rows, content text
constexpr int FONT_CHROME = COURIER_10_FONT_ID;     // values, hints, status
constexpr int FONT_MONO = COURIER_12_FONT_ID;       // keyboard cells, code stamps
constexpr int FONT_EYEBROW = SMALL_FONT_ID;         // small subtle caps for eyebrows / section heads

// ── Tracking (letter-spacing) — the Magnus signature on small-caps labels ────
constexpr int TRACK_EYEBROW = 3;  // eyebrows / section heads
constexpr int TRACK_TAB = 2;      // tab labels

// ── Rules & dividers ─────────────────────────────────────────────────────────
inline void rule(const GfxRenderer& r, int x, int y, int w, int thick = RULE) { r.fillRect(x, y, w, thick, true); }

inline void vrule(const GfxRenderer& r, int x, int y, int h, int thick = HAIR) { r.fillRect(x, y, thick, h, true); }

// Faint hairline (dithered → reads as a light divider on 1-bit paper).
inline void hairline(const GfxRenderer& r, int x, int y, int w) { r.fillRectDither(x, y, w, HAIR, Color::LightGray); }

// Dashed divider (the "FROM THE STACKS" row separator).
inline void dashed(const GfxRenderer& r, int x, int y, int w, int dash = 5, int gap = 3) {
  for (int i = 0; i < w; i += dash + gap) r.fillRect(x + i, y, std::min(dash, w - i), 1, true);
}

// ── Fills & frames ───────────────────────────────────────────────────────────
inline void frame(const GfxRenderer& r, Rect b, int thick = 1) { r.drawRect(b.x, b.y, b.width, b.height, thick, true); }

inline void invFill(const GfxRenderer& r, Rect b) { r.fillRect(b.x, b.y, b.width, b.height, true); }

inline void clear(const GfxRenderer& r, Rect b) { r.fillRect(b.x, b.y, b.width, b.height, false); }

inline void ditherFill(const GfxRenderer& r, Rect b, Color c = Color::DarkGray) {
  r.fillRectDither(b.x, b.y, b.width, b.height, c);
}

// Outlined bar with a dither-hatched fill to `pct` (0..100) — progress / battery / meters.
inline void ditherBar(const GfxRenderer& r, Rect b, int pct) {
  r.drawRect(b.x, b.y, b.width, b.height);
  const int p = pct < 0 ? 0 : (pct > 100 ? 100 : pct);
  const int fw = (b.width - 4) * p / 100;
  if (fw > 0) r.fillRectDither(b.x + 2, b.y + 2, fw, b.height - 4, Color::DarkGray);
}

// ── Tracked small-caps text (UTF-8 aware) ────────────────────────────────────
inline int utf8Step(unsigned char c) {
  if (c < 0x80) return 1;
  if ((c >> 5) == 0x6) return 2;
  if ((c >> 4) == 0xE) return 3;
  if ((c >> 3) == 0x1E) return 4;
  return 1;
}

// Single-glyph advance. Fonts measure a lone space inconsistently (0 for SMALL, a tiny
// value for some Courier glyphs), which collapses word gaps in tracked small-caps. Force
// every space to a representative glyph width so words stay apart in any font.
inline int glyphAdvance(const GfxRenderer& r, int fontId, const char* glyph, EpdFontFamily::Style st) {
  if (glyph[0] == ' ' && glyph[1] == '\0') return r.getTextWidth(fontId, "n", st);
  return r.getTextWidth(fontId, glyph, st);
}

// Measure the advance width of `s` rendered with per-glyph `track`.
inline int trackedWidth(const GfxRenderer& r, int fontId, const char* s, int track,
                        EpdFontFamily::Style st = EpdFontFamily::REGULAR) {
  int w = 0;
  bool any = false;
  for (const char* p = s; *p;) {
    const int n = utf8Step((unsigned char)*p);
    char buf[5] = {0};
    for (int i = 0; i < n && p[i]; ++i) buf[i] = p[i];
    w += glyphAdvance(r, fontId, buf, st) + track;
    any = true;
    p += n;
  }
  return any ? w - track : 0;
}

// Draw `s` with per-glyph `track`; returns the advance width.
inline int tracked(const GfxRenderer& r, int fontId, int x, int y, const char* s, int track, bool black = true,
                   EpdFontFamily::Style st = EpdFontFamily::REGULAR) {
  int cx = x;
  for (const char* p = s; *p;) {
    const int n = utf8Step((unsigned char)*p);
    char buf[5] = {0};
    for (int i = 0; i < n && p[i]; ++i) buf[i] = p[i];
    r.drawText(fontId, cx, y, buf, black, st);
    cx += glyphAdvance(r, fontId, buf, st) + track;
    p += n;
  }
  return cx - x - (cx > x ? track : 0);
}

// ── Circles, discs, the Eye ──────────────────────────────────────────────────
// No circle primitive in GfxRenderer — plot by scanline. One-shot renders only
// (boot splash, cover placeholders, footer marks), so cost is irrelevant.

inline void disc(const GfxRenderer& r, int cx, int cy, int rad, bool black = true) {
  if (rad < 0) return;
  for (int dy = -rad; dy <= rad; ++dy) {
    const int hw = (int)(std::sqrt((float)(rad * rad - dy * dy)) + 0.5f);
    r.fillRect(cx - hw, cy + dy, 2 * hw + 1, 1, black);
  }
}

// Annulus (outline circle) of the given stroke thickness.
inline void ring(const GfxRenderer& r, int cx, int cy, int rad, int thick = 2, bool black = true) {
  disc(r, cx, cy, rad, black);
  disc(r, cx, cy, rad - thick, !black);
}

inline void ellipseFill(const GfxRenderer& r, int cx, int cy, int rw, int rh, bool black = true) {
  if (rw <= 0 || rh <= 0) return;
  for (int dy = -rh; dy <= rh; ++dy) {
    float t = 1.0f - (float)(dy * dy) / (float)(rh * rh);
    if (t < 0) t = 0;
    const int hw = (int)(rw * std::sqrt(t) + 0.5f);
    r.fillRect(cx - hw, cy + dy, 2 * hw + 1, 1, black);
  }
}

// Outlined ellipse (the eye's lens).
inline void ellipseRing(const GfxRenderer& r, int cx, int cy, int rw, int rh, int thick = 2, bool black = true) {
  ellipseFill(r, cx, cy, rw, rh, black);
  ellipseFill(r, cx, cy, rw - thick, rh - thick, !black);
}

// The Eye of the Institute — a lens with iris + pupil. `rings` adds the concentric
// outer circles used on the boot seal; omit for small inline marks.
inline void eye(const GfxRenderer& r, int cx, int cy, int w, int h, bool rings = false, int thick = 2) {
  if (rings) {
    const int R = w / 2;
    ring(r, cx, cy, R, thick);
    ring(r, cx, cy, R - thick - 5, thick - 1 < 1 ? 1 : thick - 1);
  }
  const int lw = rings ? (w / 2 - thick - 12) : (w / 2);
  const int lh = h / 2;
  ellipseRing(r, cx, cy, lw, lh, thick);
  const int ir = (int)(lh * 0.62f);
  ring(r, cx, cy, ir, thick);
  disc(r, cx, cy, std::max(2, ir / 2));
}

// ── Dotted halftone fill (for "unread" cover placeholders) ───────────────────
inline void dots(const GfxRenderer& r, Rect b, int step = 6) {
  for (int y = b.y + step / 2; y < b.y + b.height; y += step)
    for (int x = b.x + step / 2; x < b.x + b.width; x += step) r.fillRect(x, y, 2, 2, true);
}

// ── Diagonal hatch fill (for "in progress" cover placeholders) ───────────────
inline void hatch(const GfxRenderer& r, Rect b, int step = 7) {
  for (int d = -b.height; d < b.width; d += step) {
    for (int y = 0; y < b.height; ++y) {
      const int x = d + y;
      if (x >= 0 && x < b.width) r.fillRect(b.x + x, b.y + y, 1, 1, true);
    }
  }
}

// Standard eyebrow: small subtle caps, letter-spaced (left-aligned).
inline void eyebrow(const GfxRenderer& r, int x, int y, const char* s, bool black = true) {
  tracked(r, FONT_EYEBROW, x, y, s, TRACK_EYEBROW, black);
}

// Centered tracked caps (seals, plate headings, centered tab labels).
inline void centerTracked(const GfxRenderer& r, int fontId, int cx, int y, const char* s, int track,
                          bool black = true) {
  const int w = trackedWidth(r, fontId, s, track);
  tracked(r, fontId, cx - w / 2, y, s, track, black);
}

}  // namespace magnus
