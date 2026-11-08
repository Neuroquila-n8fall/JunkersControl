#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <esp_attr.h>
#include <esp_system.h>
#include <stdarg.h>
#include <memory>
#include <logger.h>
#include <telnet.h>
#include <timesync.h>
#include <configuration.h>

//——————————————————————————————————————————————————————————————————————————————
//  One mutex serialises every log call. Under it a line is formatted once into a
//  static buffer, written to Serial, queued for telnet and stored in two rings:
//  a 16 KB heap history (web page) and a 4 KB reset-surviving event buffer.
//  Ring entry: len(1) level|wall(1) seq(4) ts(4) text(len-11) check(1).
//——————————————————————————————————————————————————————————————————————————————

namespace
{
  constexpr size_t TextMax = 200; // message bytes including the terminating NUL
  constexpr size_t PrefixMax = 32;
  constexpr size_t EntryHeader = 10;
  constexpr size_t EntryOverhead = EntryHeader + 1;
  constexpr size_t EntryMax = EntryOverhead + TextMax;
  constexpr size_t HistorySize = 16384;
  constexpr size_t PersistentSize = 4096;
  constexpr uint32_t PersistentMagic = 0x4C4F4731; // "LOG1"
  constexpr uint8_t WallClockFlag = 0x80;
  constexpr uint8_t SseBatch = 8;

  // Rings use monotonically growing byte positions (position % size is the offset), so
  // head - tail is the used size and a reader detects eviction by pos < tail.
  struct Ring
  {
    uint8_t *data;
    uint32_t size;
    volatile uint32_t *tail;
    volatile uint32_t *head;
  };

  struct Persistent
  {
    uint32_t magic;
    uint32_t boots;
    volatile uint32_t tail;
    volatile uint32_t head;
    uint32_t check;
    uint8_t data[PersistentSize - 20];
  };
  static_assert(sizeof(Persistent) == PersistentSize, "Persistent log layout");

  // Kept in the .noinit section: survives software resets, panics and watchdog resets.
  __NOINIT_ATTR Persistent persistent;

  uint32_t historyTail = 0;
  uint32_t historyHead = 0;
  uint32_t historySeq = 0;
  uint32_t persistentSeq = 1;
  Ring history{nullptr, HistorySize, &historyTail, &historyHead};
  Ring persistentRing{persistent.data, sizeof persistent.data, &persistent.tail, &persistent.head};

  SemaphoreHandle_t lock = nullptr; // null until LogBegin: calls before it only reach Serial
  StaticSemaphore_t lockStorage;
  char lineBuffer[PrefixMax + TextMax + 8];

  AsyncEventSource *logEvents = nullptr;
  uint32_t ssePos = 0;
  char bootId[9] = "00000000"; // random per boot, sent as X-Log-Boot so clients detect restarts

  // Local-time offset, refreshed once a minute so ezTime's String-heavy conversion stays off the per-line path.
  uint32_t offsetMinute = UINT32_MAX;
  int32_t offsetSeconds = 0;

  inline void takeLock()
  {
    if (lock)
      xSemaphoreTake(lock, portMAX_DELAY);
  }

  inline void giveLock()
  {
    if (lock)
      xSemaphoreGive(lock);
  }

  char *putNumber(char *p, uint32_t value, uint8_t minDigits)
  {
    char digits[10];
    uint8_t count = 0;
    do
    {
      digits[count++] = '0' + value % 10;
      value /= 10;
    } while (value != 0 || count < minDigits);
    while (count > 0)
      *p++ = digits[--count];
    return p;
  }

