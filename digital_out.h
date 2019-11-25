// led.h
#pragma once

#include "scheduler.h"
#include "mup_util.h"
#include "home_assistant.h"

namespace ustd {

class DigitalOut {
  public:
    String DIGITALOUT_VERSION="0.1.0";
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t port;
    bool activeLogic = false;
    bool state;
    
    #ifdef __ESP__
    HomeAssistant *pHA;
    #endif

    DigitalOut(String name, uint8_t port, bool activeLogic=false )
        : name(name), port(port), activeLogic(activeLogic) {
    }

    ~DigitalOut() {
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;
		pinMode(port, OUTPUT);
        
        setOff();
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 50000);
        auto fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/switch/#", fnall);
    }

    #ifdef __ESP__
    void registerHomeAssistant(String homeAssistantFriendlyName, String projectName="", String homeAssistantDiscoveryPrefix="homeassistant") {
        pHA=new HomeAssistant(name, tID, homeAssistantFriendlyName, projectName, DIGITALOUT_VERSION, homeAssistantDiscoveryPrefix);
        pHA->addSwitch();
        pHA->begin(pSched);
    }
    #endif

    void setOn() {
        this->state=true;
        if (activeLogic) {
            digitalWrite(port, true);
        } else {
            digitalWrite(port, false);
        }
    }
    void setOff() {
        this->state=false;
        if (!activeLogic) {
            digitalWrite(port, true);
        } else {
            digitalWrite(port, false);
        }
    }

    void set(bool state) {
        if (state==this->state) return;
        this->state=state;
        if (state) {
            setOn();
            pSched->publish(name + "/switch/state", "on");
        } else {
            setOff();
            pSched->publish(name + "/switch/state", "off");
        }
    }

    void publishState() {
        if (state) {
            pSched->publish(name + "/switch/state", "on");
            this->state=true;
        } else {
            pSched->publish(name + "/switch/state", "off");
            this->state=false;
        }
    }

    void loop() {
    }

    void subsMsg(String topic, String msg, String originator) {
        char msgbuf[128];
        memset(msgbuf,0,128);
        strncpy(msgbuf,msg.c_str(),127);
        msg.toLowerCase();
        if (topic == name + "/switch/set") {
            set((msg=="on" || msg=="1"));
        }
    };
};  // DigitalOut

}  // namespace ustd
