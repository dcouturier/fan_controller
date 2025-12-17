// HTTP Server for Arduino
// This file contains HTTP server functionality
#include "http_server.h"

#include <WiFi.h>

#include "logger.h"
#include "secrets.h"
#include "thermistor.h"

WiFiServer server(80);  // Create server on port 80

// Global pointers to fans
PWMFan* g_fan1 = nullptr;
PWMFan* g_fan2 = nullptr;
PWMFan* g_fan3 = nullptr;
PWMFan* g_fan4 = nullptr;

// Global pointers to thermistors
Thermistor* g_temp1 = nullptr;
Thermistor* g_temp2 = nullptr;
Thermistor* g_temp3 = nullptr;

void setup_wifi() {
  // Check for default credentials
  if (String(ssid) == "YOUR_SSID") {
    Logger::println();
    Logger::println("ERROR: Default SSID detected in secrets.h");
    Logger::println("Please update include/secrets.h with your WiFi credentials");
    return;
  }

  // Connect to WiFi
  Logger::println();
  Logger::println(String("Connecting to WiFi: ") + ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Logger::println();
    Logger::println("WiFi connected successfully!");
    Logger::println(WiFi.localIP());
  } else {
    Logger::println();
    Logger::println("Failed to connect to WiFi");
  }
}

void setup_http_server(PWMFan* fan1, PWMFan* fan2, PWMFan* fan3, PWMFan* fan4,
                       Thermistor* temp1, Thermistor* temp2,
                       Thermistor* temp3) {
  // Store fan pointers
  g_fan1 = fan1;
  g_fan2 = fan2;
  g_fan3 = fan3;
  g_fan4 = fan4;

  // Store thermistor pointers
  g_temp1 = temp1;
  g_temp2 = temp2;
  g_temp3 = temp3;

  // Initialize HTTP server
  server.begin();
  Logger::println("HTTP Server started on port 80");
}

