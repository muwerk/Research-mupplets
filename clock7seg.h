#pragma once

#include <time.h>

//#include
//"/Users/dsc/git/avr/muwerk/borgclock/.piolibdeps/Adafruit_GFX_ID3115/Adafruit_GFX.h"
//#include "/Users/dsc/git/avr/muwerk/borgclock/.piolibdeps/Adafruit LED
// Backpack Library_ID25/Adafruit_LEDBackpack.h" #include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>

#include "scheduler.h"

// Seven segment display default i2c address:
#define DISPLAY_ADDRESS 0x70

namespace ustd {
class Clock7Seg {
  public:
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t i2cAddress;
    uint8_t buzzerPin;
    bool bStarted = false;
    Adafruit_7segment *pClockDisplay;
    unsigned long timerCounter = 0;
    int maxAlarmDuration = 60;
    time_t timerStart = 0;
    time_t alarmStart = 0;
    bool bAutobrightness = true;
    String brightnessTopic = "";
    int oldBuzzState = 0;

    Clock7Seg(String name, uint8_t i2cAddress = DISPLAY_ADDRESS,
              uint8_t buzzerPin = 0xff, bool bAutobrightness = true,
              String brightnessTopic = "")
        : name(name), i2cAddress(i2cAddress), buzzerPin(buzzerPin),
          bAutobrightness(bAutobrightness), brightnessTopic(brightnessTopic) {
    }

    ~Clock7Seg() {
    }

    uint8_t old_hr = 0xFF;
    uint8_t old_mn = 0xFF;
    uint8_t old_dots = 0xFF;
    void displayClockDigits(uint8_t hr, uint8_t mn, uint8_t dots = 0x1,
                            bool cache = true) {
        if (cache && old_hr == hr && old_mn == mn && old_dots == dots)
            return;

        old_hr = hr;
        old_mn = mn;
        old_dots = dots;
        uint8_t d0 = hr / 10;
        uint8_t d1 = hr % 10;
        uint8_t d2 = mn / 10;
        uint8_t d3 = mn % 10;
        pClockDisplay->writeDigitNum(0, d0);
        pClockDisplay->writeDigitNum(1, d1);
        pClockDisplay->writeDigitNum(3, d2);
        pClockDisplay->writeDigitNum(4, d3);
        pClockDisplay->drawColon(dots & 0x1);
        pClockDisplay->writeDisplay();
    }

    time_t old_now = -1;
    uint8_t old_extraDots = 0xFF;
    void displayTime(uint8_t extraDots = 0x0, bool cache = true) {
        time_t now = time(nullptr);
        if (cache && old_extraDots == extraDots && now == old_now)
            return;
        old_extraDots = extraDots;
        old_now = now;
        struct tm *pTm = localtime(&now);

        uint8_t dots = ((pTm->tm_sec + 1) % 2) || extraDots;
        if ((timerCounter > 0 && ((pTm->tm_sec % 5) > 1)) ||
            (timerCounter > 0 && timerCounter - (now - timerStart) < 15)) {
            long curt = timerCounter - (now - timerStart);
            if (curt < 0) {
                pSched->publish(name + "/timer/alarm", "Ding Dong");
                if (buzzerPin != 0xff) {
                    analogWrite(buzzerPin, 128);
                }
                alarmStart = time(nullptr);
                timerStart = 0;
                timerCounter = 0;
            } else {
                displayClockDigits(curt / 60, curt % 60, 1);
            }
        } else {
            displayClockDigits(pTm->tm_hour, pTm->tm_min, dots);
        }
        if (alarmStart > 0) {
            if (time(nullptr) - alarmStart > maxAlarmDuration) {
                if (buzzerPin != 0xff) {
                    analogWrite(buzzerPin, 0);
                }
                alarmStart = 0;
                timerStart = 0;
                timerCounter = 0;
            } else {
                if (buzzerPin != 0xff) {
                    int buzzState = (time(nullptr) - alarmStart + 1) % 2;
                    if (buzzState != oldBuzzState) {
                        oldBuzzState = buzzState;
                        analogWrite(buzzerPin, 128 * buzzState);
                    }
                }
            }
        }
    }

    int oldIBr = -1;
    void setBrightness(double fLevel) {  // 0.0 - 1.0
        double brL = fLevel;
        if (brL < 0.0)
            brL = 0.0;
        if (brL > 1.0)
            brL = 1.0;
        int iBr = (int)(brL * 15.0);
        pClockDisplay->setBrightness(iBr);  // Brightness [0..15]
        if (oldIBr != iBr) {
            oldIBr = iBr;
            String sbr = String(iBr);
            pSched->publish(name + "/ibrightness", sbr);
        }
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        pClockDisplay = new Adafruit_7segment();

        pClockDisplay->begin(i2cAddress);
        pClockDisplay->clear();

        if (buzzerPin != 0xff) {
            analogWrite(buzzerPin, 0);
        }
        alarmStart = 0;
        /* std::function<void()> */
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 50000);

        /* std::function<void(String, String, String)> */
        auto fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/#", fnall);
        if (bAutobrightness) {
            if (brightnessTopic != "")
                pSched->subscribe(tID, brightnessTopic, fnall);
        }
        bStarted = true;
    }

    void loop() {
        if (bStarted) {
            displayTime();
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (msg == "dummyOn") {
            return;  // Ignore, homebridge hack
        }
        if (topic == name + "/display/set") {
#ifdef USE_SERIAL_DBG
            Serial.print("Message arrived [");
            Serial.print(topic.c_str());
            Serial.println("] ");
#endif
        }
        if (topic == name + "/timer/set") {
            timerCounter = atoi(msg.c_str()) * 60;
            if (timerCounter > 0)
                timerStart = time(nullptr);
            else
                timerStart = 0;
            if (buzzerPin != 0xff) {
                analogWrite(buzzerPin, 0);
            }
            alarmStart = 0;
        }
        if (topic == name + "/timer/stop" || topic == name + "/alarm/stop") {
            timerCounter = 0;
            timerStart = 0;
            alarmStart = 0;
            if (buzzerPin != 0xff) {
                analogWrite(buzzerPin, 0);
            }
        }
        if (topic == name + "/alarm/start") {
            timerCounter = 0;
            timerStart = 0;
            alarmStart = time(nullptr);
            if (buzzerPin != 0xff) {
                analogWrite(buzzerPin, 128);
            }
        }
        if (bAutobrightness) {
            if (pSched->mqttmatch(topic, brightnessTopic)) {
                double unitBrightness = atof(msg.c_str());
                setBrightness(unitBrightness);
            }
        }
    }
};
};  // namespace ustd
