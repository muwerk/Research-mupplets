// frequency_counter.h
#pragma once

#include "scheduler.h"
#include "home_assistant.h"

namespace ustd {

#ifdef __ESP32__
#define G_INT_ATTR IRAM_ATTR
#else
#ifdef __ESP__
#define G_INT_ATTR ICACHE_RAM_ATTR
#else
#define G_INT_ATTR
#endif
#endif

#define USTD_MAX_PIRQS (10)

volatile unsigned long pirqcounter[USTD_MAX_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile unsigned long plastIrqTimer[USTD_MAX_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
volatile unsigned long pbeginIrqTimer[USTD_MAX_PIRQS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void G_INT_ATTR ustd_pirq_master(uint8_t irqno) {
    unsigned long curr = micros();
    if (pbeginIrqTimer[irqno] == 0)
        pbeginIrqTimer[irqno] = curr;
    else
        ++pirqcounter[irqno];
    plastIrqTimer[irqno] = curr;
}

void G_INT_ATTR ustd_pirq0() {
    ustd_pirq_master(0);
}
void G_INT_ATTR ustd_pirq1() {
    ustd_pirq_master(1);
}
void G_INT_ATTR ustd_pirq2() {
    ustd_pirq_master(2);
}
void G_INT_ATTR ustd_pirq3() {
    ustd_pirq_master(3);
}
void G_INT_ATTR ustd_pirq4() {
    ustd_pirq_master(4);
}
void G_INT_ATTR ustd_pirq5() {
    ustd_pirq_master(5);
}
void G_INT_ATTR ustd_pirq6() {
    ustd_pirq_master(6);
}
void G_INT_ATTR ustd_pirq7() {
    ustd_pirq_master(7);
}
void G_INT_ATTR ustd_pirq8() {
    ustd_pirq_master(8);
}
void G_INT_ATTR ustd_pirq9() {
    ustd_pirq_master(9);
}

void (*ustd_pirq_table[USTD_MAX_PIRQS])() = {ustd_pirq0, ustd_pirq1, ustd_pirq2, ustd_pirq3,
                                             ustd_pirq4, ustd_pirq5, ustd_pirq6, ustd_pirq7,
                                             ustd_pirq8, ustd_pirq9};

unsigned long getResetpIrqCount(uint8_t irqno) {
    unsigned long count = (unsigned long)-1;
    noInterrupts();
    if (irqno < USTD_MAX_PIRQS) {
        count = pirqcounter[irqno];
        pirqcounter[irqno] = 0;
    }
    interrupts();
    return count;
}

double frequencyMultiplicator = 1000000.0;
double getResetpIrqFrequency(uint8_t irqno, unsigned long minDtUs = 50) {
    double frequency = 0.0;
    noInterrupts();
    if (irqno < USTD_MAX_PIRQS) {
        unsigned long count = pirqcounter[irqno];
        unsigned long dt = timeDiff(pbeginIrqTimer[irqno], plastIrqTimer[irqno]);
        if (dt > minDtUs) {                                     // Ignore small Irq flukes
            frequency = (count * frequencyMultiplicator) / dt;  // = count/2.0*1000.0000 uS / dt;
                                                                // no. of waves (count/2) / dt.
        }
        pbeginIrqTimer[irqno] = 0;
        pirqcounter[irqno] = 0;
        plastIrqTimer[irqno] = 0;
    }
    interrupts();
    return frequency;
}

class FrequencyCounter {
  public:
    enum InterruptMode { IM_RISING, IM_FALLING, IM_CHANGE };
    String FREQUENCY_COUNTER_VERSION = "0.1.0";
    Scheduler *pSched;
    int tID;

    String name;
    uint8_t pin_input;
    uint8_t irqno_input;
    int8_t interruptIndex_input;
    InterruptMode irqMode;
    bool detectZeroChange;

    bool irqsAttached = false;
    ustd::sensorprocessor frequency = ustd::sensorprocessor(4, 600, 0.01);
    double inputFrequencyVal = 0.0;

    double frequencyRenormalisation = 1.0;
    uint8_t ipin = 255;

#ifdef __ESP__
    HomeAssistant *pHA;
#endif

    FrequencyCounter(String name, uint8_t pin_input, int8_t interruptIndex_input,
                     InterruptMode irqMode = InterruptMode::IM_FALLING,
                     bool detectZeroChange = true)
        : name(name), pin_input(pin_input), interruptIndex_input(interruptIndex_input),
          irqMode(irqMode), detectZeroChange(detectZeroChange) {
    }

    ~FrequencyCounter() {
        if (irqsAttached) {
            detachInterrupt(irqno_input);
        }
    }

    bool begin(Scheduler *_pSched) {
        pSched = _pSched;

        pinMode(pin_input, INPUT_PULLUP);

        if (interruptIndex_input >= 0 && interruptIndex_input < USTD_MAX_PIRQS) {
            irqno_input = digitalPinToInterrupt(pin_input);
            switch (irqMode) {
            case IM_FALLING:
                attachInterrupt(irqno_input, ustd_pirq_table[interruptIndex_input], FALLING);
                frequencyMultiplicator = 1000000.0;
                break;
            case IM_RISING:
                attachInterrupt(irqno_input, ustd_pirq_table[interruptIndex_input], RISING);
                frequencyMultiplicator = 1000000.0;
                break;
            case IM_CHANGE:
                attachInterrupt(irqno_input, ustd_pirq_table[interruptIndex_input], CHANGE);
                frequencyMultiplicator = 500000.0;
                break;
            }
            irqsAttached = true;
        } else {
            return false;
        }

        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 2000000);  // uS schedule

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/frequency/#", fnall);
        return true;
    }

#ifdef __ESP__
    void registerHomeAssistant(String homeAssistantFriendlyName, String projectName = "",
                               String icon = "mdi:gauge",
                               String homeAssistantDiscoveryPrefix = "homeassistant") {
        pHA = new HomeAssistant(name, tID, homeAssistantFriendlyName, projectName,
                                FREQUENCY_COUNTER_VERSION, homeAssistantDiscoveryPrefix);
        pHA->addSensor("frequency", "Frequency", "Hz", "None", icon);
        pHA->begin(pSched);
        publish();
    }
#endif

    void publish_frequency() {
        char buf[32];
        sprintf(buf, "%9.1f", inputFrequencyVal);
        char *p1 = buf;
        while (*p1 == ' ')
            ++p1;
        pSched->publish(name + "/sensor/frequency", p1);
    }

    void publish() {
        publish_frequency();
    }

    void loop() {
        double freq = getResetpIrqFrequency(interruptIndex_input, 0) * frequencyRenormalisation;
        if (detectZeroChange) {
            if ((frequency.lastVal == 0.0 && freq > 0.0) ||
                (frequency.lastVal > 0.0 && freq == 0.0))
                frequency.reset();
        }
        if (freq >= 0.0 && freq < 1000000.0) {
            if (frequency.filter(&freq)) {
                inputFrequencyVal = freq;
                publish_frequency();
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/state/get") {
            publish();
        }
        if (topic == name + "/sensor/frequency/get") {
            publish_frequency();
        }
    };
};  // FrequencyCounter

}  // namespace ustd
