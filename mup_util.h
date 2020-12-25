// i2c_pwm.h
// Adafruit PWM Servo Driver Library,
// https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library
#pragma once

//#ifdef __USE_SPIFFS_FS__
//#include <FS.h>
//#ifdef __ESP32__
//#include "SPIFFS.h"
//#endif
//#endif

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

    if ((!strcmp((const char *)buff, "on")) || (!strcmp((const char *)buff, "true"))) {
        br = 1.0;
    } else {
        if ((!strcmp((const char *)buff, "off")) || (!strcmp((const char *)buff, "false"))) {
            br = 0.0;
        } else {
            if ((strlen(buff) > 4) && (!strncmp((const char *)buff, "pct ", 4))) {
                br = atoi((char *)(buff + 4)) / 100.0;
            } else {
                if (strlen(buff) > 1 && buff[strlen(buff) - 1] == '%') {
                    buff[strlen(buff) - 1] = 0;
                    br = atoi((char *)buff) / 100.0;
                } else {
                    if (strchr((char *)buff, '.')) {
                        br = atof((char *)buff);
                    } else {
                        br = atoi((char *)buff) / 100.0;
                    }
                }
            }
        }
    }
    if (br < 0.0)
        br = 0.0;
    if (br > 1.0)
        br = 1.0;
    return br;
}

#ifdef __ESP__
bool fsBeginDone = false;

bool writeJson(String filename, JSONVar jsonobj) {
    if (!fsBeginDone) {
#ifdef __USE_OLD_FS__
        SPIFFS.begin();
#else
        LittleFS.begin();
#endif
        fsBeginDone = true;
    }
#ifdef __USE_OLD_FS__
    fs::File f = SPIFFS.open(filename, "w");
#else
    fs::File f = LittleFS.open(filename, "w");
#endif
    if (!f) {
        return false;
    }
    String jsonstr = JSON.stringify(jsonobj);
    f.println(jsonstr);
    f.close();
    return true;
}

bool readJson(String filename, String &content) {
    if (!fsBeginDone) {
#ifdef __USE_OLD_FS__
        SPIFFS.begin();
#else
        LittleFS.begin();
#endif
        fsBeginDone = true;
    }
    content = "";
#ifdef __USE_OLD_FS__
    fs::File f = SPIFFS.open(filename, "w");
#else
    fs::File f = LittleFS.open(filename, "w");
#endif
    if (!f) {
        return false;
    } else {
        while (f.available()) {
            String lin = f.readStringUntil('\n');
            content = content + lin;
        }
        f.close();
    }
    return true;
}

bool readNetJsonString(String key, String &value) {
    /*
        SPIFFS.begin();
        fs::File f = SPIFFS.open("/net.json", "r");
        if (!f) {
            return false;s
        } else {
            String jsonstr = "";
            while (f.available()) {
                // Lets read line by line from the file
                String lin = f.readStringUntil('\n');
                jsonstr = jsonstr + lin;
            }
            f.close();
    */
    String jsonstr;
    if (readJson("/net.json", jsonstr)) {
        JSONVar configObj = JSON.parse(jsonstr);
        if (JSON.typeof(configObj) == "undefined") {
            return false;
        }
        if (configObj.hasOwnProperty(key.c_str())) {
            value = (const char *)configObj[key.c_str()];
        } else {
            return false;
        }
        return true;
    } else {
        return false;
    }
}

bool readFriendlyName(String &friendlyName) {
    if (readNetJsonString("friendlyname", friendlyName))
        return true;
    if (readNetJsonString("hostname", friendlyName))
        return true;
    return false;
}
#endif  // __ESP__

}  // namespace ustd
