#pragma once

// Platformio lib finder collapses on space in 'Adafruit NeoPixel_ID28' name...
#include "../.pio/libdeps/huzzah/Adafruit NeoPixel/Adafruit_NeoPixel.h"
#include "scheduler.h"
#include "mup_util.h"

//#include "Adafruit_NeoPixel.h"

// Neopixel default hardware:
// Soldered to pin 15 on neopixel feather-wing
#define NEOCANDLE_PIN 15
// neopixel feather-wing number of leds:
#define NEOCANDLE_NUMPIXELS 32
// defaults for 4x8 adafruit feather-wing:
#define NEOCANDLEX_OPTIONS (NEO_GRB + NEO_KHZ800)

namespace ustd {
class NeoCandle {
  public:
    String NEOCANDLE_VERSION = "0.1.0";
    Scheduler *pSched;
    int tID;
    String name;
    bool bStarted = false;
    uint8_t pin;
    uint16_t numPixels;
    uint8_t options;
    // Max brightness of butterlamp 0..100
    int amp = 0;
    // Max wind flicker 0..100
    int wind = 0;
    Adafruit_NeoPixel *pPixels;
    time_t manualSet = 0;
    bool bUseModulator = true;
    bool bAutobrightness = true;
    String brightnessTopic = "";
    bool bUnitBrightness = false;
    double unitBrightness = 0.0;
    double oldMx = -1.0;
    bool state = false;
    bool useAutoTimer = true;
    uint8_t start_hour = 18, start_minute = 0, end_hour = 0, end_minute = 0;

#ifdef __ESP__
    HomeAssistant *pHA;
#endif

    NeoCandle(String name, uint8_t pin = NEOCANDLE_PIN, uint16_t numPixels = NEOCANDLE_NUMPIXELS,
              uint8_t options = NEOCANDLEX_OPTIONS, bool bUseModulator = true,
              bool bAutobrightness = true, String brightnessTopic = "")
        : name(name), pin(pin), numPixels(numPixels), options(options),
          bUseModulator(bUseModulator), bAutobrightness(bAutobrightness),
          brightnessTopic(brightnessTopic) {
        if (bAutobrightness) {
            if (brightnessTopic == "") {
                brightnessTopic = name + "/sensor/unitilluminance";
            }
        }
        if (bUseModulator) {
            wind = 30;
            amp = 20;
        }
    }

    ~NeoCandle() {
    }

#ifdef __ESP__
    void registerHomeAssistant(String homeAssistantFriendlyName, String projectName = "",
                               String homeAssistantDiscoveryPrefix = "homeassistant") {
        pHA = new HomeAssistant(name, tID, homeAssistantFriendlyName, projectName,
                                NEOCANDLE_VERSION, homeAssistantDiscoveryPrefix);
        pHA->addLight();
        pHA->begin(pSched);
    }
#endif

    static int cmpHourMinuteTime(uint8_t h1, uint8_t m1, uint8_t h2, uint8_t m2) {
        /*! compare two hour/minute times h1:m1 and h2:m2
        @param h1 hour 0-23 of time-1
        @param m1 minute 0-59 of time-1
        @param h2 hour of time-2
        @param m2 minute of time-2
        @return 1: if h2:m2 is later than h1:m1, -1 if earlier, 0 if equal.
        */
        if (h2 > h1)
            return 1;
        if (h2 < h1)
            return -1;
        if (m2 > m1)
            return 1;
        if (m2 < m1)
            return -1;
        return 0;
    }

    static int deltaHourMinuteTime(uint8_t h1, uint8_t m1, uint8_t h2, uint8_t m2) {
        /*! time difference between h2:m2 and h1:m1 in minutes.
        @param h1 hour 0-23 of time-1
        @param m1 minute 0-59 of time-1
        @param h2 hour of time-2
        @param m2 minute of time-2
        @return time difference in minutes.
        */
        if (m2 < m1) {
            m2 += 60;
            if (h2 > 0)
                --h2;
            else
                h2 = 23;
        }
        if (h2 < h1)
            h2 += 24;
        return (h2 - h1) * 60 + m2 - m1;
    }

    static bool inHourMinuteInterval(uint8_t test_hour, uint8_t test_minute, uint8_t start_hour,
                                     uint8_t start_minute, uint8_t end_hour, uint8_t end_minute) {
        /*! test if test_hour:test_minute is in interval [start_hour:start_minute, end_hour,
        end_minute]
        @returns true if test_hour:test_minute is between start end end time.
        */
        if (cmpHourMinuteTime(start_hour, start_minute, end_hour, end_minute) > 0) {
            if (cmpHourMinuteTime(test_hour, test_minute, start_hour, start_minute) <= 0 &&
                cmpHourMinuteTime(test_hour, test_minute, end_hour, end_minute) >= 0)
                return true;
            else
                return false;
        } else {
            if (cmpHourMinuteTime(test_hour, test_minute, start_hour, start_minute) > 0 &&
                cmpHourMinuteTime(test_hour, test_minute, end_hour, end_minute) < 0)
                return false;
            else
                return true;
        }
        return false;
    }

