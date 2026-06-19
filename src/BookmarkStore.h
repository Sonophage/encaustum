#pragma once

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

// Stores starred/bookmarked pages for a single book.
// Persisted as a binary file on SD card within the book's cache directory.
// Snippets are lazy-loaded: only spine+page index is read on book open.
// Full snippets are loaded on first getAll() call (e.g. when viewing bookmark list).
class BookmarkStore {
 public:
  struct Bookmark {
    uint16_t spineIndex;
    uint16_t pageNumber;
    std::string snippet;  // First ~60 chars of text from the bookmarked page
    // Highlight ("Bookmark a Line") support. When isHighlight is true, this
    // record stores user-selected line/range text in `preview`, exported by
    // "Export Highlights". Plain page bookmarks leave isHighlight=false and
    // preview empty. (v3 on-disk addition; older files load as page bookmarks.)
    bool isHighlight = false;
    std::string preview;
  };

  // Load bookmark index from cache directory (compact: no snippets).
  void load(const std::string& cachePath) {
    basePath = cachePath;
    bookmarks.clear();
    dirty = false;
    snippetsLoaded = false;

    FsFile f;
    if (!Storage.openFileForRead("BKM", getFilePath(), f)) {
      snippetsLoaded = true;  // Nothing to load
      return;
    }

    uint8_t version;
    if (f.read(reinterpret_cast<uint8_t*>(&version), sizeof(version)) != sizeof(version) || version < 1 || version > FILE_VERSION) {
      f.close();
      snippetsLoaded = true;
      return;
    }

    uint16_t count;
    if (f.read(reinterpret_cast<uint8_t*>(&count), sizeof(count)) != sizeof(count) || count > MAX_BOOKMARKS) {
      LOG_ERR("BKM", "Invalid bookmark count: %u", static_cast<unsigned>(count));
      f.close();
      snippetsLoaded = true;
      return;
    }

    bookmarks.reserve(count);
    for (uint16_t i = 0; i < count; i++) {
      Bookmark bm;
      if (f.read(reinterpret_cast<uint8_t*>(&bm.spineIndex), sizeof(bm.spineIndex)) != sizeof(bm.spineIndex) ||
          f.read(reinterpret_cast<uint8_t*>(&bm.pageNumber), sizeof(bm.pageNumber)) != sizeof(bm.pageNumber)) {
        LOG_ERR("BKM", "Truncated bookmarks file at entry %d", i);
        bookmarks.clear();
        f.close();
        snippetsLoaded = true;
        return;
      }
      // v2: skip snippet data (will be loaded on demand via loadSnippets)
      if (version >= 2) {
        uint8_t snippetLen = 0;
        if (f.read(&snippetLen, 1) == 1 && snippetLen > 0) {
          f.seekCur(snippetLen);
        }
      }
      // v3: read the highlight flag now (cheap, needed for isEmpty-style
      // queries and to gate export) but defer the preview text to loadSnippets.
      if (version >= 3) {
        uint8_t isHighlight = 0;
        if (f.read(&isHighlight, 1) == 1) {
          bm.isHighlight = isHighlight != 0;
        }
        uint8_t previewLen = 0;
        if (f.read(&previewLen, 1) == 1 && previewLen > 0) {
          f.seekCur(previewLen);
        }
      }
      bookmarks.push_back(bm);
    }

    f.close();
    LOG_DBG("BKM", "Loaded %d bookmark indices (snippets deferred)", count);
  }

