#include "EpubReaderChapterSelectionActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "components/themes/magnus/MagnusGlobals.h"
#include "fontIds.h"

int EpubReaderChapterSelectionActivity::getTotalItems() const { return epub->getTocItemsCount(); }

int EpubReaderChapterSelectionActivity::getPageItems() const {
  // Layout constants used in renderScreen
  constexpr int lineHeight = 30;

  const int screenHeight = renderer.getScreenHeight();
  const auto orientation = renderer.getOrientation();
  // In inverted portrait, the button hints are drawn near the logical top.
  // Reserve vertical space so list items do not collide with the hints.
  const bool isPortraitInverted = orientation == GfxRenderer::Orientation::PortraitInverted;
  const int hintGutterHeight = isPortraitInverted ? 50 : 0;
  // Magnus needs a taller header band (eyebrow + tall Garamond title) than the stock layout.
  const int headerH = (SETTINGS.uiTheme == CrossPointSettings::MAGNUS) ? 84 : 60;
  const int startY = headerH + hintGutterHeight;
  const int availableHeight = screenHeight - startY - lineHeight;
  // Clamp to at least one item to avoid division by zero and empty paging.
  return std::max(1, availableHeight / lineHeight);
}

void EpubReaderChapterSelectionActivity::onEnter() {
  Activity::onEnter();

  if (!epub) {
    return;
  }

  selectorIndex = epub->getTocIndexForSpineIndex(currentSpineIndex);
  if (selectorIndex == -1) {
    selectorIndex = 0;
  }

  // Trigger first update
  requestUpdate();
}

void EpubReaderChapterSelectionActivity::onExit() { Activity::onExit(); }

void EpubReaderChapterSelectionActivity::loop() {
  const int pageItems = getPageItems();
  const int totalItems = getTotalItems();

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    const auto newSpineIndex = epub->getSpineIndexForTocIndex(selectorIndex);
    if (newSpineIndex == -1) {
      ActivityResult result;
      result.isCancelled = true;
      setResult(std::move(result));
      finish();
    } else {
      // Pass anchor from TOC entry for within-chapter navigation
      // (multiple TOC entries may share the same spine item with different anchors)
      const auto tocItem = epub->getTocItem(selectorIndex);
      setResult(ChapterResult{newSpineIndex, tocItem.anchor});
      finish();
    }
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
  }

  buttonNavigator.onNextRelease([this, totalItems] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, totalItems);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, totalItems] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, totalItems);
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this, totalItems, pageItems] {
    selectorIndex = ButtonNavigator::nextPageIndex(selectorIndex, totalItems, pageItems);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, totalItems, pageItems] {
    selectorIndex = ButtonNavigator::previousPageIndex(selectorIndex, totalItems, pageItems);
    requestUpdate();
  });
}

void EpubReaderChapterSelectionActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto orientation = renderer.getOrientation();
  // Landscape orientation: reserve a horizontal gutter for button hints.
  const bool isLandscapeCw = orientation == GfxRenderer::Orientation::LandscapeClockwise;
  const bool isLandscapeCcw = orientation == GfxRenderer::Orientation::LandscapeCounterClockwise;
  // Inverted portrait: reserve vertical space for hints at the top.
  const bool isPortraitInverted = orientation == GfxRenderer::Orientation::PortraitInverted;
  const int hintGutterWidth = (isLandscapeCw || isLandscapeCcw) ? 30 : 0;
  // Landscape CW places hints on the left edge; CCW keeps them on the right.
  const int contentX = isLandscapeCw ? hintGutterWidth : 0;
  const int contentWidth = pageWidth - hintGutterWidth;
  const int hintGutterHeight = isPortraitInverted ? 50 : 0;
  const int contentY = hintGutterHeight;
  const int pageItems = getPageItems();
  const int totalItems = getTotalItems();

  // Theme-selected fonts: Magnus uses Garamond rows + Courier eyebrow/chrome.
  const bool mag = SETTINGS.uiTheme == CrossPointSettings::MAGNUS;
  const int titleFont = mag ? magnus::FONT_TITLE : UI_12_FONT_ID;
  const EpdFontFamily::Style titleStyle = mag ? EpdFontFamily::REGULAR : EpdFontFamily::BOLD;
  const int itemFont = mag ? magnus::FONT_BODY : UI_10_FONT_ID;

  // Header (+ Magnus eyebrow + rule). Garamond's line box is tall, so the Magnus header band
  // is 84px (matches getPageItems) with generous spacing; stock keeps its 60px / y=15 layout.
  const int listBase = (mag ? 84 : 60) + contentY;
  if (mag) {
    magnus::eyebrow(renderer, contentX + 14, contentY + 8, "CONTENTS");
    const int titleX =
        contentX + (contentWidth - renderer.getTextWidth(titleFont, tr(STR_SELECT_CHAPTER), titleStyle)) / 2;
    renderer.drawText(titleFont, titleX, contentY + 36, tr(STR_SELECT_CHAPTER), true, titleStyle);
    magnus::rule(renderer, contentX, listBase - 6, contentWidth);
  } else {
    const int titleX =
        contentX + (contentWidth - renderer.getTextWidth(titleFont, tr(STR_SELECT_CHAPTER), titleStyle)) / 2;
    renderer.drawText(titleFont, titleX, 15 + contentY, tr(STR_SELECT_CHAPTER), true, titleStyle);
  }

  const auto pageStartIndex = selectorIndex / pageItems * pageItems;
  // Highlight only the content area, not the hint gutters.
  renderer.fillRect(contentX, listBase + (selectorIndex % pageItems) * 30 - 2, contentWidth - 1, 30);

  for (int i = 0; i < pageItems; i++) {
    int itemIndex = pageStartIndex + i;
    if (itemIndex >= totalItems) break;
    const int displayY = listBase + i * 30;
    const bool isSelected = (itemIndex == selectorIndex);

    auto item = epub->getTocItem(itemIndex);

    // Indent per TOC level while keeping content within the gutter-safe region.
    int indentSize = contentX + 20 + (item.level - 1) * 15;
    if (mag && isSelected) {
      renderer.drawText(magnus::FONT_CHROME, indentSize - 6, displayY + 2, "\xE2\x96\xB8", false);
      indentSize += 14;
    }
    const std::string chapterName =
        renderer.truncatedText(itemFont, item.title.c_str(), contentWidth - 40 - indentSize);

    renderer.drawText(itemFont, indentSize, displayY, chapterName.c_str(), !isSelected);

    // hairline divider beneath unselected rows (Magnus)
    if (mag && !isSelected)
      magnus::hairline(renderer, contentX + 20, displayY + 26, contentWidth - 40);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
