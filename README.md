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

#### Messages sent by ldr mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/unitluminosity` | luminosity [0.0-1.0] | 


<img src="https://github.com/muwerk/mupplets/blob/master/Resources/ldr.png" width="30%" height="30%">
Hardware: LDR, 10kΩ resistor

#### Sample code
```cpp
#include "ldr.h"

ustd::Scheduler sched;
ustd::Ldr ldr("myLDR",A0);

void task0(String topic, String msg, String originator) {
    if (topic == "myLDR/unitluminosity") {
        Serial.print("Lumin: ");
        Serial.prinln(msg);  // String float [0.0, 1.0]
    }
}

void setup() {
   ldr.begin(&sched);

   sched.subscribe(tID, "myLDR/unitluminosity", task0);
}
```
### led.h

Allows to set LEDs via digital logic or PWM brightness.

<img src="https://github.com/muwerk/mupplets/blob/master/Resources/led.png" width="30%" height="30%">
Hardware: 330Ω resistor, led.

#### Messages send by led mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/unitluminosity` | luminosity [0.0-1.0] | Not send on automatic changes (e.g. pulse mode)
| `<mupplet-name>/state` | `on` or `off` | current led state (`on` is not sent on pwm intermediate values)

#### Message received by led mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/set` | `on`, `off`, `true`, `false`, `pct 34`, `34%`, `0.34` | Led can be set fully on or off with on/true and off/false. A fractional brightness of 0.34 (within interval [0.0, 1.0]) can be sent as either `pct 34`, or `0.34`, or `34%`.
| `<mupplet-name>/mode/set` | `passive`, `blink <intervall_ms>`, or `pulse <intervall_ms>` | Mode passive does no automatic led state changes, `blink` changes the led state very `interval_ms` on/off, `pulse` uses pwm to for soft changes between on and off states.

Example: sending an MQTT message with topic `<led-name>/mode/set` and message `pulse 1000` causes the led to softly pulse between on and off every 1000ms.

### Sample code

```cpp
#include "led.h"

ustd::Led led("myLed",D5,false); 
            // Led connected to pin D5, 
            // false: led is on when D5 low
            // (inverted logic)
            // messages are sent/received to myLed/...

void setup() {

    led.begin(&sched);
    led.setmode(led.Mode::PULSE, 1000);
            // soft pwm pulsing in 1000ms intervals
            // same can be accomplished by publishing
            // topic myLed/led/setmode  msg "pulse 1000"
```