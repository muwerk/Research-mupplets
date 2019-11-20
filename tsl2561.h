// tsl2561.h
// Substantial parts of the code are taken from Adafruit's example
// sensorapi.h included with the TSL2561 library by Adafruit.
#pragma once

#include "scheduler.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

namespace ustd {
class Illuminance {
  public:
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t port;
    double luxvalue = 0.0;
    double unitIlluminanceValue = 0.0;
    double maxLux = 800.0;
    ustd::sensorprocessor illuminanceSensor = ustd::sensorprocessor(20, 600, 5.0);
    Adafruit_TSL2561_Unified *pTsl;
    bool bActive = false;
    tsl2561Gain_t tGain = TSL2561_GAIN_1X;
    tsl2561IntegrationTime_t tSpeed = TSL2561_INTEGRATIONTIME_101MS;
    bool bAutogain = false;
    double amp = 1.0;

    Illuminance(String _name, uint8_t _port, String _gain = "16x",
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

    ~Illuminance() {
    }

    double calcLux() {
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
            Serial.println("TSL2561 illuminance-sensor overload");
#endif
        }
        return val;
    }

    double getLux() {
        return luxvalue;
    }

    double getUnitIlluminance() {
        return unitIlluminanceValue;
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
            pSched->subscribe(tID, name + "/sensor/illuminance/#", fnall);
            pSched->subscribe(tID, name + "/sensor/unitilluminance/#", fnall);
            bActive = true;
        }
    }

    void publishIlluminance() {
        char buf[32];
        sprintf(buf, "%4.0f", luxvalue);
        pSched->publish(name + "/sensor/illuminance", buf);
        sprintf(buf, "%5.3f", unitIlluminanceValue);
        pSched->publish(name + "/sensor/unitilluminance", buf);
    }

    void loop() {
        if (bActive) {
            double val = calcLux() * amp;
            if (illuminanceSensor.filter(&val)) {
                luxvalue = val;
                unitIlluminanceValue = val / maxLux;
                if (unitIlluminanceValue > 1.0)
                    unitIlluminanceValue = 1.0;
                publishIlluminance();
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/illuminance/get" ||
            topic == name + "/sensor/unitilluminance/get") {
            publishIlluminance();
        }
    };
};  // Illuminance

}  // namespace ustd