  // "YYYY-MM-DD HH:MM:SS" for a local-time stamp, "+<seconds>.<millis>s" for uptime.
  char *putTime(char *p, uint32_t ts, bool wall)
  {
    if (!wall)
    {
      p = putNumber(p, ts / 1000, 1);
      *p++ = '.';
      p = putNumber(p, ts % 1000, 3);
      *p++ = 's';
      return p;
    }
    tmElements_t tm;
    ezt::breakTime(ts, tm);
    p = putNumber(p, tm.Year + 1970, 4);
    *p++ = '-';
    p = putNumber(p, tm.Month, 2);
    *p++ = '-';
    p = putNumber(p, tm.Day, 2);
    *p++ = ' ';
    p = putNumber(p, tm.Hour, 2);
    *p++ = ':';
    p = putNumber(p, tm.Minute, 2);
    *p++ = ':';
    p = putNumber(p, tm.Second, 2);
    return p;
  }

  // Returns true with local epoch seconds in *ts once ezTime is set, else false with uptime millis.
  bool stamp(uint32_t *ts)
  {
    if (timeStatus() == timeNotSet)
    {
      *ts = millis();
      return false;
    }
    uint32_t utc = UTC.now();
    if (utc / 60 != offsetMinute)
    {
      offsetMinute = utc / 60;
      offsetSeconds = (int32_t)(myTZ.tzTime(utc, UTC_TIME) - utc);
    }
    *ts = utc + offsetSeconds;
    return true;
  }

  // Header checksum over magic, boots, tail and head (position-dependent mix, not a plain XOR).
  uint32_t persistentChecksum()
  {
    uint32_t sum = 0x5A17C0DE;
    const uint32_t fields[] = {persistent.magic, persistent.boots, persistent.tail, persistent.head};
    for (uint32_t field : fields)
      sum = ((sum << 5) | (sum >> 27)) ^ field * 0x9E3779B1u;
    return sum;
  }

  void persistentSeal()
  {
    persistent.check = persistentChecksum();
  }

  uint8_t entryCheck(const uint8_t *entry, size_t length, uint8_t check = 0xA5)
  {
    for (size_t i = 0; i < length; i++)
      check ^= entry[i];
    return check;
  }

  void copyIn(const Ring &ring, uint32_t pos, const uint8_t *src, size_t length)
  {
    size_t at = pos % ring.size;
    size_t first = min(length, (size_t)(ring.size - at));
    memcpy(ring.data + at, src, first);
    memcpy(ring.data, src + first, length - first);
  }

  void copyOut(const Ring &ring, uint32_t pos, uint8_t *dst, size_t length)
  {
    size_t at = pos % ring.size;
    size_t first = min(length, (size_t)(ring.size - at));
    memcpy(dst, ring.data + at, first);
    memcpy(dst + first, ring.data, length - first);
  }

  // Caller holds the lock. New bytes land in free space and become visible with the single
  // head update, so a crash mid-write leaves the entries before it intact.
  void ringPush(Ring &ring, LogLevel level, bool wall, uint32_t seq, uint32_t ts, const char *text, size_t length)
  {
    if (!ring.data)
      return;
    size_t total = EntryOverhead + length;
    while (ring.size - (*ring.head - *ring.tail) < total)
    {
      uint8_t oldest;
      copyOut(ring, *ring.tail, &oldest, 1);
      if (oldest < EntryOverhead || oldest > *ring.head - *ring.tail)
      {
        *ring.tail = *ring.head; // damaged framing: drop everything
        break;
      }
      *ring.tail += oldest;
    }
    uint8_t header[EntryHeader];
    header[0] = total;
    header[1] = level | (wall ? WallClockFlag : 0);
    memcpy(header + 2, &seq, 4);
    memcpy(header + 6, &ts, 4);
    uint8_t check = entryCheck((const uint8_t *)text, length, entryCheck(header, EntryHeader));
    uint32_t pos = *ring.head;
    copyIn(ring, pos, header, EntryHeader);
    copyIn(ring, pos + EntryHeader, (const uint8_t *)text, length);
    copyIn(ring, pos + EntryHeader + length, &check, 1);
    __sync_synchronize();
    *ring.head += total;
    if (&ring == &persistentRing)
      persistentSeal(); // covers the eviction and the head store; a reset before it is handled by persistentValid
  }

