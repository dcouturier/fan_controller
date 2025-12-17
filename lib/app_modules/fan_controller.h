#ifndef FAN_CONTROLLER_H
#define FAN_CONTROLLER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <vector>

#include "pwm_fan.h"
#include "thermistor.h"

// FanController - Automatic fan speed control based on water cooling
// temperatures
//
// Controls 3 case fans and 1 water pump based on coolant and ambient
// temperatures. Uses a hybrid algorithm that considers both temperature
// differential (DeltaT) and absolute water temperature to handle varying
// ambient conditions.
//
// Temperature Inputs:
// - Ambient (A0): Outside air temperature
// - Coolant In (A1): Water temperature before CPU/GPU
// - Coolant Out (A2): Water temperature after CPU/GPU and radiator
//
// Speed Calculation:
// Fan speed is determined by weighted combination of two factors:
// - DeltaT Factor (70% weight): Difference between ambient and highest water
// temp
//   * 0°C DeltaT = minimum speed, 10°C+ DeltaT = maximum contribution
// - Water Temp Factor (30% weight): Absolute water temperature boost
//   * 22°C water = baseline, 35°C+ water = maximum contribution
//
// This hybrid approach ensures fans ramp up appropriately even when ambient is
// warm (e.g., 26°C ambient, 32°C water yields higher fan speed than pure DeltaT
// would suggest)
//
// Configuration:
// - Fans 1-3: 35% minimum speed (case fans)
// - Fan 4 (pump): 50% minimum speed (never drops below for flow assurance)
// - Update interval: 1 second
// - Error handling: Sets all fans to 100% if any thermistor reports error
//
class FanController {
 public:
  // Constructor takes vectors of fans and pumps
  // Pumps have special minimum speed requirements and are not controlled by
  // temperature
  FanController(const std::vector<PWMFan*>& fans,
                const std::vector<PWMFan*>& pumps, Thermistor* ambient_temp,
                Thermistor* coolant_in_temp, Thermistor* coolant_out_temp);

  ~FanController();

  // Start the control task
  void Start();

  // Get the current DeltaT (temperature differential)
  float GetDeltaT() const { return current_delta_t_; }

  // Get the current target fan speed percentage
  float GetTargetFanSpeed() const { return target_fan_speed_; }

 private:
  // Fan pointers
  std::vector<PWMFan*> fans_;
  std::vector<PWMFan*> pumps_;

  // Thermistor pointers
  Thermistor* ambient_temp_;
  Thermistor* coolant_in_temp_;
  Thermistor* coolant_out_temp_;

  // Current state
  volatile float current_delta_t_;
  volatile float target_fan_speed_;

  // FreeRTOS task handle
  TaskHandle_t control_task_handle_;

  // Control parameters
  static constexpr float kMaxFanSpeedPercent = 100.0f;
  static constexpr float kMinDeltaT =
      5.0f;  // DeltaT below which kDeltaTWeight has no effect
  static constexpr float kMaxDeltaT = 8.0f;  // DeltaT at which fans run at 100%
  static constexpr float kBaseWaterTemp =
      25.0f;  // Reference water temp (comfortable baseline)
  static constexpr float kMaxWaterTemp =
      30.0f;  // Water temp at which we add maximum boost
  static constexpr float kDeltaTWeight = 0.4f;  // 40% weight on deltaT
  static constexpr float kWaterTempWeight =
      0.6f;  // 60% weight on absolute water temp
  static constexpr unsigned long kUpdateIntervalMs =
      1000;  // Update every second

  // FreeRTOS task function
  static void ControlTask(void* parameter);

  // Update fan speeds based on current temperatures
  void UpdateFanSpeeds();

  // Helper to apply speed to a group of fans
  void ApplyFanSpeed(const std::vector<PWMFan*>& fans, float intensity,
                     const String& type_name);

  // Calculate target fan speed based on DeltaT and water temperature
  float CalculateFanSpeed(float delta_t, float water_temp);
};

#endif  // FAN_CONTROLLER_H
