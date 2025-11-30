#include <Arduino.h>
#include <unity.h>

#include "logger.h"


void test_logger_logic(void) {
  Logger::clear();
  String testMsg = "Test Log Entry";
  Logger::println(testMsg);

  String logs = Logger::get();
  TEST_ASSERT_TRUE(logs.indexOf(testMsg) >= 0);
}
