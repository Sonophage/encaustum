#include "MagnusTheme.h"

#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <I18n.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>

#include "CrossPointSettings.h"
#include "MagnusGlobals.h"
#include "components/UITheme.h"
#include "fontIds.h"

// Magnus design tokens — names kept local for readability; values flow from the shared globals.
namespace {
constexpr int kSidePad = magnus::SIDE_PAD;
constexpr int kRule = magnus::RULE;   // header / footer rule thickness
constexpr int kHair = magnus::HAIR;   // hairline list divider (drawn dithered → faint)

// Type roles
constexpr int kTitleFont = magnus::FONT_TITLE;    // screen titles
constexpr int kDialogFont = magnus::FONT_TITLE;   // dialog titles
constexpr int kBodyFont = magnus::FONT_BODY;      // list/content rows (REGULAR = semibold, ITALIC available)
constexpr int kChromeFont = magnus::FONT_CHROME;  // eyebrows, values, hints, status
constexpr int kKeyFont = magnus::FONT_MONO;       // keyboard cells

using magnus::hairline;
}  // namespace

// ── Header — mono eyebrow + Garamond title, left-aligned, 2px baseline rule ─────
void MagnusTheme::drawHeader(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle) const {
  renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);

  // Battery, top-right
  const bool showPct = SETTINGS.hideBatteryPercentage != CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_ALWAYS;
  const int battX = rect.x + rect.width - 12 - MagnusMetrics::values.batteryWidth;
  drawBatteryRight(renderer,
                   Rect{battX, rect.y + 5, MagnusMetrics::values.batteryWidth, MagnusMetrics::values.batteryHeight},
                   showPct);

  // Eyebrow (subtitle) — small mono caps, letter-spaced
  int titleY = rect.y + 10;
  if (subtitle && subtitle[0] != '\0') {
    auto eyebrow = renderer.truncatedText(kChromeFont, subtitle, rect.width - kSidePad * 2 - 40, EpdFontFamily::REGULAR);
    magnus::eyebrow(renderer, rect.x + kSidePad, rect.y + 6, eyebrow.c_str());
    titleY = rect.y + 22;
  }

  // Title — Garamond, left aligned
  if (title && title[0] != '\0') {
    const int maxW = rect.width - kSidePad * 2 - 40;  // leave room for battery
    auto t = renderer.truncatedText(kTitleFont, title, maxW, EpdFontFamily::REGULAR);
    renderer.drawText(kTitleFont, rect.x + kSidePad, titleY, t.c_str(), true, EpdFontFamily::REGULAR);
  }

  // 2px baseline rule
  magnus::rule(renderer, rect.x, rect.y + rect.height - kRule, rect.width);
}

// ── Sub-header — mono eyebrow with optional right-aligned value ──────────────────
void MagnusTheme::drawSubHeader(const GfxRenderer& renderer, Rect rect, const char* label,
                                const char* rightLabel) const {
  int rightSpace = kSidePad;
  if (rightLabel && rightLabel[0] != '\0') {
    auto r = renderer.truncatedText(kChromeFont, rightLabel, 200, EpdFontFamily::REGULAR);
    const int rw = renderer.getTextWidth(kChromeFont, r.c_str());
    renderer.drawText(kChromeFont, rect.x + rect.width - kSidePad - rw, rect.y + 6, r.c_str(), true);
    rightSpace += rw + 10;
  }
  auto l = renderer.truncatedText(kChromeFont, label, rect.width - kSidePad - rightSpace, EpdFontFamily::REGULAR);
  magnus::eyebrow(renderer, rect.x + kSidePad, rect.y + 6, l.c_str());
}

