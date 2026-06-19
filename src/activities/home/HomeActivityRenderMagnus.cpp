#include "HomeActivity.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>

#include "BookStats.h"
#include "CrossPetSettings.h"
#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "activities/tools/ReadingStatsActivity.h"
#include "components/UITheme.h"
#include "components/themes/magnus/MagnusGlobals.h"
#include "components/themes/magnus/MagnusTheme.h"
#include "fontIds.h"

// ── Magnus homepage — cover-led "currently reading" hero + "from the stacks"
//    list + bottom destination tabs. Has its own navigation loop (loopMagnus):
//      selectorIndex 0           → hero (resume statement)
//      selectorIndex 1..stacks   → a stacks row
//      selectorIndex barStart..  → a bottom tab (Archive / Reading / Stats / Settings)
namespace {
constexpr int MH_STATUS_H = 30;
constexpr int MH_PAD = magnus::SIDE_PAD;
constexpr int MH_COVER_W = 108;
constexpr int MH_COVER_H = 162;
constexpr int MH_TABBAR_H = 58;
constexpr int MH_MAX_STACKS = 5;   // Magnus stacks cap (own loop; decoupled from CrossPet)
constexpr int MH_STACK_ROW = 76;   // two-line rows: serif title + mono fonds line (Garamond is tall)
constexpr int MH_BOTTOM_ITEMS = 4;

using magnus::bookCode;
using magnus::fondsFor;

void timeStr(char* buf, size_t n) {
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  if (t.tm_year >= 125)
    snprintf(buf, n, "%02d:%02d", t.tm_hour, t.tm_min);
  else
    snprintf(buf, n, "--:--");
}
}  // namespace