  // Save bookmarks to SD card (only if changed).
  void save() {
    if (!dirty || basePath.empty()) {
      return;
    }

    // Ensure snippets are loaded before saving so we don't lose existing ones
    loadSnippets();

    if (bookmarks.size() > UINT16_MAX) {
      LOG_ERR("BKM", "Too many bookmarks to save: %u", static_cast<unsigned>(bookmarks.size()));
      return;
    }

    FsFile f;
    if (!Storage.openFileForWrite("BKM", getFilePath(), f)) {
      LOG_ERR("BKM", "Failed to save bookmarks");
      return;
    }

    auto writePodChecked = [&f](const auto& value) {
      return f.write(reinterpret_cast<const uint8_t*>(&value), sizeof(value)) == sizeof(value);
    };

    const uint16_t count = static_cast<uint16_t>(bookmarks.size());
    bool ok = writePodChecked(FILE_VERSION) && writePodChecked(count);

    for (const auto& bm : bookmarks) {
      ok = ok && writePodChecked(bm.spineIndex) && writePodChecked(bm.pageNumber);
      // v2: write snippet (length-prefixed, capped to MAX_SNIPPET_LEN)
      const uint8_t snippetLen = static_cast<uint8_t>(std::min(bm.snippet.size(), static_cast<size_t>(MAX_SNIPPET_LEN)));
      ok = ok && writePodChecked(snippetLen);
      if (snippetLen > 0) {
        ok = ok && f.write(reinterpret_cast<const uint8_t*>(bm.snippet.c_str()), snippetLen) == snippetLen;
      }
      // v3: write highlight flag + length-prefixed preview (capped).
      const uint8_t isHighlight = bm.isHighlight ? 1 : 0;
      ok = ok && writePodChecked(isHighlight);
      const uint8_t previewLen =
          static_cast<uint8_t>(std::min(bm.preview.size(), static_cast<size_t>(MAX_PREVIEW_LEN)));
      ok = ok && writePodChecked(previewLen);
      if (previewLen > 0) {
        ok = ok && f.write(reinterpret_cast<const uint8_t*>(bm.preview.c_str()), previewLen) == previewLen;
      }
    }

    ok = ok && f.close();
    if (!ok) {
      LOG_ERR("BKM", "Failed while writing bookmarks");
      return;
    }
    dirty = false;
    LOG_DBG("BKM", "Saved %d bookmarks", count);
  }

  // Toggle bookmark for the given page. Returns true if now starred, false if removed.
  bool toggle(uint16_t spineIndex, uint16_t pageNumber, const std::string& snippet = "") {
    auto it = find(spineIndex, pageNumber);
    if (it != bookmarks.end()) {
      bookmarks.erase(it);
      dirty = true;
      return false;
    }
    bookmarks.push_back({spineIndex, pageNumber, snippet.substr(0, MAX_SNIPPET_LEN), false, {}});
    dirty = true;
    return true;
  }

  // Add a highlight ("Bookmark a Line") record for the given page, carrying the
  // user-selected line/range text as preview. Unlike toggle(), highlights are
  // additive — multiple highlights may share a page — so this always appends.
  // Returns false if the bookmark limit is reached.
  bool addHighlight(uint16_t spineIndex, uint16_t pageNumber, const std::string& preview) {
    if (bookmarks.size() >= MAX_BOOKMARKS) return false;
    bookmarks.push_back({spineIndex, pageNumber, "", true, preview.substr(0, MAX_PREVIEW_LEN)});
    dirty = true;
    return true;
  }

  // True if any stored bookmark is a highlight (gates Export Highlights).
  // The isHighlight flag is populated by load() from the compact index, so no
  // snippet/preview load is needed here.
  [[nodiscard]] bool hasHighlights() const {
    return std::any_of(bookmarks.begin(), bookmarks.end(), [](const Bookmark& bm) { return bm.isHighlight; });
  }

  // Check if a page is starred (works on compact index, no snippet load needed).
  [[nodiscard]] bool has(uint16_t spineIndex, uint16_t pageNumber) const {
    return std::any_of(bookmarks.begin(), bookmarks.end(), [spineIndex, pageNumber](const Bookmark& bm) {
      return bm.spineIndex == spineIndex && bm.pageNumber == pageNumber;
    });
  }

