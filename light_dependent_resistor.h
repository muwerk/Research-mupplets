// ldr.h
#pragma once

#include "scheduler.h"

namespace ustd {
class Ldr {
  public:
    Scheduler *pSched;
    uint8_t port;
    String name;
    double ldrvalue;
    ustd::sensorprocessor ldrsens = ustd::sensorprocessor(8, 600, 0.01);

    Ldr(uint8_t port, String name) : port(port), name(name) {
    }

    ~Ldr() {
    }

    void begin(Scheduler *_pSched) {
        // Make sure _clientName is Unique! Otherwise LDR server will rapidly
        // disconnect.
        pSched = _pSched;

        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        std::function<void()> ft = [=]() { this->loop(); };
        pSched->add(ft, 200000);

        std::function<void(String, String, String)> fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(name + "/luminosity/#", fnall);
    }

    void loop() {
        double val = analogRead(port) / 1023.0;
        if (ldrsens.filter(&val)) {
            ldrvalue = val;
            char buf[32];
            sprintf(buf, "%5.1f", ldrvalue);
            pSched->publish(name + "/luminosity", buf);
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/luminosity/get") {
            sprintf(buf, "%5.1f", ldrvalue);
            pSched->publish(name + "/luminosity", buf);
        }
    };
};  // Ldr

}  // namespace ustd
