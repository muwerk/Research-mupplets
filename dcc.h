// dcc.h

// After:
// NMRA National Model Railroad Association
// Standard S-9.1, Electrical standards for digital command control
// Version 2006
// https://www.nmra.org/index-nmra-standards-and-recommended-practices
// https://www.nmra.org/sites/default/files/standards/sandrp/pdf/s-9.1_electrical_standards_2006.pdf

#pragma once

#include "scheduler.h"
//#include "home_assistant.h"

namespace ustd {

#ifdef __ESP32__
#define G_INT_ATTR IRAM_ATTR
#else
#ifdef __ESP__
#error ESP8266 not yet supported!
#define G_INT_ATTR ICACHE_RAM_ATTR
#else
#define G_INT_ATTR
#endif
#endif

#define USTD_MAX_DCC_TIMERS (4)

hw_timer_t * dccTimer[4] = {NULL,NULL,NULL,NULL};
portMUX_TYPE dccTimerMux[4] = {portMUX_INITIALIZER_UNLOCKED,portMUX_INITIALIZER_UNLOCKED,portMUX_INITIALIZER_UNLOCKED,portMUX_INITIALIZER_UNLOCKED};
volatile bool waveState[4]={false, false, false, false};
volatile uint8_t wavePin[4]={0xff,0xff,0xff,0xff};

void G_INT_ATTR ustd_dcc_timer_irq_master(uint8_t timerNo) {
    portENTER_CRITICAL(&(dccTimerMux[timerNo])); // generate 58uSec changes on pin_enable1 port as default PWM
    if (waveState[timerNo]) {
        waveState[timerNo]=false;
        digitalWrite(wavePin[timerNo],false);
    } else {
        waveState[timerNo]=true;
        digitalWrite(wavePin[timerNo],true);
    }
    portEXIT_CRITICAL(&(dccTimerMux[timerNo]));
}

void G_INT_ATTR ustd_dcc_timer_irq0() {ustd_dcc_timer_irq_master(0);}
void G_INT_ATTR ustd_dcc_timer_irq1() {ustd_dcc_timer_irq_master(1);}
void G_INT_ATTR ustd_dcc_timer_irq2() {ustd_dcc_timer_irq_master(2);}
void G_INT_ATTR ustd_dcc_timer_irq3() {ustd_dcc_timer_irq_master(3);}

void (*ustd_dcc_timer_irq_table[USTD_MAX_DCC_TIMERS])()={ustd_dcc_timer_irq0, ustd_dcc_timer_irq1,ustd_dcc_timer_irq2,ustd_dcc_timer_irq3};

class Dcc {
  public:
    String DCC_VERSION="0.1.0";
    enum Mode { Default, Rising, Falling, Flipflop, Timer, Duration};
    Scheduler *pSched;
    int tID;

    String name;
    uint8_t pin_in1,pin_in2,pin_enable1;
    uint8_t iTimer;
    bool timerStarted=false;

    /*
    #ifdef __ESP__
    HomeAssistant *pHA;
    #endif
    */

    Dcc(String name, uint8_t pin_in1, uint8_t pin_in2, uint8_t pin_enable1, int8_t iTimer=0)
        : name(name), pin_in1(pin_in1), pin_in2(pin_in2), pin_enable1(pin_enable1), iTimer(iTimer) {
    }

    ~Dcc() {
        if (timerStarted) {
            timerEnd(dccTimer[iTimer]);
            timerStarted=false;
        }
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        digitalWrite(pin_in1, true);
        digitalWrite(pin_in2, false);
        digitalWrite(pin_enable1, false);
        pinMode(pin_in1, OUTPUT);
        pinMode(pin_in2, OUTPUT);
        pinMode(pin_enable1, OUTPUT);

        if (iTimer>=0 && iTimer<USTD_MAX_DCC_TIMERS) {
            // Use iTimer=[0..3] timer.
            // Set 8 divider for prescaler, ->100ns. (see ESP32 Technical Reference Manual for more
            // info).
            dccTimer[iTimer] = timerBegin(iTimer, 8, true);
            // Attach onTimer function to our timer.
            timerAttachInterrupt(dccTimer[iTimer], ustd_dcc_timer_irq_table[iTimer], true);
            // Set alarm to call onTimer function every second (value in microseconds).
            // Repeat the alarm (third parameter)
            timerAlarmWrite(dccTimer[iTimer], 580, true); // every 58 uSec (580*100ns)
            // Start repeating alarm
            timerStarted=true;
            wavePin[iTimer]=pin_enable1;
            timerAlarmEnable(dccTimer[iTimer]);
        }

        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 50000);

        auto fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/dcc/#", fnall);
    }

/*
    #ifdef __ESP__
    void registerHomeAssistant(String homeAssistantFriendlyName, String projectName="", String homeAssistantDiscoveryPrefix="homeassistant") {
        pHA=new HomeAssistant(name, tID, homeAssistantFriendlyName, projectName, DCC_VERSION, homeAssistantDiscoveryPrefix);
        pHA->addDcc();
        pHA->begin(pSched);
    }
    #endif
*/

    void loop() {
        if (timerStarted) {

        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/dcc/state/get") {
            char buf[32];
            sprintf(buf,"bad");
            pSched->publish(name + "/dcc/state", buf);
        }
    }
};  // Dcc

}  // namespace ustd
