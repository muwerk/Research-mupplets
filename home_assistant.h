// home_assistant.h
// Support auto-discovery MQTT feature of Home Assistant.
//

#pragma once

#ifdef __ESP__
#include "scheduler.h"

namespace ustd {

// Default version, if none is given during HomeAssistant instance creation
#define __HA_VERSION__ "0.1.0"

class HomeAssistant {
  public:
    Scheduler *pSched;
    bool useHA = false;
    String devName;
    int tID;
    String HAname = "";
    String HAprefix = "";
    String macAddress = "";
    String ipAddress = "";
    String hostName = "";
    String capHostName;
    String swVersion;
    String muProject;
    long rssiVal = -99;
    String HAmuPrefix = "";
    String HAcmd = "";
    String willTopic = "";
    String willMessage = "";

    ustd::array<String> sensor_topic_sub_names;
    ustd::array<String> sensor_friendlyNames;
    ustd::array<String> sensor_unitDescs;
    ustd::array<String> sensor_classNames;
    ustd::array<String> sensor_iconNames;

    ustd::array<String> switch_iconNames;
    ustd::array<String> light_iconNames;

    //    ustd::array<String> sensor_devNames;
    //    ustd::array<String> sensor_HAnames;
    ustd::array<String> sensorsAttribs;

    int nrLights = 0;
    //    ustd::array<String> light_devNames;
    //    ustd::array<String> light_HAnames;
    ustd::array<String> lightsAttribs;

    int nrSwitches = 0;
    //    ustd::array<String> switch_devNames;
    //    ustd::array<String> switch_HAnames;
    ustd::array<String> switchesAttribs;

    HomeAssistant(String _devName, int _tID, String homeAssistantFriendlyName, String project = "",
                  String version = __HA_VERSION__,
                  String homeAssistantDiscoveryPrefix = "homeassistant") {
        if (homeAssistantFriendlyName == "")
            HAname = _devName;
        else
            HAname = homeAssistantFriendlyName;
        macAddress = WiFi.macAddress();
        HAprefix = homeAssistantDiscoveryPrefix;
        tID = _tID;
        devName = _devName;
        muProject = project;
        swVersion = version;
    }

    ~HomeAssistant() {
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;
        useHA = true;
        auto fnmq = [=](String topic, String msg, String originator) {
            this->mqMsg(topic, msg, originator);
        };
        if (macAddress == "")
            macAddress = WiFi.macAddress();
        pSched->subscribe(tID, "mqtt/state", fnmq);
        pSched->subscribe(tID, "mqtt/config", fnmq);
        pSched->subscribe(tID, "net/network", fnmq);
        pSched->subscribe(tID, "net/rssi", fnmq);
        pSched->publish("mqtt/state/get");
    }

    void addSensor(/*String devName, String HAname, */ String topic_sub_name, String friendlyName,
                   String unitDesc, String className, String iconName) {
        // sensor_devNames.add(devName);
        // sensor_HAnames.add(HAname);
        if (macAddress == "")
            macAddress = WiFi.macAddress();
        sensor_topic_sub_names.add(topic_sub_name);
        sensor_friendlyNames.add(friendlyName);
        sensor_unitDescs.add(unitDesc);
        sensor_classNames.add(className);
        sensor_iconNames.add(iconName);
    }

    void addLight(String iconName = "mdi:lightbulb") {
        // light_devNames.add(devName);
        // light_HAnames.add(HAname);
        light_iconNames.add(iconName);
        if (macAddress == "")
            macAddress = WiFi.macAddress();
        ++nrLights;
    }

    void addSwitch(String iconName = "mdi:light-switch") {
        // switch_devNames.add(devName);
        // switch_HAnames.add(HAname);
        switch_iconNames.add(iconName);
        if (macAddress == "")
            macAddress = WiFi.macAddress();
        ++nrSwitches;
    }

    void publishAttrib(String attrTopic) {
        if (macAddress == "")
            macAddress = WiFi.macAddress();
        String attrib = "{\"Rssi\":" + String(rssiVal) + "," + "\"Mac\": \"" + macAddress + "\"," +
                        "\"IP\": \"" + ipAddress + "\"," + "\"Host\": \"" + hostName + "\"," +
                        "\"Version\": \"" + swVersion + "\"," + "\"Manufacturer\": \"muWerk\"";
        if (muProject != "") {
            attrib += ",\"Project\": \"" + muProject + "\"";
        }
        attrib += "}";
        pSched->publish(attrTopic, attrib);
    }

    void publishAttribs() {
        for (unsigned int i = 0; i < sensorsAttribs.length(); i++) {
            publishAttrib(sensorsAttribs[i]);
        }
        for (unsigned int i = 0; i < lightsAttribs.length(); i++) {
            publishAttrib(lightsAttribs[i]);
        }
        for (unsigned int i = 0; i < switchesAttribs.length(); i++) {
            publishAttrib(switchesAttribs[i]);
        }
    }

