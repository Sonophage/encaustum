#include "BootActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "CrossPointSettings.h"
#include "components/themes/magnus/MagnusGlobals.h"
#include "fontIds.h"
#include "images/Logo120.h"

void BootActivity::onEnter() {
  Activity::onEnter();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  if (SETTINGS.uiTheme == CrossPointSettings::MAGNUS) {
    const int cx = pageWidth / 2;
    // institute eyebrow
    magnus::centerTracked(renderer, magnus::FONT_CHROME, cx, pageHeight / 2 - 170, "THE MAGNUS INSTITUTE", 3);
    // the Eye seal (concentric rings + lens)
    magnus::eye(renderer, cx, pageHeight / 2 - 70, 150, 96, true, 3);
    // title + subtitle
    const char* title = "Magnus Reader";
    const int tw = renderer.getTextWidth(magnus::FONT_DISPLAY, title);
    renderer.drawText(magnus::FONT_DISPLAY, cx - tw / 2, pageHeight / 2 + 30, title, true);
    magnus::centerTracked(renderer, magnus::FONT_CHROME, cx, pageHeight / 2 + 74, "CROSSING FIRMWARE \xC2\xB7 XTEINK X4", 2);
    // status line + indexing bar
    magnus::centerTracked(renderer, magnus::FONT_CHROME, cx, pageHeight - 150, "MOUNTING THE ARCHIVE\xE2\x80\xA6", 2);
    const int barW = pageWidth - 2 * 60;
    magnus::ditherBar(renderer, Rect{60, pageHeight - 128, barW, 14}, 72);
    renderer.drawText(magnus::FONT_CHROME, 60, pageHeight - 104, "STATEMENTS INDEXED", true);
    const char* pct = "72%";
    renderer.drawText(magnus::FONT_CHROME, pageWidth - 60 - renderer.getTextWidth(magnus::FONT_CHROME, pct),
                      pageHeight - 104, pct, true);
    // footer
    char footer[48];
    snprintf(footer, sizeof(footer), "FW %s \xC2\xB7 SSD1677", CROSSPOINT_VERSION);
    magnus::centerTracked(renderer, magnus::FONT_CHROME, cx, pageHeight - 36, footer, 1);
    renderer.displayBuffer();
    return;
  }

  renderer.drawImage(Logo120, (pageWidth - 120) / 2, (pageHeight - 120) / 2, 120, 120);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 70, tr(STR_CROSSPOINT), true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 95, tr(STR_BOOTING));
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - 30, CROSSPOINT_VERSION);
  renderer.displayBuffer();
}
