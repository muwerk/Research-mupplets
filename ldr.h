// ldr.h
#pragma once

#include "scheduler.h"
#include "home_assistant.h"

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
    HomeAssistant *pHA;

  public:
    ustd::sensorprocessor illuminanceSensor = ustd::sensorprocessor(4, 600, 0.005);
    
    Ldr(String name, uint8_t port) : name(name), port(port) {
    }

    ~Ldr() {
    }

    void publishIlluminance() {
        char buf[32];
        sprintf(buf, "%5.3f", ldrvalue);
        pSched->publish(name + "/sensor/unitilluminance", buf);
    }

    double getUnitIlluminance() {
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
        pSched->subscribe(tID, name + "/sensor/unitilluminance/#", fnall);
    }

    void registerHomeAssistant(String homeAssistantFriendlyName, String homeAssistantDiscoveryPrefix="homeassistant") {
        pHA=new HomeAssistant(name, tID, homeAssistantFriendlyName, homeAssistantDiscoveryPrefix);
        pHA->addSensor(name, homeAssistantFriendlyName, "unitilluminance", "Unit-Illuminance", "[0..1]","illuminance","mdi:brightness-6");
        pHA->begin(pSched);
    }

  private:
    void loop() {
        double val = analogRead(port) / (adRange-1.0);
        if (illuminanceSensor.filter(&val)) {
            ldrvalue = val;
            publishIlluminance();
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/unitilluminance/get") {
            publishIlluminance();
        }
    };
};  // Ldr

}  // namespace ustd
