// serial control for TV (LG_TV)

#pragma once

#include "scheduler.h"
#include "home_assistant.h"

namespace ustd {

class TvSerialProtocol {
  public:
    enum TvInput {DIGITALTV, ANALOGTV, VIDEO1, VIDEO2, COMPONENT1, COMPONENT2, RGBDTV, RGBPC, HDMI1, HDMI2};

    virtual ~TvSerialProtocol() {}; // Otherwise destructor of derived classes is never called!
    virtual bool begin() { return false;}
    virtual bool setOn() { return false; }
    virtual bool setOff() { return false; }
    virtual bool setState(bool state) { return false;}
    virtual bool setInput(TvInput input) { return false; }
    virtual bool asyncCheckState() { return false; }
    virtual bool asyncCheckInput() { return false; }
    virtual bool asyncReceive(Scheduler *, String) { return false; }
    virtual bool asyncSend() { return false; }
};



class TVSerialLG : TvSerialProtocol {
  private:
    const uint8_t recBufLen=32;
    uint8_t recBuf[32];
    enum recStateType {none, started};
    recStateType recState=recStateType::none;
    uint8_t recBufPtr=0;
    int curState=-1;
    int curInput=-1;
    unsigned long lastSend=0;
    unsigned long minSendIntervall=150;

    
    ustd::queue<uint8_t *> asyncSendQueue = ustd::queue<uint8_t *>(16);

    void _sendTV(const char *pData, uint8_t dataLen=0) {
        if (timeDiff(lastSend, millis()) < minSendIntervall) {
            uint8_t *buf=(uint8_t *)malloc(dataLen+1);
            if (buf) {
                *buf=dataLen;
                memcpy(&buf[1],pData,dataLen);
                if (!asyncSendQueue.push(buf)) {
                    free(buf);
                }
            }
            return;
        }
        pSer->write((const uint8_t *)pData, dataLen);
        pSer->write('\n');
        lastSend=millis();
    }

  public:
    HardwareSerial *pSer;
    TVSerialLG(HardwareSerial *pSer) : pSer(pSer) {}

    virtual bool asyncSend() override {
        if (timeDiff(lastSend, millis()) >= minSendIntervall) {
            if (!asyncSendQueue.isEmpty()) {
                char *buf=(char *)asyncSendQueue.pop();
                if (buf) {
                    _sendTV(buf, strlen(buf));
                    free(buf);
                }
            }
        }
        return true;
    }

    virtual bool begin() override {
        pSer->begin(9600);
        lastSend=millis()+500;
        asyncCheckState();
        asyncCheckInput();
        return true;
    }

    virtual bool setOn() override {
        const char *cmd="ta 01 01";
        _sendTV(cmd, strlen(cmd));
        return true;
    }

    virtual bool setOff() override {
        const char *cmd="ta 01 00";
        _sendTV(cmd, strlen(cmd));
        return true;
    }

    virtual bool setState(bool state) override {
        if (state) return setOn();
        else return setOff();
    }

    /*
                        "00" => "digitalTV",
                        "01" => "analogueTV",
                        "02" => "video1",
                        "03" => "video2",
                        "04" => "component1",
                        "05" => "component2",
                        "06" => "rgbDTV",
                        "07" => "rgbPC",
                        "08" => "hdmi1",
                        "09" => "hdmi2"
    */
    virtual bool setInput(TvInput chan) {
        const char *cmd;
        switch (chan) {
            case TvInput::DIGITALTV:
                cmd="kb 01 00";
                _sendTV(cmd,strlen(cmd));
                return true;
            case TvInput::ANALOGTV:
                cmd="kb 01 01";
                _sendTV(cmd,strlen(cmd));
                return true;
                break;
            case TvInput::VIDEO1:
                cmd="kb 01 02";
                _sendTV(cmd,strlen(cmd));
                return true;
                break;
            case TvInput::VIDEO2:
                cmd="kb 01 03";
                _sendTV(cmd,strlen(cmd));
                return true;
                break;
            case TvInput::HDMI1:
                cmd="kb 01 08";
                _sendTV(cmd,strlen(cmd));
                return true;
                break;
            case TvInput::HDMI2:
                cmd="kb 01 09";
                _sendTV(cmd,strlen(cmd));
                return true;
                break;
            default:
                return false;
                break;
        }
    }


    virtual bool asyncCheckState() {
        const char *cmd="ta 01 ff";
        _sendTV(cmd, strlen(cmd));
        return true;
    }

    virtual bool asyncCheckInput() {
        const char *cmd="tb 01 ff";
        _sendTV(cmd, strlen(cmd));
        return true;
    }


