// switch.h
#pragma once

#include "scheduler.h"
#include "home_assistant.h"

namespace ustd {

#ifdef __ESP32__
#define G_INT_ATTR IRAM_ATTR
#else
#ifdef __ESP__
#define G_INT_ATTR IRAM_ATTR
#else
#define G_INT_ATTR
#endif
#endif

#define USTD_MAX_IRQS (10)

volatile unsigned long irqcounter[USTD_MAX_IRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile unsigned long lastIrq[USTD_MAX_IRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile unsigned long debounceMs[USTD_MAX_IRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void G_INT_ATTR ustd_irq_master(uint8_t irqno) {
    unsigned long curr = millis();
    noInterrupts();
    if (debounceMs[irqno]) {
        if (timeDiff(lastIrq[irqno], curr) < debounceMs[irqno]) {
            interrupts();
            return;
        }
    }
    ++irqcounter[irqno];
    lastIrq[irqno] = curr;
    interrupts();
}

void G_INT_ATTR ustd_irq0() {
    ustd_irq_master(0);
}
void G_INT_ATTR ustd_irq1() {
    ustd_irq_master(1);
}
void G_INT_ATTR ustd_irq2() {
    ustd_irq_master(2);
}
void G_INT_ATTR ustd_irq3() {
    ustd_irq_master(3);
}
void G_INT_ATTR ustd_irq4() {
    ustd_irq_master(4);
}
void G_INT_ATTR ustd_irq5() {
    ustd_irq_master(5);
}
void G_INT_ATTR ustd_irq6() {
    ustd_irq_master(6);
}
void G_INT_ATTR ustd_irq7() {
    ustd_irq_master(7);
}
void G_INT_ATTR ustd_irq8() {
    ustd_irq_master(8);
}
void G_INT_ATTR ustd_irq9() {
    ustd_irq_master(9);
}

void (*ustd_irq_table[USTD_MAX_IRQS])() = {ustd_irq0, ustd_irq1, ustd_irq2, ustd_irq3, ustd_irq4,
                                           ustd_irq5, ustd_irq6, ustd_irq7, ustd_irq8, ustd_irq9};

unsigned long getResetIrqCount(uint8_t irqno) {
    unsigned long count = (unsigned long)-1;
    noInterrupts();
    if (irqno < USTD_MAX_IRQS) {
        count = irqcounter[irqno];
        irqcounter[irqno] = 0;
    }
    interrupts();
    return count;
}

class Switch {
  public:
    String SWITCH_VERSION = "0.1.0";
    enum Mode { Default, Rising, Falling, Flipflop, Timer, Duration };
    Scheduler *pSched;
    int tID;

    String name;
    uint8_t port;
    Mode mode;
    bool activeLogic;
    String customTopic;
    int8_t interruptIndex;
    unsigned long debounceTimeMs;

    bool useInterrupt = false;
    uint8_t ipin = 255;
    unsigned long lastChangeMs = 0;
    int physicalState = -1;
    int logicalState = -1;
    bool overriddenPhysicalState = false;
    bool overridePhysicalActive = false;

    bool flipflop = true;  // This starts with 'off', since state is initially changed once.
    unsigned long activeTimer = 0;
    unsigned long timerDuration = 1000;  // ms
    unsigned long startEvent = 0;        // ms
    unsigned long durations[2] = {3000, 30000};
#ifdef __ESP__
    HomeAssistant *pHA;
#endif

    Switch(String name, uint8_t port, Mode mode = Mode::Default, bool activeLogic = false,
           String customTopic = "", int8_t interruptIndex = -1, unsigned long debounceTimeMs = 0)
        : name(name), port(port), mode(mode), activeLogic(activeLogic), customTopic(customTopic),
          interruptIndex(interruptIndex), debounceTimeMs(debounceTimeMs) {
        setMode(mode);
    }

    ~Switch() {
        if (useInterrupt)
            detachInterrupt(ipin);
    }

    void setDebounce(long ms) {
        if (ms < 0)
            ms = 0;
        if (ms > 1000)
            ms = 1000;
        debounceTimeMs = (unsigned long)ms;
    }

    void setTimerDuration(unsigned long ms) {
        timerDuration = ms;
    }

    void setMode(Mode newmode, unsigned long duration = 0) {
        if (useInterrupt)
            flipflop = false;  // This starts with 'off', since state is
                               // initially changed once.
        else
            flipflop = true;  // This starts with 'off', since state is
                              // initially changed once.
        activeTimer = 0;
        timerDuration = duration;
        physicalState = -1;
        logicalState = -1;
        overriddenPhysicalState = false;
        overridePhysicalActive = false;
        lastChangeMs = 0;
        mode = newmode;
        startEvent = (unsigned long)-1;
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        pinMode(port, INPUT_PULLUP);

        if (interruptIndex >= 0 && interruptIndex < USTD_MAX_IRQS) {
#ifdef __ESP32__
            ipin = digitalPinToInterrupt(port);
#else
            ipin = digitalPinToInterrupt(port);
#endif
            switch (mode) {
            case Mode::Falling:
                attachInterrupt(ipin, ustd_irq_table[interruptIndex], FALLING);
                break;
            case Mode::Rising:
                attachInterrupt(ipin, ustd_irq_table[interruptIndex], RISING);
                break;
            default:
                attachInterrupt(ipin, ustd_irq_table[interruptIndex], CHANGE);
                break;
            }
            debounceMs[interruptIndex] = debounceTimeMs;
            useInterrupt = true;
        }

        readState();

        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        /* std::function<void()> */ auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 50000);

        /* std::function<void(String, String, String)> */
        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/switch/#", fnall);
        pSched->subscribe(tID, "mqtt/state", fnall);
    }

#ifdef __ESP__
    void registerHomeAssistant(String homeAssistantFriendlyName, String projectName = "",
                               String customIcon = "mdi:light-switch",
                               String homeAssistantDiscoveryPrefix = "homeassistant") {
        pHA = new HomeAssistant(name, tID, homeAssistantFriendlyName, projectName, SWITCH_VERSION,
                                homeAssistantDiscoveryPrefix);
        pHA->addSwitch(customIcon);
        pHA->begin(pSched);
    }
#endif

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
            if (customTopic != "")
                pSched->publish(customTopic, textState);
            break;
        case Mode::Rising:
            if (lState == true) {
                pSched->publish(name + "/switch/state", "trigger");
                if (customTopic != "")
                    pSched->publish(customTopic, "trigger");
            }
            break;
        case Mode::Falling:
            if (lState == false) {
                pSched->publish(name + "/switch/state", "trigger");
                if (customTopic != "")
                    pSched->publish(customTopic, "trigger");
            }
            break;
        case Mode::Duration:
            if (lState == true) {
                startEvent = millis();
            } else {
                if (startEvent != (unsigned long)-1) {
                    unsigned long dt = timeDiff(startEvent, millis());
                    char msg[32];
                    sprintf(msg, "%ld", dt);
                    pSched->publish(name + "/switch/duration", msg);
                    if (dt < durations[0]) {
                        pSched->publish(name + "/switch/shortpress", "trigger");
                    } else if (dt < durations[1]) {
                        pSched->publish(name + "/switch/longpress", "trigger");
                    } else {
                        pSched->publish(name + "/switch/verylongpress", "trigger");
                    }
                }
            }
            break;
        }
    }

    void setLogicalState(bool newLogicalState) {
        if (logicalState != newLogicalState) {
            logicalState = newLogicalState;
            publishLogicalState(logicalState);
        }
    }

    void decodeLogicalState(bool physicalState) {
        switch (mode) {
        case Mode::Default:
        case Mode::Rising:
        case Mode::Falling:
        case Mode::Duration:
            setLogicalState(physicalState);
            break;
        case Mode::Flipflop:
            if (physicalState == false) {
                flipflop = !flipflop;
                setLogicalState(flipflop);
            }
            break;
        case Mode::Timer:
            if (physicalState == false) {
                activeTimer = millis();
            } else {
                setLogicalState(true);
            }
            break;
        }
    }

    void setPhysicalState(bool newState, bool override) {
        if (mode != Mode::Timer) {
            activeTimer = 0;
        }
        if (override) {
            overriddenPhysicalState = physicalState;
            overridePhysicalActive = true;
            if (newState != physicalState) {
                physicalState = newState;
                decodeLogicalState(newState);
            }
        } else {
            if (overridePhysicalActive && newState != overriddenPhysicalState) {
                overridePhysicalActive = false;
            }
            if (overridePhysicalActive)
                return;
            if (newState != physicalState || mode == Mode::Falling || mode == Mode::Rising) {
                if (timeDiff(lastChangeMs, millis()) > debounceTimeMs || useInterrupt) {
                    lastChangeMs = millis();
                    physicalState = newState;
                    decodeLogicalState(physicalState);
                }
            }
        }
    }

    void readState() {
        if (useInterrupt) {
            unsigned long count = getResetIrqCount(interruptIndex);
            int curstate = digitalRead(port);
            char msg[32];
            if (count) {
                sprintf(msg, "%ld", count);
                pSched->publish(name + "/switch/irqcount/0", msg);
                if (curstate == HIGH)
                    curstate = true;
                else
                    curstate = false;
                switch (mode) {
                case Mode::Rising:
                    for (unsigned long i = 0; i < count; i++) {
                        if (activeLogic) {
                            setPhysicalState(false, false);
                            setPhysicalState(true, false);
                        } else {
                            setPhysicalState(true, false);
                            setPhysicalState(false, false);
                        }
                    }
                    break;
                case Mode::Falling:
                    for (unsigned long i = 0; i < count; i++) {
                        if (activeLogic) {
                            setPhysicalState(true, false);
                            setPhysicalState(false, false);
                        } else {
                            setPhysicalState(false, false);
                            setPhysicalState(true, false);
                        }
                    }
                    break;
                default:
                    bool iState = ((count % 2) == 0);
                    if (curstate)
                        iState = !iState;
                    for (unsigned long i = 0; i < count; i++) {
                        if (activeLogic) {
                            setPhysicalState(iState, false);
                        } else {
                            setPhysicalState(!iState, false);
                        }
                        iState = !iState;
                    }
                    break;
                }
            }
        } else {
            int newstate = digitalRead(port);
            if (newstate == HIGH)
                newstate = true;
            else
                newstate = false;
            if (!activeLogic)
                newstate = !newstate;
            setPhysicalState(newstate, false);
        }
    }

    void loop() {
        readState();
        if (mode == Mode::Timer && activeTimer) {
            if (timeDiff(activeTimer, millis()) > timerDuration) {
                activeTimer = 0;
                setLogicalState(false);
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/switch/state/get") {
            char buf[32];
            if (logicalState)
                sprintf(buf, "on");
            else
                sprintf(buf, "off");
            pSched->publish(name + "/switch/state", buf);
        }
        if (topic == name + "/switch/physicalstate/get") {
            char buf[32];
            if (physicalState)
                sprintf(buf, "on");
            else
                sprintf(buf, "off");
            pSched->publish(name + "/switch/physicalstate", buf);
        }
        if (topic == name + "/switch/mode/set") {
            char buf[32];
            memset(buf, 0, 32);
            strncpy(buf, msg.c_str(), 31);
            char *p = strchr(buf, ' ');
            char *p2 = nullptr;
            if (p) {
                *p = 0;
                ++p;
                p2 = strchr(p, ',');
                if (p2) {
                    *p2 = 0;
                    ++p2;
                }
            }
            if (!strcmp(buf, "default")) {
                setMode(Mode::Default);
            } else if (!strcmp(buf, "rising")) {
                setMode(Mode::Rising);
            } else if (!strcmp(buf, "falling")) {
                setMode(Mode::Falling);
            } else if (!strcmp(buf, "flipflop")) {
                setMode(Mode::Flipflop);
            } else if (!strcmp(buf, "timer")) {
                unsigned long dur = 1000;
                if (p)
                    dur = atol(p);
                setMode(Mode::Timer, dur);
            } else if (!strcmp(buf, "duration")) {
                durations[0] = 3000;
                durations[1] = 30000;
                if (p) {
                    durations[0] = atol(p);
                    if (p2)
                        durations[1] = atol(p2);
                }
                if (durations[0] > durations[1]) {
                    durations[1] = (unsigned long)-1;
                }
                setMode(Mode::Duration);
            }
        }
        if (topic == name + "/switch/set") {
            char buf[32];
            memset(buf, 0, 32);
            strncpy(buf, msg.c_str(), 31);
            if (!strcmp(buf, "on") || !strcmp(buf, "true")) {
                // setPhysicalState(true, true);
                setLogicalState(true);
            }
            if (!strcmp(buf, "off") || !strcmp(buf, "false")) {
                // setPhysicalState(false, true);
                setLogicalState(false);
            }
            if (!strcmp(buf, "toggle")) {
                setPhysicalState(!physicalState, true);
            }
            if (!strcmp(buf, "pulse")) {
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
            long dbt = atol(msg.c_str());
            setDebounce(dbt);
        }
        if (topic == "mqtt/state") {
            if (mode == Mode::Default || mode == Mode::Flipflop) {
                if (msg == "connected") {
                    publishLogicalState(logicalState);
                }
            }
        }
    };
};  // Switch

}  // namespace ustd
