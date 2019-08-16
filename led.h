// led.h
#pragma once

#include "scheduler.h"
#include "mup_util.h"

namespace ustd {

class Led {
  public:
    enum Mode { Passive, Blink, Wave, Pulse, Pattern};

    Scheduler *pSched;
    int tID;
    String name;
    uint8_t port;
    bool activeLogic = false;
    double brightlevel;
    bool state;
    uint16_t pwmrange;
    Mode mode;
    unsigned long interval;
    double phase=0.0;
    unsigned long uPhase=0;
    unsigned long oPeriod=0;


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
        mode=Mode::Passive;
        interval=1000; //ms
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
        if (!_automatic) mode=Mode::Passive;
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

    void setMode(Mode newmode, uint interval_ms=1000, double phase_unit=0.0) {
        mode=newmode;
        if (mode==Mode::Passive) return;
        phase=phase_unit;
        if (phase<0.0) phase=0.0;
        if (phase>1.0) phase=1.0;
        interval=interval_ms;
        if (interval<100) interval=100;
        if (interval>100000) interval=100000;
        uPhase=(unsigned long)(2.0*(double)interval*phase);
        oPeriod=(millis()+uPhase)%interval;
        //last=millis(); //-interval+((millis()%interval)+((unsigned long)((double)interval*phase))%interval);
    }

    void brightness(double bright, bool _automatic=false) {
        uint16_t bri;
        
        if (!_automatic) mode=Mode::Passive;
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
        if (mode==Mode::Passive) return;
        unsigned long period=(millis()+uPhase)%(2*interval);
        if (mode==Mode::Blink) {
            if (period<oPeriod) {
                set(false, true);
            } else {
                if (period>interval && oPeriod<interval) {
                    set(true, true);
                }
            }
        }
        if (mode==Mode::Wave) {
            unsigned long period=(millis()+uPhase)%(2*interval);
            double br=0.0;
            if (period<interval) {
                br=(double)period/(double)interval;
            } else {
                br=(double)(2*interval - period)/(double)interval;
            }
            brightness(br, true);
        }
        oPeriod=period;
    }

    void subsMsg(String topic, String msg, String originator) {
        char msgbuf[32];
        memset(msgbuf,0,32);
        strncpy(msgbuf,msg.c_str(),31);
        if (topic == name + "/led/set") {
            double br;
            br = parseUnitLevel(msg);
            brightness(br);
        }
        if (topic == name+"/led/mode/set") {
            char *p=strchr(msgbuf,' ');
            char *p2=nullptr;
            if (p) {
                *p=0;
                ++p;
                p2=strchr(p,',');
                if (p2) {
                    *p2=0;
                    ++p2;
                }
            }
            int t=1000;
            double phs=0.0;
            if (!strcmp(msgbuf, "passive")) {
                setMode(Mode::Passive);
            } else if (!strcmp(msgbuf, "blink")) {
                if (p) t=atoi(p);
                if (p2) phs=atof(p2);
                setMode(Mode::Blink, t, phs);
            } else if (!strcmp(msgbuf, "wave")) {
                if (p) t=atoi(p);
                if (p2) phs=atof(p2);
                setMode(Mode::Blink, t, phs);
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
