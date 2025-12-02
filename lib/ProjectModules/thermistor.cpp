#include "thermistor.h"

#include <Arduino.h>

#include <cmath>
#include <cstdint>

#include "logger.h"

Thermistor::Thermistor(uint8_t analog_pin, const String& id)
    : analog_pin_(analog_pin),
      id_(id),
      type_(kThermistorTypeCalibrationError),
      sampling_task_handle_(nullptr),
      buffer_index_(0),
      sample_count_(0) {
  pinMode(analog_pin_, INPUT);

  // Initialize buffer
  for (int i = 0; i < kBufferSize; i++) {
    temperature_buffer_[i] = 0.0f;
  }

  // Auto-calibrate: try each thermistor type and see which gives valid
  // temperature Try 10K first (most common)
  float temp_10k = CalculateTemperature(kThermistorType10K);
  if (IsValidTemperature(temp_10k)) {
    type_ = kThermistorType10K;
    Logger::println(String("Thermistor ") + id_ +
                    " calibrated as 10K @ 25C, temp: " + String(temp_10k, 1) +
                    "C");
    // Start sampling task
    xTaskCreate(SamplingTask, "Therm_Sample", 2048, this, 1,
                &sampling_task_handle_);
    return;
  }

  // Try 50K
  float temp_50k = CalculateTemperature(kThermistorType50K);
  if (IsValidTemperature(temp_50k)) {
    type_ = kThermistorType50K;
    Logger::println(String("Thermistor ") + id_ +
                    " calibrated as 50K @ 25C, temp: " + String(temp_50k, 1) +
                    "C");
    // Start sampling task
    xTaskCreate(SamplingTask, "Therm_Sample", 2048, this, 1,
                &sampling_task_handle_);
    return;
  }

  // Neither worked - calibration error
  type_ = kThermistorTypeCalibrationError;
  Logger::println(String("ERROR: Thermistor ") + id_ +
                  " calibration failed. 10K temp: " + String(temp_10k, 1) +
                  "C, 50K temp: " + String(temp_50k, 1) + "C");
}

Thermistor::~Thermistor() {
  if (sampling_task_handle_ != nullptr) {
    vTaskDelete(sampling_task_handle_);
    sampling_task_handle_ = nullptr;
  }
}

float Thermistor::CalculateTemperature(ThermistorType type) {
  // Read voltage in millivolts
  // We use analogReadMilliVolts because the ESP32 ADC is not linear and
  // the default attenuation does not map 0-4095 to 0-3.3V.
  uint32_t voltage_mv = analogReadMilliVolts(analog_pin_);
  float v_out = voltage_mv / 1000.0f;

  // Convert voltage to resistance
  // Voltage divider: Vout = Vin * (R_thermistor / (R_series + R_thermistor))
  // R_thermistor = R_series * (Vout / (Vin - Vout))
  float resistance;
  if (v_out <= 0.01f) {
    resistance = 0.0f;  // Short circuit
  } else if (v_out >= kReferenceVoltage - 0.01f) {
    resistance = INFINITY;  // Open circuit
  } else {
    resistance = kSeriesResistor * (v_out / (kReferenceVoltage - v_out));
  }

  // Select parameters based on thermistor type
  float r_nominal, beta;
  if (type == kThermistorType10K) {
    r_nominal = kResistance10kNominal;
    beta = kBeta10k;
  } else {  // kThermistorType50K
    r_nominal = kResistance50kNominal;
    beta = kBeta50k;
  }

  // Steinhart-Hart equation (simplified beta parameter equation)
  // 1/T = 1/T0 + (1/B) * ln(R/R0)
  // T = 1 / (1/T0 + (1/B) * ln(R/R0))
  float steinhart;
  steinhart = resistance / r_nominal;            // (R/R0)
  steinhart = log(steinhart);                    // ln(R/R0)
  steinhart /= beta;                             // (1/B) * ln(R/R0)
  steinhart += 1.0f / (kTempNominal + 273.15f);  // + (1/T0)
  steinhart = 1.0f / steinhart;                  // Invert
  steinhart -= 273.15f;                          // Convert to Celsius

  return steinhart;
}

void Thermistor::SamplingTask(void* arg) {
  Thermistor* t = static_cast<Thermistor*>(arg);
  while (true) {
    t->PerformSampling();
    vTaskDelay(pdMS_TO_TICKS(500));  // Sample 2 times a second
  }
}

void Thermistor::PerformSampling() {
  if (type_ == kThermistorTypeCalibrationError) return;

  float temp = CalculateTemperature(type_);

  // Basic validation
  if (!IsValidTemperature(temp)) return;

  // "Wildly out of range" check
  // Calculate average of current buffer if we have samples
  if (sample_count_ > 10) {
    float sum = 0;
    int count = (sample_count_ < kBufferSize) ? sample_count_ : kBufferSize;
    for (int i = 0; i < count; i++) {
      sum += temperature_buffer_[i];
    }
    float avg = sum / count;

    // If deviation is more than 5 degrees, ignore this sample
    // This filters out noise spikes
    if (abs(temp - avg) > 5.0f) {
      return;
    }
  }

  // Add to buffer
  temperature_buffer_[buffer_index_] = temp;
  buffer_index_ = (buffer_index_ + 1) % kBufferSize;
  sample_count_++;
}

StatusOr<float> Thermistor::GetSampledTemperature() {
  // Check if calibration failed
  if (type_ == kThermistorTypeCalibrationError) {
    return Status::CalibrationError(String("Thermistor ") + id_ +
                                    " not calibrated");
  }

  if (sample_count_ == 0) {
    return Status(StatusCode::kInternalError, "No temperature samples yet");
  }

  // Average last 3 valid readings
  int max_samples = (sample_count_ < kBufferSize) ? sample_count_ : kBufferSize;
  int samples_to_avg = 3;
  int count = 0;
  float sum = 0.0f;

  // We need to iterate backwards from the *last written* index.
  // buffer_index_ points to the *next* write location.
  int current_idx = (buffer_index_ - 1 + kBufferSize) % kBufferSize;

  for (int i = 0; i < samples_to_avg && i < max_samples; i++) {
    sum += temperature_buffer_[current_idx];
    count++;
    current_idx = (current_idx - 1 + kBufferSize) % kBufferSize;
  }

  if (count == 0) {
    return Status(StatusCode::kInternalError, "No valid samples to average");
  }

  return sum / count;
}

StatusOr<float> Thermistor::GetTemperature() {
  // Check if calibration failed
  if (type_ == kThermistorTypeCalibrationError) {
    return Status::CalibrationError(String("Thermistor ") + id_ +
                                    " not calibrated");
  }

  // Read temperature
  float temp = CalculateTemperature(type_);

  // Validate temperature range
  if (!IsValidTemperature(temp)) {
    String error_msg = String("Thermistor ") + id_ +
                       " temperature out of range: " + String(temp, 1) + "C";
    Logger::println(String("ERROR: ") + error_msg);
    return Status::OutOfRange(error_msg);
  }

  return temp;
}
