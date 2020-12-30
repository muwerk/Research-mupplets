// airq_bme280.h
// adapted from:
// https://github.com/adafruit/Adafruit_BME280_Library/blob/master/examples/bme280_unified/bme280_unified.ino
#pragma once

#include "scheduler.h"

#include "sensors.h"

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include "home_assistant.h"

namespace ustd {
class AirQualityBme280 {
  public:
    enum SampleMode { FAST, MEDIUM, LONGTERM };
    String AIRQUALITY_VERSION = "0.1.0";
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t i2c_addr;
    uint8_t sampleMode;
    // uint8_t i2caddr;
    double temperatureVal = 0.0, humidityVal = 0.0, pressureVal = 0.0;
    time_t startTime = 0;
    bool bStartup = false;
    bool bActive = false;
    String errmsg = "";
    ustd::sensorprocessor temperature = ustd::sensorprocessor(4, 30, 0.1);
    ustd::sensorprocessor humidity = ustd::sensorprocessor(4, 30, 0.1);
    ustd::sensorprocessor pressure = ustd::sensorprocessor(4, 30, 0.01);
    Adafruit_BME280 *pAirQuality;
    Adafruit_Sensor *bme_temp;
    Adafruit_Sensor *bme_pressure;
    Adafruit_Sensor *bme_humidity;

#ifdef __ESP__
    HomeAssistant *pHA;
#endif

    AirQualityBme280(String name, uint8_t i2c_addr = BME280_ADDRESS,
                     uint8_t sampleMode = SampleMode::LONGTERM)  // i2c_addr: usually 0x77 or 0x76
        : name(name), i2c_addr(i2c_addr), sampleMode(sampleMode) {
        pAirQuality = new Adafruit_BME280();  // seems to be on i2c port 0x77 always
        setSampleMode(sampleMode, true);
    }

    ~AirQualityBme280() {
    }

    void setSampleMode(uint8_t mode, bool silent = false) {
        switch (mode) {
        case SampleMode::FAST:  // Fast
            sampleMode = SampleMode::FAST;
            temperature.smoothInterval = 1;
            temperature.pollTimeSec = 15;
            temperature.eps = 0.1;
            temperature.reset();
            humidity.smoothInterval = 1;
            humidity.pollTimeSec = 15;
            humidity.eps = 0.1;
            humidity.reset();
            pressure.smoothInterval = 1;
            pressure.pollTimeSec = 15;
            pressure.eps = 0.01;
            pressure.reset();
            break;
        case SampleMode::MEDIUM:  // Medium
            sampleMode = SampleMode::MEDIUM;
            temperature.smoothInterval = 4;
            temperature.pollTimeSec = 180;
            temperature.eps = 0.2;
            temperature.reset();
            humidity.smoothInterval = 4;
            humidity.pollTimeSec = 180;
            humidity.eps = 0.5;
            humidity.reset();
            pressure.smoothInterval = 8;
            pressure.pollTimeSec = 180;
            pressure.eps = 0.1;
            pressure.reset();
            break;
        case SampleMode::LONGTERM:
        default:  // long-term
            sampleMode = SampleMode::LONGTERM;
            temperature.smoothInterval = 16;
            temperature.pollTimeSec = 600;
            temperature.eps = 0.5;
            temperature.reset();
            humidity.smoothInterval = 16;
            humidity.pollTimeSec = 600;
            humidity.eps = 0.5;
            humidity.reset();
            pressure.smoothInterval = 16;
            pressure.pollTimeSec = 600;
            pressure.eps = 1.0;
            pressure.reset();
            break;
        }
        if (!silent)
            publishSampleMode();
    }

    double getTemperature() {
        return temperatureVal;
    }

    double getHumidity() {
        return humidityVal;
    }

    double getPressure() {
        return pressureVal;
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        if (!pAirQuality->begin(i2c_addr)) {
            errmsg = "Could not find a valid BME280 sensor, check wiring!";
#ifdef USE_SERIAL_DBG
            Serial.println(errmsg);
#endif
        } else {

            bme_temp = pAirQuality->getTemperatureSensor();
            bme_pressure = pAirQuality->getPressureSensor();
            bme_humidity = pAirQuality->getHumiditySensor();

            bActive = true;
            pSched->publish(name + "/sensor/result", "OK");
            configureSensor();
        }

        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 2000000);  // every 2sec

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/sensor/#", fnall);
    }

#ifdef __ESP__
    void registerHomeAssistant(String homeAssistantFriendlyName, String projectName = "",
                               String homeAssistantDiscoveryPrefix = "homeassistant") {
        pHA = new HomeAssistant(name, tID, homeAssistantFriendlyName, projectName,
                                AIRQUALITY_VERSION, homeAssistantDiscoveryPrefix);
        pHA->addSensor("temperature", "Temperature", "\\u00B0C", "temperature", "mdi:thermometer");
        pHA->addSensor("humidity", "Humidity", "%", "humidity", "mdi:water-percent");
        pHA->addSensor("pressure", "Pressure", "hPa", "pressure", "mdi:altimeter");
        pHA->begin(pSched);
    }
#endif

