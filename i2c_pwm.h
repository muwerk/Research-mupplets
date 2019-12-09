// i2c_pwm.h
// Adafruit PWM Servo Driver Library, https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library
#pragma once

#include "scheduler.h"
#include "Wire.h"
#include <Adafruit_PWMServoDriver.h>

namespace ustd {
class I2CPWM {
  public:
    enum Mode { GPIO, PWM, SERVO};
    Scheduler *pSched;
    int tID;
    String name;
    Mode mode;
    uint8_t  i2c_address;
    int frequency;
    int servoMin=150; // out of 4096
    int servoMax=600;
    int servoFrequency=60;
    int ledFrequency=1000;

    Adafruit_PWMServoDriver *pPwm;
    bool bActive = false;

    I2CPWM(String name, Mode mode=Mode::PWM, uint8_t i2c_address=0x40) : name(name), 
            mode(mode), i2c_address(i2c_address) {
        if (mode==Mode::SERVO) {
            frequency=servoFrequency;
        } else {
            frequency=ledFrequency;
        }
    }

    ~I2CPWM() {
    }

    void setFrequency(int freq) {
        frequency=freq;
        pPwm->setPWMFreq(frequency);
    }

    void setServoMinMax(int minP=150, int maxP=600) { // pulses out of 4096 at 60hz (frequency)
        if (minP<0) minP=0;
        if (maxP>4096) maxP=4096;
        servoMin=minP;
        servoMax=maxP;
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        pPwm = new Adafruit_PWMServoDriver(i2c_address, Wire);

        pPwm->begin();
        pPwm->setPWMFreq(frequency);
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 300000);

        auto fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/i2cpwm/#", fnall);
        bActive = true;
    }

    void loop() {
    }

    void setState(uint8_t port, bool state) {
        if (mode!=Mode::SERVO) {
            if (state) { // XXX: negative logic, save state?!
                pPwm->setPWM(port, 4096, 0);  // turns pin fully on
            } else {
                pPwm->setPWM(port, 0, 4096);  // turns pin fully off
            }
        }
    }

    void setUnitLevel(uint8_t port, double level) { // 0.0 ... 1.0
        if (mode!=Mode::SERVO) {
            int l1=(int)(4096.0*level);
            int l2=4096-l1;
            pPwm->setPWM(port, l1, l2);
        } else { // Servo
            int pulseLen=(int)((double)(servoMax-servoMin)*level)+servoMin;
            pPwm->setPWM(port, 0, pulseLen);
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        String wct=name+"/i2cpwm/set/*";
        int port;
        if (pSched->mqttmatch(topic,wct )) {
            if (topic.length() >= wct.length()) {
                const char *p=topic.c_str();
                port=atoi(&p[name.length()+strlen("/i2cpwm/")]);
                double level=parseUnitLevel(msg);
                setUnitLevel(port, level);
            }
        }
    }
};  // I2CPWM

}  // namespace ustd
