#include "thermistor.h"

#include <Arduino.h>

#include <cmath>
#include <cstdint>

#include "logger.h"

Thermistor::Thermistor(uint8_t analog_pin, const String& id)
    : analog_pin_(analog_pin), id_(id), type_(kThermistorTypeCalibrationError) {
  pinMode(analog_pin_, INPUT);

  // Auto-calibrate: try each thermistor type and see which gives valid
  // temperature Try 10K first (most common)
  float temp_10k = CalculateTemperature(kThermistorType10K);
  if (IsValidTemperature(temp_10k)) {
    type_ = kThermistorType10K;
    Logger::println(String("Thermistor ") + id_ +
                    " calibrated as 10K @ 25C, temp: " + String(temp_10k, 1) +
                    "C");
    return;
  }

  // Try 50K
  float temp_50k = CalculateTemperature(kThermistorType50K);
  if (IsValidTemperature(temp_50k)) {
    type_ = kThermistorType50K;
    Logger::println(String("Thermistor ") + id_ +
                    " calibrated as 50K @ 25C, temp: " + String(temp_50k, 1) +
                    "C");
    return;
  }

  // Neither worked - calibration error
  type_ = kThermistorTypeCalibrationError;
  Logger::println(String("ERROR: Thermistor ") + id_ +
                  " calibration failed. 10K temp: " + String(temp_10k, 1) +
                  "C, 50K temp: " + String(temp_50k, 1) + "C");
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
