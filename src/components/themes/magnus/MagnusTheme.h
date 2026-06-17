#pragma once

#include "components/themes/BaseTheme.h"

// Magnus theme — archival / typewriter aesthetic for the Xteink X4 (480x800, 1-bit).
// Pure ink on paper, no gray: tone comes from dither hatching, selection from inversion.
// Type: EB Garamond (serif titles/content) + Courier Prime (mono chrome).
namespace MagnusMetrics {
constexpr ThemeMetrics values = {.batteryWidth = 18,
                                 .batteryHeight = 12,
                                 .topPadding = 6,
                                 .batteryBarHeight = 22,
                                 .headerHeight = 74,
                                 .verticalSpacing = 12,
                                 .contentSidePadding = 22,
                                 .listRowHeight = 46,
                                 .listWithSubtitleRowHeight = 64,
                                 .menuRowHeight = 52,
                                 .menuSpacing = 10,
                                 .tabSpacing = 10,
                                 .tabBarHeight = 40,
                                 .scrollBarWidth = 4,
                                 .scrollBarRightOffset = 5,
                                 .homeTopPadding = 56,
                                 .homeCoverHeight = 226,
                                 .homeCoverTileHeight = 242,
                                 .homeRecentBooksCount = 1,
                                 .buttonHintsHeight = 42,
                                 .sideButtonHintsWidth = 30,
                                 .progressBarHeight = 12,
                                 .progressBarMarginTop = 2,
                                 .statusBarHorizontalMargin = 6,
                                 .statusBarVerticalMargin = 19,
                                 .keyboardKeyWidth = 31,
                                 .keyboardKeyHeight = 48,
                                 .keyboardKeySpacing = 5,
                                 .keyboardBottomKeyHeight = 44,
                                 .keyboardBottomKeySpacing = 5,
                                 .keyboardBottomAligned = true,
                                 .keyboardCenteredText = true,
                                 .keyboardVerticalOffset = 0,
                                 .keyboardTextFieldWidthPercent = 85,
                                 .keyboardWidthPercent = 94,
                                 .keyboardKeyCornerRadius = 0};
}  // namespace MagnusMetrics

// Magnus design language: ink-on-paper, sharp rects, 2px header rules, hairline (dithered)
// list dividers, inverse selection with a typewriter caret marker, bordered mono keyboard cells.
// Inherits BaseTheme; overrides the Tier-1 surface that reskins ~17 activities at once.
class MagnusTheme : public BaseTheme {
 public:
  void drawHeader(const GfxRenderer& renderer, Rect rect, const char* title,
                  const char* subtitle = nullptr) const override;
  void drawSubHeader(const GfxRenderer& renderer, Rect rect, const char* label,
                     const char* rightLabel = nullptr) const override;
  void drawTabBar(const GfxRenderer& renderer, Rect rect, const std::vector<TabInfo>& tabs,
                  bool selected) const override;
  void drawList(const GfxRenderer& renderer, Rect rect, int itemCount, int selectedIndex,
                const std::function<std::string(int index)>& rowTitle,
                const std::function<std::string(int index)>& rowSubtitle, const std::function<UIIcon(int index)>& rowIcon,
                const std::function<std::string(int index)>& rowValue, bool highlightValue) const override;
  void drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                       const char* btn4) const override;
  void drawKeyboardKey(const GfxRenderer& renderer, Rect rect, const char* label, const bool isSelected,
                       const char* secondaryLabel = nullptr, KeyboardKeyType keyType = KeyboardKeyType::Normal,
                       bool inactiveSelection = false) const override;
  void drawProgressBar(const GfxRenderer& renderer, Rect rect, size_t current, size_t total) const override;
  void drawStatusBar(GfxRenderer& renderer, float bookProgress, int currentPage, int pageCount, std::string title,
                     int paddingBottom, int textYOffset, bool isStarred) const override;
  Rect drawPopup(const GfxRenderer& renderer, const char* message) const override;
};