    void configureSensor() {
        pAirQuality->setSampling(Adafruit_BME280::MODE_FORCED,
                                 Adafruit_BME280::SAMPLING_X1,      // temperature
                                 Adafruit_BME280::SAMPLING_X1,      // pressure
                                 Adafruit_BME280::SAMPLING_X1,      // humidity
                                 Adafruit_BME280::FILTER_OFF,       // no exponential sample filter
                                 Adafruit_BME280::STANDBY_MS_0_5);  // minimal standby
    }

    void publishTemperature() {
        if (bActive && !bStartup) {
            char buf[32];
            sprintf(buf, "%5.1f", temperatureVal);
            pSched->publish(name + "/sensor/result", "OK");
            pSched->publish(name + "/sensor/temperature", buf);
        }
    }

    void publishHumidity() {
        if (bActive && !bStartup) {
            char buf[32];
            sprintf(buf, "%5.1f", humidityVal);
            pSched->publish(name + "/sensor/result", "OK");
            pSched->publish(name + "/sensor/humidity", buf);
        }
    }

    void publishPressure() {
        if (bActive && !bStartup) {
            char buf[32];
            sprintf(buf, "%5.1f", pressureVal);
            pSched->publish(name + "/sensor/result", "OK");
            pSched->publish(name + "/sensor/pressure", buf);
        }
    }

    void publishSampleMode() {
        switch (sampleMode) {
        case SampleMode::FAST:
            pSched->publish(name + "/sensor/mode", "FAST");
            break;
        case SampleMode::MEDIUM:
            pSched->publish(name + "/sensor/mode", "MEDIUM");
            break;
        case SampleMode::LONGTERM:
            pSched->publish(name + "/sensor/mode", "LONGTERM");
            break;
        default:
            pSched->publish(name + "/sensor/mode", "ERROR");
            break;
        }
    }

    void loop() {
#ifdef USE_SERIAL_DBG
        Serial.println("BME280 enter loop");
#endif
        if (startTime < 100000)
            startTime = time(NULL);  // NTP data available.
        if (bActive) {
            double t, h, p;
            sensors_event_t temp_event, pressure_event, humidity_event;
            bme_temp->getEvent(&temp_event);
            bme_pressure->getEvent(&pressure_event);
            bme_humidity->getEvent(&humidity_event);

            Serial.print(temp_event.temperature);
            Serial.print(humidity_event.relative_humidity);
            Serial.print(pressure_event.pressure);

            t = temp_event.temperature;
            h = humidity_event.relative_humidity;
            p = pressure_event.pressure;

            configureSensor();  // reload parameters after each measurement

            if (temperature.filter(&t)) {
                temperatureVal = t;
                publishTemperature();
            }
            if (humidity.filter(&h)) {
                humidityVal = h;
                publishHumidity();
            }
            if (pressure.filter(&p)) {
                pressureVal = p;
                publishPressure();
            }
#ifdef USE_SERIAL_DBG
            Serial.println(t);
            Serial.println(h);
            Serial.println(p);
#endif
        } else {
#ifdef USE_SERIAL_DBG
            Serial.println("AirQualityBme280 sensor not active.");
#endif
            if (errmsg != "") {
                pSched->publish(name + "/sensor/result", errmsg);
                errmsg = "";
            }
        }
#ifdef USE_SERIAL_DBG
        Serial.println("BME280 exit loop.");
#endif
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/temperature/get") {
            publishTemperature();
        }
        if (topic == name + "/sensor/humidity/get") {
            publishHumidity();
        }
        if (topic == name + "/sensor/pressure/get") {
            publishPressure();
        }
        if (topic == name + "/sensor/mode/get") {
            publishSampleMode();
        }
        if (topic == name + "/sensor/mode/set") {
            if (msg == "fast" || msg == "FAST") {
                setSampleMode(SampleMode::FAST);
            } else {
                if (msg == "medium" || msg == "MEDIUM") {
                    setSampleMode(SampleMode::MEDIUM);
                } else {
                    setSampleMode(SampleMode::LONGTERM);
                }
            }
        }
    };
};  // AirQuality

}  // namespace ustd
