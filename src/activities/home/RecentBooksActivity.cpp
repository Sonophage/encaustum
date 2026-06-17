#include "RecentBooksActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "components/themes/magnus/MagnusGlobals.h"
#include "components/themes/magnus/MagnusTheme.h"
#include "fontIds.h"

namespace {
// Must match the home screen's CP_COVER_H so we reuse the same cached thumbnails
constexpr int THUMB_H = 188;
}

void RecentBooksActivity::loadRecentBooks() {
  recentBooks.clear();
  const auto& books = RECENT_BOOKS.getBooks();
  recentBooks.reserve(books.size());
  for (const auto& book : books) {
    if (!Storage.exists(book.path.c_str())) continue;
    recentBooks.push_back(book);
  }
}

int RecentBooksActivity::totalPages() const {
  if (recentBooks.empty()) return 1;
  return (static_cast<int>(recentBooks.size()) + itemsPerPage - 1) / itemsPerPage;
}

void RecentBooksActivity::ensurePageForIndex() {
  if (itemsPerPage <= 0) return;
  const int page = selectorIndex / itemsPerPage;
  pageOffset = page * itemsPerPage;
}

void RecentBooksActivity::onEnter() {
  Activity::onEnter();
  loadRecentBooks();
  selectorIndex = 0;
  pageOffset = 0;
  requestUpdate();
}

void RecentBooksActivity::onExit() {
  Activity::onExit();
  recentBooks.clear();
}