  // Caller holds the lock. Copies the entry at *pos into raw (EntryMax bytes) and advances *pos;
  // false at `end`, or on framing/check failure (a damaged entry ends the stream).
  bool ringRead(const Ring &ring, uint32_t *pos, uint32_t end, uint8_t *raw)
  {
    if (!ring.data)
      return false;
    uint32_t tail = *ring.tail;
    uint32_t head = *ring.head;
    if ((int32_t)(*pos - tail) < 0)
      *pos = tail; // evicted meanwhile: continue with the oldest entry left
    if ((int32_t)(*pos - end) >= 0 || (int32_t)(*pos - head) >= 0)
      return false;
    uint8_t length;
    copyOut(ring, *pos, &length, 1);
    if (length < EntryOverhead || length > EntryMax || length > head - *pos)
      return false;
    copyOut(ring, *pos, raw, length);
    if (raw[length - 1] != entryCheck(raw, length - 1) || (raw[1] & ~WallClockFlag) > Debug)
      return false;
    *pos += length;
    return true;
  }

  // "seq\tlevel\ttime\tmessage" without line ending (at most 233 bytes).
  size_t formatEntry(char *out, const uint8_t *raw)
  {
    uint32_t seq, ts;
    memcpy(&seq, raw + 2, 4);
    memcpy(&ts, raw + 6, 4);
    char *p = putNumber(out, seq, 1);
    *p++ = '\t';
    *p++ = '0' + (raw[1] & ~WallClockFlag);
    *p++ = '\t';
    bool wall = raw[1] & WallClockFlag;
    if (!wall)
      *p++ = '+';
    p = putTime(p, ts, wall);
    *p++ = '\t';
    size_t length = raw[0] - EntryOverhead;
    memcpy(p, raw + EntryHeader, length);
    return p + length - out;
  }

  // Validates the surviving buffer at boot; trims it to its last intact entry. A header whose checksum
  // does not match is either garbage or torn by a reset between the head store and the checksum store
  // in ringPush (the entries themselves are written first and checked individually): it is kept only if
  // at least one entry validates, so a crash right there does not wipe the buffer.
  bool persistentValid(uint32_t *lastSeq)
  {
    if (persistent.magic != PersistentMagic || persistent.head - persistent.tail > sizeof persistent.data)
      return false;
    bool sealed = persistent.check == persistentChecksum();
    uint8_t raw[EntryMax];
    uint32_t pos = persistent.tail;
    uint32_t end = persistent.head;
    uint32_t entries = 0;
    *lastSeq = 0;
    while (ringRead(persistentRing, &pos, end, raw))
    {
      memcpy(lastSeq, raw + 2, 4);
      entries++;
    }
    if (!sealed && entries == 0)
      return false;
    persistent.head = pos; // drop a torn or corrupt tail
    return true;
  }

