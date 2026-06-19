#include "ReadingStatsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "BookStats.h"
#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "ReadingStats.h"
#include "components/UITheme.h"
#include "components/themes/magnus/MagnusGlobals.h"
#include "components/themes/magnus/MagnusTheme.h"
#include "fontIds.h"
#include "util/StringUtils.h"

#include <ctime>

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void ReadingStatsActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void ReadingStatsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
  }
}

// ── Render ────────────────────────────────────────────────────────────────────

void ReadingStatsActivity::render(RenderLock&&) {
  if (SETTINGS.uiTheme == CrossPointSettings::MAGNUS) {
    renderMagnus();
    return;
  }

  const int pageWidth  = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  constexpr int MARGIN = 28;

  renderer.clearScreen();

  const int lhSmall = renderer.getLineHeight(SMALL_FONT_ID);
  const int lhUi10  = renderer.getLineHeight(UI_10_FONT_ID);
  const int lhUi12  = renderer.getLineHeight(UI_12_FONT_ID);
  const int sepMargin = MARGIN + 20;
  const int sepW = pageWidth - 2 * sepMargin;

  // Header: "Reading Stats"
  int y = 24;
  renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_READING_STATS), true, EpdFontFamily::BOLD);
  y += lhUi12 + 6;
  renderer.fillRect(sepMargin, y, sepW, 1);
  y += 14;

  // --- TODAY ---
  renderer.drawText(SMALL_FONT_ID, MARGIN, y, tr(STR_STATS_TODAY));
  y += lhSmall + 4;

  char todayBuf[32];
  StringUtils::formatReadingDuration(todayBuf, sizeof(todayBuf), READ_STATS.todayReadSeconds);
  renderer.drawText(UI_10_FONT_ID, MARGIN, y, todayBuf, true, EpdFontFamily::BOLD);
  y += lhUi10 + 14;
  renderer.fillRect(sepMargin, y, sepW, 1);
  y += 14;

  // --- ALL TIME ---
  renderer.drawText(SMALL_FONT_ID, MARGIN, y, tr(STR_STATS_ALL_TIME));
  y += lhSmall + 4;

  char totalBuf[32];
  StringUtils::formatReadingDuration(totalBuf, sizeof(totalBuf), READ_STATS.totalReadSeconds);
  renderer.drawText(UI_10_FONT_ID, MARGIN, y, totalBuf, true, EpdFontFamily::BOLD);
  {
    const float ppm = READ_STATS.totalReadSeconds > 0
        ? (READ_STATS.totalPagesTurned * 60.0f / READ_STATS.totalReadSeconds)
        : 0.0f;
    char paceBuf[44];
    snprintf(paceBuf, sizeof(paceBuf), "%u pages today  \xC2\xB7  %.1f pg/min",
             (unsigned)READ_STATS.pagesToday, ppm);
    const int pw = renderer.getTextWidth(SMALL_FONT_ID, paceBuf);
    renderer.drawText(SMALL_FONT_ID, pageWidth - MARGIN - pw, y + (lhUi10 - lhSmall) / 2, paceBuf);
  }
  y += lhUi10 + 14;
  renderer.fillRect(sepMargin, y, sepW, 1);
  y += 14;

  // --- STATS GRID: Sessions | Books | Streak | Best ---
  {
    const int colW = (pageWidth - MARGIN * 2) / 4;
    const char* labels[] = {tr(STR_STATS_SESSIONS), tr(STR_STATS_BOOKS_DONE),
                             tr(STR_STATS_STREAK), tr(STR_STATS_BEST_STREAK)};
    char values[4][16];
    snprintf(values[0], sizeof(values[0]), "%u", (unsigned)READ_STATS.totalSessions);
    snprintf(values[1], sizeof(values[1]), "%u", (unsigned)READ_STATS.booksFinished);
    snprintf(values[2], sizeof(values[2]), tr(STR_STATS_DAYS), (unsigned)READ_STATS.currentStreak);
    snprintf(values[3], sizeof(values[3]), tr(STR_STATS_DAYS), (unsigned)READ_STATS.longestStreak);

    // Card outline around the stats grid
    const int cardPad = 8;
    const int cardH = lhUi10 + lhSmall + 8 + cardPad * 2;
    renderer.drawRoundedRect(MARGIN - cardPad, y - cardPad, pageWidth - MARGIN * 2 + cardPad * 2, cardH, 1, 10, true);

    // Thin vertical dividers between columns
    for (int i = 1; i < 4; i++) {
      const int divX = MARGIN + i * colW;
      renderer.drawLine(divX, y - cardPad + 4, divX, y + lhUi10 + lhSmall + 8 + cardPad - 4);
    }

    for (int i = 0; i < 4; i++) {
      const int cx = MARGIN + i * colW + colW / 2;
      const int vw = renderer.getTextWidth(UI_10_FONT_ID, values[i]);
      renderer.drawText(UI_10_FONT_ID, cx - vw / 2, y, values[i], true, EpdFontFamily::BOLD);
      const int lw = renderer.getTextWidth(SMALL_FONT_ID, labels[i]);
      renderer.drawText(SMALL_FONT_ID, cx - lw / 2, y + lhUi10 + 2, labels[i]);
    }
    y += lhUi10 + lhSmall + 18;
  }
  renderer.fillRect(sepMargin, y, sepW, 1);
  y += 14;

  // --- RECENT BOOKS ---
  renderer.drawText(SMALL_FONT_ID, MARGIN, y, tr(STR_STATS_LAST_BOOK));
  y += lhSmall + 6;

  const auto& allBooks = BOOK_STATS.getBooks();
  if (!allBooks.empty()) {
    // Collect up to 5 most-recently-read books
    struct BookRef { const char* title; uint32_t secs; uint8_t progress; uint32_t ts; };
    BookRef recent[5];
    int count = 0;
    for (const auto& kv : allBooks) {
      const auto& e = kv.second;
      if (count < 5) {
        recent[count++] = {e.title, e.totalSeconds, e.progress, e.lastReadTimestamp};
      } else {
        int minIdx = 0;
        for (int j = 1; j < 5; j++) {
          if (recent[j].ts < recent[minIdx].ts) minIdx = j;
        }
        if (e.lastReadTimestamp > recent[minIdx].ts) {
          recent[minIdx] = {e.title, e.totalSeconds, e.progress, e.lastReadTimestamp};
        }
      }
    }
    // Sort descending by timestamp
    for (int i = 1; i < count; i++) {
      BookRef tmp = recent[i];
      int j = i - 1;
      while (j >= 0 && recent[j].ts < tmp.ts) { recent[j + 1] = recent[j]; j--; }
      recent[j + 1] = tmp;
    }

    constexpr int BAR_H = 4;
    const int barW = pageWidth - MARGIN * 2;
    const int maxY = pageHeight - 20;

    for (int i = 0; i < count && y < maxY; i++) {
      char timeBuf[24];
      StringUtils::formatReadingDuration(timeBuf, sizeof(timeBuf), recent[i].secs);
      const int timeW = renderer.getTextWidth(SMALL_FONT_ID, timeBuf);
      const int titleMaxW = pageWidth - MARGIN * 2 - timeW - 8;
      const std::string titleStr = renderer.truncatedText(
          SMALL_FONT_ID, recent[i].title, titleMaxW, EpdFontFamily::BOLD);
      renderer.drawText(SMALL_FONT_ID, MARGIN, y, titleStr.c_str(), true, EpdFontFamily::BOLD);
      renderer.drawText(SMALL_FONT_ID, pageWidth - MARGIN - timeW, y, timeBuf);
      y += lhSmall + 3;

      // Progress bar + percentage
      char progBuf[8];
      snprintf(progBuf, sizeof(progBuf), "%u%%", (unsigned)recent[i].progress);
      const int progW = renderer.getTextWidth(SMALL_FONT_ID, progBuf);
      const int progBarW = barW - progW - 8;

      renderer.fillRect(MARGIN, y, progBarW, 1);
      renderer.fillRect(MARGIN, y + BAR_H - 1, progBarW, 1);
      renderer.fillRect(MARGIN, y, 1, BAR_H);
      renderer.fillRect(MARGIN + progBarW - 1, y, 1, BAR_H);
      const int filledW = static_cast<int>((progBarW - 2) * recent[i].progress / 100);
      if (filledW > 0) {
        renderer.fillRect(MARGIN + 1, y + 1, filledW, BAR_H - 2);
      }
      renderer.drawText(SMALL_FONT_ID, MARGIN + progBarW + 6, y - (lhSmall - BAR_H) / 2, progBuf);
      y += BAR_H + 10;
    }
  } else {
    renderer.drawText(UI_10_FONT_ID, MARGIN, y, tr(STR_STATS_NO_BOOKS), true, EpdFontFamily::REGULAR);
  }

  // Button hint: Back to exit
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), nullptr, nullptr, nullptr);
  GUI.drawButtonHints(renderer, labels.btn1, nullptr, nullptr, nullptr);

  renderer.displayBuffer();
}