void RecentBooksActivity::loop() {
  const int total = static_cast<int>(recentBooks.size());
  if (total == 0) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      onGoHome();
    }
    return;
  }

  // D-pad navigation: left/right within row, up/down between rows
  buttonNavigator.onNext([this, total] {
    // Next = move right, wrap to next row
    selectorIndex = (selectorIndex + 1) % total;
    ensurePageForIndex();
    requestUpdate();
  });

  buttonNavigator.onPrevious([this, total] {
    selectorIndex = (selectorIndex - 1 + total) % total;
    ensurePageForIndex();
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (selectorIndex < total) {
      onSelectBook(recentBooks[selectorIndex].path);
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
  }
}

void RecentBooksActivity::renderCover(int bookIdx, int gridCol, int gridRow,
                                       int cardW, int cardH, int startX, int startY, bool selected) {
  const int cx = startX + gridCol * (cardW + COVER_GAP);
  const int cy = startY + gridRow * (cardH + 30);  // 30 = space for title below

  // Cover border — thicker when selected
  if (selected) {
    renderer.drawRoundedRect(cx - 1, cy - 1, cardW + 2, cardH + 2, 3, COVER_R + 1, true);
  } else {
    renderer.drawRoundedRect(cx, cy, cardW, cardH, 1, COVER_R, true);
  }

  // Cover thumbnail
  const RecentBook& book = recentBooks[bookIdx];
  if (!book.coverBmpPath.empty()) {
    const std::string thumbPath = UITheme::getCoverThumbPath(book.coverBmpPath, THUMB_H);
    FsFile f;
    if (Storage.openFileForRead("RBA", thumbPath, f)) {
      Bitmap bmp(f);
      if (bmp.parseHeaders() == BmpReaderError::Ok) {
        int bmpW = std::min((int)bmp.getWidth(), cardW - 4);
        int bmpH = std::min((int)bmp.getHeight(), cardH - 4);
        renderer.drawBitmap(bmp, cx + (cardW - bmpW) / 2, cy + 2, bmpW, bmpH);
      }
      f.close();
    }
  }

  // Progress bar at bottom of card
  const int barY = cy + cardH - 5;
  const int barX = cx + 4;
  const int barW = cardW - 8;
  renderer.fillRect(barX, barY, barW, 4, false);
  const int fillW = barW * book.progressPercent / 100;
  if (fillW > 1) renderer.fillRect(barX, barY, fillW, 4);

  // Title below card (truncated, centered, bold when selected)
  const int titleY = cy + cardH + 4;
  const auto titleStyle = selected ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
  auto title = renderer.truncatedText(SMALL_FONT_ID, book.title.c_str(), cardW - 2, titleStyle);
  const int titleW = renderer.getTextWidth(SMALL_FONT_ID, title.c_str(), titleStyle);
  renderer.drawText(SMALL_FONT_ID, cx + (cardW - titleW) / 2, titleY, title.c_str(), true, titleStyle);
}

void RecentBooksActivity::render(RenderLock&&) {
  if (SETTINGS.uiTheme == CrossPointSettings::MAGNUS) {
    renderMagnus();
    return;
  }
  renderer.clearScreen();

  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_MENU_RECENT_BOOKS));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (recentBooks.empty()) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_RECENT_BOOKS));
  } else {
    // Calculate grid layout
    const int sidePad = 14;
    const int areaW = pageWidth - 2 * sidePad;
    const int totalGapW = COVER_GAP * (COLS - 1);
    const int cardW = (areaW - totalGapW) / COLS;
    const int cardH = (int)(cardW * 1.4f);
    const int rowH = cardH + 30;  // card + title space
    const int rows = contentHeight / rowH;
    itemsPerPage = std::max(1, rows * COLS);

    const int total = static_cast<int>(recentBooks.size());
    const int endIdx = std::min(pageOffset + itemsPerPage, total);

    for (int i = pageOffset; i < endIdx; i++) {
      const int localIdx = i - pageOffset;
      const int col = localIdx % COLS;
      const int row = localIdx / COLS;
      renderCover(i, col, row, cardW, cardH, sidePad, contentTop + 4, i == selectorIndex);
    }

    // Page indicator if multiple pages
    if (totalPages() > 1) {
      char pageStr[16];
      snprintf(pageStr, sizeof(pageStr), "%d/%d", (selectorIndex / itemsPerPage) + 1, totalPages());
      const int pw = renderer.getTextWidth(SMALL_FONT_ID, pageStr);
      renderer.drawText(SMALL_FONT_ID, pageWidth - pw - 10,
                        pageHeight - metrics.buttonHintsHeight - 16, pageStr, true);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

// ── Magnus theme: "The Stacks / Recently Opened" 2-column cover grid ──────────
namespace {
// Small inverted (filled) chip with tracked white caps — READING / FILED ribbons.
void chip(const GfxRenderer& r, int x, int y, const char* label) {
  const int tw = magnus::trackedWidth(r, magnus::FONT_CHROME, label, 1);
  const int h = r.getLineHeight(magnus::FONT_CHROME) + 4;
  const int w = tw + 12;
  r.fillRect(x, y, w, h, true);
  magnus::tracked(r, magnus::FONT_CHROME, x + 6, y + 2, label, 1, false);
}
}  // namespace

void RecentBooksActivity::renderMagnusCard(int bookIdx, int cardX, int cardY, int cardW, int cardH, bool selected) {
  const RecentBook& book = recentBooks[bookIdx];
  const bool done = book.progressPercent >= 100;

  // card frame (thicker when selected)
  magnus::frame(renderer, Rect{cardX, cardY, cardW, cardH}, selected ? 3 : 1);

  bool coverDrawn = false;
  if (!book.coverBmpPath.empty()) {
    const std::string thumbPath = UITheme::getCoverThumbPath(book.coverBmpPath, THUMB_H);
    FsFile f;
    if (Storage.openFileForRead("RBA_MAG", thumbPath, f)) {
      Bitmap bmp(f);
      if (bmp.parseHeaders() == BmpReaderError::Ok) {
        const int bmpW = std::min((int)bmp.getWidth(), cardW - 6);
        const int bmpH = std::min((int)bmp.getHeight(), cardH - 6);
        renderer.drawBitmap(bmp, cardX + (cardW - bmpW) / 2, cardY + 3, bmpW, bmpH);
        coverDrawn = true;
      }
      f.close();
    }
  }

  if (!coverDrawn) {
    // archival placeholder: texture by reading state, serif title, the Eye, fonds beneath
    const Rect inner{cardX + 3, cardY + 3, cardW - 6, cardH - 6};
    if (done)
      ;  // finished → clean paper card
    else if (book.progressPercent > 0)
      magnus::hatch(renderer, inner);  // in progress
    else
      magnus::dots(renderer, inner);  // not yet opened

    // case number, top
    const std::string code = magnus::bookCode(book.path);
    magnus::centerTracked(renderer, magnus::FONT_CHROME, cardX + cardW / 2, cardY + 10, code.c_str(), 1);

    // title (serif, wrapped, with a solid backing for legibility over texture)
    auto lines = renderer.wrappedText(magnus::FONT_TITLE, book.title.c_str(), cardW - 18, 3, EpdFontFamily::REGULAR);
    const int tlH = renderer.getLineHeight(magnus::FONT_TITLE);
    int ty = cardY + 30;
    for (const auto& ln : lines) {
      const int lw = renderer.getTextWidth(magnus::FONT_TITLE, ln.c_str());
      const int lx = cardX + (cardW - lw) / 2;
      renderer.fillRect(lx - 4, ty - 2, lw + 8, tlH + 2, false);
      renderer.drawText(magnus::FONT_TITLE, lx, ty, ln.c_str(), true, EpdFontFamily::REGULAR);
      ty += tlH;
    }

    // the Eye, centred in the lower half
    const int eyeCy = cardY + cardH / 2 + 10;
    magnus::eye(renderer, cardX + cardW / 2, eyeCy, 46, 30, false, 2);

    // fonds beneath the Eye (kept clear of the ribbon + progress strip below)
    const char* fonds = magnus::fondsFor(book.path);
    magnus::centerTracked(renderer, magnus::FONT_CHROME, cardX + cardW / 2, eyeCy + 24, fonds, 1);
  }

  // progress strip along the bottom edge (inside the frame)
  magnus::ditherBar(renderer, Rect{cardX + 3, cardY + cardH - 8, cardW - 6, 5}, book.progressPercent);

  // ribbons
  if (done) {
    const int tw = magnus::trackedWidth(renderer, magnus::FONT_CHROME, "FILED", 1);
    chip(renderer, cardX + cardW - (tw + 12) - 4, cardY + 4, "FILED");
  } else if (bookIdx == 0) {
    const int h = renderer.getLineHeight(magnus::FONT_CHROME) + 4;
    chip(renderer, cardX + 4, cardY + cardH - 8 - h - 2, "READING");
  }

  // caption beneath the card: case number (left) + percent/DONE (right), so neither truncates
  const int capY = cardY + cardH + 6;
  char tail[8];
  if (done)
    snprintf(tail, sizeof(tail), "DONE");
  else
    snprintf(tail, sizeof(tail), "%d%%", book.progressPercent);
  const int tw = renderer.getTextWidth(magnus::FONT_CHROME, tail);
  const std::string code = magnus::bookCode(book.path);
  auto codeT = renderer.truncatedText(magnus::FONT_CHROME, code.c_str(), cardW - tw - 10);
  renderer.drawText(magnus::FONT_CHROME, cardX, capY, codeT.c_str(), true);
  renderer.drawText(magnus::FONT_CHROME, cardX + cardW - tw, capY, tail, true);
}

void RecentBooksActivity::renderMagnus() {
  renderer.clearScreen();
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();
  const int PAD = magnus::SIDE_PAD;

  // ── Status strip ────────────────────────────────────────────────────────────
  {
    char clk[8];
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    if (t.tm_year >= 125)
      snprintf(clk, sizeof(clk), "%02d:%02d", t.tm_hour, t.tm_min);
    else
      snprintf(clk, sizeof(clk), "--:--");
    renderer.drawText(magnus::FONT_CHROME, PAD, 8, clk, true);
    const bool showPct =
        SETTINGS.hideBatteryPercentage != CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_ALWAYS;
    const int battX = sw - 12 - MagnusMetrics::values.batteryWidth;
    GUI.drawBatteryRight(
        renderer, Rect{battX, 7, MagnusMetrics::values.batteryWidth, MagnusMetrics::values.batteryHeight}, showPct);
    magnus::rule(renderer, 0, 29, sw, 1);
  }

  // ── Title block ───────────────────────────────────────────────────────────────
  magnus::eyebrow(renderer, PAD, 44, "THE STACKS");
  renderer.drawText(magnus::FONT_TITLE, PAD, 58, "Recently Opened", true, EpdFontFamily::REGULAR);
  {
    // right: catalogue count
    char cnt[24];
    snprintf(cnt, sizeof(cnt), "%d CATALOGUED", (int)recentBooks.size());
    const int cw = renderer.getTextWidth(magnus::FONT_CHROME, cnt);
    renderer.drawText(magnus::FONT_CHROME, sw - PAD - cw, 64, cnt, true);
  }
  const int titleBottom = 58 + renderer.getLineHeight(magnus::FONT_TITLE) + 8;
  magnus::rule(renderer, 0, titleBottom, sw);

  const int hintsH = MagnusMetrics::values.buttonHintsHeight;

  if (recentBooks.empty()) {
    magnus::eyebrow(renderer, PAD, titleBottom + 40, "THE STACKS ARE EMPTY");
    renderer.drawText(magnus::FONT_BODY, PAD, titleBottom + 66, "No statements on record.", true);
  } else {
    // ── Grid ──────────────────────────────────────────────────────────────────
    const int gridTop = titleBottom + 16;
    constexpr int cols = 2;
    constexpr int gap = 18;
    const int areaW = sw - 2 * PAD;
    const int cardW = (areaW - gap * (cols - 1)) / cols;
    const int cardH = (int)(cardW * 1.42f);
    const int captionH = renderer.getLineHeight(magnus::FONT_CHROME) + 6;
    const int rowH = cardH + captionH + 16;
    const int contentBottom = sh - hintsH - 10;
    const int rows = std::max(1, (contentBottom - gridTop) / rowH);
    itemsPerPage = std::max(1, rows * cols);

    const int total = static_cast<int>(recentBooks.size());
    const int endIdx = std::min(pageOffset + itemsPerPage, total);
    for (int i = pageOffset; i < endIdx; i++) {
      const int local = i - pageOffset;
      const int col = local % cols;
      const int row = local / cols;
      const int cx = PAD + col * (cardW + gap);
      const int cy = gridTop + row * rowH;
      renderMagnusCard(i, cx, cy, cardW, cardH, i == selectorIndex);
    }

    // page indicator
    if (totalPages() > 1) {
      char pageStr[16];
      snprintf(pageStr, sizeof(pageStr), "%d / %d", (selectorIndex / itemsPerPage) + 1, totalPages());
      const int pw = renderer.getTextWidth(magnus::FONT_CHROME, pageStr);
      renderer.drawText(magnus::FONT_CHROME, sw - PAD - pw, sh - hintsH - 18, pageStr, true);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
