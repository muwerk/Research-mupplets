// pressure.h
// adapted from:
// https://github.com/adafruit/Adafruit_BMP085_Unified/blob/master/examples/sensorapi/sensorapi.pde
#pragma once

#include "scheduler.h"
#include "sensors.h"

//#include <Adafruit_Sensor.h>
#include <Adafruit_MLX90614.h>
#include <home_assistant.h>

namespace ustd {
class Gy906 {
  public:
    String GY906_TEMP_VERSION="0.1.0";
    Scheduler *pSched;
    int tID;
    String name;
    double temperatureAmbientSensorVal;
    double temperatureIRSensorVal;
    bool bActive = false;
    bool fastIR = false;
    ustd::sensorprocessor temperatureAmbientSensor = ustd::sensorprocessor(4, 600, 0.1);
    ustd::sensorprocessor temperatureIRSensor = ustd::sensorprocessor(4, 600, 0.1);
    Adafruit_MLX90614 *pGy;
    #ifdef __ESP__
    HomeAssistant *pHA;
    #endif

    Gy906(String name) : name(name) {
        pGy = new Adafruit_MLX90614();
    }

    ~Gy906() {
    }

    double getAmbientTemp() {
        return temperatureAmbientSensorVal;
    }

    void setIRTempMode(bool fastMeasureMode=false) {
        fastIR=fastMeasureMode;
    }

    double getIRTemp() {
        return temperatureIRSensorVal;
    }

    void begin(Scheduler *_pSched, int _fastIR=false) {
        pSched = _pSched;
        fastIR = _fastIR;

        Wire.begin();
        if (pGy->begin()) {
            bActive = true;
        }

        auto ft = [=]() { this->loop(); };
        /* tID = */ pSched->add(ft, name, 500000);

        auto fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/sensor/ambient_temperature/get", fnall);
        pSched->subscribe(tID, name + "/sensor/ir_temperature/get", fnall);
    }

    #ifdef __ESP__
    void registerHomeAssistant(String homeAssistantFriendlyName, String projectName="", String homeAssistantDiscoveryPrefix="homeassistant") {
        pHA=new HomeAssistant(name, tID, homeAssistantFriendlyName, projectName, GY906_TEMP_VERSION, homeAssistantDiscoveryPrefix);
        pHA->addSensor("temperature", "Ambient temperature", "\\u00B0C","temperature","mdi:thermometer");
        pHA->addSensor("temperature", "IR Temperature", "\\u00B0C","pressure","mdi:thermometer");
        pHA->begin(pSched);
    }
    #endif

    void publishIRTemperature() {
        char buf[32];
        pSched->publish(name+"/sensor/result","OK");
        sprintf(buf, "%5.2f", temperatureIRSensorVal);
        pSched->publish(name + "/sensor/ir_temperature", buf);

    }

    void publishAmbientTemperature() {
        char buf[32];
        pSched->publish(name+"/sensor/result","OK");
        sprintf(buf, "%5.2f", temperatureAmbientSensorVal);
        pSched->publish(name + "/sensor/ambient_temperature", buf);

    }

    void loop() {
        if (bActive) {
            double t=pGy->readAmbientTempC();
            if (temperatureAmbientSensor.filter(&t)) {
                temperatureAmbientSensorVal = t;
                publishAmbientTemperature();
            }
            t=pGy->readObjectTempC();
            if (fastIR) {
                if (t!=temperatureIRSensorVal) {
                    temperatureIRSensorVal = t;
                    publishIRTemperature();
                }
            } else {
                if (temperatureIRSensor.filter(&t)) {
                    temperatureIRSensorVal = t;
                    publishIRTemperature();
                }
            }
        } else {
            pSched->publish(name+"/sensor/result", "hardware not initialized");
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/ir_temperature/get") {
            publishIRTemperature();
        }
        if (topic == name + "/sensor/ambient_temperature/get") {
            publishAmbientTemperature();
        }
    };
};  // Gy906

}  // namespace ustd
