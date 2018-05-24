// led.h
#pragma once

#include "scheduler.h"

namespace ustd {
class Led {
  public:
    Scheduler *pSched;
    String name;
    uint8_t port;
    double ledvalue;
    ustd::sensorprocessor ledsens = ustd::sensorprocessor(4, 600, 0.005);

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
        pSched->add(ft, 200000);

        std::function<void(String, String, String)> fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(name + "/led/#", fnall);
    }

    void loop() {
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/unitluminosity/get") {
            char buf[32];
            sprintf(buf, "%5.3f", ledvalue);
            pSched->publish(name + "/unitluminosity", buf);
        }
    };
};  // Led

}  // namespace ustd