// ── Tab bar — even-width cells (overflow-proof), small-caps labels, selected underlined ──
void MagnusTheme::drawTabBar(const GfxRenderer& renderer, Rect rect, const std::vector<TabInfo>& tabs,
                             bool selected) const {
  const int n = static_cast<int>(tabs.size());
  if (n <= 0) {
    return;
  }
  const int cellW = rect.width / n;
  for (int i = 0; i < n; i++) {
    const int cx = rect.x + i * cellW;
    auto label = renderer.truncatedText(kChromeFont, tabs[i].label, cellW - 8, EpdFontFamily::REGULAR);
    const int tw = renderer.getTextWidth(kChromeFont, label.c_str());
    const int tx = cx + (cellW - tw) / 2;
    const int ty = rect.y + (rect.height - renderer.getLineHeight(kChromeFont)) / 2;

    if (tabs[i].selected) {
      // Inverse fill when this tab bar has focus, else just an underline
      if (selected) {
        renderer.fillRect(cx + 2, rect.y + 2, cellW - 4, rect.height - 4, true);
      } else {
        renderer.fillRect(cx + 6, rect.y + rect.height - 5, cellW - 12, 2, true);
      }
    }
    renderer.drawText(kChromeFont, tx, ty, label.c_str(), !(tabs[i].selected && selected));
  }
  // bottom rule
  renderer.fillRect(rect.x, rect.y + rect.height - 1, rect.width, 1, true);
}

// ── List — Garamond rows, hairline dividers, inverse selection with caret ───────
void MagnusTheme::drawList(const GfxRenderer& renderer, Rect rect, int itemCount, int selectedIndex,
                           const std::function<std::string(int index)>& rowTitle,
                           const std::function<std::string(int index)>& rowSubtitle,
                           const std::function<UIIcon(int index)>& rowIcon,
                           const std::function<std::string(int index)>& rowValue, bool highlightValue) const {
  const int rowHeight =
      (rowSubtitle != nullptr) ? MagnusMetrics::values.listWithSubtitleRowHeight : MagnusMetrics::values.listRowHeight;
  const int pageItems = (rowHeight > 0) ? rect.height / rowHeight : 0;
  if (pageItems <= 0) {
    return;
  }

  const int contentWidth = rect.width - MagnusMetrics::values.scrollBarWidth - MagnusMetrics::values.scrollBarRightOffset;
  const int pageStartIndex = (selectedIndex / pageItems) * pageItems;
  const int valueReserve = (rowValue != nullptr) ? 70 : 0;

  for (int i = pageStartIndex; i < itemCount && i < pageStartIndex + pageItems; i++) {
    const int slot = i % pageItems;
    const int itemY = rect.y + slot * rowHeight;
    const bool selected = (i == selectedIndex);
    const int textW = contentWidth - kSidePad * 2 - valueReserve;

    auto itemName = rowTitle(i);

    // Section header marker (\x01 prefix): tracked mono caps, no row chrome
    if (!itemName.empty() && itemName[0] == '\x01') {
      magnus::eyebrow(renderer, rect.x + kSidePad, itemY + rowHeight - 16, itemName.c_str() + 1);
      continue;
    }

    // Selection block — full-width inversion
    if (selected) {
      renderer.fillRect(rect.x, itemY, rect.width, rowHeight, true);
    }

    const int caretW = selected ? 16 : 0;
    const int titleX = rect.x + kSidePad + caretW;
    const int titleY = itemY + (rowHeight - renderer.getLineHeight(kBodyFont)) / 2;

    if (selected) {
      // Typewriter caret marker
      renderer.drawText(kChromeFont, rect.x + kSidePad, titleY + 2, "\xE2\x96\xB8", false);  // ▸
    }

    auto t = renderer.truncatedText(kBodyFont, itemName.c_str(), textW - caretW, EpdFontFamily::REGULAR);
    renderer.drawText(kBodyFont, titleX, titleY, t.c_str(), !selected, EpdFontFamily::REGULAR);

    if (rowSubtitle != nullptr) {
      auto sub = renderer.truncatedText(kChromeFont, rowSubtitle(i).c_str(), textW - caretW, EpdFontFamily::REGULAR);
      renderer.drawText(kChromeFont, titleX, itemY + rowHeight - 22, sub.c_str(), !selected);
    }

    if (rowValue != nullptr) {
      auto v = rowValue(i);
      // Toggles render as a checkbox square (matches the Magnus settings sheet);
      // compare against the same i18n strings so it stays locale-correct.
      const std::string onStr = I18N.get(StrId::STR_STATE_ON);
      const std::string offStr = I18N.get(StrId::STR_STATE_OFF);
      if (v == onStr || v == offStr) {
        const int box = 18;
        const int bx = rect.x + contentWidth - kSidePad - box;
        const int by = itemY + (rowHeight - box) / 2;
        renderer.drawRect(bx, by, box, box, 1, !selected);
        if (v == onStr) renderer.fillRect(bx + 4, by + 4, box - 8, box - 8, !selected);
      } else {
        const int vw = renderer.getTextWidth(kChromeFont, v.c_str());
        const int vy = itemY + (rowHeight - renderer.getLineHeight(kChromeFont)) / 2;
        renderer.drawText(kChromeFont, rect.x + contentWidth - kSidePad - vw, vy, v.c_str(), !selected);
      }
    }

    // Hairline divider beneath unselected rows
    if (!selected) {
      hairline(renderer, rect.x + kSidePad, itemY + rowHeight - kHair, contentWidth - kSidePad * 2);
    }
  }

  // Pagination caret (▾) when more pages exist
  const int totalPages = (itemCount + pageItems - 1) / pageItems;
  if (totalPages > 1) {
    const int cx = rect.x + rect.width - MagnusMetrics::values.scrollBarRightOffset - 6;
    const int cy = rect.y + rect.height - 8;
    for (int k = 0; k < 6; ++k) {
      renderer.drawLine(cx - (5 - k), cy + k, cx + (5 - k), cy + k, true);
    }
  }
}

