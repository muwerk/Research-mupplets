// dht.h
#pragma once

#include "DHT.h"   // from "DHT sensor library", https://github.com/adafruit/DHT-sensor-library
                   // and "Adafruit Unified Sensor", https://github.com/adafruit/Adafruit_Sensor
#include "scheduler.h"
#include "sensors.h"

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
        //* std::function<void()> */
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 5000000);

        /* std::function<void(String, String, String)> */
        auto fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/sensor/temperature/get", fnall);
        pSched->subscribe(tID, name + "/sensor/humidity/get", fnall);
    }

    void publishTemperature() {
        char buf[32];
        sprintf(buf, "%5.1f", dhtTempVal);
        pSched->publish(name + "/sensor/temperature", buf);
    }

    void publishHumidity() {
        char buf[32];
        sprintf(buf, "%5.1f", dhtHumidVal);
        pSched->publish(name + "/sensor/humidity", buf);
    }
    void loop() {
        double t = pDht->readTemperature();
        double h = pDht->readHumidity();

        if (!isnan(t)) {
            if (dhtTemp.filter(&t)) {
                dhtTempVal = t;
                publishTemperature();
            }
        }
        if (!isnan(h)) {
            if (dhtHumid.filter(&h)) {
                dhtHumidVal = h;
                publishHumidity();
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/temperature/get") {
            publishTemperature();
        }
        if (topic == name + "/humidity/get") {
            publishHumidity();
        }
    };
};  // Dht

}  // namespace ustd
