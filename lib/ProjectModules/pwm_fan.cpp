#include "pwm_fan.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define kDefaultPwmFrequency 25000  // 25kHz
#define kPwmResolution 8            // 256 gives ~0.4% granularity
#define kPwmDefaultDutyCyclePercent 50
#define kTachSampleIntervalMs 1000
#define kSmoothingPeriodMs 200  // Update duty cycle smoothing every 200ms
#define kSmoothingStepPercent \
  0.05f  // 10% of the difference per smoothing cycle

PWMFan::PWMFan(uint8_t pwm_pin, uint8_t tach_pin, uint8_t channel_number,
               RpmCalculationMethod method, float minimum_duty_cycle_percent)
    : pwm_pin_(pwm_pin),
      tach_pin_(tach_pin),
      channel_number_(channel_number),
      calculation_method_(method),
      tach_pulses_(0),
      latest_rpm_(0),
      last_tach_time_(0),
      current_duty_cycle_(kPwmDefaultDutyCyclePercent),
      target_duty_cycle_(kPwmDefaultDutyCyclePercent),
      minimum_duty_cycle_(minimum_duty_cycle_percent),
      last_smooth_time_(0),
      buffer_index_(0),
      last_state_(false),
      rpm_task_handle_(nullptr),
      sampling_task_handle_(nullptr) {
  // Initialize circular buffer (used with SAMPLING method)
  for (int i = 0; i < kBufferSize; i++) {
    sample_buffer_[i] = false;
  }

  // Set pin modes
  pinMode(pwm_pin_, OUTPUT);
  pinMode(tach_pin_, INPUT_PULLUP);

  // Setup PWM
  ledcSetup(channel_number_, kDefaultPwmFrequency, kPwmResolution);
  ledcAttachPin(pwm_pin_, channel_number_);

  // Set default duty cycle (50%)
  int duty_value = (1 << kPwmResolution) * kPwmDefaultDutyCyclePercent / 100;
  ledcWrite(channel_number_, duty_value);

  // Setup based on calculation method
  if (calculation_method_ == kRpmCalculationSampling) {
    // Create FreeRTOS task for 1ms sampling
    xTaskCreate(samplingTask,           // Task function
                "Tach_Sample_Task",     // Task name
                2048,                   // Stack size
                this,                   // Parameter (this PWMFan instance)
                2,                      // Priority (higher than RPM task)
                &sampling_task_handle_  // Task handle
    );
  } else {
    // kRpmCalculationDefault: Attach interrupt for tachometer on rising edge
    attachInterruptArg(digitalPinToInterrupt(tach_pin_), tachISR, this, RISING);
  }

  // Create FreeRTOS task for RPM calculation (runs every 1 second)
  xTaskCreate(rpmCalculationTask,  // Task function
              "RPM_Task",          // Task name
              2048,                // Stack size
              this,                // Parameter (this PWMFan instance)
              1,                   // Priority
              &rpm_task_handle_    // Task handle
  );
}

PWMFan::~PWMFan() {
  if (sampling_task_handle_ != nullptr) {
    vTaskDelete(sampling_task_handle_);
    sampling_task_handle_ = nullptr;
  }

  if (rpm_task_handle_ != nullptr) {
    vTaskDelete(rpm_task_handle_);
    rpm_task_handle_ = nullptr;
  }

  if (calculation_method_ == kRpmCalculationDefault) {
    detachInterrupt(digitalPinToInterrupt(tach_pin_));
  }
}

void IRAM_ATTR PWMFan::tachISR(void* arg) {
  PWMFan* fan = static_cast<PWMFan*>(arg);
  unsigned long current_time = millis();

  // Debounce: ignore if less than 5ms since last pulse
  if (current_time - fan->last_tach_time_ >= 5) {
    fan->tach_pulses_++;
    fan->last_tach_time_ = current_time;
  }
}

