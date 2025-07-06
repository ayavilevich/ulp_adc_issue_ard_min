# ESP32, ULP, ADC bug in some SDK versions - reproduce case

This project demonstrates a bug with the ESP32 SDK where in some versions of the SDK(s) the ULP processor will get stuck doing an ADC read operation.

The issue happens only if doing an ADC read and using PlatformIO Espressif32 platform 6.5.0 and up.

It is not clear where the wrong code is. The stack is:

+ https://github.com/platformio/platform-espressif32
+ https://github.com/espressif/arduino-esp32
+ https://github.com/espressif/esp-idf

Last working set is: Espressif32 6.4.0, ESP Arduino: 2.0.11, ESP-IDF: 4.4.5  
The first non working set is: Espressif32 6.5.0, ESP Arduino: 2.0.14, ESP-IDF: 4.4.6

To check how the ADC instruction relates to the bug, use the variable "ulp_do_adc" in the code to easily try the project with and without this function.
