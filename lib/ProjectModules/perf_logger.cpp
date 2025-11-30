#include "perf_logger.h"

#include <LittleFS.h>
#include <WiFi.h>

#include <algorithm>

#include "logger.h"

#define LOG_INTERVAL_MS 1000
#define RECORDS_PER_FILE 195  // < 4KB per file
#define MAX_FILES 20
#define SERVER_PORT 5599

PerfLogger::PerfLogger(PWMFan* fan1, PWMFan* fan2, PWMFan* fan3, PWMFan* pump,
                       Thermistor* ambient, Thermistor* coolant_in,
                       Thermistor* coolant_out) {
  fans_[0] = fan1;
  fans_[1] = fan2;
  fans_[2] = fan3;
  fans_[3] = pump;

  thermistors_[0] = ambient;
  thermistors_[1] = coolant_in;
  thermistors_[2] = coolant_out;

  current_file_index_ = 0;
  current_record_count_ = 0;
}

void PerfLogger::Start() {
  if (!LittleFS.begin(true)) {
    Logger::println("PerfLogger: LittleFS Mount Failed");
    return;
  }

  // Scan for existing files to determine current index
  int max_index = -1;
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while (file) {
    String name = file.name();
    if (name.startsWith("perf_logger_") && name.endsWith(".dat")) {
      // Extract number
      int start = 12;               // length of "perf_logger_"
      int end = name.length() - 4;  // length of ".dat"
      String numStr = name.substring(start, end);
      int num = numStr.toInt();
      if (num > max_index) {
        max_index = num;
      }
    }
    file = root.openNextFile();
  }

  // Always start a new file on initialization to avoid mixing logs
  if (max_index >= 0) {
    current_file_index_ = max_index + 1;
  } else {
    current_file_index_ = 0;
  }
  current_record_count_ = 0;

  // Ensure we have space for the new file
  RotateFiles();

  Logger::println("PerfLogger: Starting at file index " +
                  String(current_file_index_) + " record " +
                  String(current_record_count_));

  xTaskCreate(LoggingTask, "PerfLogTask", 4096, this, 1, NULL);
  xTaskCreate(ServerTask, "PerfServerTask", 4096, this, 1, NULL);
}

void PerfLogger::RotateFiles() {
  // List all perf logger files
  std::vector<int> file_indices;
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while (file) {
    String name = file.name();
    if (name.startsWith("perf_logger_") && name.endsWith(".dat")) {
      // Extract number
      // Note: LittleFS file names might include leading slash
      int prefixLen = name.startsWith("/perf_logger_") ? 13 : 12;
      int start = prefixLen;
      int end = name.length() - 4;
      String numStr = name.substring(start, end);
      file_indices.push_back(numStr.toInt());
    }
    file = root.openNextFile();
  }

  // If we have too many files, delete the oldest ones
  // We want to keep MAX_FILES - 1 (since we are about to create a new one)
  // Actually, if we are just appending to current, we might not need to delete
  // yet. But if we are starting a NEW file (which calls this), we should ensure
  // space.

  if (file_indices.size() >= MAX_FILES) {
    std::sort(file_indices.begin(), file_indices.end());

    // Delete oldest until we have space
    int to_delete = file_indices.size() - (MAX_FILES - 1);
    for (int i = 0; i < to_delete; i++) {
      String path = "/perf_logger_" + String(file_indices[i]) + ".dat";
      LittleFS.remove(path);
      Logger::println("PerfLogger: Deleted old file " + path);
    }
  }
}

String PerfLogger::GetCurrentFileName() {
  return "/perf_logger_" + String(current_file_index_) + ".dat";
}

uint8_t PerfLogger::EncodeTemperature(float temp_c) {
  // 0 is 10C, 256 (255) is 50C
  // Range 10 to 50 = 40 degrees
  if (temp_c < 10.0f) return 0;
  if (temp_c > 50.0f) return 255;
  return (uint8_t)((temp_c - 10.0f) * 255.0f / 40.0f);
}

uint8_t PerfLogger::EncodeDutyCycle(float duty_percent) {
  if (duty_percent < 0.0f) return 0;
  if (duty_percent > 100.0f) return 255;
  return (uint8_t)(duty_percent * 255.0f / 100.0f);
}

