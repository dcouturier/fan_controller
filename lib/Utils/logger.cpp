#include "logger.h"

namespace Logger {

const int LOG_CAPACITY = 50;
static String buffer[LOG_CAPACITY];
static int head = 0;   // index of oldest entry
static int tail = 0;   // index to write next
static int count = 0;  // number of stored entries

// internal push
static void pushLine(const String& line) {
  Serial.println(line);
  buffer[tail] = line;
  tail = (tail + 1) % LOG_CAPACITY;
  if (count < LOG_CAPACITY) {
    ++count;
  } else {
    // buffer full, advance head to overwrite oldest
    head = tail;
  }
}

// public API
void println() { pushLine(String()); }

void println(const char* s) { pushLine(String(s)); }

void println(const String& s) { pushLine(s); }

void println(const IPAddress& ip) { pushLine(ip.toString()); }

void printf(const char* format, ...) {
  char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  pushLine(String(buf));
}

// Note: avoid a catch-all template here. Use the String overload to
// accept formatted/concatenated Arduino StringSumHelper values via
// implicit conversion to String.

// Return all buffered lines concatenated with '\n' between lines.
String get() {
  String out;
  out.reserve(256);  // small reserve to avoid too many reallocs
  for (int i = 0; i < count; ++i) {
    int index = (head + i) % LOG_CAPACITY;
    out += buffer[index];
    if (i + 1 < count) out += '\n';
  }
  return out;
}

void clear() {
  // Clear strings to free memory
  for (int i = 0; i < count; ++i) {
    int index = (head + i) % LOG_CAPACITY;
    buffer[index].remove(0);
  }
  head = tail = count = 0;
}

}  // namespace Logger
