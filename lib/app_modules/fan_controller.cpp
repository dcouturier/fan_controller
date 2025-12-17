#include "fan_controller.h"

#include "logger.h"

FanController::FanController(const std::vector<PWMFan*>& fans,
                             const std::vector<PWMFan*>& pumps,
                             Thermistor* ambient_temp,
                             Thermistor* coolant_in_temp,
                             Thermistor* coolant_out_temp)
    : fans_(fans),
      pumps_(pumps),
      ambient_temp_(ambient_temp),
      coolant_in_temp_(coolant_in_temp),
      coolant_out_temp_(coolant_out_temp),
      current_delta_t_(0.0f),
      target_fan_speed_(0.0f),
      control_task_handle_(nullptr) {
  Logger::println("FanController initialized");
}

void FanController::Start() {
  Logger::println("Starting FanController task...");
  // Create FreeRTOS task for fan control (runs every second)
  xTaskCreate(ControlTask,           // Task function
              "Fan_Control_Task",    // Task name
              4096,                  // Stack size
              this,                  // Parameter (this FanController instance)
              1,                     // Priority
              &control_task_handle_  // Task handle
  );
}

FanController::~FanController() {
  if (control_task_handle_ != nullptr) {
    vTaskDelete(control_task_handle_);
    control_task_handle_ = nullptr;
  }
}

void FanController::ControlTask(void* parameter) {
  FanController* controller = static_cast<FanController*>(parameter);

  while (true) {
    controller->UpdateFanSpeeds();
    vTaskDelay(pdMS_TO_TICKS(kUpdateIntervalMs));
  }
}

void FanController::UpdateFanSpeeds() {
  // Read temperatures from all thermistors
  StatusOr<float> ambient_temp_result = ambient_temp_->GetSampledTemperature();
  StatusOr<float> coolant_in_temp_result =
      coolant_in_temp_->GetSampledTemperature();
  StatusOr<float> coolant_out_temp_result =
      coolant_out_temp_->GetSampledTemperature();

  // Check if all temperature readings are valid
  if (!ambient_temp_result.ok()) {
    Logger::println(String("FanController: Ambient temp error: ") +
                    ambient_temp_result.status().message());
    for (auto fan : fans_) {
      fan->SetDutyCycle(kMaxFanSpeedPercent);
    }
    return;
  }

  // Check if at least one coolant sensor is working
  if (!coolant_in_temp_result.ok() && !coolant_out_temp_result.ok()) {
    Logger::println("FanController: Both coolant sensors failed!");
    for (auto fan : fans_) {
      fan->SetDutyCycle(kMaxFanSpeedPercent);
    }
    return;
  }
  if (!coolant_in_temp_result.ok()) {
    Logger::println(String("FanController: Coolant In temp error: ") +
                    coolant_in_temp_result.status().message());
  }
  if (!coolant_out_temp_result.ok()) {
    Logger::println(String("FanController: Coolant Out temp error: ") +
                    coolant_out_temp_result.status().message());
  }

  float ambient_temp = ambient_temp_result.value();
  float highest_coolant_temp = 0.0f;

  if (coolant_in_temp_result.ok() && coolant_out_temp_result.ok()) {
    highest_coolant_temp =
        max(coolant_in_temp_result.value(), coolant_out_temp_result.value());
  } else if (coolant_in_temp_result.ok()) {
    highest_coolant_temp = coolant_in_temp_result.value();
  } else {
    highest_coolant_temp = coolant_out_temp_result.value();
  }

  // Calculate DeltaT: difference between ambient and highest coolant
  // temperature
  float delta_t = highest_coolant_temp - ambient_temp;

  // Ensure DeltaT is not negative
  if (delta_t < 0.0f) {
    delta_t = 0.0f;
  }

  current_delta_t_ = delta_t;

  // Calculate target fan speed intensity (0-100%) based on DeltaT and water
  // temperature
  float fan_speed_intensity = CalculateFanSpeed(delta_t, highest_coolant_temp);
  target_fan_speed_ = fan_speed_intensity;

  // Apply fan speed to fans, scaling to their individual minimums
  ApplyFanSpeed(fans_, fan_speed_intensity, "Fan");

  // Apply fan speed to pumps, scaling to their individual minimums
  // Currently, pumps run at the same speed as fans.
  ApplyFanSpeed(pumps_, fan_speed_intensity, "Pump");

  // Log status periodically (every update)
  String in_str = coolant_in_temp_result.ok()
                      ? String(coolant_in_temp_result.value(), 1)
                      : "ERR";
  String out_str = coolant_out_temp_result.ok()
                       ? String(coolant_out_temp_result.value(), 1)
                       : "ERR";

  String log_msg = String("FanController: CAmb=") + String(ambient_temp, 1) +
                   "C, " + "CIn=" + in_str + "C, " + "COut=" + out_str + "C, " +
                   "DT=" + String(delta_t, 1) + "C";

  for (size_t i = 0; i < fans_.size(); i++) {
    auto res = fans_[i]->GetDutyCycle();
    String str = res.ok() ? String(res.value(), 1) : "ERR";
    log_msg += ", F" + String(i + 1) + "=" + str + "%";
  }

  for (size_t i = 0; i < pumps_.size(); i++) {
    auto res = pumps_[i]->GetDutyCycle();
    String str = res.ok() ? String(res.value(), 1) : "ERR";
    log_msg += ", Pmp" + String(i + 1) + "=" + str + "%";
  }

  Logger::println(log_msg);
}

void FanController::ApplyFanSpeed(const std::vector<PWMFan*>& fans,
                                  float intensity, const String& type_name) {
  for (size_t i = 0; i < fans.size(); i++) {
    float min_duty = 0.0f;
    StatusOr<float> min_res = fans[i]->GetMinDutyCycle();
    if (min_res.ok()) min_duty = min_res.value();

    float target = min_duty + (intensity / 100.0f) * (100.0f - min_duty);
    Status status = fans[i]->SetTargetDutyCycle(target);

    if (!status.ok()) {
      Logger::println(String("FanController: ") + type_name + " " +
                      String(i + 1) + " error: " + status.message());
    }
  }
}

float FanController::CalculateFanSpeed(float delta_t, float water_temp) {
  // Hybrid formula: considers both deltaT and absolute water temperature
  // This ensures fans ramp up earlier on hot ambient days

  // Factor 1: DeltaT contribution (0.0 to 1.0)
  if (delta_t < kMinDeltaT) delta_t = kMinDeltaT;
  float delta_t_factor = (delta_t - kMinDeltaT) / (kMaxDeltaT - kMinDeltaT);
  if (delta_t_factor > 1.0f) delta_t_factor = 1.0f;

  // Factor 2: Absolute water temperature contribution (0.0 to 1.0)
  float water_temp_factor =
      (water_temp - kBaseWaterTemp) / (kMaxWaterTemp - kBaseWaterTemp);
  if (water_temp_factor > 1.0f) water_temp_factor = 1.0f;
  if (water_temp_factor < 0.0f) water_temp_factor = 0.0f;

  // Combine factors with weighting
  float combined_factor =
      (kDeltaTWeight * delta_t_factor) + (kWaterTempWeight * water_temp_factor);

  // Calculate final speed intensity (0-100%)
  float speed = combined_factor * 100.0f;

  // Clamp to valid range
  if (speed < 0.0f) {
    speed = 0.0f;
  }
  if (speed > 100.0f) {
    speed = 100.0f;
  }

  return speed;
}