    static bool parseHourMinuteString(String hourMinute, int *hour, int *minute) {
        int ind = hourMinute.indexOf(':');
        if (ind == -1)
            return false;
        int hr = hourMinute.substring(0, ind).toInt();
        int mn = hourMinute.substring(ind + 1).toInt();
        if (hr < 0 || hr > 23)
            return false;
        if (mn < 0 || mn > 59)
            return false;
        *hour = hr;
        *minute = mn;
        return true;
    }

    double modulator() {
        double m1 = 1.0;
        double m2 = 0.0;
        time_t now = time(nullptr);

        if (!bUseModulator)
            return m1;
        long dt = now - manualSet;
        if (dt < 3600) {
            m2 = (3600.0 - (double)dt) / 3600.0;
        }

        if (useAutoTimer) {
            struct tm *pTm = localtime(&now);

            if (inHourMinuteInterval(pTm->tm_hour, pTm->tm_min, start_hour, start_minute, end_hour,
                                     end_minute)) {
                int deltaAll = deltaHourMinuteTime(start_hour, start_minute, end_hour, end_minute);
                int deltaCur =
                    deltaHourMinuteTime(start_hour, start_minute, pTm->tm_hour, pTm->tm_min);
                if (deltaAll > 0.0)
                    m1 = (deltaAll - deltaCur) / (double)deltaAll;
                else
                    m1 = 0.0;
            } else {
                m1 = 0.0;
            }
            /*
            if (pTm->tm_hour < 18)
                m1 = 0.0;
            else {
                m1 = (24.0 - (pTm->tm_hour + pTm->tm_min / 60.0)) / (24.0 - 18.0);
            }
            */
        } else {
            m1 = 0.0;
        }

        if (bUnitBrightness) {
            if (m1 > 0.0 || m2 > 0.0) {
                m1 = m1 * unitBrightness;
                m2 = m2 * unitBrightness;
            }
        }
        if (m2 != 0.0) {
            if (m2 > 0.75)
                m1 = m2;
            else
                m1 = (m1 + m2) / 2.0;
        }
        if (m1 == 0) {
            if (state) {
                state = false;
                publishState();
            }
        } else {
            if (!state) {
                state = true;
                publishState();
            }
        }
        return m1;
    }

    void begin(Scheduler *_pSched, int _start_hour = -1, int _start_minute = -1, int _end_hour = -1,
               int _end_minute = -1) {
        // Make sure _clientName is Unique! Otherwise MQTT server will
        // rapidly disconnect.
        // char buf[32];
        pSched = _pSched;

        if (_start_hour == -1 || _start_minute == -1 || _end_hour == -1 || _end_minute == -1) {
            useAutoTimer = false;
        } else {
            useAutoTimer = true;
            start_hour = _start_hour;
            start_minute = _start_minute;
            end_hour = _end_hour;
            end_minute = _end_minute;
        }

        pPixels = new Adafruit_NeoPixel(numPixels, pin, options);
        pPixels->begin();
        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        /* std::function<void()> */
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 100000);

        /* std::function<void(String, String, String)> */
        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/light/set", fnall);
        pSched->subscribe(tID, name + "/light/windlevel/set", fnall);
        if (bAutobrightness)
            pSched->subscribe(tID, brightnessTopic, fnall);

        publishState();

        manualSet = 0;
        bStarted = true;
    }

    void publishState() {
        if (state) {
            pSched->publish(name + "/light/state", "on");
            this->state = true;
        } else {
            pSched->publish(name + "/light/state", "off");
            this->state = false;
        }
        char buf[32];
        sprintf(buf, "%5.3f", unitBrightness);
        pSched->publish(name + "/light/unitbrightness", buf);
    }

    void brightness(double bright) {
        if (bright < 0.0) {
            bright = 0.0;
        }
        if (bright > 1.0)
            bright = 1.0;
        unitBrightness = bright;
        bUnitBrightness = true;
        if (bright > 0.0) {
            manualSet = time(nullptr);
        } else {
            manualSet = 0;
        }
        publishState();
    }

