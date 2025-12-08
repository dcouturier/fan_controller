This project uses PlatformIO as a framework.

The board used is a Xiao ESP32C3.

The code should be written in c++ with the main.cpp being the file containing the main loop and initial setup code.

The purpose of this project is to control 4 computer fans using PWM signal produced by the ESP32C3 using 3 thermistor as input to control the fan's speed. It should also be able to read the fans' RPM using using the frequency of the fan's tachometer.

Use FreeRTOS for setting up the interrupts.

# Style guide

Use Google c++ style guide when making code recommendations: https://google.github.io/styleguide/cppguide.html.

For example, code should make use of Status/StatusOr error handling as much as possible. Every getter should be returning a StatusOr<T> and every void call should be returning a Status code (even if the implementation always returns OkStatus()). The Status/StatusOr implementation is a lightweight embedded-friendly version located in `include/status.h` (not Abseil, as Abseil is too heavy for ESP32 Arduino framework).

You should not create accronyms for variable names.

# Directory Structure

The project is organized as follows:
*   `src/`: Contains `main.cpp`.
*   `lib/ProjectModules/`: Contains core logic modules (`fan_controller`, `pwm_fan`, `http_server`, `perf_logger`, `logger`, `status`).
*   `lib/Sensors/`: Contains sensor drivers (`thermistor`).

# Main

## Setup

During setup,
1. Initialize the Logger: the Serial connection should be established. All logs should be printed to Serial.
1. Initialize the Thermistors (A0, A1, A2).
1. Initialize the PWMFan objects: 4 fans in total with pairs of pins starting from D3 all the way to D10.
1. Initialize the FanController with vectors of fans and pumps.
1. Initialize the WiFi.
1. Initialize the HTTPServer.

## Loop
For now simply have the main loop sleep using FreeRTOS's function. We will later add contents to the loop.

# PWMFan
There should be a PWMFan object with 1 setter for duty cycle taking a float percentage that can range between 0.0% and 100%. Each fan should have a getter for the current RPM.
The PWMFan constructor, should take 2 pins as parameters (pwmPin, tachPin) and the PWM channel number. The constructor should set the pin mode for its pins.
The fan's PWM frequency should be 25kz and their defaut duty cycle should be 50%.
The getter for the RPM should simply return the latest calculated value for the fan's RPM. 

When initialized, the PWM fan should have a default minimum speed of 50% unless that minimum is overriden in the constructor. The fan speed setter shouldn't allow for setting a fan speed which is lower than the fan's minimum speed as specified in the constructor.

## Tachometer

The tachometer pin is very noisy and thus we can't just set an interrupt on pullup to count the PWM frequency. Instead, we will be polling every 1 ms for the current state of the tachometer pin and use a 5-deep circular buffer to keep the last 5 states of the pulse in memory. If 3 out of 5 of the last pulses are up then we would consider the current state as up and compare to the last state. If a change of state is detected, we would update the state. If the state change is from off to on, we count this as a tick.

If a full second has passed, the task will count the number of ticks since last check to establish the RPM of the fan and reset the tick counter. 

The fan maintains current rpm as a class attribute which can be read.

## PWM control
When initializing PWMFans use ledcSetup and ledcAttachPin as follows:
```
#define DEFAULT_PWM_FREQUENCY 25000  // 25kHz
#define PWM_RESOLUTION 10  // 1024 gives ~0.1% granularity.
#define PWM_DEFAULT_DUTY_CYCLE_PERCENT 50

// Constructor:
ledcSetup(channelNumber, DEFAULT_PWM_FREQUENCY, PWM_RESOLUTION);
ledcAttachPin(pwmPin, channelNumber);
```

Setting the duty cycle should use:
`ledcWrite(this.channelNumber, (1 << PWM_RESOLUTION)*PWM_DEFAULT_DUTY_CYCLE_PERCENT/100);`

### Smoothing

We don't want to have the fans change speed rapidly. For this reason, when changing the fan speed, we should set the PWMFan's targetDutyCycle and maintain its currentDutyCycle. Inside of the tachometer's routine, we should add a check which runs every 200 ms to update the currentDutyCucle of the fan using a smoothing curve.

For the smoothing curve parameters, lets approach the targetDutyCycle by a maximum of 2% of the difference between currentDutyCycle and targetDutyCycle on each checks performed every 200 ms. Make sure that the effect is a minimum of at least 1 hardware step (100.0f / 1024) to avoid never reaching the target.

# HTTP Server
The HTTPServer is a class which sets up an HTTP server on port 80. It should display the current status of each fan (duty cycle and rpm).

There should be a form to set the duty cycle of each fan which relies on POST to apply the changes and refresh the page.

Below that, it should list the contents of Logger::get().

Consider getting the static contents from an HTTP template file and setting the values using string replace or any other templating system.

It should set its own interrupt to handle requests.


# Thermistor

There are 3 thermistors probes hooked on to the 3 analog pins of the Xiao ESP32C3 (A0, A1 and A2). All of them are wired this way: 3.3V microcontroller's voltage reference -> 10k Ohm Resistor -> Termistor -> Ground. The probe is hooked between the 10k Ohm Resistor and the termistor.

