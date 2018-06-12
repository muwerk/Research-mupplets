// switch.h
#pragma once

#include "scheduler.h"

namespace ustd {
class Switch {
  public:
    enum customtopic_t { NONE, BOTH, ON, OFF };
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t port;
    unsigned long lastChangeMs = 0;
    time_t lastChangeTime = 0;
    unsigned long debounceTimeMs;
    int state = -1;
    customtopic_t customTopicType;
    String customTopic;

    uint8_t mode;

    Switch(String name, uint8_t port, unsigned long debounceTimeMs = 20,
           customtopic_t customTopicType = customtopic_t::NONE,
           String customTopic = "")
        : name(name), port(port), debounceTimeMs(debounceTimeMs),
          customTopicType(customTopicType), customTopic(customTopic) {
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
        tID = pSched->add(ft, name, 10000);

        std::function<void(String, String, String)> fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/switch/#", fnall);
    }

    void publishState() {
        String textState;
        if (state == LOW)
            textState = "on";
        else
            textState = "off";
        pSched->publish(name + "/state", textState);
        if (state == LOW) {
            if (customTopicType == customtopic_t::BOTH ||
                customTopicType == customtopic_t::ON) {
                pSched->publish(customTopic, "on");
            }
        } else {
            if (customTopicType == customtopic_t::BOTH ||
                customTopicType == customtopic_t::OFF) {
                pSched->publish(customTopic, "off");
            }
        }
    }

    int readState() {
        int newstate = digitalRead(port);
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
                if (time(nullptr) - lastChangeTime > 2 ||
                    timeDiff(lastChangeMs, millis()) > debounceTimeMs) {
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
