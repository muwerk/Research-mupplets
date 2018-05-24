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
    String name;
    uint8_t port;
    double luminvalue = 0.0;
    double unitLuminvalue = 0.0;
    double maxLuminLux = 600.0;
    ustd::sensorprocessor luminsens = ustd::sensorprocessor(10, 600, 10.0);
    Adafruit_TSL2561_Unified *pTsl;
    bool bActive = false;

    Lumin(String name, uint8_t port) : name(name), port(port) {
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
            Serial.println("Sensor overload");
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
        /* You can also manually set the gain or enable auto-gain support */
        // tsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light
        // to avoid sensor saturation */ tsl.setGain(TSL2561_GAIN_16X);     /*
        // 16x gain ... use in low light to boost sensitivity */
        pTsl->enableAutoRange(
            true); /* Auto-gain ... switches automatically between 1x and 16x */

        /* Changing the integration time gives you better sensor resolution
         * (402ms = 16-bit data) */
        pTsl->setIntegrationTime(
            TSL2561_INTEGRATIONTIME_13MS); /* fast but low resolution */
        // pTsl->setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium
        // resolution and speed   */
        // pTsl->setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit
        // data but slowest conversions */

#ifdef USE_SERIAL_DBG
        /* Update these values depending on what you've set above! */
        Serial.println("------------------------------------");
        Serial.print("Gain:         ");
        Serial.println("Auto");
        Serial.print("Timing:       ");
        Serial.println("13 ms");
        Serial.println("------------------------------------");
#endif
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
            std::function<void()> ft = [=]() { this->loop(); };
            pSched->add(ft, 250000);

            std::function<void(String, String, String)> fnall =
                [=](String topic, String msg, String originator) {
                    this->subsMsg(topic, msg, originator);
                };
            pSched->subscribe(name + "/luminosity/#", fnall);
            pSched->subscribe(name + "/unitluminosity/#", fnall);
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
            double val = calcLumin();
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
