// home_assistant.h
// Support auto-discovery MQTT feature of Home Assistant.
// 

#pragma once

#include "scheduler.h"

namespace ustd {
class HomeAssistant {
  public:
    Scheduler* pSched;
    bool useHA=false;
    String devName;
    int tID;
    String HAname="";
    String HAprefix="";
    String macAddress="";
    String ipAddress="";
    String hostName="";
    String swVersion="0.1.0";
    long rssiVal=-99;

    ustd::array<String> sensor_topic_sub_names;
    ustd::array<String> sensor_friendlyNames;
    ustd::array<String> sensor_unitDescs;
    ustd::array<String> sensor_classNames;
    ustd::array<String> sensor_iconNames;
 
    ustd::array<String> sensorsAttribs;

    int nrLights=0;
    ustd::array<String> lightsAttribs;

    int nrSwitches=0;
    ustd::array<String> switchesAttribs;

    HomeAssistant(String _devName, int _tID, String homeAssistantFriendlyName="", String homeAssistantDiscoveryPrefix="homeassistant") {
        if (homeAssistantFriendlyName=="") HAname=_devName;
        else HAname=homeAssistantFriendlyName;
        HAprefix=homeAssistantDiscoveryPrefix;
        tID=_tID;
        devName=_devName;
    }

    ~HomeAssistant() {
    }

    void begin(Scheduler *_pSched) {
        pSched=_pSched;
        useHA=true;
        auto fnmq=
            [=](String topic, String msg, String originator) {
                this->mqMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, "mqtt/state", fnmq);
        pSched->subscribe(tID, "net/network", fnmq);
        pSched->subscribe(tID, "net/rssi", fnmq);
        pSched->publish("mqtt/state/get");
    }

    void addSensor(String topic_sub_name, String friendlyName, String unitDesc, String className, String iconName) {
        sensor_topic_sub_names.add(topic_sub_name);
        sensor_friendlyNames.add(friendlyName);
        sensor_unitDescs.add(unitDesc);
        sensor_classNames.add(className);
        sensor_iconNames.add(iconName);
    }

    void addLight() {
        ++nrLights;
    }

    void addSwitch() {
        ++nrSwitches;
    }

    void publishAttrib(String attrTopic) {
        String attrib="{\"Rssi\":"+String(rssiVal)+","+
                       "\"Mac\": \""+macAddress+"\","+
                       "\"IP\": \""+ipAddress+"\","+
                       "\"Host\": \""+hostName+"\","+
                       "\"Version\": \""+swVersion+"\","+
                       "\"Manufacturer\": \"muWerk\""+
                      "}";
        pSched->publish(attrTopic,attrib);
    }

    void publishAttribs() {
        for (unsigned int i=0; i<sensorsAttribs.length(); i++) {
            publishAttrib(sensorsAttribs[i]);
        }
        for (unsigned int i=0; i<lightsAttribs.length(); i++) {
            publishAttrib(lightsAttribs[i]);
        }
        for (unsigned int i=0; i<switchesAttribs.length(); i++) {
            publishAttrib(switchesAttribs[i]);
        }
    }
 