void HomeActivity::renderMagnus() {
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();
  renderer.clearScreen();

  const int recentCount = std::max(0, std::min(MH_MAX_STACKS, static_cast<int>(recentBooks.size()) - 1));
  const int barStart = 1 + recentCount;

  // ── Status strip ────────────────────────────────────────────────────────────
  {
    char clk[8];
    timeStr(clk, sizeof(clk));
    renderer.drawText(magnus::FONT_CHROME, MH_PAD, 8, clk, true);
    const bool showPct =
        SETTINGS.hideBatteryPercentage != CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_ALWAYS;
    const int battX = sw - 12 - MagnusMetrics::values.batteryWidth;
    GUI.drawBatteryRight(renderer, Rect{battX, 7, MagnusMetrics::values.batteryWidth, MagnusMetrics::values.batteryHeight},
                         showPct);
    magnus::rule(renderer, 0, MH_STATUS_H - 1, sw, 1);
  }

  // ── Empty state ───────────────────────────────────────────────────────────────
  if (recentBooks.empty()) {
    magnus::eyebrow(renderer, MH_PAD, MH_STATUS_H + 40, "THE STACKS ARE EMPTY");
    auto line = std::string("Open the Archive below to begin.");
    renderer.drawText(magnus::FONT_BODY, MH_PAD, MH_STATUS_H + 66, line.c_str(), true);
  } else {
    const RecentBook& hero = recentBooks[0];
    const int heroTop = MH_STATUS_H + 14;

    // eyebrow + code stamp
    magnus::eyebrow(renderer, MH_PAD, heroTop, "CURRENTLY READING");
    {
      const std::string code = bookCode(hero.path);
      const int cw = magnus::trackedWidth(renderer, magnus::FONT_EYEBROW, code.c_str(), magnus::TRACK_EYEBROW);
      magnus::tracked(renderer, magnus::FONT_EYEBROW, sw - MH_PAD - cw, heroTop, code.c_str(), magnus::TRACK_EYEBROW);
    }

    // cover (left)
    const int coverX = MH_PAD;
    const int coverY = heroTop + 22;
    magnus::frame(renderer, Rect{coverX, coverY, MH_COVER_W, MH_COVER_H}, 2);
    bool coverDrawn = false;
    if (!hero.coverBmpPath.empty()) {
      const std::string thumb = UITheme::getCoverThumbPath(hero.coverBmpPath, MH_COVER_H);
      FsFile f;
      if (Storage.openFileForRead("MAGNUS_HOME", thumb, f)) {
        Bitmap bmp(f);
        if (bmp.parseHeaders() == BmpReaderError::Ok) {
          const int aw = std::min((int)bmp.getWidth(), MH_COVER_W - 4);
          const int ah = std::min((int)bmp.getHeight(), MH_COVER_H - 4);
          renderer.drawBitmap(bmp, coverX + 2 + (MH_COVER_W - 4 - aw) / 2, coverY + 2, aw, ah);
          coverDrawn = true;
        }
        f.close();
      }
    }
    if (!coverDrawn) {
      // dithered placeholder with the title set into it
      magnus::ditherFill(renderer, Rect{coverX + 3, coverY + 3, MH_COVER_W - 6, MH_COVER_H - 6}, Color::LightGray);
      auto tl = renderer.wrappedText(magnus::FONT_BODY, hero.title.c_str(), MH_COVER_W - 16, 3, EpdFontFamily::REGULAR);
      int ty = coverY + 14;
      for (const auto& ln : tl) {
        renderer.fillRect(coverX + 6, ty - 2, renderer.getTextWidth(magnus::FONT_BODY, ln.c_str()) + 6,
                          renderer.getLineHeight(magnus::FONT_BODY) + 2, false);
        renderer.drawText(magnus::FONT_BODY, coverX + 8, ty, ln.c_str(), true);
        ty += renderer.getLineHeight(magnus::FONT_BODY) + 2;
      }
    }

    // meta (right of cover)
    const int metaX = coverX + MH_COVER_W + 16;
    const int metaW = sw - metaX - MH_PAD;
    int my = coverY + 2;
    auto titleLines = renderer.wrappedText(magnus::FONT_TITLE, hero.title.c_str(), metaW, 2, EpdFontFamily::REGULAR);
    for (const auto& ln : titleLines) {
      renderer.drawText(magnus::FONT_TITLE, metaX, my, ln.c_str(), true, EpdFontFamily::REGULAR);
      my += renderer.getLineHeight(magnus::FONT_TITLE);
    }
    if (!hero.author.empty()) {
      my += 2;
      // "Statement of {author}." — the archive frames every book as a recorded statement.
      std::string statement = std::string("Statement of ") + hero.author + ".";
      auto a = renderer.truncatedText(magnus::FONT_BODY, statement.c_str(), metaW, EpdFontFamily::ITALIC);
      renderer.drawText(magnus::FONT_BODY, metaX, my, a.c_str(), true, EpdFontFamily::ITALIC);
    }

    // progress band — full width, below the cover row (its own space, no column cramping)
    const int progY = coverY + MH_COVER_H + 12;
    magnus::ditherBar(renderer, Rect{MH_PAD, progY, sw - 2 * MH_PAD, 12}, hero.progressPercent);
    {
      // left: percent + estimated minutes left (uppercased to read as archive chrome)
      char meta[48];
      const auto* bs = BOOK_STATS.getBook(hero.path.c_str());
      const uint32_t mins = bs ? bs->totalSeconds / 60 : 0;
      if (mins > 0 && hero.progressPercent > 0 && hero.progressPercent < 100) {
        uint32_t est = mins * (100 - hero.progressPercent) / hero.progressPercent;
        snprintf(meta, sizeof(meta), "%d%%  \xC2\xB7  %u MIN LEFT", hero.progressPercent, (unsigned)est);
      } else {
        snprintf(meta, sizeof(meta), "%d%%", hero.progressPercent);
      }
      renderer.drawText(magnus::FONT_CHROME, MH_PAD, progY + 16, meta, true);
      // right: the fonds this book is filed under
      const char* fonds = fondsFor(hero.path);
      const int fw = magnus::trackedWidth(renderer, magnus::FONT_CHROME, fonds, magnus::TRACK_TAB);
      magnus::tracked(renderer, magnus::FONT_CHROME, sw - MH_PAD - fw, progY + 16, fonds, magnus::TRACK_TAB);
    }

    // RESUME STATEMENT pill — full width below the progress line. Filled when hero selected, else outlined.
    const int pillY = progY + 16 + renderer.getLineHeight(magnus::FONT_CHROME) + 10;
    const int pillH = 36;
    const Rect pill{MH_PAD, pillY, sw - 2 * MH_PAD, pillH};
    const bool heroSel = (selectorIndex == 0);
    if (heroSel) {
      magnus::invFill(renderer, pill);
    } else {
      magnus::frame(renderer, pill, 1);
    }
    {
      const char* label = "RESUME STATEMENT";
      const int lw = magnus::trackedWidth(renderer, magnus::FONT_CHROME, label, magnus::TRACK_EYEBROW);
      const int caretW = renderer.getTextWidth(magnus::FONT_CHROME, "\xE2\x96\xB8 ");
      const int tx = pill.x + (pill.width - lw - caretW) / 2;
      const int tyy = pill.y + (pillH - renderer.getLineHeight(magnus::FONT_CHROME)) / 2;
      renderer.drawText(magnus::FONT_CHROME, tx, tyy, "\xE2\x96\xB8 ", !heroSel);
      magnus::tracked(renderer, magnus::FONT_CHROME, tx + caretW, tyy, label, magnus::TRACK_EYEBROW, !heroSel);
    }

    // 2px rule under hero
    const int afterHero = pillY + pillH + 14;
    magnus::rule(renderer, 0, afterHero, sw);

    // ── From the stacks ─────────────────────────────────────────────────────────
    int sy = afterHero + 16;
    if (recentCount > 0) {
      magnus::eyebrow(renderer, MH_PAD, sy, "FROM THE STACKS");
    }
    sy += 20;
    // Fixed in-row offsets — FONT_TITLE's reported line height (~40px) is larger than the
    // glyph cap, so positioning the fonds line by titleLineH pushed it outside the 60px
    // selection fill (invisible as white-on-white when the row was selected).
    constexpr int MH_TITLE_Y = 6;   // title top within the row
    constexpr int MH_SUB_Y = 48;    // fonds line top within the row (clears the tall serif title)
    for (int i = 1; i <= recentCount; i++) {
      const RecentBook& b = recentBooks[i];
      const int rowY = sy + (i - 1) * MH_STACK_ROW;
      const bool sel = (selectorIndex == i);
      if (sel) magnus::invFill(renderer, Rect{0, rowY, sw, MH_STACK_ROW});

      // right: percent, or DONE when finished
      char pct[8];
      const bool done = b.progressPercent >= 100;
      if (done)
        snprintf(pct, sizeof(pct), "DONE");
      else
        snprintf(pct, sizeof(pct), "%d%%", b.progressPercent);
      const int pw = renderer.getTextWidth(magnus::FONT_CHROME, pct);
      const int titleW = sw - 2 * MH_PAD - pw - 16;

      // title (serif) + percent aligned to it
      auto t = renderer.truncatedText(magnus::FONT_TITLE, b.title.c_str(), titleW, EpdFontFamily::REGULAR);
      renderer.drawText(magnus::FONT_TITLE, MH_PAD, rowY + MH_TITLE_Y, t.c_str(), !sel, EpdFontFamily::REGULAR);
      renderer.drawText(magnus::FONT_CHROME, sw - MH_PAD - pw, rowY + MH_TITLE_Y + 4, pct, !sel);

      // fonds line (mono): "MAG-XXXXXXX · THE SPIRAL"
      std::string sub = bookCode(b.path) + "  \xC2\xB7  " + fondsFor(b.path);
      renderer.drawText(magnus::FONT_CHROME, MH_PAD, rowY + MH_SUB_Y, sub.c_str(), !sel);

      // dashed divider beneath unselected rows
      if (!sel) magnus::dashed(renderer, MH_PAD, rowY + MH_STACK_ROW - 1, sw - 2 * MH_PAD);
    }
  }

  // ── Bottom destination tabs ───────────────────────────────────────────────────
  {
    const int barY = sh - MH_TABBAR_H;
    magnus::rule(renderer, 0, barY - magnus::RULE, sw);
    const char* labels[] = {"ARCHIVE", "READING", "STATS", "SETTINGS"};
    const int n = MH_BOTTOM_ITEMS;
    const int cellW = sw / n;
    for (int i = 0; i < n; i++) {
      const int cx = i * cellW;
      const bool sel = (selectorIndex == barStart + i);
      if (sel) magnus::invFill(renderer, Rect{cx, barY, cellW, MH_TABBAR_H});
      if (i > 0) magnus::vrule(renderer, cx, barY + 6, MH_TABBAR_H - 12, 1);
      const int lw = magnus::trackedWidth(renderer, magnus::FONT_CHROME, labels[i], magnus::TRACK_TAB);
      const int ty = barY + (MH_TABBAR_H - renderer.getLineHeight(magnus::FONT_CHROME)) / 2;
      magnus::tracked(renderer, magnus::FONT_CHROME, cx + (cellW - lw) / 2, ty, labels[i], magnus::TRACK_TAB, !sel);
    }
  }

  renderer.displayBuffer();

  // ── Post-render: trigger async cover-thumbnail loading (same as CrossPet) ─────
  if (!firstRenderDone) {
    firstRenderDone = true;
    bool needsLoad = false;
    for (const auto& b : recentBooks) {
      if (!b.coverBmpPath.empty() &&
          !Storage.exists(UITheme::getCoverThumbPath(b.coverBmpPath, MH_COVER_H).c_str())) {
        needsLoad = true;
        break;
      }
    }
    if (needsLoad)
      requestUpdate();
    else
      recentsLoaded = true;
  } else if (!recentsLoaded && !recentsLoading) {
    recentsLoading = true;
    loadRecentCovers(MH_COVER_H);
  }
}

