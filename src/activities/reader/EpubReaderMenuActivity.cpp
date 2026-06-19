#include "EpubReaderMenuActivity.h"

#include <FontManager.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "components/themes/magnus/MagnusGlobals.h"
#include "fontIds.h"

// Font family/size label arrays for rendering current values
static const StrId fontFamilyLabels[] = {StrId::STR_NOTO_SERIF, StrId::STR_LEXEND, StrId::STR_BOKERLAM};
static const StrId fontSizeLabels[] = {StrId::STR_SMALL, StrId::STR_MEDIUM, StrId::STR_LARGE, StrId::STR_X_LARGE};

EpubReaderMenuActivity::EpubReaderMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                               const std::string& title, const int currentPage, const int totalPages,
                                               const int bookProgressPercent, const uint8_t currentOrientation,
                                               const bool hasFootnotes, const bool hasStarredPages,
                                               const uint8_t currentPageTurnOption, const bool hasDictionary,
                                               const bool hasLookupHistory)
    : Activity("EpubReaderMenu", renderer, mappedInput),
      menuItems(buildMenuItems(hasFootnotes, hasStarredPages, hasDictionary, hasLookupHistory)),
      sections(buildSections(hasFootnotes, hasStarredPages, hasDictionary, hasLookupHistory)),
      title(title),
      pendingOrientation(currentOrientation),
      selectedPageTurnOption(currentPageTurnOption),
      currentPage(currentPage),
      totalPages(totalPages),
      bookProgressPercent(bookProgressPercent) {}

std::vector<EpubReaderMenuActivity::MenuItem> EpubReaderMenuActivity::buildMenuItems(bool hasFootnotes,
                                                                                     bool hasStarredPages,
                                                                                     bool hasDictionary,
                                                                                     bool hasLookupHistory) {
  std::vector<MenuItem> items;
  items.reserve(15);

  // NAVIGATION section
  items.push_back({MenuAction::SELECT_CHAPTER, StrId::STR_SELECT_CHAPTER});
  items.push_back({MenuAction::GO_TO_PERCENT, StrId::STR_GO_TO_PERCENT});
  if (hasFootnotes) items.push_back({MenuAction::FOOTNOTES, StrId::STR_FOOTNOTES});
  items.push_back({MenuAction::STAR_PAGE, StrId::STR_STAR_PAGE});
  if (hasStarredPages) items.push_back({MenuAction::STARRED_PAGES, StrId::STR_STARRED_PAGES});
  // Dictionary lookup entry points: shown only when StarDict files are present
  // on the SD card. The history list additionally requires a per-book lookups.txt.
  if (hasDictionary) items.push_back({MenuAction::LOOKUP, StrId::STR_LOOKUP});
  if (hasLookupHistory) items.push_back({MenuAction::LOOKED_UP_WORDS, StrId::STR_LOOKED_UP_WORDS});

  // DISPLAY section
  items.push_back({MenuAction::FONT_FAMILY, StrId::STR_FONT_FAMILY});
  items.push_back({MenuAction::FONT_SIZE, StrId::STR_FONT_SIZE});
  items.push_back({MenuAction::ROTATE_SCREEN, StrId::STR_ORIENTATION});
  items.push_back({MenuAction::AUTO_PAGE_TURN, StrId::STR_AUTO_PAGE_TURN});
  items.push_back({MenuAction::AUTO_PAGE_TURN_SPEED, StrId::STR_AUTO_TURN_PAGES_PER_MIN});

  // FEATURES section
  items.push_back({MenuAction::SCREENSHOT, StrId::STR_SCREENSHOT_BUTTON});
  items.push_back({MenuAction::DISPLAY_QR, StrId::STR_DISPLAY_QR});
  items.push_back({MenuAction::SYNC, StrId::STR_SYNC_PROGRESS});
  items.push_back({MenuAction::GO_HOME, StrId::STR_GO_HOME_BUTTON});
  items.push_back({MenuAction::DELETE_CACHE, StrId::STR_DELETE_CACHE});
#ifdef ENABLE_BLE
  items.push_back({MenuAction::BLE_REMOTE, StrId::STR_BLE_REMOTE});
#endif

  return items;
}

