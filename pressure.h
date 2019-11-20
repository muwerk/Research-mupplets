// pressure.h
// adapted from:
// https://github.com/adafruit/Adafruit_BMP085_Unified/blob/master/examples/sensorapi/sensorapi.pde
#pragma once

#include "scheduler.h"
#include "sensors.h"

#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>

namespace ustd {
class Pressure {
  public:
    Scheduler *pSched;
    int tID;
    String name;
    double pressureTempVal;
    double pressureVal;
    bool bActive = false;
    ustd::sensorprocessor pressureTemp = ustd::sensorprocessor(4, 600, 0.1);
    ustd::sensorprocessor pressure = ustd::sensorprocessor(4, 600, 1.0);
    Adafruit_BMP085_Unified *pPressure;

    Pressure(String name) : name(name) {
        pPressure = new Adafruit_BMP085_Unified(10085);
    }

    ~Pressure() {
    }

    double getTemp() {
        return pressureTempVal;
    }

    double getPressure() {
        return pressureVal;
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        if (pPressure->begin()) {
            bActive = true;
        }

        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        /* std::function<void()> */
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 5000000);

        /* std::function<void(String, String, String)> */
        auto fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/sensor/temperature/get", fnall);
        pSched->subscribe(tID, name + "/sensor/pressure/get", fnall);
    }

    void publishPressure() {
        char buf[32];
        sprintf(buf, "%5.1f", pressureVal);
        pSched->publish(name + "/sensor/pressure", buf);

    }

    void publishPressureTemperature() {
        char buf[32];
        sprintf(buf, "%5.1f", pressureTempVal);
        pSched->publish(name + "/sensor/temperature", buf);

    }

    void loop() {
        if (bActive) {
            sensors_event_t event;
            pPressure->getEvent(&event);
            if (event.pressure) {
                double p, t;
                float tf;
                /* Display atmospheric pressue in hPa */
                p = event.pressure;  // hPa
                pPressure->getTemperature(&tf);
                t = (double)tf;

                if (pressureTemp.filter(&t)) {
                    pressureTempVal = t;
                    publishPressureTemperature();
                }
                if (pressure.filter(&p)) {
                    pressureVal = p;
                    publishPressure();
                }
            }
        } else {
            pSched->publish(name+"/sensor/error", "hardware not initialized");
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/temperature/get") {
            publishPressureTemperature();
        }
        if (topic == name + "/sensor/pressure/get") {
            publishPressure();
        }
    };
};  // Pressure

}  // namespace ustd
