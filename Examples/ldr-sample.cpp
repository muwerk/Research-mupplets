#define USE_SERIAL_DBG 1

#include "platform.h"
#include "scheduler.h"

#include "ldr.h"

void appLoop();

ustd::Scheduler sched;
ustd::Ldr ldr("myLDR",A0);

void task0(String topic, String msg, String originator) {
    if (topic == "myLDR/sensor/illuminance") {
#ifdef USE_SERIAL_DBG
        Serial.print("Lumin: ");
        Serial.prinln(msg);  // String representing float 0.0 .. 1.0
#endif
    }
}


void setup() {
#ifdef USE_SERIAL_DBG
    Serial.begin(115200);
    Serial.println("Startup");
#endif  // USE_SERIAL_DBG
    tID = sched.add(appLoop, "main");
    ldr.begin(&sched);

    sched.subscribe(tID, "myLDR/sensor/illuminance", task0);
}

void appLoop() {
    // your code goes here.
}

// Never add code to this loop, use appLoop() instead.
void loop() {
    sched.loop();
}