// dht.h
#pragma once

//#include "../.piolibdeps/DHT sensor library_ID19/DHT.h"
#include "DHT.h"
#include "scheduler.h"

namespace ustd {
class Dht {
  public:
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t port;
    uint8_t type;
    double dhtTempVal;
    double dhtHumidVal;
    ustd::sensorprocessor dhtTemp = ustd::sensorprocessor(4, 600, 0.1);
    ustd::sensorprocessor dhtHumid = ustd::sensorprocessor(4, 600, 1.0);
    DHT *pDht;

    Dht(String name, uint8_t port, uint8_t type = DHT22)
        : name(name), port(port), type(type) {
        pDht = new DHT(port, type);

        pinMode(port, INPUT_PULLUP);
    }

    ~Dht() {
    }

    double getTemp() {
        return dhtTempVal;
    }

    double getHumid() {
        return dhtHumidVal;
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        pDht->begin();

        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        std::function<void()> ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 5000000);

        std::function<void(String, String, String)> fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/temperature/get", fnall);
        pSched->subscribe(tID, name + "/humidity/get", fnall);
    }

    void loop() {
        double t = pDht->readTemperature();
        double h = pDht->readHumidity();

        if (dhtTemp.filter(&t)) {
            dhtTempVal = t;
            char buf[32];
            sprintf(buf, "%5.1f", dhtTempVal);
            pSched->publish(name + "/temperature", buf);
        }
        if (dhtHumid.filter(&h)) {
            dhtHumidVal = h;
            char buf[32];
            sprintf(buf, "%5.1f", dhtHumidVal);
            pSched->publish(name + "/humidity", buf);
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/temperature/get") {
            char buf[32];
            sprintf(buf, "%5.3f", dhtTempVal);
            pSched->publish(name + "/temperature", buf);
        }
        if (topic == name + "/humidity/get") {
            char buf[32];
            sprintf(buf, "%5.3f", dhtHumidVal);
            pSched->publish(name + "/humidity", buf);
        }
    };
};  // Dht

}  // namespace ustd
