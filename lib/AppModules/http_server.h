#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "pwm_fan.h"
#include "thermistor.h"

void setup_wifi();
void setup_http_server(PWMFan* fan1, PWMFan* fan2, PWMFan* fan3, PWMFan* fan4,
                       Thermistor* temp1, Thermistor* temp2, Thermistor* temp3);
void handle_http_request();
void stop_http_server();

#endif  // HTTP_SERVER_H
