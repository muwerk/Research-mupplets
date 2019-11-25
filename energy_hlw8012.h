// License: This module is based on the HLW8012 project, license is GPL-3
#pragma once

#include "scheduler.h"
#include "home_assistant.h"

#include <HLW8012.h>

bool HLW_IRQ_IN_USE=false;  // currently only one instance is allowed!
HLW8012 hlw8012;

// from: https://github.com/xoseperez/hlw8012/blob/master/examples/HLW8012_Interrupts/HLW8012_Interrupts.ino
// When using interrupts we have to call the library entry point
// whenever an interrupt is triggered
void ICACHE_RAM_ATTR hlw8012_cf1_interrupt() {
    hlw8012.cf1_interrupt();
}
void ICACHE_RAM_ATTR hlw8012_cf_interrupt() {
    hlw8012.cf_interrupt();
}

#define CURRENT_RESISTOR                0.001
#define VOLTAGE_RESISTOR_UPSTREAM       ( 5 * 470000 )
#define VOLTAGE_RESISTOR_DOWNSTREAM     ( 1000 )

namespace ustd {
class EnergyHlw8012 {
  private:
    String ENERGYHLW8012_VERSION="0.1.0";
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t pin_CF, pin_CF1, pin_SELi;
    bool current_mode;
    double voltageValue;
    double currentValue;
    double activePowerValue;
    double apparentPowerValue;
    double energyValue;
#ifdef __ESP__
    HomeAssistant *pHA;
#endif

  public:
    ustd::sensorprocessor voltageSensor = ustd::sensorprocessor(4, 600, 0.1);
    ustd::sensorprocessor currentSensor = ustd::sensorprocessor(4, 600, 0.1);
    ustd::sensorprocessor activePowerSensor = ustd::sensorprocessor(4, 600, 0.1);
    ustd::sensorprocessor apparentPowerSensor = ustd::sensorprocessor(4, 600, 0.1);
    ustd::sensorprocessor energySensor = ustd::sensorprocessor(4, 600, 0.1);
    
    EnergyHlw8012(String name, uint8_t pin_CF, uint8_t pin_CF1, uint8_t pin_SELi, bool current_mode=true) : name(name), pin_CF(pin_CF), pin_CF1(pin_CF1), pin_SELi(pin_SELi), current_mode(current_mode) {
    }

    ~EnergyHlw8012() {
    }

    void publishVoltage() {
        char buf[32];
        sprintf(buf,"%6.1f",voltageValue);
        pSched->publish(name + "/sensor/voltage", buf);
    }

    void publishCurrent() {
        char buf[32];
        sprintf(buf,"%6.1f",currentValue);
        pSched->publish(name + "/sensor/current", buf);
    }

    void publishActivePower() {
        char buf[32];
        sprintf(buf,"%6.1f",activePowerValue);
        pSched->publish(name + "/sensor/activepower", buf);
    }

    void publishApparentPower() {
        char buf[32];
        sprintf(buf,"%6.1f",apparentPowerValue);
        pSched->publish(name + "/sensor/apparentpower", buf);
    }

    void publishEnergy() {
        char buf[32];
        sprintf(buf,"%6.1f",energyValue);
        pSched->publish(name + "/sensor/energy", buf);
    }

    void publish() {
        publishVoltage();
        publishCurrent();
        publishActivePower();
        publishApparentPower();
        publishEnergy();
    }

    bool begin(Scheduler *_pSched) {
        if (HLW_IRQ_IN_USE) return false; // Can't have two instances!
        pSched = _pSched;
        HLW_IRQ_IN_USE=true;

        // from: https://github.com/xoseperez/hlw8012/blob/master/examples/HLW8012_Interrupts/HLW8012_Interrupts.ino
        // * cf_pin, cf1_pin and sel_pin are GPIOs to the HLW8012 IC
        // * currentWhen is the value in sel_pin to select current sampling
        // * set use_interrupts to true to use interrupts to monitor pulse widths
        // * leave pulse_timeout to the default value, recommended when using interrupts
        hlw8012.begin(pin_CF, pin_CF1, pin_SELi, current_mode, true);

        // These values are used to calculate current, voltage and power factors as per datasheet formula
        // These are the nominal values for the Sonoff POW resistors:
        // * The CURRENT_RESISTOR is the 1milliOhm copper-manganese resistor in series with the main line
        // * The VOLTAGE_RESISTOR_UPSTREAM are the 5 470kOhm resistors in the voltage divider that feeds the V2P pin in the HLW8012
        // * The VOLTAGE_RESISTOR_DOWNSTREAM is the 1kOhm resistor in the voltage divider that feeds the V2P pin in the HLW8012
        hlw8012.setResistors(CURRENT_RESISTOR, VOLTAGE_RESISTOR_UPSTREAM, VOLTAGE_RESISTOR_DOWNSTREAM);

        attachInterrupt(pin_CF1, hlw8012_cf1_interrupt, CHANGE);
        attachInterrupt(pin_CF, hlw8012_cf_interrupt, CHANGE);
        
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 200000);

        auto fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/sensor/#", fnall);
        return true;
    }

#ifdef __ESP__
    void registerHomeAssistant(String homeAssistantFriendlyName, String projectName="", String homeAssistantDiscoveryPrefix="homeassistant") {
        pHA=new HomeAssistant(name, tID, homeAssistantFriendlyName, projectName, ENERGYHLW8012_VERSION, homeAssistantDiscoveryPrefix);
        pHA->addSensor("voltage", "Voltage", "V","None","mdi:gauge");
        pHA->addSensor("current", "Current", "A","None","mdi:gauge");
        pHA->addSensor("activepower", "Active Power", "W","power","mdi:gauge");
        pHA->addSensor("apparentpower", "Apparent Power", "VA","None","mdi:gauge");
        pHA->addSensor("energy", "Energy", "Ws","None","mdi:gauge");
        pHA->begin(pSched);
        publish();
    }
#endif

  private:
    void loop() {
        double val;
        val=hlw8012.getVoltage();
        if (voltageSensor.filter(&val)) {
            voltageValue=val;
            publishVoltage();
        }
        val=hlw8012.getCurrent();
        if (currentSensor.filter(&val)) {
            currentValue=val;
            publishCurrent();
        }
        val=hlw8012.getActivePower();
        if (activePowerSensor.filter(&val)) {
            activePowerValue=val;
            publishActivePower();
        }
        val=hlw8012.getApparentPower();
        if (apparentPowerSensor.filter(&val)) {
            apparentPowerValue=val;
            publishApparentPower();
        }
        val=hlw8012.getEnergy();
        if (energySensor.filter(&val)) {
            energyValue=val;
            publishEnergy();
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/voltage/get") {
            publishVoltage();
        }
        if (topic == name + "/sensor/current/get") {
            publishCurrent();
        }
        if (topic == name + "/sensor/activepower/get") {
            publishActivePower();
        }
        if (topic == name + "/sensor/apparentpower/get") {
            publishApparentPower();
        }
        if (topic == name + "/sensor/energy/get") {
            publishEnergy();
        }
    };
};  // EnergyHlw8012

}  // namespace ustd
