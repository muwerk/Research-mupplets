// led.h
#pragma once

#include "scheduler.h"

namespace ustd {
class Led {
  public:
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t port;
    double brightlevel;
    bool state;
    bool activeLogic;
    uint16_t pwmrange;
    uint8_t mode;

    Led(String name, uint8_t port, bool activeLogic = false)
        : name(name), port(port), activeLogic(activeLogic) {
    }

    ~Led() {
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        pinMode(port, OUTPUT);
        #ifdef __ESP__
        pwmrange=1023;
        #else
        pwmrange=255;
        #endif
        if (activeLogic) { // activeLogic true: HIGH is ON
            digitalWrite(port, LOW);  // OFF

        } else { // activeLogic false: LOW is ON
            digitalWrite(port, HIGH);  // OFF
        }

        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        std::function<void()> ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 200000);

        std::function<void(String, String, String)> fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/led/#", fnall);
    }

    void set(bool state) {
        this->state=state;
        if (state == activeLogic) {
            digitalWrite(port, HIGH);
            pSched->publish(name + "/led/unitluminosity", "1.0");
        } else {
            digitalWrite(port, LOW);
            pSched->publish(name + "/led/unitluminosity", "0.0");
        }
    }

    void brightness(double bright) {
        uint16_t bri;
        
        if (bright==1.0) {
            set(true);
            return;
        }
        if (bright==0.0) {
            set(false);
            return;
        }

        if (bright < 0.0)
            bright = 0.0;
        if (bright > 1.0)
            bright = 1.0;
        brightlevel=bright;
        bri = (uint16_t)(bright * (double)pwmrange);
        if (!bri) state=false;
        else state=true;
        if (!activeLogic)
            bri = pwmrange - bri;
        analogWrite(port, bri);

        char buf[32];
        sprintf(buf, "%5.3f", bright);
        pSched->publish(name + "/led/unitluminosity", buf);
    }

    void loop() {
    }

    double parseBrightness(String msg) {
        char buff[32];
        int l;
        int len = msg.length();
        double br = 0.0;
        memset(buff, 0, 32);
        if (len > 31)
            l = 31;
        else
            l = len;
        strncpy(buff, (const char *)msg.c_str(), l);

        if ((!strcmp((const char *)buff, "on")) ||
            (!strcmp((const char *)buff, "true"))) {
            br = 1.0;
        } else {
            if ((!strcmp((const char *)buff, "off")) ||
                (!strcmp((const char *)buff, "false"))) {
                br = 0.0;
            } else {
                if ((strlen(buff) > 4) &&
                    (!strncmp((const char *)buff, "pct ", 4))) {
                    br = atoi((char *)(buff + 4)) / 100.0;
                } else {
                    if (strlen(buff) > 1 && buff[strlen(buff) - 1] == '%') {
                        buff[strlen(buff) - 1] = 0;
                        br = atoi((char *)buff) / 100.0;
                    } else {
                        br = atof((char *)buff);
                    }
                }
            }
        }
        return br;
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/led/set") {
            double br;
            br = parseBrightness(msg);
            brightness(br);
        }
        if (topic == name + "/led/unitluminosity/get") {
            char buf[32];
            sprintf(buf, "%5.3f", brightlevel);
            pSched->publish(name + "/led/unitluminosity", buf);
        }
    };
};  // Led

}  // namespace ustd