void PWMFan::samplingTask(void* arg) {
  PWMFan* fan = static_cast<PWMFan*>(arg);

  while (true) {
    // Read current pin state and add to circular buffer
    bool current_reading = digitalRead(fan->tach_pin_);
    fan->sample_buffer_[fan->buffer_index_] = current_reading;
    fan->buffer_index_ = (fan->buffer_index_ + 1) % kBufferSize;

    // Count how many samples are HIGH
    int high_count = 0;
    for (int i = 0; i < kBufferSize; i++) {
      if (fan->sample_buffer_[i]) {
        high_count++;
      }
    }

    // Determine current state by majority vote
    bool current_state = (high_count >= 3);  // At least 3 out of 5 samples HIGH

    // Detect rising edge (LOW to HIGH transition)
    if (current_state && !fan->last_state_) {
      fan->tach_pulses_++;
    }

    fan->last_state_ = current_state;

    // Smoothing: Update current duty cycle towards target every
    // kSmoothingPeriodMs
    fan->UpdateDutyCycleSmoothing();

    // Wait 1ms before next sample
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void PWMFan::rpmCalculationTask(void* arg) {
  PWMFan* fan = static_cast<PWMFan*>(arg);

  while (true) {
    // For DEFAULT method, also handle smoothing here since there's no
    // samplingTask
    if (fan->calculation_method_ == kRpmCalculationDefault) {
      // Check smoothing every kSmoothingPeriodMs
      for (int i = 0; i < 5; i++) {
        vTaskDelay(pdMS_TO_TICKS(kSmoothingPeriodMs));
        fan->UpdateDutyCycleSmoothing();
      }
    } else {
      // For SAMPLING method, smoothing is handled in samplingTask, just wait 1
      // second
      vTaskDelay(pdMS_TO_TICKS(kTachSampleIntervalMs));
    }

    // Calculate RPM (standard PC fans emit 2 pulses per revolution)
    int pulses = fan->tach_pulses_;
    fan->tach_pulses_ = 0;
    fan->latest_rpm_ = (pulses / 2) * (60000 / kTachSampleIntervalMs);
  }
}

void PWMFan::UpdateDutyCycleSmoothing() {
  unsigned long current_time = millis();
  if (current_time - last_smooth_time_ >= kSmoothingPeriodMs) {
    last_smooth_time_ = current_time;

    float difference = target_duty_cycle_ - current_duty_cycle_;
    if (abs(difference) >
        0.001f) {  // Only update if there's a meaningful difference
      // Approach by kSmoothingStepPercent of the difference
      float step = difference * kSmoothingStepPercent;

      // Ensure minimum step of 2/(2^kPwmResolution) to avoid stalling
      float min_step = 2.0f / (1 << kPwmResolution);
      if (abs(step) < min_step) {
        step = (difference > 0) ? min_step : -min_step;
      }

      current_duty_cycle_ += step;

      // Clamp to avoid overshooting
      if ((difference > 0 && current_duty_cycle_ > target_duty_cycle_) ||
          (difference < 0 && current_duty_cycle_ < target_duty_cycle_)) {
        current_duty_cycle_ = target_duty_cycle_;
      }

      // Apply the new duty cycle to PWM hardware
      int duty_value = (1 << kPwmResolution) * current_duty_cycle_ / 100.0f;
      ledcWrite(channel_number_, duty_value);
    }
  }
}

Status PWMFan::SetTargetDutyCycle(float percent) {
  if (percent > 100.0f) percent = 100.0f;
  if (percent < minimum_duty_cycle_) percent = minimum_duty_cycle_;

  // Set target duty cycle - smoothing will gradually approach this value
  target_duty_cycle_ = percent;
  return OkStatus();
}

Status PWMFan::SetDutyCycle(float percent) {
  if (percent > 100.0f) percent = 100.0f;
  if (percent < minimum_duty_cycle_) percent = minimum_duty_cycle_;

  target_duty_cycle_ = percent;
  current_duty_cycle_ = percent;

  // Apply the new duty cycle to PWM hardware immediately
  int duty_value = (1 << kPwmResolution) * current_duty_cycle_ / 100.0f;
  ledcWrite(channel_number_, duty_value);

  return OkStatus();
}

StatusOr<int> PWMFan::GetRpm() const { return static_cast<int>(latest_rpm_); }

StatusOr<float> PWMFan::GetDutyCycle() const { return current_duty_cycle_; }

StatusOr<float> PWMFan::GetTargetDutyCycle() const {
  return target_duty_cycle_;
}
