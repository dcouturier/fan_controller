#ifndef THERMISTOR_H
#define THERMISTOR_H

#include <Arduino.h>

#include "status.h"


enum ThermistorType {
  kThermistorType10K = 0,  // 10k Ohm @ 25C, 3435K
  kThermistorType50K = 1,  // 50k Ohm @ 25C, 3970K
  kThermistorTypeCalibrationError = 2
};

// Thermistor - Auto-calibrating temperature sensor reader
//
// Reads temperature from a thermistor connected via voltage divider to an ADC
// pin. The thermistor is wired: 3.3V -> 10kΩ resistor -> ADC pin -> Thermistor
// -> Ground
//
// Features:
// - Auto-detection of thermistor type (10K or 50K) during initialization
// - Steinhart-Hart equation for accurate temperature calculation
// - Temperature range validation (10-50°C) with error reporting
// - StatusOr-based error handling for calibration and range errors
//
// Calibration:
// At initialization, attempts to identify thermistor type by trying each
// configuration and checking if the calculated temperature falls within valid
// range. If no valid configuration is found, enters
// kThermistorTypeCalibrationError state.
//
// Error States:
// - Calibration error: Unable to determine thermistor type
// - Out of range: Temperature outside 10-50°C (likely sensor fault or
// disconnection)
//
class Thermistor {
 public:
  // Constructor performs auto-calibration to determine thermistor type
  Thermistor(uint8_t analog_pin, const String& id);

  // Get temperature in Celsius
  StatusOr<float> GetTemperature();

  // Get the detected thermistor type
  ThermistorType GetType() const { return type_; }

  // Get the thermistor ID
  const String& GetId() const { return id_; }

 private:
  uint8_t analog_pin_;
  String id_;
  ThermistorType type_;

  // Constants for temperature calculation
  static constexpr float kReferenceVoltage = 3.3f;
  static constexpr float kSeriesResistor = 10000.0f;  // 10k Ohm
  static constexpr float kAdcMax = 4095.0f;           // 12-bit ADC

  // Thermistor parameters
  static constexpr float kTempNominal =
      25.0f;  // Temperature for nominal resistance (Celsius)
  static constexpr float kResistance10kNominal = 10000.0f;
  static constexpr float kBeta10k = 3435.0f;
  static constexpr float kResistance50kNominal = 50000.0f;
  static constexpr float kBeta50k = 3970.0f;

  // Valid temperature range (Celsius)
  static constexpr float kMinValidTemp = 10.0f;
  static constexpr float kMaxValidTemp = 50.0f;

  // Calculate temperature for a given thermistor type
  float CalculateTemperature(ThermistorType type);

  // Check if temperature is in valid range
  bool IsValidTemperature(float temp) const {
    return temp >= kMinValidTemp && temp <= kMaxValidTemp;
  }
};

#endif  // THERMISTOR_H