std::vector<EpubReaderMenuActivity::SectionInfo> EpubReaderMenuActivity::buildSections(bool hasFootnotes,
                                                                                       bool hasStarredPages,
                                                                                       bool hasDictionary,
                                                                                       bool hasLookupHistory) {
  std::vector<SectionInfo> sects;
  int idx = 0;

  // NAVIGATION: 3 fixed (chapter, go-to, star) + optional footnotes + starred + dictionary entries
  const int navCount = 3 + (hasFootnotes ? 1 : 0) + (hasStarredPages ? 1 : 0) + (hasDictionary ? 1 : 0) +
                       (hasLookupHistory ? 1 : 0);
  sects.push_back({"NAVIGATION", idx, navCount});
  idx += navCount;

  // DISPLAY: font family, font size, orientation, auto page turn toggle, speed
  sects.push_back({"DISPLAY", idx, 5});
  idx += 5;

  // FEATURES: 5 base + optional BLE
  int featureCount = 5;
#ifdef ENABLE_BLE
  featureCount++;
#endif
  sects.push_back({"FEATURES", idx, featureCount});

  return sects;
}

void EpubReaderMenuActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void EpubReaderMenuActivity::onExit() { Activity::onExit(); }

void EpubReaderMenuActivity::loop() {
  // Handle navigation — operates on flat menuItems index (unchanged)
  buttonNavigator.onNext([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(menuItems.size()));
    requestUpdate();
  });

  buttonNavigator.onPrevious([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(menuItems.size()));
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    const auto selectedAction = menuItems[selectedIndex].action;
    if (selectedAction == MenuAction::FONT_FAMILY) {
      setResult(MenuResult{static_cast<int>(selectedAction), pendingOrientation, selectedPageTurnOption});
      finish();
      return;
    }
    if (selectedAction == MenuAction::FONT_SIZE) {
      SETTINGS.fontSize = (SETTINGS.fontSize + 1) % CrossPointSettings::FONT_SIZE_COUNT;
      SETTINGS.saveToFile();
      requestUpdate();
      return;
    }
    if (selectedAction == MenuAction::ROTATE_SCREEN) {
      // Cycle orientation preview locally; actual rotation happens on menu exit.
      pendingOrientation = (pendingOrientation + 1) % orientationLabels.size();
      requestUpdate();
      return;
    }

    if (selectedAction == MenuAction::AUTO_PAGE_TURN) {
      // Toggle auto page turn on/off
      SETTINGS.autoPageTurnEnabled = SETTINGS.autoPageTurnEnabled ? 0 : 1;
      if (SETTINGS.autoPageTurnEnabled && SETTINGS.autoPageTurnSpeed == 0) {
        SETTINGS.autoPageTurnSpeed = 1;
      }
      selectedPageTurnOption = SETTINGS.autoPageTurnEnabled ? SETTINGS.autoPageTurnSpeed : 0;
      SETTINGS.saveToFile();
      requestUpdate();
      return;
    }
    if (selectedAction == MenuAction::AUTO_PAGE_TURN_SPEED) {
      // Cycle speed 1 → 2 → ... → 20 → 1
      SETTINGS.autoPageTurnSpeed = (SETTINGS.autoPageTurnSpeed >= AUTO_TURN_MAX_PPM) ? 1 : SETTINGS.autoPageTurnSpeed + 1;
      selectedPageTurnOption = SETTINGS.autoPageTurnEnabled ? SETTINGS.autoPageTurnSpeed : 0;
      SETTINGS.saveToFile();
      requestUpdate();
      return;
    }

    setResult(MenuResult{static_cast<int>(selectedAction), pendingOrientation, selectedPageTurnOption});
    finish();
    return;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    result.data = MenuResult{-1, pendingOrientation, selectedPageTurnOption};
    setResult(std::move(result));
    finish();
    return;
  }
}

void EpubReaderMenuActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto pageWidth = renderer.getScreenWidth();
  const auto orientation = renderer.getOrientation();
  // Landscape orientation: button hints are drawn along a vertical edge, so we
  // reserve a horizontal gutter to prevent overlap with menu content.
  const bool isLandscapeCw = orientation == GfxRenderer::Orientation::LandscapeClockwise;
  const bool isLandscapeCcw = orientation == GfxRenderer::Orientation::LandscapeCounterClockwise;
  // Inverted portrait: button hints appear near the logical top, so we reserve
  // vertical space to keep the header and list clear.
  const bool isPortraitInverted = orientation == GfxRenderer::Orientation::PortraitInverted;
  const int hintGutterWidth = (isLandscapeCw || isLandscapeCcw) ? 30 : 0;
  // Landscape CW places hints on the left edge; CCW keeps them on the right.
  const int contentX = isLandscapeCw ? hintGutterWidth : 0;
  const int contentWidth = pageWidth - hintGutterWidth;
  const int hintGutterHeight = isPortraitInverted ? 50 : 0;
  const int contentY = hintGutterHeight;

  // Theme-selected fonts: Magnus uses Garamond titles/items + Courier chrome.
  const bool mag = SETTINGS.uiTheme == CrossPointSettings::MAGNUS;
  const int titleFont = mag ? magnus::FONT_TITLE : UI_12_FONT_ID;
  const EpdFontFamily::Style titleStyle = mag ? EpdFontFamily::REGULAR : EpdFontFamily::BOLD;
  const int itemFont = mag ? magnus::FONT_BODY : UI_10_FONT_ID;
  const int subFont = mag ? magnus::FONT_CHROME : UI_10_FONT_ID;
  const int valFont = mag ? magnus::FONT_CHROME : UI_10_FONT_ID;

  // Header. Magnus stacks each element by its real line height (Garamond's line box is much
  // taller than the stock UI font, so fixed offsets collided); stock keeps its fixed offsets.
  int titleY, subY, listY;
  if (mag) {
    int hy = contentY + 6;
    magnus::eyebrow(renderer, contentX + 14, hy, "READING MENU");
    hy += renderer.getLineHeight(magnus::FONT_EYEBROW) + 2;
    titleY = hy;
    hy += renderer.getLineHeight(titleFont);
    subY = hy;
    hy += renderer.getLineHeight(subFont) + 10;
    listY = hy;
  } else {
    titleY = 15 + contentY;
    subY = 42 + contentY;
    listY = 64 + contentY;
  }

  const std::string truncTitle =
      renderer.truncatedText(titleFont, title.c_str(), contentWidth - 40, titleStyle);
  const int titleX =
      contentX + (contentWidth - renderer.getTextWidth(titleFont, truncTitle.c_str(), titleStyle)) / 2;
  renderer.drawText(titleFont, titleX, titleY, truncTitle.c_str(), true, titleStyle);

  // Subtitle: "Chapter X · Page Y of Z · 42%"
  char subtitle[64];
  if (totalPages > 0) {
    snprintf(subtitle, sizeof(subtitle), "%s%d \xC2\xB7 %d/%d \xC2\xB7 %d%%",
             tr(STR_CHAPTER_PREFIX), currentPage, currentPage, totalPages, bookProgressPercent);
  } else {
    snprintf(subtitle, sizeof(subtitle), "%d%%", bookProgressPercent);
  }
  const int subW = renderer.getTextWidth(subFont, subtitle);
  renderer.drawText(subFont, contentX + (contentWidth - subW) / 2, subY, subtitle, true);

  // Sectioned list
  int y = listY;
  if (mag) magnus::rule(renderer, contentX, y - 8, contentWidth);
  constexpr int lineHeight = 34;
  constexpr int sectionHeaderH = 26;

  for (const auto& section : sections) {
    // Section header — small-caps label (tracked for Magnus)
    if (mag)
      magnus::eyebrow(renderer, contentX + 14, y + 6, section.label);
    else
      renderer.drawText(SMALL_FONT_ID, contentX + 14, y + 4, section.label, true, EpdFontFamily::REGULAR);
    y += sectionHeaderH;

    // Items within section
    for (int j = 0; j < section.count; j++) {
      const int itemIdx = section.startIndex + j;
      const bool isSelected = (itemIdx == selectedIndex);

      if (isSelected) {
        // Selection fill with side padding (not edge-to-edge)
        renderer.fillRect(contentX + 14, y, contentWidth - 28, lineHeight, true);
      }

      // Item label (inverted text when selected; Magnus adds a typewriter caret)
      int labelX = contentX + 20;
      if (mag && isSelected) {
        renderer.drawText(magnus::FONT_CHROME, contentX + 16, y + 6, "\xE2\x96\xB8", false);
        labelX = contentX + 34;
      }
      renderer.drawText(itemFont, labelX, y + 4, I18N.get(menuItems[itemIdx].labelId), !isSelected);

      // Right-aligned value for font family (show external font name if active)
      if (menuItems[itemIdx].action == MenuAction::FONT_FAMILY) {
        const int extIdx = FontMgr.getSelectedIndex();
        const char* value;
        char extLabel[48];
        if (extIdx >= 0) {
          const FontInfo* info = FontMgr.getFontInfo(extIdx);
          if (info) {
            const char* modeSuffix = FontMgr.isExternalPrimary() ? " [P]" : " [S]";
            snprintf(extLabel, sizeof(extLabel), "%s (%dpt)%s", info->name, info->size, modeSuffix);
            value = extLabel;
          } else {
            value = I18N.get(fontFamilyLabels[SETTINGS.fontFamily]);
          }
        } else {
          value = I18N.get(fontFamilyLabels[SETTINGS.fontFamily]);
        }
        const auto width = renderer.getTextWidth(valFont, value);
        renderer.drawText(valFont, contentX + contentWidth - 20 - width, y + 4, value, !isSelected);
      }

      // Right-aligned value for font size
      if (menuItems[itemIdx].action == MenuAction::FONT_SIZE) {
        const char* value = I18N.get(fontSizeLabels[SETTINGS.fontSize]);
        const auto width = renderer.getTextWidth(valFont, value);
        renderer.drawText(valFont, contentX + contentWidth - 20 - width, y + 4, value, !isSelected);
      }

      // Right-aligned value for orientation item
      if (menuItems[itemIdx].action == MenuAction::ROTATE_SCREEN) {
        const char* value = I18N.get(orientationLabels[pendingOrientation]);
        const auto width = renderer.getTextWidth(valFont, value);
        renderer.drawText(valFont, contentX + contentWidth - 20 - width, y + 4, value, !isSelected);
      }

      // Right-aligned value for auto page turn toggle: On/Off
      if (menuItems[itemIdx].action == MenuAction::AUTO_PAGE_TURN) {
        const char* value = SETTINGS.autoPageTurnEnabled ? I18N.get(StrId::STR_STATE_ON) : I18N.get(StrId::STR_STATE_OFF);
        const auto width = renderer.getTextWidth(valFont, value);
        renderer.drawText(valFont, contentX + contentWidth - 20 - width, y + 4, value, !isSelected);
      }

      // Right-aligned value for auto page turn speed: PPM number
      if (menuItems[itemIdx].action == MenuAction::AUTO_PAGE_TURN_SPEED) {
        snprintf(pageTurnValueBuf, sizeof(pageTurnValueBuf), "%d", SETTINGS.autoPageTurnSpeed);
        const char* value = pageTurnValueBuf;
        const auto width = renderer.getTextWidth(valFont, value);
        renderer.drawText(valFont, contentX + contentWidth - 20 - width, y + 4, value, !isSelected);
      }

      y += lineHeight;
    }

    y += 4;  // Gap between sections
  }

  // Button hints
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
