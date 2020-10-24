// airq_bme680.h
// adapted from:
// https://github.com/adafruit/Adafruit_BME680/blob/master/examples/bme680test/bme680test.ino
#pragma once

#include "scheduler.h"

#include "sensors.h"

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"

#include "home_assistant.h"


namespace ustd {
class AirQualityBme680 {
  public:
    String AIRQUALITY_VERSION="0.1.0";
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t i2caddr;
    double kOhmsVal=0.0, temperatureVal=0.0, humidityVal=0.0, pressureVal=0.0;
    time_t startTime=0;
    bool bStartup=false;
    bool bActive = false;
    String errmsg="";
    ustd::sensorprocessor kOhmsGas = ustd::sensorprocessor(4, 30, 1.0);
    ustd::sensorprocessor temperature = ustd::sensorprocessor(4, 30, 0.1);
    ustd::sensorprocessor humidity = ustd::sensorprocessor(4, 30, 0.1);
    ustd::sensorprocessor pressure = ustd::sensorprocessor(4, 30, 0.01);
    Adafruit_BME680 *pAirQuality;
    #ifdef __ESP__
    HomeAssistant *pHA;
    #endif

    AirQualityBme680(String name, uint8_t i2caddr = 0)
        : name(name), i2caddr(i2caddr) {
        pAirQuality = new Adafruit_BME680();
    }

    ~AirQualityBme680() {
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

    double getkOhmsGas() {
        return kOhmsVal;
    }


    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        if (!pAirQuality->begin()) {
            errmsg="Could not find a valid BME680 sensor, check wiring!";
            #ifdef USE_SERIAL_DBG
            Serial.println(errmsg);
            #endif
        } else {
            // Set up oversampling and filter initialization
            pAirQuality->setTemperatureOversampling(BME680_OS_8X);
            pAirQuality->setHumidityOversampling(BME680_OS_2X);
            pAirQuality->setPressureOversampling(BME680_OS_4X);
            pAirQuality->setIIRFilterSize(BME680_FILTER_SIZE_3);
            pAirQuality->setGasHeater(320, 150); // 320*C for 150 ms

            bActive=true;
        }
        
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 2000000); // every 2sec

        auto fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/sensor/#", fnall);
    }

    #ifdef __ESP__
    void registerHomeAssistant(String homeAssistantFriendlyName, String projectName="", String homeAssistantDiscoveryPrefix="homeassistant") {
        pHA=new HomeAssistant(name, tID, homeAssistantFriendlyName, projectName, AIRQUALITY_VERSION, homeAssistantDiscoveryPrefix);
        pHA->addSensor("temperature", "Temperature", "\\u00B0C","temperature","mdi:thermometer");
        pHA->addSensor("humidity", "Humidity", "%","humidity","mdi:water-percent");
        pHA->addSensor("pressure", "Pressure", "hPa","pressure","mdi:altimeter");
        pHA->addSensor("kohmsgas", "kOhmsGas", "k\\u003a9","None","mdi:air-filter");
        pHA->begin(pSched);
    }
    #endif

    void publishTemperature() {
        if (bActive && !bStartup) {
            char buf[32];
            sprintf(buf, "%5.1f", temperatureVal);
            pSched->publish(name + "/sensor/temperature", buf);
        }
    }

    void publishHumidity() {
        if (bActive && !bStartup) {
            char buf[32];
            sprintf(buf, "%5.1f", humidityVal);
            pSched->publish(name + "/sensor/humidity", buf);
        }
    }

    void publishPressure() {
        if (bActive && !bStartup) {
            char buf[32];
            sprintf(buf, "%5.1f", pressureVal);
            pSched->publish(name + "/sensor/pressure", buf);
        }
    }

    void publishkOhmsGas() {
        if (bActive && !bStartup) {
            char buf[32];
            sprintf(buf, "%5.1f", kOhmsVal);
            pSched->publish(name + "/sensor/kohmsgas", buf);
        }
    }

    void loop() {
        #ifdef USE_SERIAL_DBG
        Serial.println("BME680 enter loop");
        #endif
        if (startTime<100000) startTime=time(NULL); // NTP data available.
        if (bActive) {
            #if defined(__ESP__) && !defined(__ESP32__)
            ESP.wdtDisable();
            #endif
            if (pAirQuality->performReading()) {
                #if defined(__ESP__) && !defined(__ESP32__)
                ESP.wdtEnable(WDTO_8S);
                #endif
                double t,h,p,k;
#ifdef USE_SERIAL_DBG
                Serial.println("AirQualityBme680 sensor data available");
#endif
                t=pAirQuality->temperature;
                h=pAirQuality->humidity;
                p = pAirQuality->pressure/100.0;
                k=pAirQuality->gas_resistance/1000.0;
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
                if (kOhmsGas.filter(&k)) {
                    kOhmsVal = k;
                    publishkOhmsGas();
                }
                #ifdef USE_SERIAL_DBG
                Serial.println(t);
                Serial.println(h);
                Serial.println(p);
                Serial.println(k);
                #endif
            } else {
                #if defined(__ESP__) && !defined(__ESP32__)
                ESP.wdtEnable(WDTO_8S);
                #endif
#ifdef USE_SERIAL_DBG
                Serial.println("AirQualityBme680 sensor no data available");
#endif
            }
        } else {
#ifdef USE_SERIAL_DBG
            Serial.println("AirQualityBme680 sensor not active.");
#endif
            if (errmsg!="") {
                pSched->publish(name+"/error",errmsg);
                errmsg="";
            }
        }
        #ifdef USE_SERIAL_DBG
        Serial.println("BME680 exit loop.");
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
        if (topic == name + "/sensor/kohmsgas/get") {
            publishkOhmsGas();
        }
    };
};  // AirQuality

}  // namespace ustd