void handle_http_request() {
  // Check if a client has connected
  WiFiClient client = server.available();

  if (client) {
    Logger::println("New client connected");

    String request = "";
    String postData = "";
    boolean isPost = false;
    int contentLength = 0;

    // Read the HTTP request
    while (client.connected()) {
      if (client.available()) {
        String line = client.readStringUntil('\n');

        if (line.startsWith("GET") || line.startsWith("POST")) {
          request = line;
          if (line.startsWith("POST")) {
            isPost = true;
          }
        }

        if (line.startsWith("Content-Length:")) {
          contentLength = line.substring(15).toInt();
        }

        // Empty line indicates end of headers
        if (line == "\r" || line.length() == 1) {
          // If POST, read the body
          if (isPost && contentLength > 0) {
            char bodyBuf[contentLength + 1];
            int index = 0;
            unsigned long timeout = millis() + 1000;
            while (index < contentLength && millis() < timeout) {
              if (client.available()) {
                bodyBuf[index++] = client.read();
              }
            }
            bodyBuf[index] = '\0';
            postData = String(bodyBuf);
          }
          break;
        }
      }
    }

    // Process POST data if present
#if ENABLE_OVERRIDING_FAN_SPEEDS
    if (isPost && postData.length() > 0) {
      // Check for resets first
      for (int i = 1; i <= 4; i++) {
        String resetKey = "reset_fan" + String(i) + "=";
        if (postData.indexOf(resetKey) >= 0) {
          PWMFan* fan = nullptr;
          if (i == 1)
            fan = g_fan1;
          else if (i == 2)
            fan = g_fan2;
          else if (i == 3)
            fan = g_fan3;
          else if (i == 4)
            fan = g_fan4;

          if (fan != nullptr) {
            fan->Reset();
            Logger::println(String("Fan ") + i + " override reset");
          }
        }
      }

      // Parse duty cycles for each fan
      // Expected format: fan1=50&fan2=75&fan3=25&fan4=100
      for (int i = 1; i <= 4; i++) {
        String fanKey = "fan" + String(i) + "=";
        int fanIdx = postData.indexOf(fanKey);
        if (fanIdx >= 0) {
          int fanStart = fanIdx + fanKey.length();
          int fanEnd = postData.indexOf('&', fanStart);
          if (fanEnd < 0) fanEnd = postData.length();
          String dutyStr = postData.substring(fanStart, fanEnd);
          float newDuty = dutyStr.toFloat();

          if (newDuty >= 0.0f && newDuty <= 100.0f) {
            PWMFan* fan = nullptr;
            if (i == 1)
              fan = g_fan1;
            else if (i == 2)
              fan = g_fan2;
            else if (i == 3)
              fan = g_fan3;
            else if (i == 4)
              fan = g_fan4;

            if (fan != nullptr) {
              fan->LockDutyCycle();
              Status status = fan->SetDutyCycle(newDuty, true);
              if (status.ok()) {
                Logger::println(String("Fan ") + i +
                                " duty cycle set to: " + newDuty + "%");
              } else {
                Logger::println(String("Failed to set Fan ") + i + ": " +
                                status.message());
              }
            }
          }
        }
      }
    }
#endif

    // Build HTML response
    String html = "<html><head><style>";
    html +=
        "body { font-family: Arial, sans-serif; margin: 20px; background: "
        "#f5f5f5; }";
    html += "h1 { color: #333; }";
    html += ".container { max-width: 800px; margin: 0 auto; }";
    html +=
        ".fan-grid { display: grid; grid-template-columns: repeat(4, 1fr); "
        "gap: 20px; margin-bottom: 20px; }";
    html +=
        ".fan-card { background: white; padding: 15px; border-radius: 8px; "
        "box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";
    html += ".fan-card h3 { margin-top: 0; color: #007bff; }";
    html +=
        ".fan-status { margin: 10px 0; padding: 10px; background: #e8f5e9; "
        "border-radius: 4px; }";
    html +=
        "form { background: white; padding: 20px; border-radius: 8px; "
        "box-shadow: 0 2px 4px rgba(0,0,0,0.1); margin-bottom: 20px; }";
    html += "label { display: block; margin-top: 10px; font-weight: bold; }";
    html +=
        "input[type=number] { width: 100%; padding: 8px; margin-top: 5px; "
        "box-sizing: border-box; border: 1px solid #ccc; border-radius: 4px; }";
    html +=
        "input[type=submit] { margin-top: 15px; padding: 10px 20px; "
        "background: #007bff; color: white; border: none; border-radius: 4px; "
        "cursor: pointer; font-size: 16px; }";
    html += "input[type=submit]:hover { background: #0056b3; }";
    html +=
        ".logger { background: white; padding: 15px; border-radius: 8px; "
        "box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";
    html +=
        ".logger pre { background: #f8f9fa; padding: 10px; border-radius: 4px; "
        "overflow-x: auto; max-height: 900px; overflow-y: auto; }";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>Fan Controller</h1>";

    // Display current temperatures
    html += "<h2>Temperatures</h2>";
    html += "<div class='fan-grid'>";  // Reuse fan-grid for layout
    Thermistor* temps[3] = {g_temp1, g_temp2, g_temp3};
    for (int i = 0; i < 3; i++) {
      html += "<div class='fan-card'>";
      if (temps[i] != nullptr) {
        html += "<h3>" + temps[i]->GetId() + "</h3>";
        html += "<div class='fan-status'>";
        StatusOr<float> temp = temps[i]->GetTemperature();
        if (temp.ok()) {
          html +=
              "<strong>Temp:</strong> " + String(temp.value(), 1) + " &deg;C";
        } else {
          html +=
              "<strong>Temp:</strong> Error (" + temp.status().message() + ")";
        }
        html += "</div>";
      } else {
        html += "<h3>Temp " + String(i + 1) + "</h3>";
        html += "<div class='fan-status'>Not initialized</div>";
      }
      html += "</div>";
    }
    html += "</div>";

    // Display current fan status
    html += "<h2>Fans</h2>";
    html += "<div class='fan-grid'>";
    PWMFan* fans[4] = {g_fan1, g_fan2, g_fan3, g_fan4};
    for (int i = 0; i < 4; i++) {
      html += "<div class='fan-card'>";
      html += "<h3>Fan " + String(i + 1) + "</h3>";
      if (fans[i] != nullptr) {
        html += "<div class='fan-status'>";
        {
          StatusOr<float> duty_cycle = fans[i]->GetDutyCycle();
          if (duty_cycle.ok()) {
            html += "<strong>Duty Cycle:</strong> " +
                    String(duty_cycle.value(), 1) + "%<br>";
          } else {
            html += "<strong>Duty Cycle:</strong> Error (" +
                    duty_cycle.status().message() + ")<br>";
          }
        }
        {
          StatusOr<int> rpm = fans[i]->GetRpm();
          if (rpm.ok()) {
            html += "<strong>RPM:</strong> " + String(rpm.value());
          } else {
            html +=
                "<strong>RPM:</strong> Error (" + rpm.status().message() + ")";
          }
        }
        html += "</div>";
      } else {
        html += "<div class='fan-status'>Not initialized</div>";
      }
      html += "</div>";
    }
    html += "</div>";

    // Control form
#if ENABLE_OVERRIDING_FAN_SPEEDS
    html += "<form method='POST' action='/'>";
    html += "<h2>Set Fan Duty Cycles</h2>";
    for (int i = 1; i <= 4; i++) {
      float currentDuty = 0.0f;
      bool isOverridden = false;
      PWMFan* fan = nullptr;
      if (i == 1)
        fan = g_fan1;
      else if (i == 2)
        fan = g_fan2;
      else if (i == 3)
        fan = g_fan3;
      else if (i == 4)
        fan = g_fan4;

      if (fan) {
        auto duty_cycle = fan->GetDutyCycle();
        if (duty_cycle.ok()) currentDuty = duty_cycle.value();
        isOverridden = fan->IsOverridden();
      }

      html +=
          "<div style='margin-bottom: 15px; padding: 10px; border: 1px solid "
          "#eee; border-radius: 4px;'>";
      html += "<label for='fan" + String(i) + "'>Fan " + String(i) +
              " Duty Cycle (0-100%):</label>";
      html += "<input type='number' id='fan" + String(i) + "' name='fan" +
              String(i) + "' ";
      html += "value='" + String(currentDuty, 1) +
              "' min='0' max='100' step='0.1'>";

      if (isOverridden) {
        html +=
            "<div style='margin-top: 5px; color: #d32f2f; font-size: "
            "0.9em;'>Override Active</div>";
        html += "<input type='submit' name='reset_fan" + String(i) +
                "' value='Reset Override' style='background: #dc3545; "
                "margin-top: 5px; padding: 5px 10px; font-size: 14px;'>";
      }
      html += "</div>";
    }
    html += "<input type='submit' value='Apply Settings'>";
    html += "</form>";
#endif

    // Logger output
    html += "<div class='logger'>";
    html += "<h2>Logger Output</h2>";
    html += "<pre>" + Logger::get() + "</pre>";
    html += "</div>";

    html += "</div></body></html>";

    // Send HTTP response
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html; charset=utf-8");
    client.println("Connection: close");
    client.print("Content-Length: ");
    client.println(html.length());
    client.println();
    client.print(html);

    // Give the web browser time to receive the data
    delay(1);
    client.stop();
    Logger::println("Client disconnected");
  }
}

void stop_http_server() {
  // Stop the HTTP server
  server.stop();
  Logger::println("HTTP Server stopped");
}
