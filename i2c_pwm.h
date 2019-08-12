// i2c_pwm.h
// Adafruit PWM Servo Driver Library, https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library
#pragma once

#include "scheduler.h"
#include <Adafruit_PWMServoDriver.h>

namespace ustd {
class I2CPWM {
  public:
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t address;
    enum Mode { GPIO, PWM, SERVO};
    Mode mode;
    int frequency;

    Adafruit_PWMServoDriver pPwm;
    bool bActive = false;

    I2CPWM(String name, int mode=Mode::PWM, uint8_t i2c_address=0x40) : name(name), 
            mode(mode), address(i2c_address) {
        if (mode==Mode::SERVO) {
            frequency=1000;
        } else {
            frequency=50;
        }
    }

    ~I2CPWM() {
    }

    void setFrequency(int freq) {
        frequency=freq;
        pPwm->setPWMFreq(1000);
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        pPwm = new Adafruit_PWMServoDriver(i2c_address);

        if (!pPwm->begin()) {
#ifdef USE_SERIAL_DBG
            Serial.print("Problem starting PWM i2c hardware!");
#endif
        } else {
            pPwm->setPWMFreq(frequency);
            // give a c++11 lambda as callback scheduler task registration of
            // this.loop():
            std::function<void()> ft = [=]() { this->loop(); };
            tID = pSched->add(ft, name, 300000);

            std::function<void(String, String, String)> fnall =
                [=](String topic, String msg, String originator) {
                    this->subsMsg(topic, msg, originator);
                };
            pSched->subscribe(tID, name + "/i2cpwm/#", fnall);
            bActive = true;
        }
    }

    void loop() {
    }

    setState(uint8_t port, bool state) {
        if (mode!=Mode::SERVO) {
            if (state) { // XXX: negative logic, save state?!
                pwm.setPWM(pin, 4096, 0);  // turns pin fully on
            } else {
                pwm.setPWM(pin, 0, 4096);  // turns pin fully off
            }
        }
    }

    setUnitLevel(double level) { // 0.0 ... 1.0
        if (mode!=Mode::SERVO) {
            int l1=(int)(4096.0*level);
            int l2=4096-l1;
            pwm.setPWM(pin, l1, l2);  // turns pin fully on
    }

    double parseUnitLevel(String msg) {
        char buff[32];
        int l;
        int len = msg.length();
        double br = 0.0;
        memset(buff, 0, 32);
        if (len > 31)
            l = 31;
        else
            l = len;
        strncpy(buff, (const char *)msg.c_str(), l);

        if ((!strcmp((const char *)buff, "on")) ||
            (!strcmp((const char *)buff, "true"))) {
            br = 1.0;
        } else {
            if ((!strcmp((const char *)buff, "off")) ||
                (!strcmp((const char *)buff, "false"))) {
                br = 0.0;
            } else {
                if ((strlen(buff) > 4) &&
                    (!strncmp((const char *)buff, "pct ", 4))) {
                    br = atoi((char *)(buff + 4)) / 100.0;
                } else {
                    if (strlen(buff) > 1 && buff[strlen(buff) - 1] == '%') {
                        buff[strlen(buff) - 1] = 0;
                        br = atoi((char *)buff) / 100.0;
                    } else {
                        br = atof((char *)buff);
                    }
                }
            }
        }
        if (br<0.0) br=0.0;
        if (br>1.0) br=1.0;
        return br;
    }

    void subsMsg(String topic, String msg, String originator) {
        String wct=name+"/i2cpwm/*";
        int port;
        if (pSched->mqttmatch(topic,wct ) {
            if (topic.length() >= wct.length()) {
                char *p=topic.c_str();
                port=atoi(&p[name.length()+strlen("/i2cpwm/")]);
                double level=parseUnitLevel(msg)
                setUnitLevel(port, level);
            }
        }
    };
};  // I2CPWM

}  // namespace ustd
