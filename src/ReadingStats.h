#pragma once
#include <cstdint>
#include <ctime>

// Tracks reading session time and book progress for the Reading Stats sleep screen.
// Stats are accumulated each time the reader activity exits (including on sleep entry).
class ReadingStats {
  static ReadingStats instance;
  time_t sessionStartTime = 0;  // Wall-clock time of session start (survives deep sleep, unlike millis())
  bool sessionActive = false;
  uint32_t sessionPages = 0;    // Forward page turns in the current session (not persisted)

 public:
  uint32_t totalReadSeconds = 0;    // All-time cumulative reading time
  uint32_t todayReadSeconds = 0;    // Current-day reading time (resets at midnight)
  int16_t  lastReadYear = 0;        // tm_year when last session was recorded
  int16_t  lastReadDayOfYear = -1;  // tm_yday when last session was recorded
  char     lastBookTitle[64] = {};  // Title of the last book read
  uint8_t  lastBookProgress = 0;    // 0-100% progress in last book

  // Extended stats (v2)
  uint16_t totalSessions = 0;       // Total reading sessions
  uint16_t booksFinished = 0;       // Books that reached 100%
  uint16_t currentStreak = 0;       // Consecutive days with reading
  uint16_t longestStreak = 0;       // Best streak ever

  // Extended stats (v3) — page counts + weekly history
  uint32_t totalPagesTurned = 0;    // All-time forward page turns (for pages/min pace)
  uint32_t pagesToday = 0;          // Forward page turns today (resets at midnight)
  uint32_t dailySeconds[7] = {};    // Per-day reading time, ring keyed by (dayNumber % 7)
  int32_t  lastDayNumber = -1;      // Local-day number of last recorded activity (see currentDayNumber)

  static ReadingStats& getInstance() { return instance; }

  // Local-day number for "now" (days since epoch in local time), or -1 if the
  // wall clock is not yet valid (NTP unsynced). Stable across a local day so it
  // can key the dailySeconds ring and detect rollover.
  static int32_t currentDayNumber();

  // Called when entering the reader activity; marks session start time.
  void startSession();

  // Called on each forward page turn during an active session.
  void recordPageTurn();

  // Called when exiting the reader activity; accumulates elapsed time and saves.
  // bookPath is optional — if provided, per-book stats are updated via BookStats.
  void endSession(const char* title, uint8_t progress, const char* bookPath = nullptr);

  bool saveToFile() const;
  bool loadFromFile();
};

#define READ_STATS ReadingStats::getInstance()