  // Get all bookmarks with snippets (triggers lazy snippet load if needed).
  [[nodiscard]] const std::vector<Bookmark>& getAll() {
    loadSnippets();
    return bookmarks;
  }

  [[nodiscard]] bool isEmpty() const { return bookmarks.empty(); }
  void markDirty() { dirty = true; }

 private:
  static constexpr uint8_t FILE_VERSION = 3;  // v2: snippet field; v3: highlight flag + preview
  static constexpr uint16_t MAX_BOOKMARKS = 1000;
  static constexpr uint8_t MAX_SNIPPET_LEN = 80;
  static constexpr uint8_t MAX_PREVIEW_LEN = 160;  // matches word-select kPreviewMax

  std::vector<Bookmark> bookmarks;
  std::string basePath;
  bool dirty = false;
  bool snippetsLoaded = false;

  [[nodiscard]] std::string getFilePath() const { return basePath + "/bookmarks.bin"; }

  std::vector<Bookmark>::iterator find(uint16_t spineIndex, uint16_t pageNumber) {
    return std::find_if(bookmarks.begin(), bookmarks.end(), [spineIndex, pageNumber](const Bookmark& bm) {
      return bm.spineIndex == spineIndex && bm.pageNumber == pageNumber;
    });
  }

  // Re-read file to populate snippet strings for all bookmarks.
  void loadSnippets() {
    if (snippetsLoaded) return;
    snippetsLoaded = true;

    FsFile f;
    if (!Storage.openFileForRead("BKM", getFilePath(), f)) return;

    uint8_t version;
    if (f.read(reinterpret_cast<uint8_t*>(&version), sizeof(version)) != sizeof(version) || version < 2) {
      f.close();
      return;
    }

    uint16_t count;
    if (f.read(reinterpret_cast<uint8_t*>(&count), sizeof(count)) != sizeof(count)) {
      f.close();
      return;
    }

    for (uint16_t i = 0; i < count && i < bookmarks.size(); i++) {
      uint16_t si, pn;
      f.read(reinterpret_cast<uint8_t*>(&si), sizeof(si));
      f.read(reinterpret_cast<uint8_t*>(&pn), sizeof(pn));

      uint8_t snippetLen = 0;
      if (f.read(&snippetLen, 1) == 1 && snippetLen > 0) {
        char buf[MAX_SNIPPET_LEN + 1];
        const uint8_t toRead = std::min(snippetLen, static_cast<uint8_t>(MAX_SNIPPET_LEN));
        if (f.read(reinterpret_cast<uint8_t*>(buf), toRead) == toRead) {
          buf[toRead] = '\0';
          // Only fill snippet if it's empty (newly toggled bookmarks already have one)
          if (bookmarks[i].snippet.empty()) {
            bookmarks[i].snippet = buf;
          }
        }
        if (snippetLen > toRead) f.seekCur(snippetLen - toRead);
      }

      // v3: highlight flag + length-prefixed preview text.
      if (version >= 3) {
        uint8_t isHighlight = 0;
        if (f.read(&isHighlight, 1) == 1) {
          bookmarks[i].isHighlight = isHighlight != 0;
        }
        uint8_t previewLen = 0;
        if (f.read(&previewLen, 1) == 1 && previewLen > 0) {
          char pbuf[MAX_PREVIEW_LEN + 1];
          const uint8_t toRead = std::min(previewLen, static_cast<uint8_t>(MAX_PREVIEW_LEN));
          if (f.read(reinterpret_cast<uint8_t*>(pbuf), toRead) == toRead) {
            pbuf[toRead] = '\0';
            if (bookmarks[i].preview.empty()) {
              bookmarks[i].preview = pbuf;
            }
          }
          if (previewLen > toRead) f.seekCur(previewLen - toRead);
        }
      }
    }

    f.close();
    LOG_DBG("BKM", "Loaded snippets for %d bookmarks", static_cast<int>(bookmarks.size()));
  }
};