// ── Magnus "Archivist's Record" ────────────────────────────────────────────────
// Surfaces tracked data: today + pages-today + all-time pace, a seven-day reading
// chart, totals/sessions/finished, and per-book recents. Reading "speed" is
// pages-per-minute (pages turned over reading time); word counts aren't tracked.
void ReadingStatsActivity::renderMagnus() {
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();
  const int PAD = magnus::SIDE_PAD;
  renderer.clearScreen();

  // status strip
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
    const bool showPct = SETTINGS.hideBatteryPercentage != CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_ALWAYS;
    const int battX = sw - 12 - MagnusMetrics::values.batteryWidth;
    GUI.drawBatteryRight(renderer,
                         Rect{battX, 7, MagnusMetrics::values.batteryWidth, MagnusMetrics::values.batteryHeight}, showPct);
    magnus::rule(renderer, 0, 29, sw, 1);
  }

  // header
  magnus::eyebrow(renderer, PAD, 44, "PERSONNEL FILE");
  renderer.drawText(magnus::FONT_TITLE, PAD, 58, "Archivist's Record", true, EpdFontFamily::REGULAR);
  const int hb = 58 + renderer.getLineHeight(magnus::FONT_TITLE) + 8;
  magnus::rule(renderer, 0, hb, sw);

  int y = hb + 16;

  // LOGGED TODAY (left) + UNBROKEN VIGIL (right)
  magnus::eyebrow(renderer, PAD, y, "LOGGED TODAY");
  {
    const char* vl = "UNBROKEN VIGIL";
    const int vlw = magnus::trackedWidth(renderer, magnus::FONT_EYEBROW, vl, magnus::TRACK_EYEBROW);
    magnus::tracked(renderer, magnus::FONT_EYEBROW, sw - PAD - vlw, y, vl, magnus::TRACK_EYEBROW);
  }
  y += 20;
  char todayBuf[32];
  StringUtils::formatReadingDuration(todayBuf, sizeof(todayBuf), READ_STATS.todayReadSeconds);
  renderer.drawText(magnus::FONT_DISPLAY, PAD, y, todayBuf, true, EpdFontFamily::REGULAR);
  {
    char vigil[24];
    snprintf(vigil, sizeof(vigil), "%u days", (unsigned)READ_STATS.currentStreak);
    const int vw = renderer.getTextWidth(magnus::FONT_DISPLAY, vigil);
    renderer.drawText(magnus::FONT_DISPLAY, sw - PAD - vw, y, vigil, true, EpdFontFamily::REGULAR);
  }
  y += renderer.getLineHeight(magnus::FONT_DISPLAY) + 2;
  // pages today + all-time pace (pages per minute)
  {
    const float ppm = READ_STATS.totalReadSeconds > 0
        ? (READ_STATS.totalPagesTurned * 60.0f / READ_STATS.totalReadSeconds)
        : 0.0f;
    char cap[56];
    snprintf(cap, sizeof(cap), "%u PAGES TODAY  \xC2\xB7  %.1f PAGES/MIN",
             (unsigned)READ_STATS.pagesToday, ppm);
    magnus::tracked(renderer, magnus::FONT_CHROME, PAD, y, cap, 1);
  }
  y += 18;
  magnus::hairline(renderer, PAD, y, sw - 2 * PAD);
  y += 16;

  // THIS WEEK — seven daily reading-time bars (today on the right, drawn solid)
  magnus::eyebrow(renderer, PAD, y, "THIS WEEK");
  y += 18;
  {
    constexpr int N = 7;
    const int chartH = 30;
    const int gap = 6;
    const int avail = sw - 2 * PAD;
    const int barW = (avail - (N - 1) * gap) / N;
    const int32_t today = ReadingStats::currentDayNumber();
    uint32_t vals[N] = {};
    uint32_t maxV = 1;
    for (int i = 0; i < N; i++) {
      uint32_t v = 0;
      if (today >= 0) {
        const int32_t d = today - (N - 1) + i;
        if (d >= 0) v = READ_STATS.dailySeconds[d % 7];
      }
      vals[i] = v;
      if (v > maxV) maxV = v;
    }
    const int baseY = y + chartH;
    for (int i = 0; i < N; i++) {
      const int bx = PAD + i * (barW + gap);
      int bh = static_cast<int>(static_cast<uint64_t>(vals[i]) * chartH / maxV);
      if (vals[i] > 0 && bh < 2) bh = 2;
      magnus::hairline(renderer, bx, baseY, barW);  // baseline tick
      if (bh > 0) renderer.fillRect(bx, baseY - bh, barW, bh);
      if (i == N - 1) renderer.fillRect(bx, baseY + 3, barW, 2);  // underline today
    }
    y = baseY + 16;
  }

  // three framed cards: ALL TIME / SESSIONS / FINISHED
  {
    const int gap = 12;
    const int cardW = (sw - 2 * PAD - 2 * gap) / 3;
    const int cardH = 68;
    // Compact duration ("3h 12m") so the value fits a card; the long form overflowed.
    char a[16];
    {
      const uint32_t mins = READ_STATS.totalReadSeconds / 60;
      if (mins >= 60)
        snprintf(a, sizeof(a), "%uh %um", (unsigned)(mins / 60), (unsigned)(mins % 60));
      else
        snprintf(a, sizeof(a), "%um", (unsigned)mins);
    }
    char b[16];
    snprintf(b, sizeof(b), "%u", (unsigned)READ_STATS.totalSessions);
    char c[16];
    snprintf(c, sizeof(c), "%u", (unsigned)READ_STATS.booksFinished);
    const char* vals[3] = {a, b, c};
    const char* labs[3] = {"ALL TIME", "SESSIONS", "FINISHED"};
    for (int i = 0; i < 3; i++) {
      const int cx = PAD + i * (cardW + gap);
      magnus::frame(renderer, Rect{cx, y, cardW, cardH}, 1);
      auto vt = renderer.truncatedText(magnus::FONT_TITLE, vals[i], cardW - 10, EpdFontFamily::REGULAR);
      const int vwi = renderer.getTextWidth(magnus::FONT_TITLE, vt.c_str());
      renderer.drawText(magnus::FONT_TITLE, cx + (cardW - vwi) / 2, y + 8, vt.c_str(), true, EpdFontFamily::REGULAR);
      magnus::centerTracked(renderer, magnus::FONT_CHROME, cx + cardW / 2, y + cardH - 16, labs[i], 1);
    }
    y += cardH + 18;
  }

  // RECENTLY READ
  magnus::eyebrow(renderer, PAD, y, "THE STACKS \xC2\xB7 RECENTLY READ");
  y += 22;

  // footer geometry — sits ABOVE the button-hint bar with a clear gap (the hint bar also
  // draws a 2px rule above itself, so leave headroom or labels clip into it).
  const int footerH = 50;
  const int footerTop = sh - MagnusMetrics::values.buttonHintsHeight - footerH - 12;
  const int listMaxY = footerTop - 12;

  const auto& allBooks = BOOK_STATS.getBooks();
  int catalogued = (int)allBooks.size();
  int inProgress = 0;
  for (const auto& kv : allBooks) {
    const auto& e = kv.second;
    if (e.progress > 0 && e.progress < 100) inProgress++;
  }

  if (!allBooks.empty()) {
    struct BookRef { const char* title; uint32_t secs; uint8_t progress; uint32_t ts; };
    BookRef recent[5];
    int count = 0;
    for (const auto& kv : allBooks) {
      const auto& e = kv.second;
      if (count < 5) {
        recent[count++] = {e.title, e.totalSeconds, e.progress, e.lastReadTimestamp};
      } else {
        int minIdx = 0;
        for (int j = 1; j < 5; j++)
          if (recent[j].ts < recent[minIdx].ts) minIdx = j;
        if (e.lastReadTimestamp > recent[minIdx].ts)
          recent[minIdx] = {e.title, e.totalSeconds, e.progress, e.lastReadTimestamp};
      }
    }
    for (int i = 1; i < count; i++) {
      BookRef tmp = recent[i];
      int j = i - 1;
      while (j >= 0 && recent[j].ts < tmp.ts) { recent[j + 1] = recent[j]; j--; }
      recent[j + 1] = tmp;
    }
    const int rowH = 40;
    for (int i = 0; i < count && y + rowH < listMaxY; i++) {
      char timeBuf[24];
      StringUtils::formatReadingDuration(timeBuf, sizeof(timeBuf), recent[i].secs);
      const int timeW = renderer.getTextWidth(magnus::FONT_CHROME, timeBuf);
      const int titleMaxW = sw - 2 * PAD - timeW - 10;
      auto titleStr = renderer.truncatedText(magnus::FONT_BODY, recent[i].title, titleMaxW, EpdFontFamily::REGULAR);
      renderer.drawText(magnus::FONT_BODY, PAD, y, titleStr.c_str(), true, EpdFontFamily::REGULAR);
      renderer.drawText(magnus::FONT_CHROME, sw - PAD - timeW, y + 2, timeBuf, true);
      const int by = y + renderer.getLineHeight(magnus::FONT_BODY) + 4;
      magnus::ditherBar(renderer, Rect{PAD, by, sw - 2 * PAD, 8}, recent[i].progress);
      y += rowH;
    }
  } else {
    renderer.drawText(magnus::FONT_BODY, PAD, y, "No statements on record.", true);
  }

  // footer: catalogued · the Eye · in progress
  magnus::rule(renderer, 0, footerTop, sw, 1);
  {
    char cat[16];
    snprintf(cat, sizeof(cat), "%d", catalogued);
    char ip[16];
    snprintf(ip, sizeof(ip), "%d", inProgress);
    const int third = sw / 3;
    // left
    const int cw = renderer.getTextWidth(magnus::FONT_TITLE, cat);
    renderer.drawText(magnus::FONT_TITLE, third / 2 - cw / 2, footerTop + 8, cat, true, EpdFontFamily::REGULAR);
    magnus::centerTracked(renderer, magnus::FONT_CHROME, third / 2, footerTop + footerH - 12, "CATALOGUED", 1);
    // center eye
    magnus::eye(renderer, sw / 2, footerTop + footerH / 2, 34, 22, false, 2);
    // right
    const int iw = renderer.getTextWidth(magnus::FONT_TITLE, ip);
    renderer.drawText(magnus::FONT_TITLE, sw - third / 2 - iw / 2, footerTop + 8, ip, true, EpdFontFamily::REGULAR);
    magnus::centerTracked(renderer, magnus::FONT_CHROME, sw - third / 2, footerTop + footerH - 12, "IN PROGRESS", 1);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), nullptr, nullptr, nullptr);
  GUI.drawButtonHints(renderer, labels.btn1, nullptr, nullptr, nullptr);

  renderer.displayBuffer();
}