// ── Button hints — mono caps in bordered cells, 2px top rule ─────────────────────
void MagnusTheme::drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                                  const char* btn4) const {
  const GfxRenderer::Orientation orig = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  if (orig == GfxRenderer::Orientation::PortraitInverted) {
    std::swap(btn1, btn4);
    std::swap(btn2, btn3);
  }

  const int pageHeight = renderer.getScreenHeight();
  constexpr int buttonWidth = 106;
  constexpr int buttonHeight = MagnusMetrics::values.buttonHintsHeight;
  const int barTop = pageHeight - buttonHeight;
  constexpr int x4Pos[] = {25, 130, 245, 350};
  constexpr int x3Pos[] = {38, 154, 268, 384};
  const int* pos = gpio.deviceIsX3() ? x3Pos : x4Pos;
  const char* labels[] = {btn1, btn2, btn3, btn4};

  // 2px rule across the top of the hint band
  magnus::rule(renderer, 0, barTop - kRule, renderer.getScreenWidth());

  for (int i = 0; i < 4; i++) {
    if (labels[i] != nullptr && labels[i][0] != '\0') {
      const int x = pos[i];
      renderer.fillRect(x, barTop, buttonWidth, buttonHeight, false);
      renderer.drawRect(x, barTop, buttonWidth, buttonHeight);
      const int tw = renderer.getTextWidth(kChromeFont, labels[i]);
      const int ty = barTop + (buttonHeight - renderer.getLineHeight(kChromeFont)) / 2;
      renderer.drawText(kChromeFont, x + (buttonWidth - tw) / 2, ty, labels[i], true);
    }
  }

  renderer.setOrientation(orig);
}

