#pragma once
#include <string>
#include <vector>

struct RecentBook {
  std::string path;
  std::string title;
  std::string author;
  std::string coverBmpPath;
  uint8_t progressPercent = 0;  // 0-100, updated when reading progress is saved

  bool operator==(const RecentBook& other) const { return path == other.path; }
};

class RecentBooksStore;
namespace JsonSettingsIO {
bool loadRecentBooks(RecentBooksStore& store, const char* json);
}  // namespace JsonSettingsIO

class RecentBooksStore {
  // Static instance
  static RecentBooksStore instance;

  std::vector<RecentBook> recentBooks;

  friend bool JsonSettingsIO::loadRecentBooks(RecentBooksStore&, const char*);

 public:
  ~RecentBooksStore() = default;

  // Get singleton instance
  static RecentBooksStore& getInstance() { return instance; }

  // Add a book to the recent list (moves to front if already exists)
  void addBook(const std::string& path, const std::string& title, const std::string& author,
               const std::string& coverBmpPath);

  void updateBook(const std::string& path, const std::string& title, const std::string& author,
                  const std::string& coverBmpPath);

  // Update a book's progress percent in memory and mark the list dirty. Does NOT
  // write to disk — this runs on every page turn, so the (full JSON) write is
  // deferred to saveIfDirty() on reader exit to keep the page-turn path cheap.
  void updateBookProgress(const std::string& path, uint8_t progressPercent);

  // Flush a pending deferred progress update to disk. Call on reader exit.
  void saveIfDirty();

  // Get the list of recent books (most recent first)
  const std::vector<RecentBook>& getBooks() const { return recentBooks; }

  // Get the count of recent books
  int getCount() const { return static_cast<int>(recentBooks.size()); }

  bool saveToFile() const;

  bool loadFromFile();
  RecentBook getDataFromBook(std::string path) const;

 private:
  bool loadFromBinaryFile();

  bool progressDirty = false;  // set by updateBookProgress, cleared by saveIfDirty/saveToFile callers
};

// Helper macro to access recent books store
#define RECENT_BOOKS RecentBooksStore::getInstance()
