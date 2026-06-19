#pragma once
#include <I18n.h>

#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"
#include "RecentBooksStore.h"
#include "util/ButtonNavigator.h"

// Shows recent books as a cover grid (3 columns) with title below each cover.
// Navigate with D-pad, Confirm opens book, Back goes home.
class RecentBooksActivity final : public Activity {
 private:
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;
  std::vector<RecentBook> recentBooks;

  static constexpr int COLS = 3;
  static constexpr int COVER_GAP = 12;
  static constexpr int COVER_R = 8;

  // Pagination
  int pageOffset = 0;    // first book index on current page
  int itemsPerPage = 6;  // calculated from screen height

  void loadRecentBooks();
  int totalPages() const;
  void ensurePageForIndex();

  // Generate cover thumbnails at the given height for any recent book missing one,
  // mirroring HomeActivity's OOM-safe pattern (skip on BLE, drop font caches around
  // the JPEG/PNG decode). Lets the grid fill each cell with the cover at cell height.
  void ensureCovers(int coverHeight);
  // Magnus grid cover-fill height (card interior), derived from screen width.
  int magnusCoverHeight() const;

  // Render a single cover card at grid position
  void renderCover(int bookIdx, int gridCol, int gridRow, int cardW, int cardH, int startX, int startY, bool selected);

  // Magnus theme: bespoke "The Stacks / Recently Opened" 2-column cover grid.
  void renderMagnus();
  void renderMagnusCard(int bookIdx, int cardX, int cardY, int cardW, int cardH, bool selected);

 public:
  explicit RecentBooksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("RecentBooks", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
