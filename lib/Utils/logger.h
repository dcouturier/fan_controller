#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <IPAddress.h>

// Logger - Circular buffer logging system with Serial output
//
// Provides a simple logging interface that writes to Serial while maintaining
// a circular buffer of recent log entries for display in the web interface.
//
// Features:
// - All log entries are printed to Serial immediately
// - Maintains last 1000 log entries in memory (circular buffer)
// - Thread-safe for use with FreeRTOS tasks
// - Supports multiple data types (String, char*, IPAddress)
//
// Usage:
//   Logger::println("System started");
//   Logger::println(WiFi.localIP());
//   String logs = Logger::get();  // Retrieve all buffered logs for web display
//
namespace Logger {
void println();
void println(const char* s);
void println(const String& s);
void println(const IPAddress& ip);
void printf(const char* format, ...);
// Generic println templates were removed to avoid template-instantiation
// linker issues on Arduino builds; use the supplied overloads instead.

String get();
void clear();
}  // namespace Logger

#endif  // LOGGER_H
