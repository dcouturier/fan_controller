#include <Arduino.h>
#ifndef DISABLE_OTA_UPDATE
#include <ArduinoOTA.h>
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <vector>

#include "fan_controller.h"
#include "http_server.h"
#include "logger.h"
#include "perf_logger.h"
#include "pwm_fan.h"
#include "thermistor.h"

// Define fan pins (pairs starting from D3 to D10)
#define FAN_1_PWM_PIN D3
#define FAN_1_TAC_PIN D4
#define FAN_2_PWM_PIN D5
#define FAN_2_TAC_PIN D6
// WARNING: D8 (GPIO 8) and D9 (GPIO 9) are strapping pins on ESP32-C3.
// D8 is used to select the boot mode (Download Boot if LOW, SPI Boot if HIGH).
// D9 is used for internal voltage selection and should be pulled up.
// Ensure your fan circuitry does not pull these pins to an invalid state during
// boot!
#define FAN_3_PWM_PIN D8
#define FAN_3_TAC_PIN D7
#define FAN_4_PWM_PIN D10
#define FAN_4_TAC_PIN D9

// Global fan objects
PWMFan* fan1 = nullptr;
PWMFan* fan2 = nullptr;
PWMFan* fan3 = nullptr;
PWMFan* pump = nullptr;

// Global thermistor objects
Thermistor* ambientTemp = nullptr;
Thermistor* coolantInTemp = nullptr;
Thermistor* coolantOutTemp = nullptr;

// Global fan controller
FanController* fanController = nullptr;

// Global perf logger
PerfLogger* perfLogger = nullptr;

void setup() {
  // 1. Initialize Logger (Serial connection)
  Serial.begin(115200);
  delay(1000);  // Wait for serial to initialize
  Logger::println("Fan Controller Starting...");

  // 2. Initialize Thermistors (A0=ambient, A1=coolant in, A2=coolant out)
  Logger::println("Initializing thermistors...");
  ambientTemp = new Thermistor(A0, "Ambient");
  coolantInTemp = new Thermistor(A1, "Coolant_In");
  coolantOutTemp = new Thermistor(A2, "Coolant_Out");
  Logger::println("All thermistors initialized");

  // 3. Initialize PWMFan objects (4 fans with pins D3-D10)
  // Fans 1-3: 35% minimum, Fan 4 (pump): 50% minimum
  Logger::println("Initializing fans...");
  fan1 = new PWMFan(FAN_1_PWM_PIN, FAN_1_TAC_PIN, 0, kRpmCalculationSampling,
                    40.0f);
  fan2 = new PWMFan(FAN_2_PWM_PIN, FAN_2_TAC_PIN, 1, kRpmCalculationSampling,
                    20.0f);
  fan3 = new PWMFan(FAN_3_PWM_PIN, FAN_3_TAC_PIN, 2, kRpmCalculationSampling,
                    25.0f);
  pump = new PWMFan(FAN_4_PWM_PIN, FAN_4_TAC_PIN, 3, kRpmCalculationSampling,
                    50.0f);  // Pump
  Logger::println("All fans initialized");

  // 4. Initialize FanController
  Logger::println("Initializing fan controller...");
  std::vector<PWMFan*> fans = {fan1, fan2, fan3};
  std::vector<PWMFan*> pumps = {pump};
  fanController = new FanController(fans, pumps, ambientTemp, coolantInTemp,
                                    coolantOutTemp);
  Logger::println("Fan controller initialized");

  // 5. Initialize WiFi
  Logger::println("Initializing WiFi...");
  setup_wifi();

#ifndef DISABLE_OTA_UPDATE
  // Initialize OTA
  ArduinoOTA.setHostname("fan-controller");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_SPIFFS
      type = "filesystem";
    }
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using
    // SPIFFS.end()
    Logger::println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() { Logger::println("\nEnd"); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    // Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Logger::println("Error: " + String(error));
    if (error == OTA_AUTH_ERROR)
      Logger::println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Logger::println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Logger::println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Logger::println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Logger::println("End Failed");
  });

  ArduinoOTA.begin();
#endif

  // 6. Initialize HTTPServer
  Logger::println("Initializing HTTP Server...");
  setup_http_server(fan1, fan2, fan3, pump, ambientTemp, coolantInTemp,
                    coolantOutTemp);

  // 7. Initialize PerfLogger
  Logger::println("Initializing PerfLogger...");
  perfLogger = new PerfLogger(fan1, fan2, fan3, pump, ambientTemp,
                              coolantInTemp, coolantOutTemp);
  perfLogger->Start();

  Logger::println("Setup complete!");
}

void loop() {
#ifndef DISABLE_OTA_UPDATE
  // Handle OTA updates
  ArduinoOTA.handle();
#endif

  // Handle HTTP requests
  handle_http_request();

  // Sleep using FreeRTOS
  vTaskDelay(pdMS_TO_TICKS(10));
}