    void mqMsg( String topic, String msg, String originator) {
        if (topic=="net/rssi") {
            JSONVar mqttJsonMsg = JSON.parse(msg);
            if (JSON.typeof(mqttJsonMsg) == "undefined") {
                return;
            }
            rssiVal=(long)mqttJsonMsg["rssi"];  // root["state"];
            publishAttribs();
        } else if (topic=="net/network") {
            JSONVar mqttJsonMsg = JSON.parse(msg);
            if (JSON.typeof(mqttJsonMsg) == "undefined") {
                return;
            }
           String state=(const char *)mqttJsonMsg["state"];  // root["state"];
            if (state == "connected") {
                ipAddress=(const char *)mqttJsonMsg["ip"];
                macAddress=(const char *)mqttJsonMsg["mac"];
                hostName=(const char *)mqttJsonMsg["hostname"];
            }
        } else if (topic=="mqtt/state") {
            if (useHA) {
                String HAmuPrefix="";
                char cmsg[180];
                char *p1=nullptr;
                memset(cmsg,0,180);   // msg is format:   [dis]connected,prefix/hostname
                strncpy(cmsg,msg.c_str(),179);
                p1=strchr(cmsg,',');
                if (p1) {
                    *p1=0;
                    ++p1;
                }
                if (p1) HAmuPrefix=p1;  // prefix/hostname, e.g. omu/myhost
                char cmd[64];
                memset(cmd,0,64);
                strncpy(cmd,HAmuPrefix.c_str(),63);
                String HAcmd="";
                char *p0=strchr(cmd,'/');  // get hostname from mqtt message, e.g. myhost
                if (p0) {
                    ++p0;
                    HAcmd=String(p0);
                }
                if (!strcmp(cmsg,"connected")) {
                    if (p1) HAmuPrefix=p1;

                    for (unsigned int i=0; i<sensor_topic_sub_names.length(); i++) {
                        String subDevNo=String(i+1);
                        String HAstateTopic=HAmuPrefix+"/"+devName+"/sensor/"+sensor_topic_sub_names[i];
                        String HAattrTopic=devName+"/sensor/"+sensor_topic_sub_names[i]+"/attribs";
                        String HAdiscoTopic="!"+HAprefix+"/sensor/"+subDevNo+"/"+devName+"/config";
                        String HAdiscoEntityDef="{\"state_topic\":\""+HAstateTopic+"\","+
                                "\"json_attributes_topic\":\""+HAmuPrefix+"/"+HAattrTopic+"\","+
                                "\"name\":\""+HAname+" "+sensor_friendlyNames[i]+"\","+
                                "\"unique_id\":\""+macAddress+"-S"+subDevNo+"\","+
                                "\"value_template\":\"{{ value | float }}\","+
                                "\"unit_of_measurement\":\""+sensor_unitDescs[i]+"\","+
                                "\"expire_after\": 1800,"+
                                "\"icon\":\""+sensor_iconNames[i]+"\","+
                                "\"device_class\":\""+sensor_classNames[i]+"\""+
                                "}";
                        sensorsAttribs.add(HAattrTopic);
                        pSched->publish(HAdiscoTopic,HAdiscoEntityDef);
                    }

                    // lights
                    for (int i=0; i<nrLights; i++) {
                        int id=i+1;
                        String subDevNo=String(id);
                        String HAcommandTopic=HAcmd+"/"+devName+"/light/set";
                        String HAstateTopic=HAmuPrefix+"/"+devName+"/light/state";
                        String HAattrTopic=devName+"/light/attribs";
                        String HAcommandBrTopic=HAcmd+"/"+devName+"/light/set";
                        String HAstateBrTopic=HAmuPrefix+"/"+devName+"/light/unitbrightness";
                        String HAdiscoTopic="!"+HAprefix+"/light/"+subDevNo+"/"+devName+"/config";
                        String HAdiscoEntityDef="{\"state_topic\":\""+HAstateTopic+"\","+
                                "\"name\":\""+HAname+"\","+
                                "\"unique_id\":\""+macAddress+"-L"+subDevNo+"\","+
                                "\"command_topic\":\""+HAcommandTopic+"\","+
                                "\"json_attributes_topic\":\""+HAmuPrefix+"/"+HAattrTopic+"\","+
                                "\"brightness_state_topic\":\""+HAstateBrTopic+"\","+
                                "\"brightness_scale\":\"100\","+
                                "\"brightness_value_template\":\"{{ value | float * 100 | round(0) }}\","+
                                "\"brightness_command_topic\":\""+HAcommandBrTopic+"\","+
                                "\"on_command_type\":\"brightness\","+
                                "\"payload_on\":\"on\","+
                                "\"payload_off\":\"off\""+
                                                    "}";
                        lightsAttribs.add(HAattrTopic);
                        pSched->publish(HAdiscoTopic,HAdiscoEntityDef);
                    }

                    // switches
                   for (int i=0; i<nrLights; i++) {
                        int id=i+1;
                        String subDevNo=String(id);
                        String HAcommandTopic=HAcmd+"/"+devName+"/switch/set";
                        String HAstateTopic=HAmuPrefix+"/"+devName+"/switch/state";
                        String HAattrTopic=devName+"/switch/attribs";
                        String HAdiscoTopic="!"+HAprefix+"/switch/"+subDevNo+"/"+devName+"/config";
                        String HAdiscoEntityDef="{\"state_topic\":\""+HAstateTopic+"\","+
                                "\"name\":\""+HAname+"\","+
                                "\"unique_id\":\""+macAddress+"-SW"+subDevNo+"\","+
                                "\"command_topic\":\""+HAcommandTopic+"\","+
                                "\"json_attributes_topic\":\""+HAmuPrefix+"/"+HAattrTopic+"\","+
                                "\"state_on\":\"on\","+
                                "\"state_off\":\"off\","+
                                "\"payload_on\":\"on\","+
                                "\"payload_off\":\"off\""+
                                                    "}";
                        switchesAttribs.add(HAattrTopic);
                        pSched->publish(HAdiscoTopic,HAdiscoEntityDef);
                    }
                }
            }
         }
    }

}; // HomeAssistant
} // namespace ustd