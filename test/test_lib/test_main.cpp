#include <Arduino.h>
#include <unity.h>

// Forward declarations of test functions
void test_pwm_fan_initial_state(void);
void test_pwm_fan_set_duty_cycle(void);
void test_pwm_fan_min_clamping(void);
void test_pwm_fan_max_clamping(void);

void test_thermistor_initialization(void);
void test_thermistor_reading(void);

void test_fan_controller_target_speed(void);

void test_perf_logger_init(void);

void test_logger_logic(void);

void setUp(void) {
  // Global setup if needed
}

void tearDown(void) {
  // Global teardown if needed
}

void setup() {
  delay(2000);
  UNITY_BEGIN();

  // PWM Fan Tests
  RUN_TEST(test_pwm_fan_initial_state);
  RUN_TEST(test_pwm_fan_set_duty_cycle);
  RUN_TEST(test_pwm_fan_min_clamping);
  RUN_TEST(test_pwm_fan_max_clamping);

  // Thermistor Tests
  RUN_TEST(test_thermistor_initialization);
  RUN_TEST(test_thermistor_reading);

  // Fan Controller Tests
  RUN_TEST(test_fan_controller_target_speed);

  // Perf Logger Tests
  RUN_TEST(test_perf_logger_init);

  // Logger Tests
  RUN_TEST(test_logger_logic);

  UNITY_END();
}

void loop() {}
