// ldr.h
#pragma once

#include "scheduler.h"

namespace ustd {
class Ldr {
  private:
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t port;
    double ldrvalue;
#ifdef __ESP32__
    double adRange=4096.0; // 12 bit default
#else
    double adRange=1024.0; // 10 bit default
#endif
    ustd::sensorprocessor ldrsens = ustd::sensorprocessor(4, 600, 0.005);

  public:
    Ldr(String name, uint8_t port) : name(name), port(port) {
    }

    ~Ldr() {
    }

    double getUnitLuminosity() {
        return ldrvalue;
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        /* std::function<void()> */
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 200000);

        /* std::function<void(String, String, String)> */
        auto fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/unitluminosity/#", fnall);
    }

  private:
    void loop() {
        double val = analogRead(port) / (adRange-1.0);
        if (ldrsens.filter(&val)) {
            ldrvalue = val;
            char buf[32];
            sprintf(buf, "%5.3f", ldrvalue);
            pSched->publish(name + "/unitluminosity", buf);
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/unitluminosity/get") {
            char buf[32];
            sprintf(buf, "%5.3f", ldrvalue);
            pSched->publish(name + "/unitluminosity", buf);
        }
    };
};  // Ldr

}  // namespace ustd
