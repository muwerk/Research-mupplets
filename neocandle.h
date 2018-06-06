#pragma once

#include "../butterlamp/src/Adafruit_NeoPixel.h"
#include "scheduler.h"

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

    NeoCandle(String name, uint8_t pin = NEOCANDLE_PIN,
              uint16_t numPixels = NEOCANDLE_NUMPIXELS,
              uint8_t options = NEOCANDLEX_OPTIONS, bool bUseModulator = true,
              bool bAutobrightness = true, String brightnessTopic = "")
        : name(name), pin(pin), numPixels(numPixels), options(options),
          bUseModulator(bUseModulator), bAutobrightness(bAutobrightness),
          brightnessTopic(brightnessTopic) {
        if (bAutobrightness) {
            if (brightnessTopic == "") {
                brightnessTopic = name + "/ldr/unitluminosity";
            }
        }
        if (bUseModulator) {
            wind = 30;
            amp = 20;
        }
    }

    ~NeoCandle() {
    }

    int parseValue(const byte *msg, unsigned int len) {
        char buff[32];
        int l;
        int ampx = 0;
        memset(buff, 0, 32);
        if (len > 31)
            l = 31;
        else
            l = len;
        strncpy(buff, (const char *)msg, l);

        if (l >= 2 && !strncmp((const char *)buff, "on", 2)) {
            ampx = 100;
        } else {
            if (l >= 3 && !strncmp((const char *)buff, "off", 3)) {
                ampx = 0;
            } else {
                if (l >= 4 && !strncmp((const char *)buff, "pct ", 4)) {
                    ampx = atoi((char *)(buff + 4));
                } else {
                    ampx = atoi((char *)buff);
                }
            }
        }
        return ampx;
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
        struct tm *pTm = localtime(&now);
        if (pTm->tm_hour < 18)
            m1 = 0.0;
        else {
            m1 = (23.0 - (pTm->tm_hour + pTm->tm_min / 60.0)) / (24.0 - 18.0);
        }
        if (bUnitBrightness) {
            if (m1 > 0.0) {
                m1 = m1 * unitBrightness;
            }
        }
        if (m2 != 0.0) {
            if (m2 > 0.75)
                m1 = m2;
            else
                m1 = (m1 + m2) / 2.0;
        }
        return m1;
    }

    void begin(Scheduler *_pSched) {
        // Make sure _clientName is Unique! Otherwise MQTT server will
        // rapidly disconnect.
        char buf[32];
        pSched = _pSched;

        pPixels = new Adafruit_NeoPixel(numPixels, pin, options);
        pPixels->begin();
        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        std::function<void()> ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 100000);

        std::function<void(String, String, String)> fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/brightness/set", fnall);
        pSched->subscribe(tID, "windlevel/set", fnall);
        pSched->subscribe(tID, name + "/windlevel/set", fnall);
        if (bAutobrightness)
            pSched->subscribe(tID, brightnessTopic, fnall);
        sprintf(buf, "%d", wind);
        pSched->publish(name + "/windlevel", buf);
        sprintf(buf, "%d", amp);
        pSched->publish(name + "/brightness", buf);
        manualSet = 0;
        bStarted = true;
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
                    pSched->publish(name + "/modulator", msg);
                }
                cr = ((double)cr * mx);
                cg = ((double)cg * mx);
                cb = ((double)cb * mx);
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
        if (topic == name + "/brightness/set") {
#ifdef USE_SERIAL_DBG
            Serial.print("Message arrived [");
            Serial.print(topic.c_str());
            Serial.println("] ");
#endif
            int amp_old = amp;
            amp =
                parseValue((const byte *)msg.c_str(), strlen(msg.c_str()) + 1);
            if (amp < 0)
                amp = 0;
            if (amp > 100)
                amp = 100;
            if (amp != amp_old) {
                char buf[32];
                sprintf(buf, "%d", amp);
                pSched->publish(name + "/brightness", buf);
                manualSet = time(nullptr);
            }
        }
        if (topic == "windlevel/set" || topic == name + "/windlevel/set") {
#ifdef USE_SERIAL_DBG
            Serial.print("Message arrived [");
            Serial.print(topic.c_str());
            Serial.println("] ");
#endif
            int wind_old = wind;
            wind =
                parseValue((const byte *)msg.c_str(), strlen(msg.c_str()) + 1);
            if (wind < 0)
                wind = 0;
            if (wind > 100)
                wind = 100;
            if (wind != wind_old) {
                char buf[32];
                sprintf(buf, "%d", wind);
                pSched->publish(name + "/windlevel", buf);
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
