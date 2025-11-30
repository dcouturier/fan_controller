#include <Arduino.h>
#include <unity.h>

#include "perf_logger.h"
#include "pwm_fan.h"
#include "thermistor.h"


void test_perf_logger_init(void) {
  PWMFan f1(D3, D4, 0);
  PWMFan f2(D5, D6, 1);
  PWMFan f3(D8, D7, 2);
  PWMFan pump(D10, D9, 3);

  Thermistor t1(A0, "T1");
  Thermistor t2(A1, "T2");
  Thermistor t3(A2, "T3");

  PerfLogger testPerfLogger(&f1, &f2, &f3, &pump, &t1, &t2, &t3);
  // Just checking it constructs without crashing
  TEST_ASSERT_TRUE(true);
}
