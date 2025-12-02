#include <Arduino.h>
#include <unity.h>

#include "pwm_fan.h"

// Define test pins
#define TEST_PWM_PIN D3
#define TEST_TACH_PIN D4
#define TEST_CHANNEL 0

void test_pwm_fan_initial_state(void) {
  PWMFan testFan(TEST_PWM_PIN, TEST_TACH_PIN, TEST_CHANNEL);
  StatusOr<float> duty = testFan.GetTargetDutyCycle();
  TEST_ASSERT_TRUE(duty.ok());
  TEST_ASSERT_EQUAL_FLOAT(50.0f, duty.value());
}

void test_pwm_fan_set_duty_cycle(void) {
  PWMFan testFan(TEST_PWM_PIN, TEST_TACH_PIN, TEST_CHANNEL);
  testFan.SetTargetDutyCycle(75.0f);
  StatusOr<float> duty = testFan.GetTargetDutyCycle();
  TEST_ASSERT_TRUE(duty.ok());
  TEST_ASSERT_EQUAL_FLOAT(75.0f, duty.value());
}

void test_pwm_fan_min_clamping(void) {
  PWMFan testFan(TEST_PWM_PIN, TEST_TACH_PIN, TEST_CHANNEL);
  // Default min is 50%
  testFan.SetTargetDutyCycle(10.0f);
  StatusOr<float> duty = testFan.GetTargetDutyCycle();
  TEST_ASSERT_TRUE(duty.ok());
  TEST_ASSERT_EQUAL_FLOAT(50.0f, duty.value());
}

void test_pwm_fan_max_clamping(void) {
  PWMFan testFan(TEST_PWM_PIN, TEST_TACH_PIN, TEST_CHANNEL);
  testFan.SetTargetDutyCycle(150.0f);
  StatusOr<float> duty = testFan.GetTargetDutyCycle();
  TEST_ASSERT_TRUE(duty.ok());
  TEST_ASSERT_EQUAL_FLOAT(100.0f, duty.value());
}
