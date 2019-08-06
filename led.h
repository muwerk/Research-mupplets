// led.h
#pragma once

#include "scheduler.h"

namespace ustd {

class Led {
  public:
    enum Mode { PASSIVE, BLINK, PULSE};

    Scheduler *pSched;
    int tID;
    String name;
    uint8_t port;
    bool activeLogic = false;
    double brightlevel;
    bool state;
    uint16_t pwmrange;
    Mode mode;
    uint interval;
    unsigned long last;


    Led(String name, uint8_t port, bool activeLogic=false )
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
        mode=Mode::PASSIVE;
        interval=1000; //ms
        last=millis();
        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        std::function<void()> ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 50000);

        std::function<void(String, String, String)> fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/led/#", fnall);
    }

    void set(bool state, bool _automatic=false) {
        this->state=state;
        if (!_automatic) mode=Mode::PASSIVE;
        if (state) {
            digitalWrite(port, activeLogic);
            if (!_automatic) {
                pSched->publish(name + "/led/unitluminosity", "1.0");
                pSched->publish(name + "/led/state", "on");
            }
        } else {
            digitalWrite(port, !activeLogic);
           if (!_automatic) {
                pSched->publish(name + "/led/unitluminosity", "0.0");
                pSched->publish(name + "/led/state", "off");
           }
        }
    }

    void setmode(Mode newmode, uint interval_ms=1000) {
        mode=newmode;
        if (mode==Mode::PASSIVE) return;
        interval=interval_ms;
        if (interval<100) interval=100;
        if (interval>100000) interval=100000;
    }

    void brightness(double bright, bool _automatic=false) {
        uint16_t bri;
        
        if (!_automatic) mode=Mode::PASSIVE;
        if (bright==1.0) {
            set(true, _automatic);
            return;
        }
        if (bright==0.0) {
            set(false, _automatic);
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
        if (!_automatic) {
            pSched->publish(name + "/led/unitluminosity", buf);
        }
    }

    void loop() {
        if (mode==Mode::PASSIVE) return;
        if (mode==Mode::BLINK) {
            if (millis()-last > interval) {
                last=millis();
                set(!state, true);
            } 
        }
        if (mode==Mode::PULSE) {
            if (millis()-last>2*interval) {
                set(false, true);
                last=millis();
            } else {
                double br=0.0;
                long dt=millis()-last;
                if (dt<(long)interval) {
                    br=(double)dt/(interval);
                } else {
                    br=(double)(2*interval - dt)/(double)interval;
                }
                brightness(br, true);
            }
        }
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
        char msgbuf[32];
        memset(msgbuf,0,32);
        strncpy(msgbuf,msg.c_str(),31);
        if (topic == name + "/led/set") {
            double br;
            br = parseBrightness(msg);
            brightness(br);
        }
        if (topic == name+"/led/mode/set") {
            char *p=strchr(msgbuf,' ');
            if (p) {
                *p=0;
                ++p;
            }
            int t=1000;
            if (!strcmp(msgbuf, "passive")) {
                setmode(Mode::PASSIVE);
            } else if (!strcmp(msgbuf, "blink")) {
                if (p) t=atoi(p);
                setmode(Mode::BLINK, t);
            } else if (!strcmp(msgbuf, "pulse")) {
                if (p) t=atoi(p);
                setmode(Mode::PULSE, t);
            }
        }
        if (topic == name + "/led/unitluminosity/get") {
            char buf[32];
            sprintf(buf, "%5.3f", brightlevel);
            pSched->publish(name + "/led/unitluminosity", buf);
        }
    };
};  // Led

}  // namespace ustd