  void emit(LogLevel level, bool store, const char *format, va_list args)
  {
    takeLock();
    char *text = lineBuffer + PrefixMax;
    int length = vsnprintf(text, TextMax, format, args);
    if (length < 0)
      length = 0;
    if ((size_t)length >= TextMax)
    {
      // Truncate on a UTF-8 boundary and mark it.
      size_t cut = TextMax - 4;
      while (cut > 0 && (text[cut] & 0xC0) == 0x80)
        cut--;
      memcpy(text + cut, "…", 3);
      length = cut + 3;
    }
    for (int i = 0; i < length; i++)
      if (text[i] == '\r' || text[i] == '\n' || text[i] == '\t')
        text[i] = ' ';

    uint32_t ts;
    bool wall = stamp(&ts);

    // Console prefix is built right-aligned in front of the text, so the line needs no copy.
    static const char *const Colors[] = {"\033[31m", "\033[33m", "\033[34m"};
    const char *color = level <= Info ? Colors[level] : nullptr;
    char prefix[PrefixMax];
    char *p = prefix;
    if (color)
    {
      memcpy(p, color, 5);
      p += 5;
    }
    *p++ = '[';
    if (!wall)
      *p++ = '+';
    p = putTime(p, ts, wall);
    *p++ = ']';
    *p++ = ' ';
    size_t prefixLength = p - prefix;
    char *start = text - prefixLength;
    memcpy(start, prefix, prefixLength);
    char *end = text + length;
    if (color)
    {
      memcpy(end, "\033[0m", 4);
      end += 4;
    }
    *end++ = '\r';
    *end++ = '\n';

    Serial.write((const uint8_t *)start, end - start);
    TelnetWrite(start, end - start);

    if (store && lock)
    {
      if (history.data)
        ringPush(history, level, wall, ++historySeq, ts, text, length);
      if (level <= Info)
        ringPush(persistentRing, level, wall, persistentSeq++, ts, text, length);
    }
    giveLock();
  }

  struct ChunkState
  {
    const Ring *ring;
    uint32_t pos;
    uint32_t end;
    uint32_t since;
    uint16_t pending;
    uint16_t sent;
    char line[240];
  };

  // Fills one response chunk with whole entries, copying under the lock one entry at a time.
  size_t fillChunk(ChunkState &state, uint8_t *out, size_t maxLength)
  {
    size_t filled = 0;
    uint8_t raw[EntryMax];
    while (filled < maxLength)
    {
      if (state.sent < state.pending)
      {
        size_t count = min(maxLength - filled, (size_t)(state.pending - state.sent));
        memcpy(out + filled, state.line + state.sent, count);
        state.sent += count;
        filled += count;
        continue;
      }
      takeLock();
      bool more = ringRead(*state.ring, &state.pos, state.end, raw);
      giveLock();
      if (!more)
        break;
      uint32_t seq;
      memcpy(&seq, raw + 2, 4);
      if (seq <= state.since)
        continue;
      state.pending = formatEntry(state.line, raw);
      state.line[state.pending++] = '\n';
      state.sent = 0;
    }
    return filled;
  }

  // Streams a snapshot of the ring (everything present now) as chunked text/plain.
  void streamLog(AsyncWebServerRequest *request, const Ring *ring, uint32_t since)
  {
    std::shared_ptr<ChunkState> state(new (std::nothrow) ChunkState());
    if (!state)
    {
      request->send(503, "text/plain", "Out of memory");
      return;
    }
    state->ring = ring;
    state->since = since;
    takeLock();
    state->pos = *ring->tail;
    state->end = *ring->head;
    giveLock();
    AsyncWebServerResponse *response = request->beginChunkedResponse(
        "text/plain; charset=utf-8",
        [state](uint8_t *buffer, size_t maxLength, size_t) -> size_t
        { return maxLength ? fillChunk(*state, buffer, maxLength) : RESPONSE_TRY_AGAIN; });
    response->addHeader("Cache-Control", "no-store");
    response->addHeader("X-Log-Boot", bootId);
    request->send(response);
  }
}

