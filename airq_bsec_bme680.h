// airq_bse_bme680.h
// adapted from:
// https://github.com/BoschSensortec/BSEC-Arduino-library/tree/master/examples/basic

// WARNING: CHECK PROPRIETARY LICENCE OF BOSCH LIBRARY!

#pragma once

#include "scheduler.h"

#include "sensors.h"

//#include <Wire.h>
#include "bsec.h"  // taints license.

#include "home_assistant.h"
/*
    output += ", " + String(iaqSensor.rawTemperature);
    output += ", " + String(iaqSensor.pressure);
    output += ", " + String(iaqSensor.rawHumidity);
    output += ", " + String(iaqSensor.gasResistance);
    output += ", " + String(iaqSensor.iaq);
    output += ", " + String(iaqSensor.iaqAccuracy);
    output += ", " + String(iaqSensor.temperature);
    output += ", " + String(iaqSensor.humidity);
    output += ", " + String(iaqSensor.staticIaq);
    output += ", " + String(iaqSensor.co2Equivalent);
    output += ", " + String(iaqSensor.breathVocEquivalent);
    */

namespace ustd {

class AirQualityBsecBme680 {
  public:
    String AIRQUALITY_VERSION = "0.1.0";
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t i2caddr;
    double rawTemperature = 0.0, temperature = 0.0, pressure = 0.0, rawHumidity = 0.0,
           humidity = 0.0, gasResistance = 0.0, iaq = 0.0, iaqAccuracy = 0.0, staticIaq = 0.0,
           co2 = 0.0, voc = 0.0;
    time_t startTime = 0;
    bool bStartup = false;
    bool bActive = false;
    String errmsg = "";
    ustd::sensorprocessor rawTemperatureSensor = ustd::sensorprocessor(4, 30, 0.1);
    ustd::sensorprocessor temperatureSensor = ustd::sensorprocessor(4, 30, 0.1);
    ustd::sensorprocessor rawHumiditySensor = ustd::sensorprocessor(4, 30, 0.1);
    ustd::sensorprocessor humiditySensor = ustd::sensorprocessor(4, 30, 0.1);
    ustd::sensorprocessor pressureSensor = ustd::sensorprocessor(4, 30, 0.01);
    ustd::sensorprocessor gasResistanceSensor = ustd::sensorprocessor(4, 30, 0.01);
    ustd::sensorprocessor iaqSensor = ustd::sensorprocessor(4, 30, 0.01);
    ustd::sensorprocessor iaqAccuracySensor = ustd::sensorprocessor(4, 30, 0.01);
    ustd::sensorprocessor staticIaqSensor = ustd::sensorprocessor(4, 30, 0.01);
    ustd::sensorprocessor co2Sensor = ustd::sensorprocessor(4, 30, 0.01);
    ustd::sensorprocessor vocSensor = ustd::sensorprocessor(4, 30, 0.01);

    Bsec *pAirQuality;
#ifdef __ESP__
    HomeAssistant *pHA;
#endif

    AirQualityBsecBme680(String name, uint8_t i2cAddress = BME680_I2C_ADDR_PRIMARY)
        : name(name), i2caddr(i2cAddress) {
        pAirQuality = new Bsec();
    }

    ~AirQualityBsecBme680() {
    }

    double getTemperature() {
        return temperature;
    }

    double getHumidity() {
        return humidity;
    }

    double getRawTemperature() {
        return rawTemperature;
    }

    double getRawHumidity() {
        return rawHumidity;
    }

    double getPressure() {
        return pressure;
    }

    double getGasResistance() {
        return gasResistance;
    }

    double getIaq() {
        return iaq;
    }

    double getIaqAccuracy() {
        return iaqAccuracy;
    }

    double getStaticIaq() {
        return staticIaq;
    }

    double getCo2() {
        return co2;
    }

    double getVoc() {
        return voc;
    }

