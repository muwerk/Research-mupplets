// neocandle.h
#pragma once

#include "scheduler.h"

#include "../lib/Adafruit NeoPixel_ID28/Adafruit_NeoPixel.h"
//#include <Adafruit_NeoPixel.h>

// Neopixel default hardware:
#define NEOCANDLE_PIN 15        // Soldered to pin 15 on neopixel feather-wing
#define NEOCANDLE_NUMPIXELS 32  // neopixel feather-wing
#define NEOCANDLE_OPTIONS                                                      \
    (NEO_GRB + NEO_KHZ800)  // defaults for 5x8 adafruit feather-wing.

namespace ustd {
class NeoCandle {
  public:
    Scheduler *pSched;
    String name;
    bool bStarted = false;
    uint8_t pin;
    int numPixels;
    int options;
    // Max brightness of butterlamp 0..100
    int amp = 20;
    // Max wind flicker 0..100
    int wind = 20;
    Adafruit_NeoPixel *pPixels;

    NeoCandle(String name, uint8_t pin = NEOCANDLE_PIN,
              int numPixels = NEOCANDLE_NUMPIXELS,
              int options = NEOCANDLE_OPTIONS)
        : name(name), pin(pin), numPixels(numPixels), options(options) {
    }

    ~NeoCandle() {
    }

    int parseValue(const byte *msg, unsigned int len) {
        char buff[32];
        int l;
        int amp = 0;
        memset(buff, 0, 32);
        if (len > 31)
            l = 31;
        else
            l = len;
        strncpy(buff, (const char *)msg, l);

        if (l >= 2 && !strncmp((const char *)buff, "on", 2)) {
            amp = 100;
        } else {
            if (l >= 3 && !strncmp((const char *)buff, "off", 3)) {
                amp = 0;
            } else {
                if (l >= 4 && !strncmp((const char *)buff, "pct ", 4)) {
                    amp = atoi((char *)(buff + 4));
                } else {
                    amp = atoi((char *)buff);
                }
            }
        }
        return amp;
    }

    void begin(Scheduler *_pSched) {
        // Make sure _clientName is Unique! Otherwise MQTT server will
        // rapidly disconnect.
        char buf[32];
        pSched = _pSched;

        pPixels = new Adafruit_NeoPixel(numPixels, pin, options);

        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        std::function<void()> ft = [=]() { this->loop(); };
        pSched->add(ft, 50000);

        std::function<void(String, String, String)> fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe("butterlamp/brightness/set", fnall);
        pSched->subscribe("butterlamp/windlevel/set", fnall);
        sprintf(buf, "%d", wind);
        pSched->publish("butterlamp/windlevel", buf);
        sprintf(buf, "%d", amp);
        pSched->publish("butterlamp/brightness", buf);
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
        if (topic == "butterlamp/brightness/set") {
            Serial.print("Message arrived [");
            Serial.print(topic.c_str());
            Serial.print("] ");
            int amp_old = amp;
            amp = parseValue((const byte *)msg.c_str(), strlen(msg.c_str()));
            if (amp < 0)
                amp = 0;
            if (amp > 100)
                amp = 100;
            if (amp != amp_old) {
                char buf[32];
                sprintf(buf, "%d", amp);
                pSched->publish("butterlamp/brightness", buf);
            }
        }
        if (topic == "butterlamp/windlevel/set") {
            Serial.print("Message arrived [");
            Serial.print(topic.c_str());
            Serial.print("] ");
            int wind_old = wind;
            wind = parseValue((const byte *)msg.c_str(), strlen(msg.c_str()));

            if (wind < 0)
                wind = 0;
            if (wind > 100)
                wind = 100;
            if (wind != wind_old) {
                char buf[32];
                sprintf(buf, "%d", wind);
                pSched->publish("butterlamp/windlevel", buf);
            }
        }
    }
};
};  // namespace ustd
