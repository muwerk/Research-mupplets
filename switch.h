// switch.h
#pragma once

#include "scheduler.h"

namespace ustd {
class Switch {
  public:
    enum Mode { NONE, BOTH, ON, OFF };
    Scheduler *pSched;
    int tID;

    String name;
    uint8_t port;
    bool activeLogic;
    unsigned long debounceTimeMs;
    Mode mode;
    String topic;

    unsigned long lastChangeMs = 0;
    time_t lastChangeTime = 0;
    int state = -1;

    Switch(String name, uint8_t port, bool activeLogic = false, unsigned long debounceTimeMs = 20,
           Mode mode = Mode::NONE,
           String topic = "")
        : name(name), port(port), activeLogic(activeLogic), debounceTimeMs(debounceTimeMs),
          mode(mode), topic(topic) {
    }

    ~Switch() {
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
        pSched->publish(name + "/state", textState);
        if (state == LOW) {
            if (mode == Mode::BOTH ||
                mode == Mode::ON) {
                pSched->publish(topic, "on");
            }
        } else {
            if (mode == Mode::BOTH ||
                mode == Mode::OFF) {
                pSched->publish(topic, "off");
            }
        }
    }

    int readState() {
        int newstate = digitalRead(port);
        if (newstate==HIGH) newstate=true;
        else newstate=false;
        if (!activeLogic) newstate=!newstate;
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
                    state = newstate;
                    lastChangeMs = millis();
                    lastChangeTime = time(nullptr);
                    publishState();
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
        if (topic == name + "/switch/set") {
            // char buf[32];
            // sprintf(buf, "%5.3f", brightlevel);
            // pSched->publish(name + "/unitluminosity", buf);
        }
    };
};  // Switch

}  // namespace ustd