// ── Navigation ────────────────────────────────────────────────────────────────
// Hero (resume) → stacks rows → four destination tabs. Decoupled from loopCrossPet
// so the stacks cap (5) and the tab destinations (Archive/Reading/Stats/Settings)
// can differ from the CrossPet theme.
void HomeActivity::loopMagnus() {
  const int recentCount = std::max(0, std::min(MH_MAX_STACKS, static_cast<int>(recentBooks.size()) - 1));
  const int barStart = 1 + recentCount;
  const int itemCount = barStart + MH_BOTTOM_ITEMS;

  buttonNavigator.onNext([this, itemCount] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, itemCount);
    requestUpdate();
  });
  buttonNavigator.onPrevious([this, itemCount] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, itemCount);
    requestUpdate();
  });

  // Back long-press = sync
  if (mappedInput.isPressed(MappedInputManager::Button::Back) && mappedInput.getHeldTime() >= 800 && !syncTriggered) {
    syncTriggered = true;
    doSync();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) syncTriggered = false;

  if (syncResultMsg && millis() > syncResultExpiry) {
    syncResultMsg = nullptr;
    requestUpdate();
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (selectorIndex == 0) {
      if (!recentBooks.empty()) onSelectBook(recentBooks[0].path);
    } else if (selectorIndex <= recentCount) {
      onSelectBook(recentBooks[selectorIndex].path);
    } else {
      switch (selectorIndex - barStart) {
        case 0: onFileBrowserOpen(); break;  // ARCHIVE
        case 1: onRecentBooksOpen(); break;  // READING
        case 2: activityManager.pushActivity(std::make_unique<ReadingStatsActivity>(renderer, mappedInput)); break;  // STATS
        case 3: onSettingsOpen(); break;     // SETTINGS
      }
    }
  }
}
