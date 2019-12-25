// serial control for TV (LG_TV)

#pragma once

#include "scheduler.h"
#include "home_assistant.h"

namespace ustd {

// Protocol defintion for TVs that can be controlled via serial
class TvSerialProtocol {
  public:
    enum TvInput {UNDEFINED, DIGITALTV, ANALOGTV, VIDEO1, VIDEO2, COMPONENT1, COMPONENT2, RGBDTV, RGBPC, HDMI1, HDMI2};

    virtual ~TvSerialProtocol() {}; // Otherwise destructor of derived classes is never called!
    virtual bool begin() { return false;}
    virtual bool setOn() { return false; }
    virtual bool setOff() { return false; }
    virtual bool setState(bool state) { return false;}
    virtual bool setInput(TvInput input) { return false; }
    virtual bool getState() { return false; }
    virtual TvInput getInput() { return TvInput::UNDEFINED; }
    virtual bool asyncCheckState() { return false; }
    virtual bool asyncCheckInput() { return false; }
    virtual bool asyncReceive(Scheduler *, String) { return false; }
    virtual bool asyncSend(Scheduler *, String) { return false; }
};


// TvSerialProtocol implementation for LG-TV
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

    // this send function makes sure that there are at least
    // minSendIntervall ms between different packets, otherwise
    // the TV protocol parser might implode...
    // Packets are queued and sent asynchronously, if necessary.
    void _sendTV(const char *pData, uint8_t dataLen=0) {
        if (timeDiff(lastSend, millis()) < minSendIntervall) {
            uint8_t *buf=(uint8_t *)malloc(dataLen+1);
            if (buf) {
                memset(buf,0,dataLen+1);
                memcpy(buf,pData,dataLen);
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

    // Check if packets are in the async queue, and sent
    // at appropriate time.
    virtual bool asyncSend(Scheduler *pSched, String name) override {
        if (timeDiff(lastSend, millis()) >= minSendIntervall) {
            if (!asyncSendQueue.isEmpty()) {
                char *buf=(char *)asyncSendQueue.pop();
                if (buf) {
                    //pSched->publish(name+"/debug",buf);
                    //char bufdbg[256];
                    //char bufbt[32];
                    //strcpy(bufdbg,"Async: ");
                    //for (unsigned int i=0; i<strlen(buf); i++) {
                    //    sprintf(bufbt," 0x%02x [%c] |",buf[i],buf[i]);
                    //    strcat(bufdbg,bufbt);
                    //}
                    //pSched->publish(name+"/debug",bufdbg);
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
        asyncCheckState(); // Current TV state (on/off)
        asyncCheckInput(); // Check current input (only works, if TV is on)
        return true;
    }

    virtual bool setOn() override {
        const char *cmd="ka 01 01";
        _sendTV(cmd, strlen(cmd));
        return true;
    }

    virtual bool setOff() override {
        const char *cmd="ka 01 00";
        _sendTV(cmd, strlen(cmd));
        return true;
    }

    virtual bool setState(bool state) override {
        if (state) return setOn();
        else return setOff();
    }

    virtual bool getState() {
        if (curState==1) return true;
        else return false;
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

    // request current state (on/off)
    virtual bool asyncCheckState() {
        const char *cmd="ka 01 ff";
        _sendTV(cmd, strlen(cmd));
        return true;
    }

    // request current input
    virtual bool asyncCheckInput() {
        if (curState==1) {
            const char *cmd="kb 01 ff";
            _sendTV(cmd, strlen(cmd));
            return true;
        } else {
            return false; // Can't check input channel, if TV off.
        }
    }

    // Parse a complete message from TV
    void parseRecBuf(Scheduler *pSched, String topic) {
        bool known=false;
        if (recBufPtr==9) {
            switch (recBuf[0]) {
                case 'a': // on/off state
                    uint8_t state;
                    state=recBuf[8]-'0';
                    switch (state) {
                        case 0:
                            if (state!=curState) pSched->publish(topic+"/switch/state", "off");
                            curState=state;
                            known=true;
                            break;
                        case 1:
                            if (state!=curState) pSched->publish(topic+"/switch/state", "on");
                            curState=state;
                            known=true;
                            break;
                        default:
                            char msg[32];
                            sprintf(msg,"UNKNOWN STATE 0x%02x",state);
                            pSched->publish(topic+"/switch/state", msg);
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
                        if (curInput!=newInput) pSched->publish(topic+"/tv/input", sval);                       
                        curInput=newInput;
                    } else {
                        char msg[32];
                        sprintf(msg,"UNKNOWN TV INPUT 0x%02x, TV off?",input);
                        pSched->publish(topic+"/tv/input", msg);
                        curInput=TvInput::UNDEFINED;
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

    // Check, if serial data is available and assemble a complete packet for parser
    virtual bool asyncReceive(Scheduler *pSched, String topic) override {
        while (pSer->available()>0) {
            uint8_t b=pSer->read();
            //char dbg[42];
            //sprintf(dbg,"RECEIVE [%02d] 0x%02x %c",recBufPtr,b,(char)b);
            //pSched->publish(topic+"/debug",dbg);
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
                    continue;
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
                    continue;
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

// Main mupplet
class TvSerial {
  private:
    Scheduler *pSched;
    int tID;

  public:
    String TVSERIAL_VERSION="0.1.0";
    TvSerialProtocol *tvProt;
    enum TV_SERIAL_TYPE {LG_TV}; // currently support serial TV types
    String name;
    HardwareSerial *pSerial;
    TV_SERIAL_TYPE tvSerialType;
    bool pollActive=true;
    #ifdef __ESP__
    HomeAssistant *pHA;
    #endif

    TvSerial(String name, HardwareSerial *pSerial, TV_SERIAL_TYPE tvSerialType=TV_SERIAL_TYPE::LG_TV) : name(name), pSerial(pSerial), tvSerialType(tvSerialType)  {
        // Instantiate the appropriate TvSerialProtocol: 
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
        tvProt->begin();
        
        auto ft = [=]() { this->loop(); };
        // Loop 50ms: check async send/receive every 50ms,
        // (and request TV status every 1sec):
        tID = pSched->add(ft, name, 50000);  // call loop() every 50ms

        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/#", fnall);
    }

    // This registers the TV as switch in home assistant and automatically
    // syncs information.
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
        if (pollActive) {
            ++checker;
            if (checker>20) { // 20*50ms = 1sec, request TV status to make sure it's 
            // in sync, if user manually switch TV.
                checker=0;
                tvProt->asyncCheckState();
                tvProt->asyncCheckInput();
            }
            tvProt->asyncReceive(pSched, name); // serial receive
            tvProt->asyncSend(pSched, name); // serial send
        }
    }

    // MQTT message subscriptions, also used by Home Assistant
    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/switch/set") {
            if (msg=="on" || msg=="true") {
                tvProt->setOn();
            }
            if (msg=="off" || msg=="false") {
                tvProt->setOff();
            }
        }
        if (topic == name + "/switch/state/get") {
            char buf[32];
            if (tvProt->getState()) sprintf(buf,"on");
            else sprintf(buf,"off");
            pSched->publish(name + "/switch/state", buf);
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
};  // TvSerial

}  // namespace ustd
