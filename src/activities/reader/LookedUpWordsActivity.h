#pragma once

// Per-book lookup history list (<book cache dir>/lookups.txt). Selecting a
// word re-opens its definition; the side rocker's Right press deletes an
// entry after confirmation. (Ported from CrumBLE.)

#include <string>
#include <vector>

#include "activities/Activity.h"

class LookedUpWordsActivity final : public Activity {
 public:
  explicit LookedUpWordsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string cachePath);

  void onEnter() override;
  void loop() override;
  void onExit() override;
  void render(RenderLock&&) override;

 private:
  std::string cachePath;
  std::vector<std::string> words;

  int selectedIndex = 0;
  int scrollOffset = 0;
  int linesPerPage = 0;

  bool isLoading = true;
  bool confirmingDelete = false;

  void loadHistory();
  void deleteSelectedWord();
};