    bool checkIaqSensorStatus(void) {
        if (pAirQuality->status != BSEC_OK) {
            if (pAirQuality->status < BSEC_OK) {
                errmsg = "BSEC error code: " + String(pAirQuality->status);
#ifdef USE_SERIAL_DBG
                Serial.println(errmsg);
#endif
                return false;
            } else {
                errmsg = "BSEC warning code: " + String(pAirQuality->status);
#ifdef USE_SERIAL_DBG
                Serial.println(errmsg);
#endif
            }
        }

        if (pAirQuality->bme680Status != BME680_OK) {
            if (pAirQuality->bme680Status < BME680_OK) {
                errmsg = "BME680 error code: " + String(pAirQuality->bme680Status);
#ifdef USE_SERIAL_DBG
                Serial.println(errmsg);
#endif
                return false;
            } else {
                errmsg = "BME680 warning code: " + String(pAirQuality->bme680Status);
#ifdef USE_SERIAL_DBG
                Serial.println(errmsg);
#endif
            }
        }
        return true;
    }
    bsec_virtual_sensor_t sensorList[10] = {
        BSEC_OUTPUT_RAW_TEMPERATURE,
        BSEC_OUTPUT_RAW_PRESSURE,
        BSEC_OUTPUT_RAW_HUMIDITY,
        BSEC_OUTPUT_RAW_GAS,
        BSEC_OUTPUT_IAQ,
        BSEC_OUTPUT_STATIC_IAQ,
        BSEC_OUTPUT_CO2_EQUIVALENT,
        BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    };

    void begin(Scheduler *_pSched) {
        pSched = _pSched;
        // wire = _wire;

        pAirQuality->begin(i2caddr, Wire);
#ifdef USE_SERIAL_DBG
        Serial.println("BSEC library version " + String(pAirQuality->version.major) + "." +
                       String(pAirQuality->version.minor) + "." +
                       String(pAirQuality->version.major_bugfix) + "." +
                       String(pAirQuality->version.minor_bugfix));
#endif

        if (checkIaqSensorStatus()) {
            bActive = true;
#ifdef USE_SERIAL_DBG
            Serial.println("Found BME680");
#endif
            pAirQuality->updateSubscription(sensorList, 10, BSEC_SAMPLE_RATE_LP);
            if (!checkIaqSensorStatus()) {
                bActive = false;
#ifdef USE_SERIAL_DBG
                Serial.println("BME680 subscription failed!");
#endif
            }
        } else {
#ifdef USE_SERIAL_DBG
            Serial.println("BME680 init failed!");
#endif
            bActive = false;
        }

        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 3000000);  // 3 seconds timing required.

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
        pHA->addSensor("co2", "CO2", "ppm", "None", "mdi:air-filter");
        pHA->addSensor("voc", "VOC", "ppb", "None", "mdi:air-filter");
        pHA->addSensor("iaq", "iaq", "0-500", "None", "mdi:air-filter");
        pHA->begin(pSched);
    }
#endif

    void publishRawTemperature() {
        if (bActive && !bStartup) {
            char buf[32];
            sprintf(buf, "%5.1f", rawTemperature);
            pSched->publish(name + "/sensor/rawtemperature", buf);
        }
    }

    void publishRawHumidity() {
        if (bActive && !bStartup) {
            char buf[32];
            sprintf(buf, "%5.1f", rawHumidity);
            pSched->publish(name + "/sensor/rawhumidity", buf);
        }
    }

    void publishTemperature() {
        if (bActive && !bStartup) {
            char buf[32];
            sprintf(buf, "%5.1f", temperature);
            pSched->publish(name + "/sensor/temperature", buf);
        }
    }

    void publishHumidity() {
        if (bActive && !bStartup) {
            char buf[32];
            sprintf(buf, "%5.1f", humidity);
            pSched->publish(name + "/sensor/humidity", buf);
        }
    }

    void publishPressure() {
        if (bActive && !bStartup) {
            char buf[32];
            sprintf(buf, "%5.1f", pressure);
            pSched->publish(name + "/sensor/pressure", buf);
        }
    }

    void publishCO2() {
        if (bActive && !bStartup) {
            char buf[32];
            sprintf(buf, "%5.1f", co2);
            pSched->publish(name + "/sensor/co2", buf);
        }
    }

    void publishVOC() {
        if (bActive && !bStartup) {
            char buf[32];
            sprintf(buf, "%5.1f", voc);
            pSched->publish(name + "/sensor/voc", buf);
        }
    }

    void publishIaq() {
        if (bActive && !bStartup) {
            char buf[32];
            sprintf(buf, "%5.1f", iaq);
            pSched->publish(name + "/sensor/iaq", buf);
        }
    }

    void publishGasResistance() {
        if (bActive && !bStartup) {
            char buf[32];
            sprintf(buf, "%5.1f", gasResistance);
            pSched->publish(name + "/sensor/kohmsgas", buf);
        }
    }

