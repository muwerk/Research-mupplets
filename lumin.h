// lumin.h
// Substantial parts of the code are taken from Adafruit's example
// sensorapi.h included with the TSL2561 library by Adafruit.
#pragma once

#include "scheduler.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

namespace ustd {
class Lumin {
  public:
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t port;
    double luminvalue = 0.0;
    double unitLuminvalue = 0.0;
    double maxLuminLux = 800.0;
    ustd::sensorprocessor luminsens = ustd::sensorprocessor(20, 600, 5.0);
    Adafruit_TSL2561_Unified *pTsl;
    bool bActive = false;
    tsl2561Gain_t tGain = TSL2561_GAIN_1X;
    tsl2561IntegrationTime_t tSpeed = TSL2561_INTEGRATIONTIME_101MS;
    bool bAutogain = false;
    double amp = 1.0;

    Lumin(String _name, uint8_t _port, String _gain = "16x",
          String _speed = "fast", double _amp = 1.0) {
        name = _name;
        port = _port;
        amp = _amp;
        if (_gain == "auto")
            bAutogain = true;
        else {
            if (_gain = "16x") {
                tGain = TSL2561_GAIN_16X;
            } else if (_gain = "1x") {
                tGain = TSL2561_GAIN_1X;
            } else {
#ifdef USE_SERIAL_DBG
                Serial.println("Illegal gain mode, reverting to gain 1x");
#endif
            }
        }
        if (_speed == "fast")
            tSpeed = TSL2561_INTEGRATIONTIME_13MS;
        else if (_speed == "medium")
            tSpeed = TSL2561_INTEGRATIONTIME_101MS;
        else if (_speed == "long")
            tSpeed = TSL2561_INTEGRATIONTIME_402MS;
        else {
#ifdef USE_SERIAL_DBG
            Serial.println("Illegal speed mode, reverting to medium");
#endif
        }
    }

    ~Lumin() {
    }

    double calcLumin() {
        double val = 0.0;
        /* Get a new sensor event */
        sensors_event_t event;
        pTsl->getEvent(&event);

        /* Display the results (light is measured in lux) */
        if (event.light) {
            val = event.light;
#ifdef USE_SERIAL_DBG
            Serial.print(event.light);
            Serial.println(" lux");
#endif
        } else {
            /* If event.light = 0 lux the sensor is probably saturated
               and no reliable data could be generated! */
#ifdef USE_SERIAL_DBG
            Serial.println("Luminosity-sensor overload");
#endif
        }
        return val;
    }

    double getLuminosity() {
        return luminvalue;
    }

    double getUnitLuminosity() {
        return unitLuminvalue;
    }

    // Taken from Adafruit's sensorapi.ino example
    void displaySensorDetails(void) {
#ifdef USE_SERIAL_DBG
        sensor_t sensor;
        pTsl->getSensor(&sensor);
        Serial.println("------------------------------------");
        Serial.print("Sensor:       ");
        Serial.println(sensor.name);
        Serial.print("Driver Ver:   ");
        Serial.println(sensor.version);
        Serial.print("Unique ID:    ");
        Serial.println(sensor.sensor_id);
        Serial.print("Max Value:    ");
        Serial.print(sensor.max_value);
        Serial.println(" lux");
        Serial.print("Min Value:    ");
        Serial.print(sensor.min_value);
        Serial.println(" lux");
        Serial.print("Resolution:   ");
        Serial.print(sensor.resolution);
        Serial.println(" lux");
        Serial.println("------------------------------------");
        Serial.println("");
#endif
    }

    // Taken from Adafruit's sensorapi.ino example
    void configureSensor(void) {
        if (bAutogain) {
            /* Auto-gain ... switches automatically between 1x and 16x */
            pTsl->enableAutoRange(true);
        } else {
            /* You can also manually set the gain or enable auto-gain support */
            // pTsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright
            // light to avoid sensor saturation */
            /* // 16x gain ... use in low light to boost sensitivity */
            pTsl->setGain(tGain);
        }

        pTsl->setIntegrationTime(tSpeed);
        /* Changing the integration time gives you better sensor resolution
         * (402ms = 16-bit data) */
        // pTsl->setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium
        // resolution and speed   */
        // pTsl->setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit
        // data but slowest conversions */
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        pTsl = new Adafruit_TSL2561_Unified(port, 12345);

        if (!pTsl->begin()) {
/* There was a problem detecting the TSL2561 ... check your
 * connections */
#ifdef USE_SERIAL_DBG
            Serial.print("Ooops, no TSL2561 detected ... Check your wiring or "
                         "I2C ADDR!");
#endif
        } else {
            // give a c++11 lambda as callback scheduler task registration of
            // this.loop():
            /* std::function<void()> */
            auto ft = [=]() { this->loop(); };
            tID = pSched->add(ft, name, 300000);

            /* std::function<void(String, String, String)> */
            auto fnall =
                [=](String topic, String msg, String originator) {
                    this->subsMsg(topic, msg, originator);
                };
            pSched->subscribe(tID, name + "/luminosity/#", fnall);
            pSched->subscribe(tID, name + "/unitluminosity/#", fnall);
            bActive = true;
        }
    }

    void publishLumin() {
        char buf[32];
        sprintf(buf, "%4.0f", luminvalue);
        pSched->publish(name + "/luminosity", buf);
        sprintf(buf, "%5.3f", unitLuminvalue);
        pSched->publish(name + "/unitluminosity", buf);
    }

    void loop() {
        if (bActive) {
            double val = calcLumin() * amp;
            if (luminsens.filter(&val)) {
                luminvalue = val;
                unitLuminvalue = val / maxLuminLux;
                if (unitLuminvalue > 1.0)
                    unitLuminvalue = 1.0;
                publishLumin();
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/luminosity/get" ||
            topic == name + "/unitluminosity/get") {
            publishLumin();
        }
    };
};  // Lumin

}  // namespace ustd
