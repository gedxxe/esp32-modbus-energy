#pragma once
#include "energy.h"

namespace fort {
constexpr uint8_t TRACE_CAPACITY = 12;
constexpr uint8_t TRACE_RX_CAPACITY = 16;
enum TraceResult : uint8_t {
  TRACE_OK, TRACE_ECHO, TRACE_TIMEOUT, TRACE_SHORT, TRACE_OVERFLOW,
  TRACE_CRC, TRACE_EXCEPTION, TRACE_INVALID
};
inline const char *traceResultName(TraceResult result) {
  const char *names[] = {"valid", "echo", "timeout", "short", "overflow", "crc", "exception", "invalid"};
  return result <= TRACE_INVALID ? names[result] : "invalid";
}
// Diagnostic classification only: it never authorizes a response or strips bytes.
inline TraceResult inspectTrace(const uint8_t *tx, const uint8_t *rx, size_t size,
                                bool overflow, uint8_t slave, uint8_t words) {
  if (overflow || size > TRACE_RX_CAPACITY) return TRACE_OVERFLOW;
  if (!size) return TRACE_TIMEOUT;
  if (size == 8 && memcmp(tx, rx, 8) == 0) return TRACE_ECHO;
  if (size < 5) return TRACE_SHORT;
  if (crc16(rx, size - 2) != (uint16_t(rx[size - 2]) | (uint16_t(rx[size - 1]) << 8))) return TRACE_CRC;
  if (validFrame(rx, size, slave, words)) return TRACE_OK;
  if (size == 5 && rx[0] == slave && rx[1] == 0x83) return TRACE_EXCEPTION;
  return TRACE_INVALID;
}
struct TraceFrame {
  uint32_t atMs = 0, baud = 0;
  uint16_t address = 0;
  uint8_t tx[8] = {}, rx[TRACE_RX_CAPACITY] = {};
  uint8_t size = 0, slave = 0, words = 0, attempt = 0;
  char parity = 'N';
  bool scan = false, overflow = false;
  TraceResult result = TRACE_TIMEOUT;
};
// Fixed RAM, first 12 attempts then frozen. Core 1 is the only writer.
struct TraceCapture {
  TraceFrame frames[TRACE_CAPACITY] = {};
  uint32_t generation = 0;
  uint8_t count = 0;
  bool active = false;
  void begin() { ++generation; count = 0; active = true; }
  void stop() { active = false; }
  void append(const uint8_t *tx, const uint8_t *rx, size_t size, bool overflow,
              uint32_t atMs, uint32_t baud, char parity, uint8_t slave,
              uint16_t address, uint8_t words, uint8_t attempt, bool scan) {
    if (!active || count >= TRACE_CAPACITY) return;
    TraceFrame frame{};
    frame.atMs = atMs; frame.baud = baud; frame.parity = parity;
    frame.slave = slave; frame.address = address; frame.words = words;
    frame.attempt = attempt; frame.scan = scan;
    frame.overflow = overflow || size > TRACE_RX_CAPACITY;
    frame.size = size > TRACE_RX_CAPACITY ? TRACE_RX_CAPACITY : uint8_t(size);
    memcpy(frame.tx, tx, sizeof(frame.tx));
    if (frame.size) memcpy(frame.rx, rx, frame.size);
    frame.result = inspectTrace(frame.tx, frame.rx, frame.size, frame.overflow, slave, words);
    frames[count++] = frame;
    if (count == TRACE_CAPACITY) active = false;
  }
};
} // namespace fort