void PerfLogger::LoggingTask(void* parameter) {
  PerfLogger* logger = (PerfLogger*)parameter;

  TickType_t xLastWakeTime;
  const TickType_t xFrequency = pdMS_TO_TICKS(LOG_INTERVAL_MS);
  xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    PerfLogRecord record;
    record.timestamp = (uint16_t)(millis() / 1000);

    // Fans
    for (int i = 0; i < 4; i++) {
      // We need to access private members or add getters for target duty cycle
      // if not available. Assuming GetDutyCycle returns current. The
      // requirement asks for Target and Current. PWMFan class has
      // GetDutyCycle() which returns current. It doesn't seem to expose
      // TargetDutyCycle directly in the public interface provided in context. I
      // will assume GetDutyCycle is the current one. For target, I might need
      // to modify PWMFan or just use current for both if target isn't
      // available. However, looking at PWMFan header: "SetDutyCycle(float
      // percent)". I should probably add GetTargetDutyCycle to PWMFan. For now,
      // I'll use GetDutyCycle for both or 0 for target if I can't get it. Let's
      // check PWMFan header again. It has `GetDutyCycle()`. I will modify
      // PWMFan to add `GetTargetDutyCycle()` later. For now I will use
      // GetDutyCycle for both.

      StatusOr<float> duty = logger->fans_[i]->GetDutyCycle();
      float d = duty.ok() ? duty.value() : 0.0f;

      StatusOr<float> target = logger->fans_[i]->GetTargetDutyCycle();
      float t = target.ok() ? target.value() : 0.0f;

      uint8_t encoded_duty = logger->EncodeDutyCycle(d);
      uint8_t encoded_target = logger->EncodeDutyCycle(t);

      if (i == 0) {
        record.fan1_target_duty = encoded_target;
        record.fan1_current_duty = encoded_duty;
        StatusOr<int> rpm = logger->fans_[i]->GetRpm();
        record.fan1_rpm = rpm.ok() ? (uint16_t)rpm.value() : 0;
      } else if (i == 1) {
        record.fan2_target_duty = encoded_target;
        record.fan2_current_duty = encoded_duty;
        StatusOr<int> rpm = logger->fans_[i]->GetRpm();
        record.fan2_rpm = rpm.ok() ? (uint16_t)rpm.value() : 0;
      } else if (i == 2) {
        record.fan3_target_duty = encoded_target;
        record.fan3_current_duty = encoded_duty;
        StatusOr<int> rpm = logger->fans_[i]->GetRpm();
        record.fan3_rpm = rpm.ok() ? (uint16_t)rpm.value() : 0;
      } else if (i == 3) {
        record.fan4_target_duty = encoded_target;
        record.fan4_current_duty = encoded_duty;
        StatusOr<int> rpm = logger->fans_[i]->GetRpm();
        record.fan4_rpm = rpm.ok() ? (uint16_t)rpm.value() : 0;
      }
    }

    // Thermistors
    StatusOr<float> t0 = logger->thermistors_[0]->GetTemperature();
    record.temp_ambient =
        logger->EncodeTemperature(t0.ok() ? t0.value() : 0.0f);

    StatusOr<float> t1 = logger->thermistors_[1]->GetTemperature();
    record.temp_coolant_in =
        logger->EncodeTemperature(t1.ok() ? t1.value() : 0.0f);

    StatusOr<float> t2 = logger->thermistors_[2]->GetTemperature();
    record.temp_coolant_out =
        logger->EncodeTemperature(t2.ok() ? t2.value() : 0.0f);

    // Write to file
    File f = LittleFS.open(logger->GetCurrentFileName(), "a");
    if (f) {
      f.write((uint8_t*)&record, sizeof(PerfLogRecord));
      f.close();
      logger->current_record_count_++;

      if (logger->current_record_count_ >= RECORDS_PER_FILE) {
        logger->current_file_index_++;
        logger->current_record_count_ = 0;
        logger->RotateFiles();
      }
    } else {
      Logger::println("PerfLogger: Failed to open file for writing");
    }
  }
}

void PerfLogger::ServerTask(void* parameter) {
  PerfLogger* logger = (PerfLogger*)parameter;
  WiFiServer server(SERVER_PORT);
  server.begin();

  Logger::printf("PerfLogger Server started on port %d", SERVER_PORT);

  for (;;) {
    WiFiClient client = server.available();
    if (client) {
      Logger::printf("New client connected from %s",
                     client.remoteIP().toString().c_str());
      String currentLine = "";
      String requestLine = "";
      while (client.connected()) {
        if (client.available()) {
          char c = client.read();
          if (c == '\n') {
            if (currentLine.length() == 0) {
              // End of headers
              // Parse request
              if (requestLine.startsWith("GET /perf_logger_")) {
                int start = 5;  // "GET /"
                int end = requestLine.indexOf(" HTTP");
                String path = requestLine.substring(start, end);

                if (LittleFS.exists("/" + path)) {
                  File f = LittleFS.open("/" + path, "r");
                  client.println("HTTP/1.1 200 OK");
                  client.println("Content-Type: application/octet-stream");
                  client.println(
                      "Content-Disposition: attachment; filename=\"" + path +
                      "\"");
                  client.println("Connection: close");
                  client.println();

                  while (f.available()) {
                    uint8_t buf[64];
                    int n = f.read(buf, sizeof(buf));
                    client.write(buf, n);
                  }
                  f.close();
                } else {
                  client.println("HTTP/1.1 404 Not Found");
                  client.println("Connection: close");
                  client.println();
                }
              } else if (requestLine.startsWith("GET / ")) {
                // List files
                client.println("HTTP/1.1 200 OK");
                client.println("Content-Type: text/html");
                client.println("Connection: close");
                client.println();
                client.println("<html><body><h1>Perf Logs</h1><ul>");

                File root = LittleFS.open("/");
                File file = root.openNextFile();
                while (file) {
                  String name = file.name();
                  if (name.startsWith("perf_logger_") ||
                      name.startsWith("/perf_logger_")) {
                    // Handle potential leading slash
                    String cleanName =
                        name.startsWith("/") ? name.substring(1) : name;
                    client.println("<li><a href=\"/" + cleanName + "\">" +
                                   cleanName + "</a> (" + String(file.size()) +
                                   " bytes)</li>");
                  }
                  file = root.openNextFile();
                }
                client.println("</ul></body></html>");
              } else {
                client.println("HTTP/1.1 404 Not Found");
                client.println("Connection: close");
                client.println();
              }
              break;
            } else {
              if (requestLine.length() == 0) {
                requestLine = currentLine;
                Logger::printf("Request: %s", requestLine.c_str());
              }
              currentLine = "";
            }
          } else if (c != '\r') {
            currentLine += c;
          }
        } else {
          vTaskDelay(pdMS_TO_TICKS(10));
        }
      }
      client.stop();
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
