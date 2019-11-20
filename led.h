// led.h
#pragma once

#include "scheduler.h"
#include "mup_util.h"

namespace ustd {

class Led {
  public:
    enum Mode { Passive, Blink, Wave, Pulse, Pattern};

    Scheduler *pSched;
    int tID;
    String name;
    uint8_t port;
    bool activeLogic = false;
    uint8_t channel;
    double brightlevel;
    bool state;
    uint16_t pwmrange;
    Mode mode;
    unsigned long interval;
    double phase=0.0;
    unsigned long uPhase=0;
    unsigned long oPeriod=0;
    unsigned long startPulse=0;
    String pattern;
    unsigned int patternPointer=0;

    bool useHA=false;
    String HAname="";
    String HAprefix="";

    Led(String name, uint8_t port, bool activeLogic=false, uint8_t channel=0 )
        : name(name), port(port), activeLogic(activeLogic), channel(channel) {
    }

    ~Led() {
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;
        #if defined(__ESP32__)
            pinMode(port, OUTPUT);
            // use first channel of 16 channels (started from zero)
            #define LEDC_TIMER_BITS  10
            // use 5000 Hz as a LEDC base frequency
            #define LEDC_BASE_FREQ     5000
            ledcSetup(channel, LEDC_BASE_FREQ, LEDC_TIMER_BITS);
  			ledcAttachPin(port, channel);
		#else
			pinMode(port, OUTPUT);
		#endif
        #ifdef __ESP__
        pwmrange=1023;
        #else
        pwmrange=255;
        #endif
        
        setOff();
        mode=Mode::Passive;
        interval=1000; //ms
        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        /* std::function<void()> */ auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 50000);

