// i2c_pwm.h
// Adafruit PWM Servo Driver Library, https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library
#pragma once

#include "scheduler.h"

namespace ustd {

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

}  // namespace ustd
