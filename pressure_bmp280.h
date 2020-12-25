// pressure.h
// adapted from:
// https://github.com/adafruit/Adafruit_BMP280_Library/blob/master/examples/bmp280_sensortest/bmp280_sensortest.ino
#pragma once

#include "scheduler.h"
#include "sensors.h"

#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <home_assistant.h>

namespace ustd {
class PressureBmp280 {
  public:
    String PRESSURE_VERSION = "0.1.0";
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t i2c_addr;
    uint8_t chip_id;
    double temperatureSensorVal;
    double pressureSensorVal;
    bool bActive = false;
    ustd::sensorprocessor temperatureSensor = ustd::sensorprocessor(4, 600, 0.1);
    ustd::sensorprocessor pressureSensor = ustd::sensorprocessor(4, 600, 1.0);
    Adafruit_BMP280 *pPressure;
    Adafruit_Sensor *bmp_temp;
    Adafruit_Sensor *bmp_pressure;
    String errmsg;
#ifdef __ESP__
    HomeAssistant *pHA;
#endif

    // i2c_addr: usually 0x76 or 0x77, chip_id should be 0x58 for bmp280.
    PressureBmp280(String name, uint8_t i2c_addr = 0x76, uint8_t chip_id = 0x58)
        : name(name), i2c_addr(i2c_addr), chip_id(chip_id) {
        pPressure = new Adafruit_BMP280();
    }

    ~PressureBmp280() {
    }

    double getTemp() {
        return temperatureSensorVal;
    }

    double getPressure() {
        return pressureSensorVal;
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        if (pPressure->begin(i2c_addr, chip_id)) {
            bActive = true;
            /* Default settings from datasheet. */
            pPressure->setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                                   Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                                   Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                                   Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                                   Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
            bmp_temp = pPressure->getTemperatureSensor();
            bmp_pressure = pPressure->getPressureSensor();
        } else {
            errmsg = "Could not find a valid BMP280 sensor, check wiring!";
#ifdef USE_SERIAL_DBG
            Serial.println(errmsg);
#endif
        }

        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 5000000);

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/sensor/temperature/get", fnall);
        pSched->subscribe(tID, name + "/sensor/pressure/get", fnall);
    }

#ifdef __ESP__
    void registerHomeAssistant(String homeAssistantFriendlyName, String projectName = "",
                               String homeAssistantDiscoveryPrefix = "homeassistant") {
        pHA = new HomeAssistant(name, tID, homeAssistantFriendlyName, projectName, PRESSURE_VERSION,
                                homeAssistantDiscoveryPrefix);
        pHA->addSensor("temperature", "Temperature", "\\u00B0C", "temperature", "mdi:thermometer");
        pHA->addSensor("pressure", "Pressure", "hPa", "pressure", "mdi:altimeter");
        pHA->begin(pSched);
    }
#endif

    void publishPressure() {
        char buf[32];
        pSched->publish(name + "/sensor/result", "OK");
        sprintf(buf, "%5.1f", pressureSensorVal);
        pSched->publish(name + "/sensor/pressure", buf);
    }

    void publishTemperature() {
        char buf[32];
        pSched->publish(name + "/sensor/result", "OK");
        sprintf(buf, "%5.1f", temperatureSensorVal);
        pSched->publish(name + "/sensor/temperature", buf);
    }

    void loop() {
        if (bActive) {
            sensors_event_t temp_event, pressure_event;
            bmp_temp->getEvent(&temp_event);
            bmp_pressure->getEvent(&pressure_event);

            double p, t;
            /* Display atmospheric pressue in hPa */
            p = pressure_event.pressure;  // hPa
            t = temp_event.temperature;   // C

            if (temperatureSensor.filter(&t)) {
                temperatureSensorVal = t;
                publishTemperature();
            }
            if (pressureSensor.filter(&p)) {
                pressureSensorVal = p;
                publishPressure();
            }
        } else {
            pSched->publish(name + "/sensor/result", errmsg);
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/temperature/get") {
            publishTemperature();
        }
        if (topic == name + "/sensor/pressure/get") {
            publishPressure();
        }
    };
};  // Pressure

}  // namespace ustd