        /* std::function<void(String, String, String)> */ auto fnall =
            [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/light/#", fnall);
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
                String HAcommandTopic=HAcmd+"/"+name+"/light/set";
                String HAstateTopic=HAmuPrefix+"/"+name+"/light/state";
                String HAcommandBrTopic=HAcmd+"/"+name+"/light/set";
                String HAstateBrTopic=HAmuPrefix+"/"+name+"/light/unitbrightness";
                HAdiscoTopic="!"+HAprefix+"/light/"+name+"/config";
                HAdiscoEntityDef="{\"state_topic\":\""+HAstateTopic+"\","+
                        "\"command_topic\":\""+HAcommandTopic+"\","+
                        "\"brightness_state_topic\":\""+HAstateBrTopic+"\","+
                        "\"brightness_scale\":\"100\","+
                        "\"brightness_value_template\":\"{{ value | float * 100 | round(0) }}\","+
                        "\"brightness_command_topic\":\""+HAcommandBrTopic+"\","+
                        "\"name\":\""+HAname+"\","+
                        "\"on_command_type\":\"brightness\","+
                        "\"payload_on\":\"on\","+
                        "\"payload_off\":\"off\""+
                                            "}";
                pSched->publish(HAdiscoTopic,HAdiscoEntityDef);
                publishState();
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

    void setOn() {
        this->state=true;
        brightlevel=1.0;
        if (activeLogic) {
            #ifdef __ESP32__
            ledcWrite(channel,pwmrange);
            #else
            digitalWrite(port, true);
            #endif
        } else {
            #ifdef __ESP32__
            ledcWrite(channel,0);
            #else
            digitalWrite(port, false);
            #endif
        }
    }
    void setOff() {
        brightlevel=0.0;
        this->state=false;
        if (!activeLogic) {
            #ifdef __ESP32__
            ledcWrite(channel,pwmrange);
            #else
            digitalWrite(port, true);
            #endif
        } else {
            #ifdef __ESP32__
            ledcWrite(channel,0);
            #else
            digitalWrite(port, false);
            #endif
        }
    }

    void set(bool state, bool _automatic=false) {
        if (state==this->state) return;
        this->state=state;
        if (!_automatic) mode=Mode::Passive;
        if (state) {
            setOn();
            if (!_automatic) {
                pSched->publish(name + "/light/unitbrightness", "1.0");
                pSched->publish(name + "/light/state", "on");
            }
        } else {
            setOff();
            if (!_automatic) {
                pSched->publish(name + "/light/unitbrightness", "0.0");
                pSched->publish(name + "/light/state", "off");
           }
        }
    }

    void setMode(Mode newmode, unsigned int interval_ms=1000, double phase_unit=0.0) {
        mode=newmode;
        if (mode==Mode::Passive) return;
        phase=phase_unit;
        if (phase<0.0) phase=0.0;
        if (phase>1.0) phase=1.0;
        interval=interval_ms;
        if (interval<100) interval=100;
        if (interval>100000) interval=100000;
        startPulse=millis();
        uPhase=(unsigned long)(2.0*(double)interval*phase);
        oPeriod=(millis()+uPhase)%interval;
    }

    void publishState() {
        if (brightlevel>0.0) {
            pSched->publish(name + "/light/state", "on");
            this->state=true;
        } else {
            pSched->publish(name + "/light/state", "off");
            this->state=false;
        }
        char buf[32];
        sprintf(buf, "%5.3f", brightlevel);
        pSched->publish(name + "/light/unitbrightness", buf);
    }

    void brightness(double bright, bool _automatic=false) {
        uint16_t bri;
        
        if (!_automatic) mode=Mode::Passive;
        if (bright==1.0) {
            set(true, _automatic);
            return;
        }
        if (bright==0.0) {
            set(false, _automatic);
            return;
        }

        if (bright < 0.0)
            bright = 0.0;
        if (bright > 1.0)
            bright = 1.0;
        brightlevel=bright;
        bri = (uint16_t)(bright * (double)pwmrange);
        if (!bri) state=false;
        else state=true;
        if (!activeLogic)
            bri = pwmrange - bri;
        #if defined(__ESP32__)
        ledcWrite(channel, bri);
        #else
        analogWrite(port, bri);
        #endif
        if (!_automatic) {
            publishState();
        }
    }

    void loop() {
        if (mode==Mode::Passive) return;
        unsigned long period=(millis()+uPhase)%(2*interval);
        if (mode==Mode::Pulse) {
            if (millis()-startPulse < interval) {
                set(true, true);
            } else {
                set(false, true);
                setMode(Mode::Passive);
            }
        }
        if (mode==Mode::Blink) {
            if (period<oPeriod) {
                set(false, true);
            } else {
                if (period>interval && oPeriod<interval) {
                    set(true, true);
                }
            }
        }
        if (mode==Mode::Wave) {
            unsigned long period=(millis()+uPhase)%(2*interval);
            double br=0.0;
            if (period<interval) {
                br=(double)period/(double)interval;
            } else {
                br=(double)(2*interval - period)/(double)interval;
            }
            brightness(br, true);
        }
        if (mode==Mode::Pattern) {
            if (period<oPeriod) {
                if (patternPointer<pattern.length()) {
                    char c=pattern[patternPointer];
                    if (c=='r') {
                        patternPointer=0;
                        c=pattern[patternPointer];
                    }
                    if (c=='+') {
                        set(true, true);
                    }
                    if (c=='-') {
                        set (false, true);
                    }
                    if (c>='0' && c<='9') {
                        double br=(double)(c-'0')*0.1111;
                        brightness(br, true);
                    }
                    ++patternPointer;
                } else {
                    patternPointer=0;
                    set(false, true);
                    setMode(Mode::Passive);                    
                }
            }
        }
        oPeriod=period;
    }

    void subsMsg(String topic, String msg, String originator) {
        char msgbuf[128];
        memset(msgbuf,0,128);
        strncpy(msgbuf,msg.c_str(),127);
        if (topic == name + "/light/set") {
            double br;
            br = parseUnitLevel(msg);
            brightness(br);
        }
        if (topic == name+"/light/mode/set") {
            char *p=strchr(msgbuf,' ');
            char *p2=nullptr;
            char *p3=nullptr;
            if (p) {
                *p=0;
                ++p;
                p2=strchr(p,',');
                if (p2) {
                    *p2=0;
                    ++p2;
                    p3=strchr(p2,',');
                    if (p3) {
                        *p3=0;
                        ++p3;
                    }
                }
            }
            int t=1000;
            double phs=0.0;
            if (!strcmp(msgbuf, "passive")) {
                setMode(Mode::Passive);
            } else if (!strcmp(msgbuf,"pulse")) {
                if (p) t=atoi(p);
                setMode(Mode::Pulse,t);
            } else if (!strcmp(msgbuf, "blink")) {
                if (p) t=atoi(p);
                if (p2) phs=atof(p2);
                setMode(Mode::Blink, t, phs);
            } else if (!strcmp(msgbuf, "wave")) {
                if (p) t=atoi(p);
                if (p2) phs=atof(p2);
                setMode(Mode::Wave, t, phs);
            } else if (!strcmp(msgbuf, "pattern")) {
                if (p && strlen(p)>0) {
                    pattern=String(p);
                    patternPointer=0;
                    if (p2) t=atoi(p2);
                    if (p3) phs=atof(p3);
                    setMode(Mode::Pattern, t, phs);
                }
            }
        }
        if (topic == name + "/light/unitbrightness/get") {
            publishState();
        }
    };
};  // Led

}  // namespace ustd
