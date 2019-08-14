// switch.h
#pragma once

#include "scheduler.h"

namespace ustd {
class Switch {
  public:
    enum ModeOld { NONE, BOTH, ON, OFF };
    enum Mode { DEFAULT, RAISING, FALLING, FLIPFLOP, TIMER}
    enum StatMode {}
    Scheduler *pSched;
    int tID;

    String name;
    uint8_t port;
    bool activeLogic;
    unsigned long debounceTimeMs;
    Mode mode;
    String customTopic;

    unsigned long lastChangeMs = 0;
    time_t lastChangeTime = 0;
    int state = -1;
    bool overridden_state=false;
    bool override_active=false;

    Switch(String name, uint8_t port, bool activeLogic = false, unsigned long debounceTimeMs = 20,
           Mode mode = Mode::NONE,
           String customTopic = "")
        : name(name), port(port), activeLogic(activeLogic), debounceTimeMs(debounceTimeMs),
          mode(mode), customTopic(customTopic) {
    }

    ~Switch() {
    }

    void setDebounce(long ms) {
        if (ms<0) ms=0;
        if (ms>1000) ms=1000;
        debounceTimeMs=(unsigned long)ms;
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
        pSched->publish(name + "/switch/state", textState);
        if (state == LOW) {
            if (mode == Mode::BOTH ||
                mode == Mode::ON) {
                //Serial.println("SW:"+textState);
                pSched->publish(customTopic, "on");
            }
        } else {
            if (mode == Mode::BOTH ||
                mode == Mode::OFF) {
                //Serial.println("SW:"+textState);
                pSched->publish(customTopic, "off");
            }
        }
    }

    void setState(bool newstate, bool override=false) {
        if (override) {
            overridden_state=state;
            override_active=true;
        }
        state = newstate;
        lastChangeTime = time(nullptr);
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