void LogBegin()
{
  esp_reset_reason_t reason = esp_reset_reason();
  uint32_t lastSeq = 0;
  // No reset-reason special case: validation alone decides. Power-on and EN/RTS resets lose DRAM on the
  // bench board, so their garbage is rejected by the magic, checksum and per-entry checks.
  if (!persistentValid(&lastSeq))
  {
    persistent.magic = PersistentMagic;
    persistent.boots = 0;
    persistent.tail = 0;
    persistent.head = 0;
    lastSeq = 0;
  }
  else
  {
    // Free-running positions must never reach the 2^32 wrap, where pos % size is not continuous.
    uint32_t base = persistent.tail - persistent.tail % sizeof persistent.data;
    persistent.tail -= base;
    persistent.head -= base;
  }
  persistentSeq = lastSeq + 1;
  persistent.boots++;
  persistentSeal();
  snprintf(bootId, sizeof bootId, "%08x", (unsigned)(esp_random() ^ (persistent.boots * 0x9E3779B1u)));

  history.data = (uint8_t *)malloc(HistorySize);
  lock = xSemaphoreCreateMutexStatic(&lockStorage);

  const char *name = "unknown";
  bool abnormal = false;
  switch (reason)
  {
  case ESP_RST_POWERON:
    name = "power-on";
    break;
  case ESP_RST_EXT:
    name = "external";
    break;
  case ESP_RST_SW:
    name = "software";
    break;
  case ESP_RST_PANIC:
    name = "panic";
    abnormal = true;
    break;
  case ESP_RST_INT_WDT:
    name = "int_wdt";
    abnormal = true;
    break;
  case ESP_RST_TASK_WDT:
    name = "task_wdt";
    abnormal = true;
    break;
  case ESP_RST_WDT:
    name = "wdt";
    abnormal = true;
    break;
  case ESP_RST_DEEPSLEEP:
    name = "deep-sleep";
    break;
  case ESP_RST_BROWNOUT:
    name = "brownout";
    abnormal = true;
    break;
  case ESP_RST_SDIO:
    name = "sdio";
    break;
  default:
    break;
  }
  LogWrite(abnormal ? Warn : Info, "▶ Boot #%u · reset: %s", (unsigned)persistent.boots, name);
  if (!history.data)
    LogWarn("Log history unavailable: out of memory");
}

void LogService()
{
  TelnetFlush();
  if (!logEvents)
    return;
  if (logEvents->count() == 0)
  {
    ssePos = historyHead; // nobody listening: do not build a backlog
    return;
  }
  uint8_t raw[EntryMax];
  char line[240];
  for (uint8_t i = 0; i < SseBatch; i++)
  {
    // A backed-up client would silently lose events (the library drops past SSE_MAX_QUEUED_MESSAGES):
    // leave ssePos where it is, the history ring still holds the lines.
    if (logEvents->avgPacketsWaiting() >= SSE_MAX_QUEUED_MESSAGES / 2)
      break;
    takeLock();
    bool more = ringRead(history, &ssePos, historyHead, raw);
    giveLock();
    if (!more)
      break;
    uint32_t seq;
    memcpy(&seq, raw + 2, 4);
    line[formatEntry(line, raw)] = '\0';
    logEvents->send(line, "log", seq);
  }
}

void RegisterLogRoutes(AsyncWebServer *server)
{
  // "/api/logs" also matches "/api/logs/..." in this server version: register the specific routes first.
  logEvents = new AsyncEventSource("/api/logs/events");
  server->addHandler(logEvents);
  server->on("/api/logs/persistent", HTTP_GET, [](AsyncWebServerRequest *request)
             { streamLog(request, &persistentRing, 0); });
  server->on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request)
             {
    uint32_t since = 0;
    if (request->hasParam("since"))
      since = strtoul(request->getParam("since")->value().c_str(), nullptr, 10);
    streamLog(request, &history, since); });
}

void LogWrite(LogLevel level, const char *format, ...)
{
  if (level >= Debug && !configuration.General.Debug)
    return;
  va_list args;
  va_start(args, format);
  emit(level > Debug ? Debug : level, true, format, args);
  va_end(args);
}

void LogError(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  emit(Error, true, format, args);
  va_end(args);
}

void LogWarn(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  emit(Warn, true, format, args);
  va_end(args);
}

void LogInfo(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  emit(Info, true, format, args);
  va_end(args);
}

void LogDebug(const char *format, ...)
{
  if (!configuration.General.Debug)
    return;
  va_list args;
  va_start(args, format);
  emit(Debug, true, format, args);
  va_end(args);
}

void LogCan(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  emit(Debug, false, format, args);
  va_end(args);
}
