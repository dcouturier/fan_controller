#ifndef PWM_FAN_H
#define PWM_FAN_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdint>

#include "status.h"

enum RpmCalculationMethod {
  kRpmCalculationDefault = 0,  // Simple ISR counting pullups
  kRpmCalculationSampling = 1  // Circular buffer sampling with debouncing
};

// PWMFan - Controls a 4-pin PWM computer fan with RPM monitoring
//
// This class manages a single PWM-controlled fan, providing duty cycle control
// and RPM measurement. It uses 25kHz PWM frequency with 8-bit resolution (256
// levels).
//
// Features:
// - Duty cycle control (0-100%) with configurable minimum speed enforcement
// - RPM measurement via tachometer signal with two calculation methods:
//   * kRpmCalculationDefault: ISR-based pulse counting (fast, may be noisy)
//   * kRpmCalculationSampling: Debounced sampling with 5-sample circular buffer
//   (recommended)
// - Smooth duty cycle transitions (configurable rate, default 10% per 200ms)
// - FreeRTOS task-based operation for non-blocking execution
//
// Configuration:
// - minimum_duty_cycle_percent: Enforces a floor on fan speed (e.g., 35% for
// case fans, 50% for pumps)
// - Smoothing prevents abrupt speed changes by gradually approaching target
// duty cycle
//
class PWMFan {
 public:
  PWMFan(uint8_t pwm_pin, uint8_t tach_pin, uint8_t channel_number,
         RpmCalculationMethod method = kRpmCalculationSampling,
         float minimum_duty_cycle_percent = 50.0f);

  ~PWMFan();

  // Set target duty cycle as percentage (0.0 - 100.0) - smoothed
  Status SetTargetDutyCycle(float percent);

  // Set duty cycle as percentage (0.0 - 100.0) - immediate
  Status SetDutyCycle(float percent, bool override = false);

  // Lock the duty cycle to prevent automatic updates
  void LockDutyCycle();

  // Reset override and return to default speed
  void Reset();

  // Check if override is active
  bool IsOverridden() const;

  // Get current RPM
  StatusOr<int> GetRpm() const;

  // Get current duty cycle percentage
  StatusOr<float> GetDutyCycle() const;

  // Get target duty cycle percentage
  StatusOr<float> GetTargetDutyCycle() const;

  // Get minimum duty cycle percentage
  StatusOr<float> GetMinDutyCycle() const;

 private:
  uint8_t pwm_pin_;
  uint8_t tach_pin_;
  uint8_t channel_number_;
  RpmCalculationMethod calculation_method_;
  volatile int tach_pulses_;
  volatile int latest_rpm_;
  volatile unsigned long last_tach_time_;
  float current_duty_cycle_;
  float target_duty_cycle_;
  float minimum_duty_cycle_;
  bool override_active_;
  volatile unsigned long last_smooth_time_;

  // Circular buffer for debouncing (used with SAMPLING method)
  static const int kBufferSize = 5;
  volatile bool sample_buffer_[kBufferSize];
  volatile int buffer_index_;
  volatile bool last_state_;

  // FreeRTOS task handles
  TaskHandle_t rpm_task_handle_;
  TaskHandle_t sampling_task_handle_;

  // Static ISR handler for tachometer (used with DEFAULT method)
  static void tachISR(void* arg);

  // Static task function for sampling tachometer (used with SAMPLING method)
  static void samplingTask(void* arg);

  // Static task function for RPM calculation
  static void rpmCalculationTask(void* arg);

  // Helper function for duty cycle smoothing
  void UpdateDutyCycleSmoothing();
};

#endif  // PWM_FAN_H
