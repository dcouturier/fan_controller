# ESP32C3 Fan Controller

This project implements a fan controller for a water-cooled computer system using a Seeed Studio Xiao ESP32C3 microcontroller. It controls 4 PWM fans (including a water pump) based on temperature readings from 3 thermistors, aiming to balance cooling performance with noise levels.

## Features

*   **Intelligent Fan Control**:
    *   Controls 4 PWM fans (25kHz frequency).
    *   Fan speed is calculated using a hybrid formula considering both the temperature delta (Ambient vs. Coolant) and absolute water temperature.
    *   Smooth fan speed transitions to prevent rapid RPM fluctuations.
    *   Configurable minimum speeds (Pump: 50%, Fans: 35%).
*   **Temperature Monitoring**:
    *   Supports 3 thermistor probes (Ambient, Coolant In, Coolant Out).
    *   Automatic calibration for 10k and 50k thermistors.
    *   Validation logic to detect and handle sensor errors.
*   **RPM Monitoring**:
    *   Reads fan RPM using tachometer signals.
    *   Uses a circular buffer and polling for noise filtering on tachometer inputs.
*   **Web Interface**:
    *   Built-in HTTP server (Port 80).
    *   Displays real-time status of fans (Duty Cycle, RPM) and temperatures.
    *   Allows manual override of fan duty cycles.
    *   Displays system logs.
*   **Performance Logging**:
    *   Logs system state (Fan PWM, RPM, Temperatures) every second to internal flash storage.
    *   Rotates log files automatically.
    *   Provides a separate HTTP file server (Port 5599) to download performance logs.
*   **Connectivity**:
    *   WiFi enabled.
    *   Over-the-Air (OTA) updates (via PlatformIO).

## Hardware

*   **Microcontroller**: Seeed Studio Xiao ESP32C3
*   **Inputs**:
    *   3x Thermistors (NTC 10k or 50k) on Analog pins (A0, A1, A2).
    *   4x Fan Tachometer signals.
*   **Outputs**:
    *   4x PWM Fan control signals (D3 - D10 pairs).

## Software Architecture

*   **Framework**: PlatformIO with Arduino framework for ESP32.
*   **OS**: FreeRTOS is used for task management (Main loop, Fan control, Logging).
*   **Language**: C++ (following Google C++ Style Guide).
*   **Error Handling**: Uses a custom `Status` and `StatusOr` implementation for robust error handling.

## Project Structure

*   `src/main.cpp`: Main entry point, setup, and loop.
*   `lib/ProjectModules/`: Core logic libraries.
    *   `fan_controller`: Logic for calculating fan speeds based on temperature.
    *   `pwm_fan`: Handles PWM output and tachometer reading.
    *   `thermistor`: Handles temperature reading and calibration.
    *   `http_server`: Web interface implementation.
    *   `perf_logger`: Binary logging of system performance.
    *   `logger`: Serial logging utility.
*   `tools/`: Utility scripts (e.g., for parsing binary logs).

## Getting Started

1.  **Prerequisites**: Install Visual Studio Code and the PlatformIO extension.
2.  **Configuration**:
    *   Rename `include/secrets.h.example` to `include/secrets.h`.
    *   Update `secrets.h` with your WiFi credentials.
3.  **Build & Upload**:
    *   Connect the Xiao ESP32C3 via USB.
    *   Use PlatformIO to build and upload the firmware.
4.  **Monitor**:
    *   Use the Serial Monitor to view initial connection logs and IP address.
    *   Access the web interface via the assigned IP address.

## Over-the-Air (OTA) Updates

This project supports OTA updates, allowing you to flash new firmware wirelessly.

1.  **Initial Setup**: Flash the firmware via USB once to enable OTA.
2.  **Find IP Address**: Note the IP address of the device (printed to Serial Monitor or found on your router).
3.  **Update `platformio.ini`**:
    Uncomment or add the following lines to your `platformio.ini` environment:
    ```ini
    ; upload_protocol = espota
    ; upload_port = YOUR_DEVICE_IP_ADDRESS
    ```
4.  **Flash**: Run the "Upload" task in PlatformIO. It will now upload via WiFi.