// ── Keyboard key — every cell bordered (mono), focused cell inverted ─────────────
void MagnusTheme::drawKeyboardKey(const GfxRenderer& renderer, Rect rect, const char* label, const bool isSelected,
                                  const char* secondaryLabel, const KeyboardKeyType keyType,
                                  const bool inactiveSelection) const {
  const bool invert = isSelected && !inactiveSelection;

  if (invert) {
    renderer.fillRect(rect.x, rect.y, rect.width, rect.height, true);
  } else if (isSelected && inactiveSelection) {
    renderer.fillRectDither(rect.x, rect.y, rect.width, rect.height, Color::LightGray);
  }
  // Border on every cell — the printed-key look
  renderer.drawRect(rect.x, rect.y, rect.width, rect.height, isSelected ? 2 : 1, true);

  if (keyType == KeyboardKeyType::Space) {
    const int half = rect.width * 3 / 10;
    const int cx = rect.x + rect.width / 2;
    const int ly = rect.y + rect.height / 2 + 3;
    renderer.drawLine(cx - half, ly, cx + half, ly, 3, !invert);
    return;
  }
  if (keyType == KeyboardKeyType::Del) {
    const int cx = rect.x + rect.width / 2;
    const int cy = rect.y + rect.height / 2;
    const int len = rect.width / 4;
    const int head = len / 2;
    renderer.drawLine(cx - len / 2, cy, cx + len / 2, cy, 3, !invert);
    renderer.drawLine(cx - len / 2, cy, cx - len / 2 + head, cy - head, 3, !invert);
    renderer.drawLine(cx - len / 2, cy, cx - len / 2 + head, cy + head, 3, !invert);
    return;
  }

  const int tw = renderer.getTextWidth(kKeyFont, label);
  const int tx = rect.x + (rect.width - tw) / 2;
  const int ty = rect.y + (rect.height - renderer.getLineHeight(kKeyFont)) / 2;
  renderer.drawText(kKeyFont, tx, ty, label, !invert);

  if (secondaryLabel != nullptr && secondaryLabel[0] != '\0') {
    const int sw = renderer.getTextWidth(kChromeFont, secondaryLabel);
    renderer.drawText(kChromeFont, rect.x + rect.width - sw - 2, rect.y + 1, secondaryLabel, !invert);
  }
}

// ── Progress bar — outline with dither-hatched fill ──────────────────────────────
void MagnusTheme::drawProgressBar(const GfxRenderer& renderer, Rect rect, const size_t current,
                                  const size_t total) const {
  if (total == 0) {
    return;
  }
  const int percent = static_cast<int>((static_cast<uint64_t>(current) * 100) / total);
  renderer.drawRect(rect.x, rect.y, rect.width, rect.height);
  const int fillW = (rect.width - 4) * percent / 100;
  if (fillW > 0) {
    renderer.fillRectDither(rect.x + 2, rect.y + 2, fillW, rect.height - 4, Color::DarkGray);
  }
  const std::string pct = std::to_string(percent) + "%";
  renderer.drawCenteredText(kChromeFont, rect.y + rect.height + 12, pct.c_str());
}

