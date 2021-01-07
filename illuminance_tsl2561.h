// tsl2561.h
// Substantial parts of the code are taken from Adafruit's example
// sensorapi.h included with the TSL2561 library by Adafruit.
#pragma once

#include "scheduler.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

namespace ustd {
class IlluminanceTsl2561 {
    /*! Support for TSL256 light-to-digital converter that approximates human eye response */
  public:
    String TSL_VERSION = "0.2.0";
    enum SampleGainMode {
        FAST_GAINX1,
        FAST_GAINX16,
        MEDIUM_GAINX1,
        MEDIUM_GAINX16,
        PRECISE_GAINX1,
        PRECISE_GAINX16
    };
    enum FilterMode { FAST, MEDIUM, LONGTERM };
    Scheduler *pSched;
    int tID;

    String name;
    uint8_t i2c_address;
    SampleGainMode sampleGainMode;
    double amp = 1.0;

    FilterMode filterMode;
    unsigned long usSampleThread = 250000;  // default poll time in us.
    double luxvalue = 0.0;
    double unitIlluminanceValue = 0.0;
    double maxLux = 800.0;
    ustd::sensorprocessor illuminanceSensor = ustd::sensorprocessor(40, 600, 5.0);

    Adafruit_TSL2561_Unified *pTsl;
    bool bActive = false;
    tsl2561Gain_t tsl2561Gain = TSL2561_GAIN_1X;
    tsl2561IntegrationTime_t tsl2561Speed = TSL2561_INTEGRATIONTIME_101MS;
#ifdef __ESP__
    HomeAssistant *pHA;
#endif

    IlluminanceTsl2561(String name, uint8_t i2c_address,
                       SampleGainMode sampleGainMode = PRECISE_GAINX16, double amp = (double)1.0)
        : name(name), i2c_address(i2c_address), sampleGainMode(sampleGainMode), amp(amp) {
        switch (sampleGainMode) {
        case FAST_GAINX1:
            tsl2561Speed = TSL2561_INTEGRATIONTIME_13MS;
            tsl2561Gain = TSL2561_GAIN_1X;
            usSampleThread = 250000;
            setFilterMode(FAST, true);
            break;
        case FAST_GAINX16:
            tsl2561Speed = TSL2561_INTEGRATIONTIME_13MS;
            tsl2561Gain = TSL2561_GAIN_16X;
            usSampleThread = 250000;
            setFilterMode(FAST, true);
            break;
        case MEDIUM_GAINX1:
            tsl2561Speed = TSL2561_INTEGRATIONTIME_101MS;
            tsl2561Gain = TSL2561_GAIN_1X;
            usSampleThread = 500000;
            setFilterMode(MEDIUM, true);
            break;
        case MEDIUM_GAINX16:
            tsl2561Speed = TSL2561_INTEGRATIONTIME_101MS;
            tsl2561Gain = TSL2561_GAIN_16X;
            usSampleThread = 500000;
            setFilterMode(MEDIUM, true);
            break;
        case PRECISE_GAINX1:
            tsl2561Speed = TSL2561_INTEGRATIONTIME_402MS;
            tsl2561Gain = TSL2561_GAIN_1X;
            usSampleThread = 1000000;
            setFilterMode(LONGTERM, true);
            break;
        case PRECISE_GAINX16:
            tsl2561Speed = TSL2561_INTEGRATIONTIME_402MS;
            tsl2561Gain = TSL2561_GAIN_16X;
            usSampleThread = 1000000;
            setFilterMode(LONGTERM, true);
            break;
        default:
            DBG("Undefined state for TSL2561 SampleMode");
            break;
        }
    }

    ~IlluminanceTsl2561() {
    }

    void setFilterMode(FilterMode mode, bool silent = false) {
        switch (mode) {
        case FAST:
            filterMode = FAST;
            illuminanceSensor.smoothInterval = 1;
            illuminanceSensor.pollTimeSec = 15;
            illuminanceSensor.eps = 1.0;
            illuminanceSensor.reset();
            break;
        case MEDIUM:
            filterMode = MEDIUM;
            illuminanceSensor.smoothInterval = 2;
            illuminanceSensor.pollTimeSec = 300;
            illuminanceSensor.eps = 1.0;
            illuminanceSensor.reset();
            break;
        case LONGTERM:
        default:
            filterMode = LONGTERM;
            illuminanceSensor.smoothInterval = 4;
            illuminanceSensor.pollTimeSec = 600;
            illuminanceSensor.eps = 0.1;
            illuminanceSensor.reset();
            break;
        }
        if (!silent)
            publishFilterMode();
    }

    double calcLux() {
        double val = 0.0;
        /* Get a new sensor event */
        sensors_event_t event;
        pTsl->getEvent(&event);

        /* Display the results (light is measured in lux) */
        if (event.light) {
            val = event.light;
        } else {
            /* If event.light = 0 lux the sensor is probably saturated
               and no reliable data could be generated!
               Unfortunately this can't be discriminated from darkness! */
            val = 0.0;
        }
        return val;
    }

