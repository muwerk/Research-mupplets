# mupplets [WIP]

**Note:** This is very much a work-in-progress.

muwerk applets: functional units that support specific hardware or reusable applications
muwerk applets use muwerks messaging to pass information. Access via external MQTT is transpartent:
any mupplet can be accessed via MQTT.

## Dependencies

* Note: **All** mupplets require the libraries [ustd](https://github.com/muwerk/ustd) and [muwerk](https://github.com/muwerk/muwerk).

Currently, mupplets work only with ESP8266 or ESP32, due to usage of `std::function<>` to register member tasks. This restriction will
change in a later release.

It is recommended to use [munet](https://github.com/muwerk/munet) for network connectivity.

### Additional hardware-dependent libraries

| mupplet     | Function | Hardware | Dependencies |
| ----------- | -------- | -------- | ------------ |
| airqual.h   | Air quality sensor CO<sub>2</sub>, VOC | [CCS811](https://www.sparkfun.com/products/14193) | [SparkFun CCS811 Arduino Library](https://github.com/sparkfun/SparkFun_CCS811_Arduino_Library) |
| clock7seg.h | Simple 4 digit clock with timer | [4x 7segment display with HT16K33](https://www.adafruit.com/product/881) | [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library) [Adafruit LED Backpack Library](https://github.com/adafruit/Adafruit_LED_Backpack) |
| dhtxx.h     | Temperature, humidity sensor | DHT 11, DHT 21, DHT 22 | [DHT sensor library](https://github.com/adafruit/DHT-sensor-library) |
| ldr.h       | Luminosity | LDR connected to analog port | |
| led.h       | LED diode | Digital out or PWM connected to led: [D-out]--[led<]--(Vcc) | |
| lumin.h     |
| neocandle.h |
| pressure.h  |
| switch.h    |

## Application notes

### ldr.h

The ldr mupplet measures luminosity using a simple analog LDR (light dependent resistor)

The mupplet sends messages with topic `<mupplet-name>/unitluminosity`, the message body contains a float (string encoded) with values between [0.0-1.0].

<img src="https://github.com/muwerk/mupplets/blob/master/Resources/ldr.png" width="30%" height="30%">
Hardware: LDR, 10kΩ resistor

### led.h

Allows to set LEDs via digital logic or PWM brightness.

<img src="https://github.com/muwerk/mupplets/blob/master/Resources/led.png" width="30%" height="30%">
Hardware: 330Ω resistor
