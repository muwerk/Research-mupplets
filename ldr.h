// ldr.h
#pragma once

#include "scheduler.h"

namespace ustd {
class Ldr {
  private:
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t port;
    double ldrvalue;
#ifdef __ESP32__
    double adRange=4096.0; // 12 bit default
#else
    double adRange=1024.0; // 10 bit default
#endif
    bool useHA=false;
    String HAname="";
    String HAprefix="";

  public:
    ustd::sensorprocessor illuminanceSensor = ustd::sensorprocessor(4, 600, 0.005);
    
    Ldr(String name, uint8_t port) : name(name), port(port) {
    }

    ~Ldr() {
    }

    void publishIlluminance() {
        char buf[32];
        sprintf(buf, "%5.3f", ldrvalue);
        pSched->publish(name + "/sensor/unitilluminance", buf);
    }

    double getUnitIlluminance() {
        return ldrvalue;
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        /* std::function<void()> */
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 200000);

        /* std::function<void(String, String, String)> */
        auto fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/sensor/unitilluminance/#", fnall);
        auto fnmq=
            [=](String topic, String msg, String originator) {
                this->mqMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, "mqtt/state", fnmq);
 
    }

    void mqMsg( String topic, String msg, String originator) {
        if (useHA) {
            String HAmuPrefix="";
            String HAdiscoTopic="";
            String HAdiscoEntityDef="";
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
                String HAstateTopic=HAmuPrefix+"/"+name+"/sensor/unitilluminance";
                HAdiscoTopic="!"+HAprefix+"/sensor/"+name+"/config";
                HAdiscoEntityDef="{\"state_topic\":\""+HAstateTopic+"\","+
                        "\"name\":\""+HAname+" Unit-Illuminance\","+
                        "\"value_template\":\"{{ value | float }}\","+
                        "\"unit_of_measurement\":\"[0..1]\","+
                        "\"expire_after\": 1800,"+
                        "\"icon\":\"mdi:brightness-6\","+
                        "\"device_class\":\"illuminance\""+
                                            "}";
                pSched->publish(HAdiscoTopic,HAdiscoEntityDef);
                publishIlluminance();
            }
        }
    }

    void registerHomeAssistant(String homeAssistantFriendlyName="", String homeAssistantDiscoveryPrefix="homeassistant") {
        if (homeAssistantFriendlyName=="") homeAssistantFriendlyName=name;
        useHA=true;
        HAname=homeAssistantFriendlyName;
        HAprefix=homeAssistantDiscoveryPrefix;
        pSched->publish("mqtt/state/get");
    }

  private:
    void loop() {
        double val = analogRead(port) / (adRange-1.0);
        if (illuminanceSensor.filter(&val)) {
            ldrvalue = val;
            publishIlluminance();
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/sensor/unitilluminance/get") {
            publishIlluminance();
        }
    };
};  // Ldr

}  // namespace ustd
