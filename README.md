# ESP32, ULP, ADC bug in some SDK versions - minimal reproduce case

This project demonstrates a bug with the Arduino-ESP32 SDK where in some versions of the SDK(s) the ULP processor will get stuck doing an ADC read operation.

When the ULP is stuck it doesn't wake due to ULP and the code will fall back to a wake using a timer.

The issue happens only if doing an ADC read and using Arduino-ESP32 2.0.14 and up.

Arduino-ESP32 2.0.14 is using ESP-IDF 4.4.6 . If using this code with pure ESP-IDF 4.4.6 then the issue doesn't happen! Comment out the Arduino.h include for an ESP-IDF compatible code.

Same issue when using Arduino-ESP32 from Platform.IO . This repo is a ready made PIO project.

Last working set for Arduino-ESP32 is: ESP Arduino: 2.0.13, ESP-IDF: 4.4.5  
Last working set for Platform.IO is: Espressif32 6.4.0, ESP Arduino: 2.0.11, ESP-IDF: 4.4.5  

To check how the ADC instruction relates to the bug, use the variable "ulp_do_adc" in the code to easily try the project with and without this function.
