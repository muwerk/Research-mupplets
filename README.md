# mupplets [WIP]

**Note:** This is very much a work-in-progress. Interface and messages are NOT stable or final.

**mu**werk a**pplets**; mupplets: functional units that support specific hardware or reusable applications.

**mupplets** use muwerks MQTT-style messaging to pass information between each other on the same device. If connected to an MQTT-server via munet, all functionallity is externally available through an MQTT server such as Mosquitto.

See [mupplet led and switch example](https://github.com/muwerk/Examples/tree/master/led) for a complete example that illustrates how a
switch mupplet (interfacing to a physical button) and a led mupplet (infacing to a physical led) communicate using muwerk.
Both switch and led are also accessible via external MQTT without extra code.

## Dependencies

* Note: **All** mupplets require the libraries [ustd](https://github.com/muwerk/ustd) and [muwerk](https://github.com/muwerk/muwerk).

For ESP8266 and ESP32, it is recommended to use [munet](https://github.com/muwerk/munet) for network connectivity.

### Additional hardware-dependent libraries

| mupplet     | Function | Hardware | Dependencies | Platform
| ----------- | -------- | -------- | ------------ | --------
| airqual.h   | Air quality sensor CO<sub>2</sub>, VOC | [CCS811](https://www.sparkfun.com/products/14193) | [SparkFun CCS811 Arduino Library](https://github.com/sparkfun/SparkFun_CCS811_Arduino_Library) | ESP
| clock7seg.h | Simple 4 digit clock with timer | [4x 7segment display with HT16K33](https://www.adafruit.com/product/881) | [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library) [Adafruit LED Backpack Library](https://github.com/adafruit/Adafruit_LED_Backpack) | ESP
| dhtxx.h     | Temperature, humidity sensor | DHT 11, DHT 21, DHT 22 | [DHT sensor library](https://github.com/adafruit/DHT-sensor-library), [Adafruit unified sensor](https://github.com/adafruit/Adafruit_Sensor) | ESP
| ldr.h       | Luminosity | LDR connected to analog port | | ESP
| led.h       | LED diode | Digital out or PWM connected to led: [D-out]--[led<]--(Vcc) | | ESP
| lumin.h     |
| mp3.h       | MP3 player | OpenSmart v1.1 [OpenSmart MP3 player](https://www.aliexpress.com/item/32782488336.html?spm=a2g0o.productlist.0.0.5a0e7823gMVTMa&algo_pvid=8fd3c7b0-09a7-4e95-bf8e-f3d37bd18300&algo_expid=8fd3c7b0-09a7-4e95-bf8e-f3d37bd18300-0&btsid=d8c8aa30-444b-4212-ba19-2decc528c422&ws_ab_test=searchweb0_0,searchweb201602_6,searchweb201603_52) | | ESP, ESP32
| neocandle.h |
| pressure.h  | Air pressure and temperature sensor | BMP085 | [Adafruit BMP085 unified](https://github.com/adafruit/Adafruit_BMP085_Unified), [Adafruit unified sensor](https://github.com/adafruit/Adafruit_Sensor) | ESP
| switch.h    | any push button |   | ESP, ESP32 (no intr.)
| i2c_pwm.h   | 16 channel PWM via I2C | [PCA9685 based I2C 16 channel board](https://www.adafruit.com/products/815) | https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library | ESP

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
        Serial.prinln(msg);  // String float [0.0, ..., 1.0]
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

#### Messages sent by led mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/led/unitluminosity` | luminosity [0.0-1.0] | `0.34`: Float value encoded as string. Not send on automatic changes (e.g. pulse mode)
| `<mupplet-name>/led/state` | `on` or `off` | current led state (`on` is not sent on pwm intermediate values)

#### Message received by led mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/led/set` | `on`, `off`, `true`, `false`, `pct 34`, `34%`, `0.34` | Led can be set fully on or off with on/true and off/false. A fractional brightness of 0.34 (within interval [0.0, 1.0]) can be sent as either `pct 34`, or `0.34`, or `34%`.
| `<mupplet-name>/led/mode/set` | `passive`, `pulse <duration_ms>`, `blink <intervall_ms>[,<phase-shift>]`, `pattern <pattern>[,<intervall>[,<phase>]]` or `wave <intervall_ms>[,<phase-shift>]` | Mode passive does no automatic led state changes, `pulse` switches the led on for `<duration_ms>` ms, then led goes back to passive mode. `blink` changes the led state very `interval_ms` on/off, `wave` uses pwm to for soft changes between on and off states. Optional comma-speratated phase [0.0, ..., 1.0] can be added as a phase-shift. Two leds, one with `wave 1000` and one with `wave 1000,0.5` blink inverse. Patterns can be specified as string containing `+`,`-`,`0`..`9` or `r`. `+` is led on during `<intervall>` ms, `-` is off, `0`..`9` brightness-level. An `r` at the end of the pattern repeats the pattern. `"pattern +-+-+-+++-+++-+++-+-+-+---r,100"` lets the board signal SOS.

Example: sending an MQTT message with topic `<led-name>/mode/set` and message `wave 1000` causes the led to softly pulse between on and off every 1000ms.

Multiple leds are time and phase synchronized.

### Sample code

```cpp
#include "led.h"

ustd::Led led("myLed",D5,false); 
            // Led connected to pin D5, 
            // false: led is on when D5 low
            // (inverted logic)
            // messages are sent/received to myLed/led/...

void setup() {

    led.begin(&sched);
    led.setmode(led.Mode::WAVE, 1000);
            // soft pwm pulsing in 1000ms intervals
            // same can be accomplished by publishing
            // topic myLed/led/setmode  msg "wave 1000"
```

See [mupplet led and switch example](https://github.com/muwerk/Examples/tree/master/led) for a complete example.

## Switch

Support switches with automatic debouncing.

<img src="https://github.com/muwerk/mupplets/blob/master/Resources/switch.png" width="50%" height="30%">
Hardware: 330Ω resistor, led, switch.

#### Messages sent by switch mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/switch/state` | `on`, `off` or `trigger` | switch state, usually `on` or `off`. In modes `falling` and `rising` only `trigger`
messages are sent on rising or falling signal.
| `<mupplet-name>/switch/debounce` | <time-in-ms> | reply to `<mupplet-name>/switch/debounce/get`, switch debounce time in ms [0..1000]ms.
| `<custom-topic>` |  | `on`, `off` or `trigger` | If a custom-topic is given during switch init, an addtional message is publish on switch state changes with that topic, The message is identical to ../switch/state', usually `on` or `off`. In modes `falling` and `rising` only `trigger`.
| `<mupplet-name>/switch/shortpress` | `trigger` | Switch is in `duration` mode, and button is pressed for less than `<shortpress_ms>` (default 3000ms).
| `<mupplet-name>/switch/longpress` | `trigger` | Switch is in `duration` mode, and button is pressed for less than `<longpress_ms>` (default 30000ms), yet longer than shortpress.
| `<mupplet-name>/switch/verylongtpress` | `trigger` | Switch is in `duration` mode, and button is pressed for longer than `<longpress_ms>` (default 30000ms).
| `<mupplet-name>/switch/duration` | `<ms>` | Switch is in `duration` mode, message contains the duration in ms the switch was pressed.


#### Message received by switch mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/switch/set` | `on`, `off`, `true`, `false`, `toggle` | Override switch setting. When setting the switch state via message, the hardware port remains overridden until the hardware changes state (e.g. button is physically pressed). Sending a `switch/set` message puts the switch in override-mode: e.g. when sending `switch/set` `on`, the state of the button is signalled `on`, even so the physical button might be off. Next time the physical button is pressed (or changes state), override mode is stopped, and the state of the actual physical button is published again.  
| `<mupplet-name>/switch/mode/set` | `default`, `rising`, `falling`, `flipflop`, `timer <time-in-ms>`, `duration [shortpress_ms[,longpress_ms]]` | Mode `default` sends `on` when a button is pushed, `off` on release. `falling` and `rising` send `trigger` on corresponding signal change. `flipflop` changes the state of the logical switch on each change from button on to off. `timer` keeps the switch on for the specified duration (ms). `duration` mode sends messages `switch/shortpress`, if button was pressed for less than `<shortpress_ms>` (default 3000ms), `switch/longpress` if pressed less than `<longpress_ms>`, and `switch/verylongpress` for longer presses.
| `<mupplet-name>/switch/debounce/set` | <time-in-ms> | String encoded switch debounce time in ms, [0..1000]ms. Default is 20ms. This is especially need, when switch is created in interrupt mode (see comment in [example](https://github.com/muwerk/Examples/tree/master/led)).

### Sample code

```cpp
#include "led.h"
#include "switch.h"

ustd::Scheduler sched;
ustd::Led led("myLed",D5,false);
ustd::Switch toggleswitch("mySwitch",D6, ustd::Switch::Mode::Default, false);

void switch_messages(String topic, String msg, String originator) {
    if (topic == "mySwitch/switch/state") {
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
    toggleswitch.setMode(ustd::Mode::Flipflop);
    sched.subscribe(tID, "mySwitch/switch/state", switch_messages);
}
```

See [mupplet led and switch example](https://github.com/muwerk/Examples/tree/master/led) for a complete example.

## DHT22, DHT11, DHT21 temperature and humidity sensors

Measures temperature and humidity.

<img src="https://github.com/muwerk/mupplets/blob/master/Resources/dht.png" width="30%" height="30%">
Hardware: 10kΩ, DHT22 sensor.

#### Messages sent by dht mupplet:

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

## I2C 16 channel PWM module based on PCA9685

Allows to control up to 16 PWM leds or servos.

<img src="https://github.com/muwerk/mupplets/blob/master/Resources/i2c_pwm_servo.png" width="70%" height="30%">
Hardware: PCA9685 based I2C-PWM board.

#### Notes

* This mupplet can either be in mode `Mode::PWM` or `Mode::SERVO`. In `PWM` mode, the pwm frequency is 1000Hz by default,
in `SERVO` mode, frequency is 60Hz. All 16 channels share the same mode. The global pwm frequency can be overriden with `setFrequency(freq)`.
* Servo minma and maxima can be configured with `void setServoMinMax(int minP=150, int maxP=600) { // pulses out of 4096 at 60hz (frequency)`. See [Adafruit's excellent documentation](https://learn.adafruit.com/16-channel-pwm-servo-driver/using-the-adafruit-library) for more details on servo callibration.
* Max 25mA per channel!


#### Messages received by i2c_pwm mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/i2cpwm/set/<channel-no>` |  `on`, `off`, `true`, `false`, `pct 34`, `34%`, `0.34` | For leds, results in set fully on or off with on/true and off/false. A fractional brightness of 0.34 (within interval [0.0, 1.0]) can be sent as either `pct 34`, or `0.34`, or `34%`. For `Mode::SERVO` this results in a servo-position proportional to the value [0..1]. 

### Sample code

```cpp
#include "i2c_pwm.h"

ustd::Scheduler sched(10,16,32);
ustd::I2CPWM servo("myServo",ustd::I2CPWM::Mode::SERVO);

double count=0.0;
void appLoop() { // change servo every 500ms
    servo.setUnitLevel(15,count);  // change servo at channel 15.
    count+=0.2;
    if (count>1.0) count=0.0;
}

void setup() {
    int tID = sched.add(appLoop, "main", 500000); // 500ms schedule for appLoop task
    servo.begin(&sched);
}

```

See [Servo](https://github.com/muwerk/Examples/tree/master/servo) for a complete example.





## MP3 player with SD-Card (OpenSmart)

Allows playback of different MP3 files.

<img src="https://github.com/muwerk/mupplets/blob/master/Resources/mp3.png" width="30%" height="30%">
Hardware: OpenSmart MP3 player (e.g. AliExpress).

#### Notes

* ⚠️ While the documentation says that the player works from 3.3 to 5V, the amplifier does not seem to work with less than 5V. It is unclear what voltage is generated for player's TX line. I had no problems connecting to 3.3V logic, but this is not documented as being safe.
* The mupplet uses non-blocking asynchronous serial read and write. Each command messages is separated automatically and asynchronously by 120ms from the closed following messages to prevent confusing the mp3 player.
* ⚠️ The player sometimes sends completely undocumented messages (MQTT topic `xmessage`). Do not rely too much on a deterministic message protocol.

##### MP3 files on SD-Card

* SD-Card needs to be fat or fat32 format.
* It can have 100 folders (`00`...`99`), and each folder can contain 256 files (`000-xxx.mp3`...`255-yyy.mp3`), a file can be accessed with folder and track number: `01/002-mysong.mp3` would be the file identified by folder 1 (=directory name `01`) and track 2 (filename `002xxx.mp3`).

#### Messages received by mp3 mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/mp3/track/set` | `folder-id`,`track-id` | To play file 01/002-mysong.mp3, message text should contain `1,2`.
| `<mupplet-name>/mp3/state/set` | `play`,`pause`,`stop` | Stops, pauses or plays current song.
| `<mupplet-name>/mp3/volume/set` | 0...30  | Sets playback volume to between 0 and 30(max).

#### Messages sent by mp3 mupplet:

| topic | message body | comment
| ----- | ------------ | -------
| `<mupplet-name>/mp3/volume` | 0...30 | Current playback volume
| `<mupplet-name>/mp3/storage` | `NONE`,`DISK`,`TF-CARD`,`SPI` | Active storage type
| `<mupplet-name>/mp3/state` | `STOP`,`PLAY`,`PAUSE`,`FASTFORWARD`,`FASTREWIND`,`PLAYING` | Current player state. State `PLAYING` is not defined in documentation and seems to be followed always by state `PLAY`.
| `<mupplet-name>/mp3/xmessage` | `hexdump` | Undocumented messages.

### Sample code

```cpp
// Do NOT use serial debug, since Serial is used for communication with MP3 player!
//#define USE_SERIAL_DBG 1
#include "mp3.h"

ustd::Scheduler sched(10,16,32);
ustd::Mp3Player mp3("mp3", &Serial, ustd::Mp3Player::MP3_PLAYER_TYPE::OPENSMART);

void setup() {
    mp3.begin(&sched);
    mp3.setVolume(4);
    mp3.playFolderTrack(1,1);
}
```

See [MP3](https://github.com/muwerk/Examples/tree/master/mp3) for a complete example.
