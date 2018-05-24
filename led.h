// led.h
#pragma once

#include "scheduler.h"

namespace ustd {
class Led {
  public:
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t port;
    double brightlevel;
    bool state;

    uint8_t mode;

    Led(String name, uint8_t port) : name(name), port(port) {
    }

    ~Led() {
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        pinMode(port, OUTPUT);
        digitalWrite(port, HIGH);  // OFF

        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        std::function<void()> ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 200000);

        std::function<void(String, String, String)> fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/led/#", fnall);
    }

    void set(bool state) {
        if (state) {
            digitalWrite(port, LOW);
        } else {
            digitalWrite(port, HIGH);
        }
    }

    void brightness(double bright) {
        uint8_t bri;
        if (bright < 0.0)
            bright = 0.0;
        if (bright > 1.0)
            bright = 1.0;
        bri = (uint8_t)(bright * 255);
        analogWrite(port, bri);
    }

    void loop() {
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/led/set") {
            char buf[32];
            sprintf(buf, "%5.3f", brightlevel);
            pSched->publish(name + "/unitluminosity", buf);
        }
    };
};  // Led

}  // namespace ustd