    void publishStaticIaq() {
        if (bActive && !bStartup) {
            char buf[32];
            sprintf(buf, "%5.1f", staticIaq);
            pSched->publish(name + "/sensor/staticiaq", buf);
        }
    }

    void publishIaqAccuracy() {
        if (bActive && !bStartup) {
            char buf[32];
            sprintf(buf, "%5.1f", iaqAccuracy);
            pSched->publish(name + "/sensor/iaqaccuracy", buf);
        }
    }

    void loop() {
#ifdef USE_SERIAL_DBG
        Serial.println("BSECBME680 enter loop");
#endif
        // if (startTime<100000) startTime=time(NULL); // NTP data available.
        if (bActive) {
#ifdef USE_SERIAL_DBG
            Serial.println("BSECBME680 active, enter run()");
#endif
            if (pAirQuality->run()) {  // If new data is available
                double t, h, rt, rh, p, k, c, v, ia, iaqacc, siaq;
#ifdef USE_SERIAL_DBG
                Serial.println("AirQualityBseBme680 sensor data available");
#endif
                rt = pAirQuality->rawTemperature;
                t = pAirQuality->temperature;
                rh = pAirQuality->rawHumidity;
                h = pAirQuality->humidity;
                p = pAirQuality->pressure / 100.0;
                k = pAirQuality->gasResistance;
                ia = pAirQuality->iaq;
                iaqacc = pAirQuality->iaqAccuracy;
                siaq = pAirQuality->staticIaq;
                c = pAirQuality->co2Equivalent;
                v = pAirQuality->breathVocEquivalent;

                if (rawTemperatureSensor.filter(&rt)) {
                    rawTemperature = rt;
                    publishRawTemperature();
                }
                if (rawHumiditySensor.filter(&rh)) {
                    rawHumidity = rh;
                    publishRawHumidity();
                }
                if (temperatureSensor.filter(&t)) {
                    temperature = t;
                    publishTemperature();
                }
                if (humiditySensor.filter(&h)) {
                    humidity = h;
                    publishHumidity();
                }
                if (pressureSensor.filter(&p)) {
                    pressure = p;
                    publishPressure();
                }
                if (gasResistanceSensor.filter(&k)) {
                    gasResistance = k;
                    publishGasResistance();
                }
                if (iaqSensor.filter(&ia)) {
                    iaq = ia;
                    publishIaq();
                }
                if (staticIaqSensor.filter(&siaq)) {
                    staticIaq = siaq;
                    publishStaticIaq();
                }
                if (iaqAccuracySensor.filter(&iaqacc)) {
                    iaqAccuracy = iaqacc;
                    publishIaqAccuracy();
                }
                if (vocSensor.filter(&v)) {
                    voc = v;
                    publishVOC();
                }
                if (co2Sensor.filter(&c)) {
                    co2 = c;
                    publishCO2();
                }
#ifdef USE_SERIAL_DBG
                Serial.println(t);
                Serial.println(h);
                Serial.println(p);
                Serial.println(co2);
                Serial.println(voc);
#endif
            } else {
#ifdef USE_SERIAL_DBG
                Serial.println("AirQualityBsecBme680 sensor no data available");
#endif
                checkIaqSensorStatus();
            }
        } else {
#ifdef USE_SERIAL_DBG
            Serial.println("AirQualityBsecBme680 sensor not active.");
#endif
            if (errmsg != "") {
                pSched->publish(name + "/sensor/result", errmsg);
                errmsg = "";
            }
        }
#ifdef USE_SERIAL_DBG
        Serial.println("BSECBME680 exit loop.");
#endif
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/rawtemperature/get") {
            publishRawTemperature();
        }
        if (topic == name + "/sensor/rawhumidity/get") {
            publishRawHumidity();
        }
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
            publishGasResistance();
        }
        if (topic == name + "/sensor/iaq/get") {
            publishIaqAccuracy();
        }
        if (topic == name + "/sensor/iaqaccuracy/get") {
            publishIaqAccuracy();
        }
        if (topic == name + "/sensor/staticiaq/get") {
            publishStaticIaq();
        }
        if (topic == name + "/sensor/voc/get") {
            publishVOC();
        }
        if (topic == name + "/sensor/co2/get") {
            publishCO2();
        }
    };
};  // AirQuality

}  // namespace ustd