    void parseRecBuf(Scheduler *pSched, String topic) {
        bool known=false;
        if (recBufPtr==9) {
            switch (recBuf[0]) {
                case 'a': // on/off state
                    uint8_t state;
                    state=recBuf[8]-'0';
                    switch (state) {
                        case 0:
                            if (state!=curState) pSched->publish(topic+"/state", "off");
                            curState=state;
                            known=true;
                            break;
                        case 1:
                            if (state!=curState) pSched->publish(topic+"/state", "on");
                            curState=state;
                            known=true;
                            break;
                        default:
                            char msg[32];
                            sprintf(msg,"UNKNOWN STATE 0x%02x",state);
                            pSched->publish(topic+"/state", msg);
                            break;
                    }
                    break;
                case 'b': // tv input
                    uint8_t input;
                    TvInput newInput;
                    const char *sval;
                    input=recBuf[8]-'0';
                    switch (input) {
                        case 0:
                            sval="digitaltv";
                            newInput=TvInput::DIGITALTV;
                            known=true;
                            break;
                        case 1:
                            sval="analogtv";
                            newInput=TvInput::ANALOGTV;
                            known=true;
                            break;
                        case 2:
                            sval="video1";
                            newInput=TvInput::VIDEO1;
                            known=true;
                            break;
                        case 3:
                            sval="video2";
                            newInput=TvInput::VIDEO2;
                            known=true;
                            break;
                        case 8:
                            sval="hdmi1";
                            newInput=TvInput::HDMI1;
                            known=true;
                            break;
                        case 9:
                            sval="hdmi2";
                            newInput=TvInput::HDMI2;
                            known=true;
                            break;
                    }
                    if (known) {
                        if (curInput!=newInput) pSched->publish(topic+"/input", sval);                       
                        curInput=newInput;
                    } else {
                        char msg[32];
                        sprintf(msg,"UNKNOWN TV INPUT 0x%02x",input);
                        pSched->publish(topic+"/input", msg);
                    }
                    break;
            }
        }
        if (!known) {
            char scratch[256];
            char part[10];
            strcpy(scratch,"");
            for (int i=0; i<recBufPtr; i++) {
                sprintf(part," 0x%02x",recBuf[i]);
                strcat(scratch,part);
            }
            pSched->publish(topic+"/tv/xmessage", scratch);
        }
    }

    virtual bool asyncReceive(Scheduler *pSched, String topic) override {
        while (pSer->available()>0) {
            uint8_t b=pSer->read();
            switch (recState) {
                case recStateType::none:
                    if (b=='a' || b=='b') { // Answer to state ('a') or input ('b')
                        recBufPtr=0;
                        recBuf[recBufPtr]=b;
                        ++recBufPtr;
                        recState=recStateType::started;
                    } else {
                        recBufPtr=0;
                    }
                break;
                case recStateType::started:
                    if (b=='x') {
                        parseRecBuf(pSched, topic);
                        recBufPtr=0;
                        recState=recStateType::none;
                    } else {
                        recBuf[recBufPtr]=b;
                        ++recBufPtr;
                        if (recBufPtr>=recBufLen) {
                            recBufPtr=0;
                            recState=recStateType::none;
                        }
                    }
                break;
                default:
                    recBufPtr=0;
                    recState=recStateType::none;
                    continue;
                break;
            }
        }
        return true;
    }
};

class TvSerial {
  private:
    Scheduler *pSched;
    int tID;

  public:
    String TVSERIAL_VERSION="0.1.0";
    TvSerialProtocol *tvProt;
    enum TV_SERIAL_TYPE {LG_TV};
    String name;
    HardwareSerial *pSerial;
    TV_SERIAL_TYPE tvSerialType;
    #ifdef __ESP__
    HomeAssistant *pHA;
    #endif

    TvSerial(String name, HardwareSerial *pSerial, TV_SERIAL_TYPE tvSerialType=TV_SERIAL_TYPE::LG_TV) : name(name), pSerial(pSerial), tvSerialType(tvSerialType)  {
        switch (tvSerialType) {
            case TV_SERIAL_TYPE::LG_TV:
                tvProt=(TvSerialProtocol *)new TVSerialLG(pSerial);
            break;
            default:
                tvProt=nullptr;
            break;   
        }
    }

    ~TvSerial() {
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;
        
        Serial.begin(9600);

        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 50000);  // call loop() every 50ms

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/tv/#", fnall);

        tvProt->begin();
    }

    #ifdef __ESP__
    void registerHomeAssistant(String homeAssistantFriendlyName, String projectName="", String homeAssistantDiscoveryPrefix="homeassistant") {
        pHA=new HomeAssistant(name, tID, homeAssistantFriendlyName, projectName, TVSERIAL_VERSION, homeAssistantDiscoveryPrefix);
        pHA->addSwitch();
        pHA->begin(pSched);
    }
    #endif

    bool setOn() { 
        if (tvProt->setOn()) {
            return tvProt->setInput(TvSerialProtocol::TvInput::HDMI1);
        } else return false;
    }
    bool setOff() { return tvProt->setOff(); }
    bool setState(bool state) {
        if (!tvProt->setState(state)) return false;
        if (state) {
            return tvProt->setInput(TvSerialProtocol::TvInput::HDMI1);
        }
    }
    bool setInput(TvSerialProtocol::TvInput input) { return tvProt->setInput(input); }
    bool asyncCheckState() { return tvProt->asyncCheckState(); }
    bool syncCheckInput() { return tvProt->asyncCheckInput(); }

  private:

    int checker=0;

    void loop() {
        ++checker;
        if (checker>20) {
            checker=0;
            tvProt->asyncCheckState();
            tvProt->asyncCheckInput();
        }
        tvProt->asyncReceive(pSched, name+"/tv");
        tvProt->asyncSend();
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/switch/set") {
            if (msg=="on" || msg=="true") {
                tvProt->setOn();
            }
            if (msg=="off" || msg=="false") {
                tvProt->setOff();
            }
        }
        if (topic == name+"/tv/input/set") {
            if (msg=="digitaltv") {
                tvProt->setInput(TvSerialProtocol::TvInput::DIGITALTV);
            }
            if (msg=="analogtv") {
                tvProt->setInput(TvSerialProtocol::TvInput::ANALOGTV);
            }
            if (msg=="video1") {
                tvProt->setInput(TvSerialProtocol::TvInput::VIDEO1);
            }
            if (msg=="video2") {
                tvProt->setInput(TvSerialProtocol::TvInput::VIDEO2);
            }
            if (msg=="hdmi1") {
                tvProt->setInput(TvSerialProtocol::TvInput::HDMI1);
            }
            if (msg=="hdmi2") {
                tvProt->setInput(TvSerialProtocol::TvInput::HDMI2);
            }
        }
    };
};  // Mp3

}  // namespace ustd
