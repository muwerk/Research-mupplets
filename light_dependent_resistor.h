// ldr.h
#pragma once

#include "scheduler.h"

namespace ustd {
class Ldr {
  public:
    Scheduler *pSched;
    uint8_t port;
    String name;

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
        pSched->add(ft);

        std::function<void(String, String, String)> fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe("#", fnall);
    }

    void loop() {
    }

    void subsMsg(String topic, String msg, String originator) {
        DynamicJsonBuffer jsonBuffer(200);
        JsonObject &root = jsonBuffer.parseObject(msg);
        if (!root.success()) {
            // DBG("ldr: Invalid JSON received: " + String(msg));
            return;
        }
        if (topic == "net/services/ldrserver") {
            // ldrServer = root["server"].as<char *>();
        }
    };
};  // Ldr

}  // namespace ustd
