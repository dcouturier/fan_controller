#include "fan_controller.h"

#include "logger.h"

FanController::FanController(PWMFan* fan1, PWMFan* fan2, PWMFan* fan3,
                             PWMFan* pump, Thermistor* ambient_temp,
                             Thermistor* coolant_in_temp,
                             Thermistor* coolant_out_temp)
    : fan1_(fan1),
      fan2_(fan2),
      fan3_(fan3),
      pump_(pump),
      ambient_temp_(ambient_temp),
      coolant_in_temp_(coolant_in_temp),
      coolant_out_temp_(coolant_out_temp),
      current_delta_t_(0.0f),
      target_fan_speed_(kMinFanSpeedPercent),
      control_task_handle_(nullptr) {
  Logger::println("FanController initializing...");

  // Create FreeRTOS task for fan control (runs every second)
  xTaskCreate(ControlTask,           // Task function
              "Fan_Control_Task",    // Task name
              4096,                  // Stack size
              this,                  // Parameter (this FanController instance)
              1,                     // Priority
              &control_task_handle_  // Task handle
  );

  Logger::println("FanController initialized");
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
  StatusOr<float> ambient_temp_result = ambient_temp_->GetTemperature();
  StatusOr<float> coolant_in_temp_result = coolant_in_temp_->GetTemperature();
  StatusOr<float> coolant_out_temp_result = coolant_out_temp_->GetTemperature();

  // Check if all temperature readings are valid
  if (!ambient_temp_result.ok()) {
    Logger::println(String("FanController: Ambient temp error: ") +
                    ambient_temp_result.status().message());
    fan1_->SetDutyCycle(kMaxFanSpeedPercent);
    fan2_->SetDutyCycle(kMaxFanSpeedPercent);
    fan3_->SetDutyCycle(kMaxFanSpeedPercent);
    return;
  }

  // Check if at least one coolant sensor is working
  if (!coolant_in_temp_result.ok() && !coolant_out_temp_result.ok()) {
    Logger::println("FanController: Both coolant sensors failed!");
    fan1_->SetDutyCycle(kMaxFanSpeedPercent);
    fan2_->SetDutyCycle(kMaxFanSpeedPercent);
    fan3_->SetDutyCycle(kMaxFanSpeedPercent);
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

  // Calculate target fan speed based on DeltaT and water temperature
  float fan_speed = CalculateFanSpeed(delta_t, highest_coolant_temp);
  target_fan_speed_ = fan_speed;

  // Apply fan speed to fans 1, 2, and 3
  Status status;
  status = fan1_->SetDutyCycle(fan_speed);
  if (!status.ok()) {
    Logger::println(String("FanController: Fan 1 error: ") + status.message());
  }

  status = fan2_->SetDutyCycle(fan_speed);
  if (!status.ok()) {
    Logger::println(String("FanController: Fan 2 error: ") + status.message());
  }

  status = fan3_->SetDutyCycle(fan_speed);
  if (!status.ok()) {
    Logger::println(String("FanController: Fan 3 error: ") + status.message());
  }

  // Pump (fan 4) maintains its current speed (minimum 50% enforced by PWMFan
  // class) We don't change the pump speed based on temperature

  // Log status periodically (every update)
  String in_str = coolant_in_temp_result.ok()
                      ? String(coolant_in_temp_result.value(), 1)
                      : "ERR";
  String out_str = coolant_out_temp_result.ok()
                       ? String(coolant_out_temp_result.value(), 1)
                       : "ERR";

  Logger::println(String("FanController: Ambient=") + String(ambient_temp, 1) +
                  "C, " + "CoolantIn=" + in_str + "C, " + "CoolantOut=" +
                  out_str + "C, " + "DeltaT=" + String(delta_t, 1) + "C, " +
                  "FanSpeed=" + String(fan_speed, 1) + "%");
}

float FanController::CalculateFanSpeed(float delta_t, float water_temp) {
  // Hybrid formula: considers both deltaT and absolute water temperature
  // This ensures fans ramp up earlier on hot ambient days

  float speed_range = kMaxFanSpeedPercent - kMinFanSpeedPercent;

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

  // Calculate final speed
  float speed = kMinFanSpeedPercent + (combined_factor * speed_range);

  // Clamp to valid range
  if (speed < kMinFanSpeedPercent) {
    speed = kMinFanSpeedPercent;
  }
  if (speed > kMaxFanSpeedPercent) {
    speed = kMaxFanSpeedPercent;
  }

  return speed;
}