    void mqMsg(String topic, String msg, String originator) {
        if (topic == "net/rssi") {
            if (msg[0] == '{') {
                JSONVar mqttJsonMsg = JSON.parse(msg);
                if (JSON.typeof(mqttJsonMsg) == "undefined") {
                    return;
                }
                rssiVal = (long)mqttJsonMsg["rssi"];  // root["state"];
            } else {
                rssiVal = (long)msg.toInt();
            }
            publishAttribs();
        } else if (topic == "net/network") {
            JSONVar mqttJsonMsg = JSON.parse(msg);
            if (JSON.typeof(mqttJsonMsg) == "undefined") {
                return;
            }
            String state = (const char *)mqttJsonMsg["state"];  // root["state"];
            if (state == "connected") {
                ipAddress = (const char *)mqttJsonMsg["ip"];
                // macAddress=(const char *)mqttJsonMsg["mac"]; // too late,
                // done already in begin()
                hostName = (const char *)mqttJsonMsg["hostname"];
                String c1 = hostName.substring(0, 1);
                c1.toUpperCase();
                String c2 = hostName.substring(1);
                capHostName = c1 + c2;
            }
        } else if (topic == "mqtt/config") {
            if (useHA) {
                HAmuPrefix = "";
                char cmsg[180];
                char *p1 = nullptr;
                char *p2 = nullptr;
                memset(cmsg, 0, 180);  // msg is format:   [dis]connected,prefix/hostname
                strncpy(cmsg, msg.c_str(), 179);
                p1 = strchr(cmsg, '+');
                if (p1) {
                    *p1 = 0;
                    ++p1;
                    HAmuPrefix = cmsg;
                    p2 = strchr(p1, '+');
                    if (p2) {
                        *p2 = 0;
                        ++p2;
                        willTopic = p1;
                        willMessage = p2;
                    }
                } else {
                    HAmuPrefix = cmsg;
                }
                // if (p1)
                //    HAmuPrefix = p1;  // prefix/hostname, e.g. omu/myhost
                char cmd[64];
                memset(cmd, 0, 64);
                strncpy(cmd, HAmuPrefix.c_str(), 63);
                HAcmd = "";
                char *p0 = strchr(cmd, '/');  // get hostname from mqtt message, e.g. myhost
                if (p0) {
                    ++p0;
                    HAcmd = String(p0);
                }
            }
        } else if (topic == "mqtt/state") {
            if (useHA) {
                if (msg == "connected") {
                    // if (p1)
                    //    HAmuPrefix = p1;
                    String HAnameNS = HAname;
                    HAnameNS.replace(" ", "_");
                    if (macAddress == "")
                        macAddress = WiFi.macAddress();
                    for (unsigned int i = 0; i < sensor_topic_sub_names.length(); i++) {
                        String subDevNo = String(i + 1);
                        String HAstateTopic =
                            HAmuPrefix + "/" + devName + "/sensor/" + sensor_topic_sub_names[i];
                        String HAattrTopic =
                            devName + "/sensor/" + sensor_topic_sub_names[i] + "/attribs";
                        String HAdiscoTopic = "!!" + HAprefix + "/sensor/" + subDevNo + "-" +
                                              HAnameNS + "/" + devName + "/config";
                        String HAdiscoEntityDef =
                            "{\"stat_t\":\"" + HAstateTopic + "\"," + "\"json_attr_t\":\"" +
                            HAmuPrefix + "/" + HAattrTopic + "\"," + "\"name\":\"" + HAname + " " +
                            sensor_friendlyNames[i] + "\"," + "\"uniq_id\":\"" + macAddress + "-" +
                            devName + "-S" + subDevNo + "\"," +
                            "\"val_tpl\":\"{{ value | float }}\"," + "\"unit_of_meas\":\"" +
                            sensor_unitDescs[i] + "\"," + "\"expire_after\": 1800," +
                            "\"icon\":\"" + sensor_iconNames[i] + "\"";
                        if (willTopic != "") {
                            HAdiscoEntityDef += ",\"avty_t\":\"" + willTopic + "\"";
                            HAdiscoEntityDef += ",\"pl_avail\":\"connected\"";
                            HAdiscoEntityDef += ",\"pl_not_avail\":\"" + willMessage + "\"";
                        }
                        if (sensor_classNames[i] != "None") {
                            HAdiscoEntityDef +=
                                ",\"device_class\":\"" + sensor_classNames[i] + "\"";
                        }
                        HAdiscoEntityDef =
                            HAdiscoEntityDef + ",\"device\":{" + "\"identifiers\":[\"" +
                            macAddress + "-" + devName + "\",\"" + macAddress + "-" + devName +
                            "-S" + subDevNo + "\"]," + "\"model\":\"" + muProject + "\"," +
                            "\"name\":\"" + capHostName + "\"," + "\"manufacturer\":\"muWerk\"," +
                            "\"connections\":[[\"IP\",\"" + ipAddress + "\"]," + "[\"Host\",\"" +
                            hostName + "\"]]}";
                        HAdiscoEntityDef += "}";
                        sensorsAttribs.add(HAattrTopic);
                        pSched->publish(HAdiscoTopic, HAdiscoEntityDef);
                    }

                    // lights
                    for (int i = 0; i < nrLights; i++) {
                        int id = i + 1;
                        String subDevNo = String(id);
                        String HAcommandTopic = HAcmd + "/" + devName + "/light/set";
                        String HAstateTopic = HAmuPrefix + "/" + devName + "/light/state";
                        String HAattrTopic = devName + "/light/attribs";
                        String HAcommandBrTopic = HAcmd + "/" + devName + "/light/set";
                        String HAstateBrTopic =
                            HAmuPrefix + "/" + devName + "/light/unitbrightness";
                        String HAdiscoTopic = "!!" + HAprefix + "/light/" + subDevNo + "-" +
                                              HAnameNS + "/" + devName + "/config";
                        String HAdiscoEntityDef =
                            "{\"stat_t\":\"" + HAstateTopic + "\"," + "\"name\":\"" + HAname +
                            "\"," + "\"uniq_id\":\"" + macAddress + "-" + devName + "-L" +
                            subDevNo + "\"," + "\"cmd_t\":\"" + HAcommandTopic + "\"," +
                            "\"json_attr_t\":\"" + HAmuPrefix + "/" + HAattrTopic + "\"," +
                            "\"bri_stat_t\":\"" + HAstateBrTopic + "\"," + "\"bri_scl\":\"100\"," +
                            "\"bri_val_tpl\":\"{{ value | float * 100 | "
                            "round(0) }}\"," +
                            "\"bri_cmd_t\":\"" + HAcommandBrTopic + "\"," +
                            "\"on_cmd_type\":\"brightness\"," + "\"pl_on\":\"on\"," +
                            "\"pl_off\":\"off\"";
                        //"\"icon\":\""+light_iconNames[i]+"\"";
                        if (willTopic != "") {
                            HAdiscoEntityDef += ",\"avty_t\":\"" + willTopic + "\"";
                            HAdiscoEntityDef += ",\"pl_avail\":\"connected\"";
                            HAdiscoEntityDef += ",\"pl_not_avail\":\"" + willMessage + "\"";
                        }
                        HAdiscoEntityDef =
                            HAdiscoEntityDef + ",\"device\":{" + "\"identifiers\":[\"" +
                            macAddress + "-" + devName + "\",\"" + macAddress + "-" + devName +
                            "-S" + subDevNo + "\"]," + "\"model\":\"" + muProject + "\"," +
                            "\"name\":\"" + capHostName + "\"," + "\"manufacturer\":\"muWerk\"," +
                            "\"connections\":[[\"IP\",\"" + ipAddress + "\"]," + "[\"Host\",\"" +
                            hostName + "\"]]}";
                        HAdiscoEntityDef += "}";
                        lightsAttribs.add(HAattrTopic);
                        pSched->publish(HAdiscoTopic, HAdiscoEntityDef);
                    }

                    // switches
                    for (int i = 0; i < nrSwitches; i++) {
                        int id = i + 1;
                        String subDevNo = String(id);
                        String HAcommandTopic = HAcmd + "/" + devName + "/switch/set";
                        String HAstateTopic = HAmuPrefix + "/" + devName + "/switch/state";
                        String HAattrTopic = devName + "/switch/attribs";
                        String HAdiscoTopic = "!!" + HAprefix + "/switch/" + subDevNo + "-" +
                                              HAnameNS + "/" + devName + "/config";
                        String HAdiscoEntityDef =
                            "{\"stat_t\":\"" + HAstateTopic + "\"," + "\"name\":\"" + HAname +
                            "\"," + "\"uniq_id\":\"" + macAddress + "-" + devName + "-SW" +
                            subDevNo + "\"," + "\"cmd_t\":\"" + HAcommandTopic + "\"," +
                            "\"json_attr_t\":\"" + HAmuPrefix + "/" + HAattrTopic + "\"," +
                            "\"state_on\":\"on\"," + "\"state_off\":\"off\"," +
                            "\"pl_on\":\"on\"," + "\"pl_off\":\"off\"," + "\"icon\":\"" +
                            switch_iconNames[i] + "\"";
                        if (willTopic != "") {
                            HAdiscoEntityDef += ",\"avty_t\":\"" + willTopic + "\"";
                            HAdiscoEntityDef += ",\"pl_avail\":\"connected\"";
                            HAdiscoEntityDef += ",\"pl_not_avail\":\"" + willMessage + "\"";
                        }
                        HAdiscoEntityDef =
                            HAdiscoEntityDef + ",\"device\":{" + "\"identifiers\":[\"" +
                            macAddress + "-" + devName + "\",\"" + macAddress + "-" + devName +
                            "-S" + subDevNo + "\"]," + "\"model\":\"" + muProject + "\"," +
                            "\"name\":\"" + capHostName + "\"," + "\"manufacturer\":\"muWerk\"," +
                            "\"connections\":[[\"IP\",\"" + ipAddress + "\"]," + "[\"Host\",\"" +
                            hostName + "\"]]}";
                        HAdiscoEntityDef += "}";
                        switchesAttribs.add(HAattrTopic);
                        pSched->publish(HAdiscoTopic, HAdiscoEntityDef);
                    }
                }
            }
        }
    }

};  // HomeAssistant
}  // namespace ustd
#endif  // __ESP__