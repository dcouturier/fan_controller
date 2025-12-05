#include <Arduino.h>
#include <unity.h>
#include <vector>

#include "fan_controller.h"
#include "pwm_fan.h"
#include "thermistor.h"

void test_fan_controller_target_speed(void) {
  PWMFan f1(D3, D4, 0);
  PWMFan f2(D5, D6, 1);
  PWMFan f3(D8, D7, 2);
  PWMFan pump(D10, D9, 3);

  Thermistor t1(A0, "T1");
  Thermistor t2(A1, "T2");
  Thermistor t3(A2, "T3");

  std::vector<PWMFan*> fans = {&f1, &f2, &f3};
  std::vector<PWMFan*> pumps = {&pump};

  FanController testController(fans, pumps, &t1, &t2, &t3);

  float speed = testController.GetTargetFanSpeed();
  TEST_ASSERT_TRUE(speed >= 0.0f && speed <= 100.0f);
}
