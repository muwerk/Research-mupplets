// switch.h
#pragma once

#include "scheduler.h"

namespace ustd {
class Switch {
  public:
    enum Mode { Default, Raising, Falling, Flipflop, Timer};
    Scheduler *pSched;
    int tID;

    String name;
    uint8_t port;
    Mode mode;
    bool activeLogic;
    String customTopic;
    unsigned long debounceTimeMs;

    unsigned long lastChangeMs = 0;
    int physicalState=-1;
    int logicalState=-1;
    bool overriddenPhysicalState=false;
    bool overridePhysicalActive=false;

    bool flipflop = true;  // This starts with 'off', since state is initially changed once.
    unsigned long activeTimer=0;
    unsigned long timerDuration=1000; //ms

    Switch(String name, uint8_t port, Mode mode = Mode::Default, bool activeLogic = false, String customTopic = "", unsigned long debounceTimeMs = 20)
        : name(name), port(port), mode(mode), activeLogic(activeLogic),
          customTopic(customTopic), debounceTimeMs(debounceTimeMs) {
    }

    ~Switch() {
    }

    void setDebounce(long ms) {
        if (ms<0) ms=0;
        if (ms>1000) ms=1000;
        debounceTimeMs=(unsigned long)ms;
    }

    void setTimerDuration(unsigned long ms) {
        timerDuration=ms;
    }

    void setMode(Mode newmode, unsigned long duration=0) {
        flipflop = true;  // This starts with 'off', since state is initially changed once.
        activeTimer=0;
        timerDuration=duration;
        physicalState=-1;
        logicalState=-1;
        overriddenPhysicalState=false;
        overridePhysicalActive=false;
        lastChangeMs=0;
        mode=newmode;
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        pinMode(port, INPUT_PULLUP);
        readState();

        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        std::function<void()> ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 50000);

        std::function<void(String, String, String)> fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/switch/#", fnall);
    }

    void publishLogicalState(bool lState) {
        String textState;
        if (lState == true)
            textState = "on";
        else
            textState = "off";
        switch (mode) {
            case Mode::Default:
            case Mode::Flipflop:
            case Mode::Timer:
                pSched->publish(name + "/switch/state", textState);
                if (customTopic!="") 
                    pSched->publish(customTopic, textState);
                break;
            case Mode::Raising:
                if (lState==true) {
                    pSched->publish(name + "/switch/state", "trigger");
                    if (customTopic!="") 
                        pSched->publish(customTopic, "trigger");
                }
                break;
            case Mode::Falling:
                if (lState==false) {
                    pSched->publish(name + "/switch/state", "trigger");
                    if (customTopic!="") 
                        pSched->publish(customTopic, "trigger");
                }
                break;
        }
    }

    void setLogicalState(bool newLogicalState) {
        if (logicalState!=newLogicalState) {
            logicalState=newLogicalState;
            publishLogicalState(logicalState);
        }
    }

    void decodeLogicalState(bool physicalState) {
        switch (mode) {
            case Mode::Default:
            case Mode::Raising:
            case Mode::Falling:
                setLogicalState(physicalState);
                break;
            case Mode::Flipflop:
                if (physicalState==false) {
                    flipflop = !flipflop;
                    setLogicalState(flipflop);
                }
                break;
            case Mode::Timer:
                if (physicalState==false) {
                    activeTimer=millis();
                } else {
                    setLogicalState(true);
                }
                break;
        }

    }

    void setPhysicalState(bool newState, bool override) {
        if (mode!=Mode::Timer) {
            activeTimer=0;
        }
        if (override) {
            overriddenPhysicalState=physicalState;
            overridePhysicalActive=true;
            if (newState!=physicalState) {
                physicalState=newState;
                decodeLogicalState(newState);
            }
        } else {
            if (overridePhysicalActive) {
                overridePhysicalActive=false;
            }
            if (newState != physicalState) {
                if (timeDiff(lastChangeMs, millis()) > debounceTimeMs) {
                    lastChangeMs = millis();
                    physicalState=newState;
                    decodeLogicalState(physicalState);
                }
            }
        }
    }

    void readState() {
        int newstate = digitalRead(port);
        if (newstate==HIGH) newstate=true;
        else newstate=false;
        if (!activeLogic) newstate=!newstate;
        setPhysicalState(newstate,false);
    }

    void loop() {
        readState();
        if (mode==Mode::Timer && activeTimer) {
            if (timeDiff(activeTimer, millis()) > timerDuration) {
                activeTimer=0;
                setLogicalState(false);
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/switch/state/get") {
            char buf[32];
            if (logicalState) sprintf(buf,"on");
            else sprintf(buf,"off");
            pSched->publish(name + "/switch/state", buf);
        }
        if (topic == name + "/switch/physicalstate/get") {
            char buf[32];
            if (physicalState) sprintf(buf,"on");
            else sprintf(buf,"off");
            pSched->publish(name + "/switch/physicalstate", buf);
        }
        if (topic ==name+"/switch/mode/set") {
            char buf[32];
            memset(buf,0,32);
            strncpy(buf,msg.c_str(),31);
            char *p=strchr(buf,' ');
            if (p) {
                *p=0;
                ++p;
            }
            if (!strcmp(buf,"default")) {
                setMode(Mode::Default);
            } else if (!strcmp(buf,"raising")) {
                setMode(Mode::Raising);
            } else if (!strcmp(buf,"falling")) {
                setMode(Mode::Falling);
            } else if (!strcmp(buf,"flipflop")) {
                setMode(Mode::Flipflop);
            } else if (!strcmp(buf,"timer")) {
                unsigned long dur=1000;
                if (p) dur=atol(p);
                setMode(Mode::Timer, dur);
            }
        }
        if (topic == name + "/switch/set") {
            char buf[32];
            memset(buf,0,32);
            strncpy(buf,msg.c_str(),31);
            if (!strcmp(buf,"on") || !strcmp(buf,"true")) {
                setPhysicalState(true, true);
            }
            if (!strcmp(buf,"off") || !strcmp(buf,"false")) {
                setPhysicalState(false, true);
            }
            if (!strcmp(buf,"toggle")) {
                setPhysicalState(!physicalState, true);
            }
            if (!strcmp(buf,"pulse")) {
                setPhysicalState(true, true);
                setPhysicalState(false, true);
            }
        }
        if (topic == name + "/switch/debounce/get") {
            char buf[32];
            sprintf(buf, "%ld", debounceTimeMs);
            pSched->publish(name + "/debounce", buf);
        }
        if (topic == name + "/switch/debounce/set") {
            long dbt=atol(msg.c_str());
            setDebounce(dbt);
        }
    };
};  // Switch

}  // namespace ustd
