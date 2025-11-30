#ifndef PERF_LOGGER_H
#define PERF_LOGGER_H

#include <Arduino.h>

#include "pwm_fan.h"
#include "thermistor.h"

// Structure for a single performance log record
// Packed to ensure consistent size on disk
// Note: Total size is 21 bytes (2 timestamp + 16 fans + 3 thermistors)
// The requirement mentioned 20 bytes but listed fields summing to 21 (or 25
// depending on interpretation). We follow the field list which results in 21
// bytes.
struct __attribute__((packed)) PerfLogRecord {
  uint16_t timestamp;  // Seconds since boot

  // Fan 1
  uint8_t fan1_target_duty;
  uint8_t fan1_current_duty;
  uint16_t fan1_rpm;

  // Fan 2
  uint8_t fan2_target_duty;
  uint8_t fan2_current_duty;
  uint16_t fan2_rpm;

  // Fan 3
  uint8_t fan3_target_duty;
  uint8_t fan3_current_duty;
  uint16_t fan3_rpm;

  // Fan 4 (Pump)
  uint8_t fan4_target_duty;
  uint8_t fan4_current_duty;
  uint16_t fan4_rpm;

  // Thermistors
  uint8_t temp_ambient;
  uint8_t temp_coolant_in;
  uint8_t temp_coolant_out;
};

class PerfLogger {
 public:
  PerfLogger(PWMFan* fan1, PWMFan* fan2, PWMFan* fan3, PWMFan* pump,
             Thermistor* ambient, Thermistor* coolant_in,
             Thermistor* coolant_out);

  // Initialize file system and start logging task
  void Start();

 private:
  // Task functions
  static void LoggingTask(void* parameter);
  static void ServerTask(void* parameter);

  // Helper to manage file rotation
  void RotateFiles();

  // Helper to get next filename
  String GetCurrentFileName();

  // Helper to encode temperature
  uint8_t EncodeTemperature(float temp_c);

  // Helper to encode duty cycle
  uint8_t EncodeDutyCycle(float duty_percent);

  PWMFan* fans_[4];
  Thermistor* thermistors_[3];

  int current_file_index_;
  int current_record_count_;
};

#endif  // PERF_LOGGER_H
