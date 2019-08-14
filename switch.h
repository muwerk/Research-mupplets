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
    time_t lastChangeTime = 0;
    int state = -1;
    bool overridden_state=false;
    bool override_active=false;
    bool flipflop = false;
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

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        pinMode(port, INPUT_PULLUP);
        state = readState();

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

    void publishState() {
        String textState;
        if (state == true)
            textState = "on";
        else
            textState = "off";
        //Serial.println("SW:"+textState);
        if (mode!=Mode::Timer) {
            activeTimer=0;
        }
        switch (mode) {
            case Mode::Default:
                pSched->publish(name + "/switch/state", textState);
                if (customTopic!="") 
                    pSched->publish(customTopic, textState);
                break;
            case Mode::Raising:
                if (state==true) {
                    pSched->publish(name + "/switch/state", textState);
                    if (customTopic!="") 
                        pSched->publish(customTopic, textState);
                }
                break;
            case Mode::Falling:
                if (state==false) {
                    pSched->publish(name + "/switch/state", textState);
                    if (customTopic!="") 
                        pSched->publish(customTopic, textState);
                }
                break;
            case Mode::Flipflop:
                if (state==false) {
                    flipflop = !flipflop;
                    pSched->publish(name + "/switch/state", flipflop);
                    if (customTopic!="") 
                        pSched->publish(customTopic, flipflop);
                }
                break;
            case Mode::Timer:
                if (state==false) {
                    activeTimer=Millis();
                }
                break;
        }
    }

    void setState(bool newstate, bool override=false) {
        if (override) {
            overridden_state=state;
            override_active=true;
        }
        if (!(newstate==false && mode==Mode::Timer) && mode!=Mode::Flipflop) {
            state = newstate;
            lastChangeTime = time(nullptr);
        }
        publishState();
    }

    int readState() {
        int newstate = digitalRead(port);
        if (newstate==HIGH) newstate=true;
        else newstate=false;
        if (!activeLogic) newstate=!newstate;

        if (override_active) {
            if (newstate==overridden_state) return state;
            override_active=false;
        }
        if (state == -1) {
            state = newstate;
            lastChangeMs = millis();
            lastChangeTime = time(nullptr);
            publishState();
            return state;
        } else {
            if (state == newstate)
                return state;
            else {
                if (timeDiff(lastChangeMs, millis()) > debounceTimeMs) {
                    lastChangeMs = millis();
                    setState(newstate);
                    return state;
                } else {
                    return state;
                }
            }
        }
    }

    void loop() {
        readState();
        if (mode==Mode::Timer && activeTimer) {
            if (timeDiff(activeTimer, Millis()) > timerDuration) {
                activeTimer=0;
                state=false;
                lastChangeTime = time(nullptr);
                pSched->publish(name + "/switch/state", "off");
                if (customTopic!="") 
                    pSched->publish(customTopic, "off");
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/switch/state/get") {
            char buf[32];
            if (state) sprintf(buf,"on");
            else sprintf(buf,"off");
            pSched->publish(name + "/switch/state", buf);
        }
        if (topic == name + "/switch/set") {
            char buf[32];
            memset(buf,0,32);
            strncpy(buf,msg.c_str(),31);
            if (!strcmp(buf,"on") || !strcmp(buf,"true")) {
                setState(true, true);
            }
            if (!strcmp(buf,"off") || !strcmp(buf,"false")) {
                setState(false, true);
            }
            if (!strcmp(buf,"toggle")) {
                setState(!state, true);
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