* A0 is an ambient temperature probe: it reads the temperature of the outside of the computer so that we can calculate the temperature delta and adjust the fan speed based on that delta.
* A1 is the computer watercooling loop coolant "In" temperature. It reads the temperature of the coolant before it gets to the CPU.
* A2 is the computer watercooling loop coolant "Out" temperature. It reads the temperature of the cooland after the coolant has gone through the CPU block, one radiator and the GPU block.

## Calibration

Upon initialization of the Thermstor class, the instance should determine what type of Thermistor is connected. The allowable types are:

1. THERMISTOR_TYPE_10K: 10k Ohm @ 25C, 3435K
1. THERMISTOR_TYPE_50K: 50k Ohm @ 25C, 3970K

To determine what kind of resistor is used, the constructor should try calculating the current temperature assuming the thermisor is a 10k initially and if the derived temperature falls outside of any valid range, try the next possible configuration. Repeat until no other configuration is possible and then fall back to a 3rd possible configuration: THERMISTOR_TYPE_INITIAL_CALIBRATION_ERROR.

### Validation logic

The thermistors should have an error state when the temperature is out of acceptable range. For example, we never expect the temperature to ne below 10 degrees Ceilsus or above 50 degree Ceilsus.

If the initial calibration of the class is unable to determine the type of thermistor used an error message should be logged to the Logger with the id of the termistor and the Thermistor instance should maintain that error state until restarted.

## Reading temperature

The Thermistor class should expose a getter for the temperature returning an absl::StatusOr<float> representation of the temperature it reads in Ceilsus. The absl::Status should be of type CALIBRATION_ERROR if the Thermistor was unable to calibrate itself during construction.

Before returning a value, the Thermistor getter should check if the value falls within acceptable range. If not, it should return an error and log the error to the Logger.

### Smoothing

The Thermistor class implements a sampling task that runs every 500ms. It maintains a circular buffer of the last 20 readings.
When a new sample is taken:
1. It is validated against the acceptable range.
2. It is checked against the average of the current buffer. If the deviation is greater than 5°C, the sample is discarded as noise ("wildly out of range" check).
3. If valid, it is added to the circular buffer.

The `GetSampledTemperature()` method returns the average of the last 5 valid readings from the buffer to provide a stable temperature value.

# FanController

There should be a single class responsible for controlling the fan speed based on the current temperature.

While called "fan controller" the fan number 4 is actually the water pump circulating the coolant in the system. Lets make sure that the minimum speed that one runs at cannot be set to anything lower than 50%. The other fans should have an override which allows them to be set as low as 35%.

## Fan Speed Calculation

The fan speed calculation uses a formula that is primarily driven by the absolute water temperature, with a boost from the temperature differential (DeltaT).

### Formula Components

1. **Water Temperature Factor**: Measures the absolute water temperature against a baseline.
   - Base water temperature: 25°C (comfortable baseline). Below this, fans run at 0% intensity (minimum speed).
   - Maximum water temperature: 30°C (temperature at which this factor contributes maximum boost).

2. **DeltaT Factor**: Measures the temperature differential between ambient and the highest coolant temperature (A1 or A2).
   - Minimum DeltaT: 5°C.
   - Maximum DeltaT: 8°C.

### Calculation Logic

The formula is:
`Speed = WaterTempFactor + (DeltaTFactor * kDeltaTInfluence * WaterTempFactor)`

Where `kDeltaTInfluence` is 0.5.

This logic ensures that:
- If water is cool (<= 25°C), fans run at minimum speed regardless of DeltaT.
- As water warms up, fan speed increases.
- DeltaT provides a boost to the speed, but this boost is scaled by the water temperature factor. This prevents high fan speeds when the water is cool but DeltaT is high (e.g., cold ambient temperature).
- If water reaches 30°C, fans approach 100% speed.

This update logic should happen once every second using a FreeRTOS task.

If when performing the update, we read an error state on any of the Thermistors, the fan and pump should be set to 100% and a log should be added to the Logger.

# PerfLogger

Every second, the state of the system will be logged. This includes:

1. Current time in seconds since start of the micro controller (2 bytes (provides 18 hours))
1. Each PWMFan's (5 bytes per PWMFan)
   1. Target duty cycle (1 byte: 0 is 0% and 256 is 100%)
   1. Current duty cycle (1 byte: 0 is 0% and 256 is 100%)
   1. Current RPM (2 bytes: actual RPM)
1. Each Thermistor's current temperature in Ceilsus (1 byte: 0 is 10C, 256 is 50C)

The 20 bytes of data is immediately flushed to disk every second. Each record contain exactly 20 bytes so to read the 31st second, one must shift their lookup to 31*20 bytes from the start of the file.

PerfLogger is initialized in the main.cpp file and provided with the 4 PWMFan and 3 Thermistor objects.

The disk should consist of 20 files containing maximum of 200 records each (3 min 20 seconds). The filename for each file should have the following format: "perf_logger_<NUMBER>.dat" where <NUMBER> represent the iteration of this file.

When creating a new file, the oldest one should be deleted. When creating a new file, the file system should be scanned for files matching this naming pattern and identify the current highest <NUMBER> and create a new file with an incremented number. If no files with that pattern are found, it should create the first file named "perf_logger_0.dat".

PerfLogger opens a listening connection on port # 55995599 where is makes those files available for download as http file server.