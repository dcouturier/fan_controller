// HTTP Server for Arduino
// This file contains HTTP server functionality
#include "http_server.h"

#include <LittleFS.h>
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
    Logger::println(
        "Please update include/secrets.h with your WiFi credentials");
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

  if (!LittleFS.begin()) {
    Logger::println("An Error has occurred while mounting LittleFS");
  }
}

// Helper to serve file
void serveFile(WiFiClient& client, const char* path, const char* contentType) {
  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: " + String(contentType));
    client.println("Connection: close");
    client.println();
    while (file.available()) {
      client.write(file.read());
    }
    file.close();
  } else {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Connection: close");
    client.println();
    client.println("File Not Found");
  }
  client.stop();
}

// Helper to serve JSON status
void serveJSONStatus(WiFiClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();

  String json = "{";

  // Thermistors
  json += "\"thermistors\":[";
  Thermistor* temps[3] = {g_temp1, g_temp2, g_temp3};
  for (int i = 0; i < 3; i++) {
    if (i > 0) json += ",";
    json += "{";
    if (temps[i]) {
      json += "\"id\":\"" + temps[i]->GetId() + "\",";
      StatusOr<float> t = temps[i]->GetTemperature();
      if (t.ok()) {
        json += "\"temp\":\"" + String(t.value(), 1) + "\"";
      } else {
        json += "\"temp\":\"ERR\"";
      }
    } else {
      json += "\"id\":\"Temp " + String(i + 1) + "\",\"temp\":\"N/A\"";
    }
    json += "}";
  }
  json += "],";

  // Fans
  json += "\"fans\":[";
  PWMFan* fans[4] = {g_fan1, g_fan2, g_fan3, g_fan4};
  for (int i = 0; i < 4; i++) {
    if (i > 0) json += ",";
    json += "{";
    if (fans[i]) {
      StatusOr<float> d = fans[i]->GetDutyCycle();
      StatusOr<int> r = fans[i]->GetRpm();
      json += "\"duty\":\"" + (d.ok() ? String(d.value(), 1) : "ERR") + "\",";
      json += "\"rpm\":\"" + (r.ok() ? String(r.value()) : "ERR") + "\"";
    } else {
      json += "\"duty\":\"N/A\",\"rpm\":\"N/A\"";
    }
    json += "}";
  }
  json += "],";

  // Logs
  String logs = Logger::get();
  logs.replace("\"", "\\\"");
  logs.replace("\n", "\\n");
  logs.replace("\r", "");
  json += "\"logs\":\"" + logs + "\",";

#if ENABLE_OVERRIDING_FAN_SPEEDS
  json += "\"overrideEnabled\":true";
#else
  json += "\"overrideEnabled\":false";
#endif

  json += "}";
  client.print(json);
  client.stop();
}

void handle_http_request() {
  // Check if a client has connected
  WiFiClient client = server.available();

  if (client) {
    Logger::println("New client connected");

    String request = "";
    String postData = "";
    String path = "/";
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
          // Extract path
          int space1 = line.indexOf(' ');
          int space2 = line.indexOf(' ', space1 + 1);
          if (space1 != -1 && space2 != -1) {
            path = line.substring(space1 + 1, space2);
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
      // Redirect back to home after POST
      client.println("HTTP/1.1 303 See Other");
      client.println("Location: /");
      client.println("Connection: close");
      client.println();
      client.stop();
      Logger::println("Client disconnected");
      return;
    }
#endif

    // Routing
    if (path == "/" || path == "/index.html") {
      serveFile(client, "/index.html", "text/html");
    } else if (path == "/style.css") {
      serveFile(client, "/style.css", "text/css");
    } else if (path == "/script.js") {
      serveFile(client, "/script.js", "application/javascript");
    } else if (path == "/api/status") {
      serveJSONStatus(client);
    } else {
      client.println("HTTP/1.1 404 Not Found");
      client.println("Connection: close");
      client.println();
      client.println("File Not Found");
      client.stop();
    }

    Logger::println("Client disconnected");
  }
}

void stop_http_server() {
  // Stop the HTTP server
  server.stop();
  Logger::println("HTTP Server stopped");
}
