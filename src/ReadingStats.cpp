#include "ReadingStats.h"

#include "BookStats.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>

#include <cstring>
#include <ctime>

namespace {
constexpr uint8_t STATS_FILE_VERSION = 3;
constexpr char STATS_FILE[] = "/.crosspoint/reading_stats.bin";
constexpr time_t MIN_VALID_TIME = 1704067200;  // 2024-01-01 00:00:00 UTC
}  // namespace

ReadingStats ReadingStats::instance;

int32_t ReadingStats::currentDayNumber() {
  time_t now = time(nullptr);
  if (now < MIN_VALID_TIME) return -1;
  struct tm tmv = {};
  localtime_r(&now, &tmv);
  // Anchor at local noon so the day number is stable regardless of DST shifts.
  tmv.tm_hour = 12;
  tmv.tm_min = 0;
  tmv.tm_sec = 0;
  time_t noon = mktime(&tmv);
  if (noon <= 0) return -1;
  return static_cast<int32_t>(noon / 86400);
}

void ReadingStats::startSession() {
  sessionStartTime = time(nullptr);
  sessionActive = true;
  sessionPages = 0;
}

void ReadingStats::recordPageTurn() {
  if (sessionActive) sessionPages++;
}

void ReadingStats::endSession(const char* title, uint8_t progress, const char* bookPath) {
  if (!sessionActive) return;

  // Check for day rollover; reset today's counter and update streak.
  // Only do this when we have a valid wall-clock time (NTP synced, >= 2024).
  time_t now = time(nullptr);
  struct tm timeinfo = {};
  localtime_r(&now, &timeinfo);
  const int16_t curYear = static_cast<int16_t>(timeinfo.tm_year);
  const int16_t curDay  = static_cast<int16_t>(timeinfo.tm_yday);
  if (now >= MIN_VALID_TIME && (curDay != lastReadDayOfYear || curYear != lastReadYear)) {
    // Check if this is the next consecutive day (streak continues)
    bool isConsecutive = false;
    if (lastReadDayOfYear >= 0) {
      if (curYear == lastReadYear && curDay == lastReadDayOfYear + 1) {
        isConsecutive = true;
      } else if (curYear == lastReadYear + 1 && curDay == 0) {
        // Year boundary: Dec 31 → Jan 1
        isConsecutive = (lastReadDayOfYear == 364 || lastReadDayOfYear == 365);
      }
    }
    if (isConsecutive) {
      currentStreak++;
    } else {
      currentStreak = 1;  // New streak starts
    }
    if (currentStreak > longestStreak) longestStreak = currentStreak;

    todayReadSeconds = 0;
    lastReadYear      = curYear;
    lastReadDayOfYear = curDay;
  }

  // Accumulate session duration using wall-clock time (survives deep sleep unlike millis()).
  // Guard: if either timestamp is before 2024-01-01 (NTP not synced), skip accumulation.
  constexpr uint32_t MAX_SESSION_SECS = 86400;
  uint32_t elapsedSecs = 0;
  if (sessionStartTime >= MIN_VALID_TIME && now >= MIN_VALID_TIME) {
    const int64_t diff = static_cast<int64_t>(now) - static_cast<int64_t>(sessionStartTime);
    if (diff > 0 && diff < static_cast<int64_t>(MAX_SESSION_SECS)) {
      elapsedSecs = static_cast<uint32_t>(diff);
    }
  }

  todayReadSeconds += elapsedSecs;
  totalReadSeconds += elapsedSecs;
  totalSessions++;

  // Weekly history ring + pages-today rollover, keyed by local day number.
  const int32_t curDayNum = currentDayNumber();
  if (curDayNum >= 0) {
    if (lastDayNumber < 0) {
      lastDayNumber = curDayNum;
    } else if (curDayNum != lastDayNumber) {
      // Clear the ring slots for each day that elapsed since the last activity
      // (capped at 7 = the whole ring, which also covers backwards clock moves).
      int32_t gap = curDayNum - lastDayNumber;
      if (gap < 0 || gap > 7) gap = 7;
      for (int32_t d = 1; d <= gap; d++) {
        dailySeconds[(lastDayNumber + d) % 7] = 0;
      }
      pagesToday = 0;
      lastDayNumber = curDayNum;
    }
    dailySeconds[curDayNum % 7] += elapsedSecs;
  }
  pagesToday += sessionPages;
  totalPagesTurned += sessionPages;

  // Track book completion
  uint8_t prevProgress = lastBookProgress;
  if (title && title[0] != '\0') {
    strncpy(lastBookTitle, title, sizeof(lastBookTitle) - 1);
    lastBookTitle[sizeof(lastBookTitle) - 1] = '\0';
  }
  lastBookProgress = progress;
  if (progress >= 100 && prevProgress < 100) {
    booksFinished++;
  }

  sessionActive = false;
  sessionPages = 0;
  saveToFile();

  // Update per-book stats if path was provided
  if (bookPath && bookPath[0] != '\0') {
    BOOK_STATS.updateBook(bookPath, title, elapsedSecs, progress);
  }
}

