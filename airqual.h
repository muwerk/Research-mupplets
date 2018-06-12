// airqual.h
// adapted from:
// https://github.com/sparkfun/SparkFun_CCS811_Arduino_Library/blob/master/examples/BasicReadings/BasicReadings.ino
#pragma once

#include "scheduler.h"
#include "sensors.h"

#include "SparkFunCCS811.h"

// Default I2C Address for
// https://learn.sparkfun.com/tutorials/ccs811-air-quality-breakout-hookup-guide
#define SPARKFUN_CCS811_ADDR 0x5B

namespace ustd {
class AirQuality {
  public:
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t i2caddr;
    double co2Val;
    double vocVal;
    bool bActive = false;
    ustd::sensorprocessor co2 = ustd::sensorprocessor(4, 600, 1.0);
    ustd::sensorprocessor voc = ustd::sensorprocessor(4, 600, 0.2);
    CCS811 *pAirQuality;

    AirQuality(String name, uint8_t i2caddr = SPARKFUN_CCS811_ADDR)
        : name(name), i2caddr(i2caddr) {
        pAirQuality = new CCS811(i2caddr);
    }

    ~AirQuality() {
    }

    double getCo2() {
        return co2Val;
    }

    double getVoc() {
        return vocVal;
    }

    void printDriverError(CCS811Core::status errorCode) {
#ifdef USE_SERIAL_DBG
        switch (errorCode) {
        case CCS811Core::SENSOR_SUCCESS:
            Serial.print("SUCCESS");
            break;
        case CCS811Core::SENSOR_ID_ERROR:
            Serial.print("ID_ERROR");
            break;
        case CCS811Core::SENSOR_I2C_ERROR:
            Serial.println("I2C_ERROR (can be caused by missing delay in "
                           ".begin() when used with ESP-chips");
            Serial.println("You need a patch: "
                           "https://github.com/sparkfun/"
                           "SparkFun_CCS811_Arduino_Library/issues/6");
            break;
        case CCS811Core::SENSOR_INTERNAL_ERROR:
            Serial.print("INTERNAL_ERROR");
            break;
        case CCS811Core::SENSOR_GENERIC_ERROR:
            Serial.print("GENERIC_ERROR");
            break;
        default:
            Serial.print("Unspecified error.");
        }
#endif
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        CCS811Core::status returnCode = pAirQuality->begin();
        if (returnCode == CCS811Core::SENSOR_SUCCESS) {
            bActive = true;
        } else {
            printDriverError(returnCode);
        }

        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        std::function<void()> ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 5000000);

        std::function<void(String, String, String)> fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/co2/get", fnall);
        pSched->subscribe(tID, name + "/voc/get", fnall);
    }

    void loop() {
        if (bActive) {
            if (pAirQuality->dataAvailable()) {
                double c, v;
#ifdef USE_SERIAL_DBG
                Serial.println("AirQuality sensor data available");
#endif
                pAirQuality->readAlgorithmResults();
                c = pAirQuality->getCO2();
                v = pAirQuality->getTVOC();
#ifdef USE_SERIAL_DBG
                Serial.print("AirQuality sensor data available, co2: ");
                Serial.print(c);
                Serial.print(" voc: ");
                Serial.println(v);
#endif
                if (co2.filter(&c)) {
                    co2Val = c;
                    char buf[32];
                    sprintf(buf, "%5.1f", co2Val);
                    pSched->publish(name + "/co2", buf);
                }
                if (voc.filter(&v)) {
                    vocVal = v;
                    char buf[32];
                    sprintf(buf, "%5.1f", vocVal);
                    pSched->publish(name + "/voc", buf);
                }
            } else {
#ifdef USE_SERIAL_DBG
                Serial.println("AirQuality sensor no data available");
#endif
            }
        } else {
#ifdef USE_SERIAL_DBG
            Serial.println("AirQuality sensor not active. Patch applied?");
#endif
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/co2/get") {
            char buf[32];
            sprintf(buf, "%5.3f", co2Val);
            pSched->publish(name + "/co2", buf);
        }
        if (topic == name + "/voc/get") {
            char buf[32];
            sprintf(buf, "%5.3f", vocVal);
            pSched->publish(name + "/voc", buf);
        }
    };
};  // AirQuality

}  // namespace ustd
