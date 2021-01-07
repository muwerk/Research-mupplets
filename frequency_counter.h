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
    /*! Interrupt driven frequency counter.

    On an ESP32, measurement of frequencies at a GPIO are possible in ranges of 0-250kHz.
    There are six measurement modes:
    LOWFREQUENCY_FAST (good for measurements <50Hz, not much filtering)
    LOWFREQUENCY_MEDIUM (good for sub-Hertz enabled measurements, e.g. Geiger-counters)
    LOWFREQUENCY_LONGTERM (good for <50Hz, little deviation, high precision)

    The HIGHFREQUENCY modes automatically reset the filter, if the input signal drops to
    0Hz.
    HIGHFREQUENCY_FAST (no filtering)
    HIGHFREQUENCY_MEDIUM (some filtering)
    HIGHFREQUENCY_LONGTERM (string filtering, good for precision measurement of signals with little
    deviation)

    Precision <80kHz is better than 0.0005%. Interrupt load > 80kHz impacts the
    performance of the ESP32 severely and error goes up to 2-5%.
    */

  public:
    enum InterruptMode { IM_RISING, IM_FALLING, IM_CHANGE };
    enum MeasureMode {
        LOWFREQUENCY_FAST,
        LOWFREQUENCY_MEDIUM,
        LOWFREQUENCY_LONGTERM,
        HIGHFREQUENCY_FAST,
        HIGHFREQUENCY_MEDIUM,
        HIGHFREQUENCY_LONGTERM
    };
    String FREQUENCY_COUNTER_VERSION = "0.1.0";
    Scheduler *pSched;
    int tID;

    String name;
    uint8_t pin_input;
    uint8_t irqno_input;
    int8_t interruptIndex_input;
    MeasureMode measureMode;
    InterruptMode irqMode;

    bool detectZeroChange = false;
    bool irqsAttached = false;
    ustd::sensorprocessor frequency = ustd::sensorprocessor(4, 600, 0.01);
    double inputFrequencyVal = 0.0;

    double frequencyRenormalisation = 1.0;
    uint8_t ipin = 255;

#ifdef __ESP__
    HomeAssistant *pHA;
#endif

    FrequencyCounter(String name, uint8_t pin_input, int8_t interruptIndex_input,
                     MeasureMode measureMode = HIGHFREQUENCY_MEDIUM,
                     InterruptMode irqMode = InterruptMode::IM_FALLING)
        : name(name), pin_input(pin_input), interruptIndex_input(interruptIndex_input),
          measureMode(measureMode), irqMode(irqMode) {
        setMeasureMode(measureMode, true);
    }

    ~FrequencyCounter() {
        if (irqsAttached) {
            detachInterrupt(irqno_input);
        }
    }

    void setMeasureMode(MeasureMode mode, bool silent = false) {
        switch (mode) {
        case LOWFREQUENCY_FAST:
            detectZeroChange = false;
            measureMode = LOWFREQUENCY_FAST;
            frequency.smoothInterval = 4;
            frequency.pollTimeSec = 15;
            frequency.eps = 0.01;
            frequency.reset();
            break;
        case LOWFREQUENCY_MEDIUM:
            detectZeroChange = false;
            measureMode = LOWFREQUENCY_MEDIUM;
            frequency.smoothInterval = 12;
            frequency.pollTimeSec = 120;
            frequency.eps = 0.01;
            frequency.reset();
            break;
        case LOWFREQUENCY_LONGTERM:
            detectZeroChange = false;
            measureMode = LOWFREQUENCY_LONGTERM;
            frequency.smoothInterval = 60;
            frequency.pollTimeSec = 600;
            frequency.eps = 0.001;
            frequency.reset();
            break;
        case HIGHFREQUENCY_FAST:
            detectZeroChange = true;
            measureMode = HIGHFREQUENCY_FAST;
            frequency.smoothInterval = 1;
            frequency.pollTimeSec = 15;
            frequency.eps = 0.1;
            frequency.reset();
            break;
        case HIGHFREQUENCY_MEDIUM:
            detectZeroChange = true;
            measureMode = HIGHFREQUENCY_MEDIUM;
            frequency.smoothInterval = 10;
            frequency.pollTimeSec = 120;
            frequency.eps = 0.01;
            frequency.reset();
            break;
        default:
        case HIGHFREQUENCY_LONGTERM:
            detectZeroChange = true;
            measureMode = HIGHFREQUENCY_LONGTERM;
            frequency.smoothInterval = 60;
            frequency.pollTimeSec = 600;
            frequency.eps = 0.001;
            frequency.reset();
            break;
        }
        if (!silent)
            publishMeasureMode();
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

    void publishMeasureMode() {
        switch (measureMode) {
        case LOWFREQUENCY_FAST:
            pSched->publish(name + "/sensor/mode", "LOWFREQUENCY_FAST");
            break;
        case LOWFREQUENCY_MEDIUM:
            pSched->publish(name + "/sensor/mode", "LOWFREQUENCY_MEDIUM");
            break;
        case LOWFREQUENCY_LONGTERM:
            pSched->publish(name + "/sensor/mode", "LOWFREQUENCY_LONGTERM");
            break;
        case HIGHFREQUENCY_FAST:
            pSched->publish(name + "/sensor/mode", "HIGHFREQUENCY_FAST");
            break;
        case HIGHFREQUENCY_MEDIUM:
            pSched->publish(name + "/sensor/mode", "HIGHFREQUENCY_MEDIUM");
            break;
        case HIGHFREQUENCY_LONGTERM:
            pSched->publish(name + "/sensor/mode", "HIGHFREQUENCY_LONGTERM");
            break;
        }
    }

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
        if (topic == name + "/sensor/mode/set") {
            if (msg == "LOWFREQUENCY_FAST" || msg == "0") {
                setMeasureMode(MeasureMode::LOWFREQUENCY_FAST);
            }
            if (msg == "LOWFREQUENCY_MEDIUM" || msg == "1") {
                setMeasureMode(MeasureMode::LOWFREQUENCY_MEDIUM);
            }
            if (msg == "LOWFREQUENCY_LONGTERM" || msg == "2") {
                setMeasureMode(MeasureMode::LOWFREQUENCY_LONGTERM);
            }
            if (msg == "HIGHFREQUENCY_FAST" || msg == "3") {
                setMeasureMode(MeasureMode::HIGHFREQUENCY_FAST);
            }
            if (msg == "HIGHFREQUENCY_MEDIUM" || msg == "4") {
                setMeasureMode(MeasureMode::HIGHFREQUENCY_MEDIUM);
            }
            if (msg == "HIGHFREQUENCY_LONGTERM" || msg == "5") {
                setMeasureMode(MeasureMode::HIGHFREQUENCY_LONGTERM);
            }
        }
        if (topic == name + "/sensor/mode/get") {
            publishMeasureMode();
        }
    };
};  // FrequencyCounter

}  // namespace ustd
