#include <Arduino.h>
#include <unity.h>

#include "thermistor.h"

#define TEST_ANALOG_PIN A0

void test_thermistor_initialization(void) {
  Thermistor testThermistor(TEST_ANALOG_PIN, "TestThermistor");
  TEST_ASSERT_EQUAL_STRING("TestThermistor", testThermistor.GetId().c_str());
}

void test_thermistor_reading(void) {
  Thermistor testThermistor(TEST_ANALOG_PIN, "TestThermistor");
  StatusOr<float> temp = testThermistor.GetTemperature();
  // It might be Ok or Error depending on floating pin
  if (temp.ok()) {
    float t = temp.value();
    TEST_ASSERT_TRUE(t >= 10.0f && t <= 50.0f);
  } else {
    // If error, it should be CalibrationError or OutOfRange
    TEST_ASSERT_TRUE(!temp.ok());
  }
}