bool ReadingStats::saveToFile() const {
  Storage.mkdir("/.crosspoint");
  FsFile file;
  if (!Storage.openFileForWrite("RST", STATS_FILE, file)) {
    LOG_ERR("RST", "Failed to open reading_stats.bin for write");
    return false;
  }
  const uint8_t version = STATS_FILE_VERSION;
  serialization::writePod(file, version);
  serialization::writePod(file, totalReadSeconds);
  serialization::writePod(file, todayReadSeconds);
  serialization::writePod(file, lastReadYear);
  serialization::writePod(file, lastReadDayOfYear);
  serialization::writePod(file, lastBookProgress);
  file.write(reinterpret_cast<const uint8_t*>(lastBookTitle), sizeof(lastBookTitle));
  // v2 extended fields
  serialization::writePod(file, totalSessions);
  serialization::writePod(file, booksFinished);
  serialization::writePod(file, currentStreak);
  serialization::writePod(file, longestStreak);
  // v3 extended fields
  serialization::writePod(file, totalPagesTurned);
  serialization::writePod(file, pagesToday);
  for (int i = 0; i < 7; i++) serialization::writePod(file, dailySeconds[i]);
  serialization::writePod(file, lastDayNumber);
  file.close();
  return true;
}

bool ReadingStats::loadFromFile() {
  FsFile file;
  if (!Storage.openFileForRead("RST", STATS_FILE, file)) {
    return false;
  }
  uint8_t version;
  serialization::readPod(file, version);
  if (version == 0 || version > STATS_FILE_VERSION) {
    LOG_ERR("RST", "Unknown reading_stats.bin version %u", version);
    file.close();
    return false;
  }
  serialization::readPod(file, totalReadSeconds);
  serialization::readPod(file, todayReadSeconds);
  serialization::readPod(file, lastReadYear);
  serialization::readPod(file, lastReadDayOfYear);
  serialization::readPod(file, lastBookProgress);
  file.read(reinterpret_cast<uint8_t*>(lastBookTitle), sizeof(lastBookTitle));
  lastBookTitle[sizeof(lastBookTitle) - 1] = '\0';
  // v2 extended fields (defaults to 0 if upgrading from v1)
  if (version >= 2) {
    serialization::readPod(file, totalSessions);
    serialization::readPod(file, booksFinished);
    serialization::readPod(file, currentStreak);
    serialization::readPod(file, longestStreak);
  }
  // v3 extended fields (defaults if upgrading from v1/v2)
  if (version >= 3) {
    serialization::readPod(file, totalPagesTurned);
    serialization::readPod(file, pagesToday);
    for (int i = 0; i < 7; i++) serialization::readPod(file, dailySeconds[i]);
    serialization::readPod(file, lastDayNumber);
  }
  file.close();
  // Re-save in the current format if loaded an older version
  if (version < STATS_FILE_VERSION) saveToFile();

  // Load per-book stats alongside global stats
  BOOK_STATS.loadFromFile();
  return true;
}
