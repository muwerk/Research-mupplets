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
    String errmsg="";
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
            errmsg="ID_ERROR";
            Serial.print(errmsg);
            break;
        case CCS811Core::SENSOR_I2C_ERROR:
            errmsg="I2C_ERROR (did you put WAK low? Required!) See code for more.";
            Serial.println(errmsg);
            Serial.println("You need a patch: "
                           "https://github.com/sparkfun/SparkFun_CCS811_Arduino_Library/issues/6");
            break;
        case CCS811Core::SENSOR_INTERNAL_ERROR:
            errmsg="INTERNAL_ERROR";
            Serial.print(errmsg);
            break;
        case CCS811Core::SENSOR_GENERIC_ERROR:
            errmsg="GENERIC_ERROR";
            Serial.print(errmsg);
            break;
        default:
            errmsg="Unspecified error.";
            Serial.print(errmsg);
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
        /* std::function<void()> */ 
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 5000000);

        /* std::function<void(String, String, String)> */ 
        auto fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "sensor/co2/get", fnall);
        pSched->subscribe(tID, name + "sensor/voc/get", fnall);
    }

    void publishCO2() {
        char buf[32];
        sprintf(buf, "%5.1f", co2Val);
        pSched->publish(name + "/sensor/co2", buf);
    }

    void publishVOC() {
        char buf[32];
        sprintf(buf, "%5.1f", vocVal);
        pSched->publish(name + "/sensor/voc", buf);
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
                    publishCO2();
                }
                if (voc.filter(&v)) {
                    vocVal = v;
                    publishVOC();
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
        if (errmsg!="") {
            pSched->publish(name+"/error",errmsg);
        }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/co2/get") {
            publishCO2();
        }
        if (topic == name + "/sensor/voc/get") {
            publishVOC();
        }
    };
};  // AirQuality

}  // namespace ustd
