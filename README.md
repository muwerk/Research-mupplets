# mupplets [WIP]

**Note:** This is very much a work-in-progress.

**mu**werk a**pplets**; mupplets: functional units that support specific hardware or reusable applications.

**mupplets** use muwerks MQTT-style messaging to pass information between each other on the same device. If connected to an MQTT-server via munet, all functionallity is externally available.

See [mupplet led and switch example](https://github.com/muwerk/Examples/tree/master/led) for a complete example that illustrates how a
switch mupplet (interfacing to a physical button) and a led mupplet (infacing to a physical led) communicate using muwerk.
Both switch and led are also accessible via external MQTT without extra code.

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
| dhtxx.h     | Temperature, humidity sensor | DHT 11, DHT 21, DHT 22 | [DHT sensor library](https://github.com/adafruit/DHT-sensor-library), [Adafruit unified sensor](https://github.com/adafruit/Adafruit_Sensor) |
| ldr.h       | Luminosity | LDR connected to analog port | |
| led.h       | LED diode | Digital out or PWM connected to led: [D-out]--[led<]--(Vcc) | |
| lumin.h     |
| neocandle.h |
| pressure.h  | Air pressure and temperature sensor | BMP085 | [Adafruit BMP085 unified](https://github.com/adafruit/Adafruit_BMP085_Unified), [Adafruit unified sensor](https://github.com/adafruit/Adafruit_Sensor) |
| switch.h    | any push button | 

## Application notes

### ldr.h

The ldr mupplet measures luminosity using a simple analog LDR (light dependent resistor)

#### Messages sent by ldr mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/unitluminosity` | luminosity [0.0-1.0] | Float value encoded as string


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
| `<mupplet-name>/led/unitluminosity` | luminosity [0.0-1.0] | `0.34`: Float value encoded as string. Not send on automatic changes (e.g. pulse mode)
| `<mupplet-name>/led/state` | `on` or `off` | current led state (`on` is not sent on pwm intermediate values)

#### Message received by led mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/led/set` | `on`, `off`, `true`, `false`, `pct 34`, `34%`, `0.34` | Led can be set fully on or off with on/true and off/false. A fractional brightness of 0.34 (within interval [0.0, 1.0]) can be sent as either `pct 34`, or `0.34`, or `34%`.
| `<mupplet-name>/led/mode/set` | `passive`, `blink <intervall_ms>`, or `pulse <intervall_ms>` | Mode passive does no automatic led state changes, `blink` changes the led state very `interval_ms` on/off, `pulse` uses pwm to for soft changes between on and off states.

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

See [mupplet led and switch example](https://github.com/muwerk/Examples/tree/master/led) for a complete example.

## Switch

Support sitches with automatic debouncing.

<img src="https://github.com/muwerk/mupplets/blob/master/Resources/switch.png" width="50%" height="30%">
Hardware: 330Ω resistor, led, switch.

#### Messages send by switch mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/switch/state` | `on` or `off` | switch state
| `<mupplet-name>/switch/debounce` | <time-in-ms> | reply to `<mupplet-name>/switch/debounce/get`, switch debounce time in ms [0..1000]ms

#### Message received by switch mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/switch/set` | `on`, `off`, `true`, `false`, `toggle` | Override switch setting. When setting the switch state via message, the hardware port remains overridden until the hardware changes state (e.g. button is physically pressed). Sending a `switch/set` message puts the switch in override-mode: e.g. when sending `switch/set` `on`, the state of the button is signalled `on`, even so the physical button might be off. Next time the physical button is pressed (or changes state), override mode is stopped, and the state of the actual physical button is published again.  
| `<mupplet-name>/switch/debounce/set` | <time-in-ms> | String encoded switch debounce time in ms, [0..1000]ms. Default is 20ms.

### Sample code

```cpp
#include "led.h"
#include "switch.h"

ustd::Scheduler sched;
ustd::Led led("myLed",D5,false);
ustd::Switch toggleswitch("mySwitch",D6, false);

void switch_messages(String topic, String msg, String originator) {
    if (topic == "mySwitch/state") {
        if (msg=="on") {
            led.set(true);
        } else {
            led.set(false);
        }
    }
}

void setup() {
    led.begin(&sched);
    toggleswitch.begin(&sched);
    sched.subscribe(tID, "mySwitch/switch/state", switch_messages);
}
```

See [mupplet led and switch example](https://github.com/muwerk/Examples/tree/master/led) for a complete example.

## DHT22, DHT11, DHT21 temperature and humidity sensors

Measures temperature and humidity.

<img src="https://github.com/muwerk/mupplets/blob/master/Resources/dht.png" width="30%" height="30%">
Hardware: 10kΩ, DHT22 sensor.

#### Messages send by switch mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/temperature` | `<temperature>` | Float, encoded as String, temperature in Celsius. "23.3"
| `<mupplet-name>/humidity` | `<humidity>` | Float, encoded as String, humidity in percent. "55.6"

### Sample code

```cpp
#include "dht.h"

ustd::Scheduler sched(10,16,32);
ustd::Dht dht("myDht",D4);

void sensor_messages(String topic, String msg, String originator) {
    if (topic == "myDht/temperature") {
        Serial.println("Temperature: "+msg);
    }
    if (topic == "myDht/humidity") {
        Serial.println("Humidity: "+msg);
    }
}


void setup() {
    Serial.begin(115200);
    dht.begin(&sched);

    sched.subscribe(tID, "myDht/#", sensor_messages);
}
```

See [Temperature and humidity](https://github.com/muwerk/Examples/tree/master/dht) for a complete example.