    double setMaxLux(double newMaxLux) {
        /*! setMaxLux defines the maximum lux value used to calculate unitIlluminance. For all
        values of lux>=maxLux, unitIlluminance is 1.0

        @param newMaxLux New limit for lux value, all values above generate unitIlluminance of 1.0
        @returns Bounds checked value for maxLux.
        */
        maxLux = newMaxLux;
        if (maxLux < 10.0)
            maxLux = 10.0;
        if (maxLux > 100000.0)
            maxLux = 100000.0;
        return maxLux;
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
        // if (bAutogain) {
        //    /* Auto-gain ... switches automatically between 1x and 16x */
        //    pTsl->enableAutoRange(true);
        //} else {
        /* You can also manually set the gain or enable auto-gain support */
        // pTsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright
        // light to avoid sensor saturation */
        /* // 16x gain ... use in low light to boost sensitivity */
        pTsl->setGain(tsl2561Gain);
        //}

        pTsl->setIntegrationTime(tsl2561Speed);
        /* Changing the integration time gives you better sensor resolution
         * (402ms = 16-bit data) */
        // pTsl->setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium
        // resolution and speed   */
        // pTsl->setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit
        // data but slowest conversions */
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        pTsl = new Adafruit_TSL2561_Unified(i2c_address, 12345);

        if (!pTsl->begin()) {
            /* There was a problem detecting the TSL2561 ... check your
             * connections */
            DBG("No TSL2561 detected, check your wiring or i2c_address (usually 0x29, 0x39, or "
                "0x49)");
        } else {
            auto ft = [=]() { this->loop(); };
            tID = pSched->add(ft, name, usSampleThread);

            auto fnall = [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
            pSched->subscribe(tID, name + "/sensor/illuminance/#", fnall);
            pSched->subscribe(tID, name + "/sensor/unitilluminance/#", fnall);
            bActive = true;
        }
    }

#ifdef __ESP__
    void registerHomeAssistant(String homeAssistantFriendlyName, String projectName = "",
                               String homeAssistantDiscoveryPrefix = "homeassistant") {
        pHA = new HomeAssistant(name, tID, homeAssistantFriendlyName, projectName, TSL_VERSION,
                                homeAssistantDiscoveryPrefix);
        pHA->addSensor("unitilluminance", "Unit-Illuminance", "[0..1]", "illuminance",
                       "mdi:brightness-6");
        pHA->addSensor("illuminance", "Illuminance", "lux", "illuminance", "mdi:brightness-6");
        pHA->begin(pSched);
    }
#endif

    void publishFilterMode() {
        switch (filterMode) {
        case FilterMode::FAST:
            pSched->publish(name + "/sensor/mode", "FAST");
            break;
        case FilterMode::MEDIUM:
            pSched->publish(name + "/sensor/mode", "MEDIUM");
            break;
        case FilterMode::LONGTERM:
            pSched->publish(name + "/sensor/mode", "LONGTERM");
            break;
        }
    }

    void publishIlluminance() {
        char buf[32];
        sprintf(buf, "%7.0f", luxvalue);
        pSched->publish(name + "/sensor/illuminance", buf);
        sprintf(buf, "%6.3f", unitIlluminanceValue);
        pSched->publish(name + "/sensor/unitilluminance", buf);
    }

    void publishMaxLux() {
        char buf[32];
        sprintf(buf, "%8.1f", maxLux);
        pSched->publish(name + "/sensor/maxlux", buf);
    }

    void loop() {
        if (bActive) {
            double val = calcLux() * amp;
            if (val != -1.0) {
                if (illuminanceSensor.filter(&val)) {
                    luxvalue = val;
                    unitIlluminanceValue = val / maxLux;
                    if (unitIlluminanceValue > 1.0)
                        unitIlluminanceValue = 1.0;
                    publishIlluminance();
                }
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/illuminance/get" ||
            topic == name + "/sensor/unitilluminance/get") {
            publishIlluminance();
        }
        if (topic == name + "/sensor/maxlux/set") {
            double tmpMaxLux = msg.toFloat();
            setMaxLux(tmpMaxLux);
        }
        if (topic == name + "/sensor/maxlux/get") {
            publishMaxLux();
        }
        if (topic == name + "/sensor/mode/set") {
            if (msg == "fast" || msg == "FAST") {
                setFilterMode(FilterMode::FAST);
            } else {
                if (msg == "medium" || msg == "MEDIUM") {
                    setFilterMode(FilterMode::MEDIUM);
                } else {
                    setFilterMode(FilterMode::LONGTERM);
                }
            }
        }
        if (topic == name + "/sensor/mode/get") {
            publishFilterMode();
        }
    };
};  // Illuminance

}  // namespace ustd