// ── Reader status bar — mono footer: page (left) · chapter (centre) · percent (right) ──
// Honors the user's status-bar toggles; renders in Courier with a hairline above the band.
void MagnusTheme::drawStatusBar(GfxRenderer& renderer, const float bookProgress, const int currentPage,
                                const int pageCount, std::string title, const int paddingBottom,
                                const int textYOffset, const bool isStarred) const {
  const auto metrics = UITheme::getInstance().getMetrics();
  int mt, mr, mb, ml;
  renderer.getOrientedViewableTRBL(&mt, &mr, &mb, &ml);
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();
  const int barH = UITheme::getInstance().getStatusBarHeight();
  if (barH == 0) return;

  const int leftEdge = ml + metrics.statusBarHorizontalMargin;
  const int rightEdge = sw - mr - metrics.statusBarHorizontalMargin;
  const int textY = sh - barH - mb - paddingBottom - 4 - textYOffset;

  // hairline separating the body from the footer band
  hairline(renderer, ml, textY - 5, sw - ml - mr);

  // optional progress bar (Magnus dither) along the very bottom of the band
  if (SETTINGS.statusBarProgressBar != CrossPointSettings::STATUS_BAR_PROGRESS_BAR::HIDE_PROGRESS) {
    const int th = (SETTINGS.statusBarProgressBarThickness + 1) * 2;
    const int by = sh - mb - th - paddingBottom;
    size_t progress;
    if (SETTINGS.statusBarProgressBar == CrossPointSettings::STATUS_BAR_PROGRESS_BAR::BOOK_PROGRESS)
      progress = static_cast<size_t>(bookProgress);
    else
      progress = (pageCount > 0) ? static_cast<size_t>(static_cast<float>(currentPage) / pageCount * 100) : 0;
    const int w = static_cast<int>((sw - ml - mr) * progress / 100);
    if (w > 0) renderer.fillRectDither(ml, by, w, th, Color::DarkGray);
  }

  // The Magnus reader draws the clock + battery in its TOP strip (see
  // EpubReaderActivity::renderStatusBar), so the footer carries only the
  // reading position — page · chapter · percent — to match the mockup and
  // avoid a duplicate clock.
  int leftX = leftEdge;

  // page count, left: "p. C / P"
  int pageW = 0;
  if (SETTINGS.statusBarChapterPageCount && pageCount > 0) {
    char pg[24];
    snprintf(pg, sizeof(pg), "p. %d / %d", currentPage, pageCount);
    renderer.drawText(kChromeFont, leftX, textY, pg, true);
    pageW = renderer.getTextWidth(kChromeFont, pg);
  }

  // percent, right
  int rightUsed = 0;
  if (SETTINGS.statusBarBookProgressPercentage) {
    char pc[8];
    snprintf(pc, sizeof(pc), "%.0f%%", bookProgress);
    const int pctW = renderer.getTextWidth(kChromeFont, pc);
    renderer.drawText(kChromeFont, rightEdge - pctW, textY, pc, true);
    rightUsed += pctW + 10;
  }
  // star indicator just left of the percent
  if (isStarred) {
    const int starW = renderer.getTextWidth(kChromeFont, "*");
    renderer.drawText(kChromeFont, rightEdge - rightUsed - starW, textY, "*", true);
    rightUsed += starW + 10;
  }

  // chapter title, centred in the space between left and right content
  if (SETTINGS.statusBarTitle != CrossPointSettings::STATUS_BAR_TITLE::HIDE_TITLE && !title.empty()) {
    const int regionL = leftX + pageW + 12;
    const int regionR = rightEdge - rightUsed - 12;
    const int avail = regionR - regionL;
    if (avail > 40) {
      auto t = renderer.truncatedText(kChromeFont, title.c_str(), avail);
      const int tw = renderer.getTextWidth(kChromeFont, t.c_str());
      renderer.drawText(kChromeFont, regionL + (avail - tw) / 2, textY, t.c_str(), true);
    }
  }
}

// ── Popup — paper card with 2px border and Garamond title ────────────────────────
Rect MagnusTheme::drawPopup(const GfxRenderer& renderer, const char* message) const {
  constexpr int margin = 16;
  const int y = static_cast<int>(renderer.getScreenHeight() * 0.075f);
  const int tw = renderer.getTextWidth(kDialogFont, message, EpdFontFamily::REGULAR);
  const int th = renderer.getLineHeight(kDialogFont);
  const int w = tw + margin * 2;
  const int h = th + margin * 2;
  const int x = (renderer.getScreenWidth() - w) / 2;

  renderer.fillRect(x - kRule, y - kRule, w + kRule * 2, h + kRule * 2, true);  // 2px frame
  renderer.fillRect(x, y, w, h, false);                                         // paper fill
  renderer.drawText(kDialogFont, x + (w - tw) / 2, y + margin - 2, message, true, EpdFontFamily::REGULAR);
  renderer.displayBuffer();
  return Rect{x, y, w, h};
}
