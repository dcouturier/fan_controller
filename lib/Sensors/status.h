#ifndef STATUS_H
#define STATUS_H

#include <Arduino.h>

#include <utility>


// Minimal Status/StatusOr implementation for embedded systems
// Inspired by Abseil but simplified for Arduino/ESP32

enum class StatusCode {
  kOk = 0,
  kCalibrationError = 1,
  kInvalidArgument = 2,
  kOutOfRange = 3,
  kInternalError = 4,
  kUnknown = 5
};

class Status {
 public:
  Status() : code_(StatusCode::kOk), message_("") {}

  Status(StatusCode code, const String& message = "")
      : code_(code), message_(message) {}

  static Status OK() { return Status(); }

  static Status CalibrationError(const String& message = "") {
    return Status(StatusCode::kCalibrationError, message);
  }

  static Status InvalidArgument(const String& message = "") {
    return Status(StatusCode::kInvalidArgument, message);
  }

  static Status OutOfRange(const String& message = "") {
    return Status(StatusCode::kOutOfRange, message);
  }

  bool ok() const { return code_ == StatusCode::kOk; }

  StatusCode code() const { return code_; }

  const String& message() const { return message_; }

 private:
  StatusCode code_;
  String message_;
};

template <typename T>
class StatusOr {
 public:
  // Construct from a value (implies OK status)
  StatusOr(const T& value)  // NOLINT
      : status_(Status::OK()), value_(value), has_value_(true) {}

  StatusOr(T&& value)  // NOLINT
      : status_(Status::OK()), value_(value), has_value_(true) {}

  // Construct from a Status (implies no value)
  StatusOr(const Status& status)  // NOLINT
      : status_(status), value_(), has_value_(false) {
    // Status must not be OK if constructing without a value
    if (status_.ok()) {
      status_ = Status(StatusCode::kInternalError,
                       "StatusOr constructed with OK status but no value");
    }
  }

  bool ok() const { return status_.ok() && has_value_; }

  const Status& status() const { return status_; }

  const T& value() const { return value_; }

  T& value() { return value_; }

  const T& operator*() const { return value(); }
  T& operator*() { return value(); }

 private:
  Status status_;
  T value_;
  bool has_value_;
};

inline Status OkStatus() { return Status::OK(); }

#endif  // STATUS_H