    int f1 = 0, f2 = 0, max_b = 20;
    void butterlamp() {
        int r, c, lc, ce, cr, cg, cb, mf;
        int flic[] = {4, 7, 8, 9, 10, 12, 16, 20, 32, 30, 32, 20, 24, 16, 8, 6};
        for (int i = 0; i < numPixels; i++) {
            r = i / 8;
            c = i % 8;
            // l = c / 2;
            lc = c % 4;
            if (((r == 1) || (r == 2)) && ((lc == 1) || (lc == 2)))
                ce = 1;  // two lamps have 2x2 centers: ce=1 -> one of the
                         // centers
            else
                ce = 0;

            if (ce == 1) {  // center of one lamp
                // pixels.Color takes RGB values, from 0,0,0 up to
                // 255,255,255
                cr = 40;
                cg = 15;
                cb = 0;
                mf = flic[f1];
                f1 += rand() % 3 - 1;
                if (f1 < 0)
                    f1 = 15;
                if (f1 > 15)
                    f1 = 0;
                mf = 32 - ((32 - mf) * wind) / 100;
            } else {  // border
                cr = 20;
                cg = 4;
                cb = 0;
                mf = flic[f2];
                f2 += rand() % 3 - 1;
                if (f2 < 0)
                    f2 = 15;
                if (f2 > 15)
                    f2 = 0;
                mf = 32 - ((32 - mf) * wind) / 100;
            }

            cr = cr + rand() % 2;
            cg = cg + rand() % 2;
            cb = cb + rand() % 1;

            if (cr > max_b)
                max_b = cr;
            if (cg > max_b)
                max_b = cg;
            if (cb > max_b)
                max_b = cb;

            cr = (cr * amp * 4 * mf) / (max_b * 50);
            cg = (cg * amp * 4 * mf) / (max_b * 50);
            cb = (cb * amp * 4 * mf) / (max_b * 50);

            if (cr > 255)
                cr = 255;
            if (cr < 0)
                cr = 0;
            if (cg > 255)
                cg = 255;
            if (cg < 0)
                cg = 0;
            if (cb > 255)
                cb = 255;
            if (cb < 0)
                cb = 0;

            if (bUseModulator) {
                double mx = modulator();
                double dx = fabs(oldMx - mx);
                if (dx > 0.05) {
                    oldMx = mx;
                    char msg[32];
                    sprintf(msg, "%6.3f", mx);
                    pSched->publish(name + "/light/modulator", msg);
                }
                cr = ((double)cr * mx);
                cg = ((double)cg * mx);
                cb = ((double)cb * mx);
            } else {
                if (unitBrightness > 0) {
                    if (!state) {
                        state = true;
                        publishState();
                    }
                } else {
                    if (state) {
                        state = false;
                        publishState();
                    }
                }
            }
            pPixels->setPixelColor(i, pPixels->Color(cr, cg, cb));
        }
        pPixels->show();  // This sends the updated pixel color to the
                          // hardware.
    }

    void loop() {
        if (bStarted)
            butterlamp();
    }

    void subsMsg(String topic, String msg, String originator) {
        if (msg == "dummyOn") {
            return;  // Ignore, homebridge hack
        }
        if (topic == name + "/light/set") {
#ifdef USE_SERIAL_DBG
            Serial.print("Message arrived [");
            Serial.print(topic.c_str());
            Serial.println("] ");
#endif
            double br;
            br = parseUnitLevel(msg);
            brightness(br);
            /*
            int amp_old = amp;
            amp = parseUnitLevel(msg) * 100;
            //    parseValue((const byte *)msg.c_str(), strlen(msg.c_str()) +
            //    1);
            if (amp < 0)
                amp = 0;
            if (amp > 100)
                amp = 100;
            if (amp != amp_old) {
                char buf[32];
                sprintf(buf, "%d", amp);
                pSched->publish(name + "/light/brightness", buf);
                manualSet = time(nullptr);
            }
            */
        }
        if (topic == "windlevel/set" || topic == name + "/light/windlevel/set") {
#ifdef USE_SERIAL_DBG
            Serial.print("Message arrived [");
            Serial.print(topic.c_str());
            Serial.println("] ");
#endif
            int wind_old = wind;
            wind = parseUnitLevel(msg) * 100;
            //    parseValue((const byte *)msg.c_str(), strlen(msg.c_str()) +
            //    1);
            if (wind < 0)
                wind = 0;
            if (wind > 100)
                wind = 100;
            if (wind != wind_old) {
                char buf[32];
                sprintf(buf, "%d", wind);
                pSched->publish(name + "/light/windlevel", buf);
                manualSet = time(nullptr);
            }
        }
        if (topic == brightnessTopic) {
            unitBrightness = atof(msg.c_str());
            bUnitBrightness = true;
        }
    }
};
};  // namespace ustd
