// specific Chinese MP3 player hardware
// WARNING: there are several similar but different ones.
// Different hardware almost certainly use slightly (or not so slightly) different protocol variations.

#pragma once

#include "scheduler.h"


namespace ustd {

class Mp3PlayerProtocol {
  public:
    enum RepeatMode {once, loop};

    virtual ~Mp3PlayerProtocol() {}; // Otherwise destructor of derived classes is never called!
    virtual bool begin() { return false;}
    virtual bool setVolume(uint8_t volumePercent) { return false;}
    virtual bool playFolderTrack(uint8_t folder, uint8_t track) { return false;}
    virtual bool playIndex(uint16_t index) { return false;}
    virtual bool interleaveFolderTrack(uint8_t folder, uint8_t track) { return false;}
    virtual bool setRepeatMode(RepeatMode mode=RepeatMode::once) {return false;}
    virtual bool pause() { return false;}
    virtual bool stopInterleave() { return false;}
    virtual bool play() { return false; }
    virtual bool stop() { return false; }
    virtual bool asyncCheckPlayMode() { return false; }
    virtual bool asyncCheckCardMode() { return false; }
    virtual bool asyncCheckVolume() { return false; }
    virtual bool asyncReceive(Scheduler *, String) { return false; }
    virtual bool asyncSend() { return false; }
};


// This: http://geekmatic.in.ua/pdf/Catalex_MP3_board.pdf
class Mp3PlayerCatalex : Mp3PlayerProtocol { // Untested!
  public:
    enum MP3_CMD {
            PLAYINDEX=0x03,
            VOLUME=0x06,
            SELECT_SD=0x09,
            PLAY=0x0d,
            PAUSE=0x0e,
            PLAYFOLDERTRACK=0x0f
        };
    enum MP3_SUBCMD {
            SELECT_SD_TF=0x02
        };

  private:
    void _sendMP3(uint8_t cmd, uint8_t d1 = 0, uint8_t d2 = 0) {
        uint8_t buf[8] = {0x7e, 0xff, 0x06, 0x00, 0x00, 0x00, 0x00, 0xef};
        buf[3] = cmd;
        buf[5] = d1;
        buf[6] = d2;
        //for (int i = 0; i < 8; i++)
        //    pSer->write(buf[i]);
        pSer->write(buf, 8);
    }

    void _selectSD() {
        _sendMP3(MP3_CMD::SELECT_SD, 0, MP3_SUBCMD::SELECT_SD_TF);
    }

  public:
    HardwareSerial *pSer;
    Mp3PlayerCatalex(HardwareSerial *pSer) : pSer(pSer) {}

    virtual bool begin() override {
        pSer->begin(9600);
        _selectSD();
        return true;
    }
    virtual bool playFolderTrack(uint8_t folder = 0, uint8_t track = 0) override {
        _sendMP3(MP3_CMD::PLAYFOLDERTRACK, folder, track);
        return true;
    }

    virtual bool playIndex(uint16_t index = 1) override {
        _sendMP3(MP3_CMD::PLAYINDEX, index/256, index%256);
        return true;
    }

    virtual bool pause() override {
        _sendMP3(MP3_CMD::PAUSE, 0, 0);
        return true;
    }
    virtual bool play() override {
        _sendMP3(MP3_CMD::PLAY, 0, 0);
        return true;
    }

    virtual bool setVolume(uint8_t vol) override {
        if (vol > 30)
            vol = 30;
        _sendMP3(MP3_CMD::VOLUME, 0, vol);
        return true;
    }   
};


// OpenSmart: http://ioxhop.info/files/OPEN-SMART-Serial-MP3-Player-A/Serial%20MP3%20Player%20A%20v1.1%20Manual.pdf
class Mp3PlayerOpenSmart : Mp3PlayerProtocol {
  public:
    enum MP3_CMD {
            PLAY=0x01,
            PAUSE=0x02,
            NEXT=0x03,
            PREVIOUS=0x04,
            STOPPLAY=0x0e,
            STOPINJECT=0x0f,

            CHECKSTATUS=0x10,
            GETVOLUME=0x11,
            GETDEVICE=0x18,

            VOLUME=0x31,
            REPEATMODE=0x33,
            SELECT_SD=0x35,
            PLAYINDEX=0x41,
            PLAYFOLDERTRACK=0x42,
            INJECTFOLDERTRACK=0x44
        };
    enum MP3_SUBCMD {
            REPEAT_LOOP=0x00,
            REPEAT_SINGLE=0x01,
            SELECT_SD_TF=0x01
        };

  private:
    const uint8_t recBufLen=32;
    uint8_t recBuf[32];
    enum recStateType {none, started};
    recStateType recState=recStateType::none;
    uint8_t recBufPtr=0;
    int curPlayState=-1;
    int curVolState=-1;
    int curSdState=-1;
    unsigned long lastSend=0;
    unsigned long minSendIntervall=150;

    
    ustd::queue<uint8_t *> asyncSendQueue = ustd::queue<uint8_t *>(16);

    void _sendMP3(uint8_t *pData, uint8_t dataLen=0) {
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
        pSer->write(0x7e);
        pSer->write(dataLen+1); // dataLength + len-field
        pSer->write(pData, dataLen);
        pSer->write(0xef);
        lastSend=millis();
    }

    void _selectSD() {
        const uint8_t len=0x02;
        uint8_t cmd[len] = {MP3_CMD::SELECT_SD, MP3_SUBCMD::SELECT_SD_TF};
        _sendMP3(cmd, len);
    }

  public:
    HardwareSerial *pSer;
    Mp3PlayerOpenSmart(HardwareSerial *pSer) : pSer(pSer) {}

    virtual bool asyncSend() override {
        if (timeDiff(lastSend, millis()) >= minSendIntervall) {
            if (!asyncSendQueue.isEmpty()) {
                uint8_t *buf=asyncSendQueue.pop();
                if (buf) {
                    uint8_t dataLen=*buf;
                    _sendMP3(&buf[1], dataLen);
                    free(buf);
                }
            }
        }
        return true;
    }

    virtual bool begin() override {
        pSer->begin(9600);
        _selectSD();
        lastSend=millis()+500;
        asyncCheckCardMode();
        asyncCheckVolume();
        return true;
    }

    virtual bool playFolderTrack(uint8_t folder = 0, uint8_t track = 0) override {
        const uint8_t len=0x03;
        uint8_t cmd[len] = {MP3_CMD::PLAYFOLDERTRACK, folder, track};
        _sendMP3(cmd, len);
        return true;
    }

    virtual bool playIndex(uint16_t index = 1) override {
        const uint8_t len=0x03;
        uint8_t index_hi=index/256;
        uint8_t index_lo=index%256;
        uint8_t cmd[len]={MP3_CMD::PLAYINDEX, index_hi, index_lo};
        _sendMP3(cmd, len);
        return true;
    }

    virtual bool pause() override {
        const uint8_t len=0x01;
        uint8_t cmd[len]={MP3_CMD::PAUSE};
        _sendMP3(cmd, len);
        return true;
    }
    
    virtual bool stopInterleave() override {
        const uint8_t len=0x01;
        uint8_t cmd[len]={MP3_CMD::STOPINJECT};
        _sendMP3(cmd, len);
        return true;
    }
    
    virtual bool play() override {
        const uint8_t len=0x01;
        uint8_t cmd[len]={MP3_CMD::PLAY};
        _sendMP3(cmd, len);
        return true;
    }

    virtual bool stop() override {
        const uint8_t len=0x01;
        uint8_t cmd[len]={MP3_CMD::STOPPLAY};
        _sendMP3(cmd, len);
        return true;
    }

    virtual bool setVolume(uint8_t vol) override {
        if (vol > 30)
            vol = 30;
        const uint8_t len=0x02;
        uint8_t cmd[len] = {MP3_CMD::VOLUME, vol};
        _sendMP3(cmd, len);
        asyncCheckVolume();
        return true;
    }   

    virtual bool interleaveFolderTrack(uint8_t folder, uint8_t track) override {
        const uint8_t len=0x03;
        uint8_t cmd[len] = {MP3_CMD::INJECTFOLDERTRACK, folder, track};
        _sendMP3(cmd, len);
        return true;
    }

    virtual bool setRepeatMode(RepeatMode rep) override {
        const uint8_t len=0x02;
        uint8_t cmd[len];
        if (rep==RepeatMode::loop) {
            cmd[0] = MP3_CMD::REPEATMODE; cmd[1]=MP3_SUBCMD::REPEAT_LOOP;
        } else {
            cmd[0] = MP3_CMD::REPEATMODE; cmd[1]=MP3_SUBCMD::REPEAT_SINGLE;
        }
        _sendMP3(cmd, len);
        return true;
    }

    virtual bool asyncCheckPlayMode() {
        const uint8_t len=0x01;
        uint8_t cmd[len]={MP3_CMD::CHECKSTATUS};
        _sendMP3(cmd, len);
        return true;
    }

    virtual bool asyncCheckCardMode() {
        const uint8_t len=0x01;
        uint8_t cmd[len]={MP3_CMD::GETDEVICE};
        _sendMP3(cmd, len);
        return true;
    }

    virtual bool asyncCheckVolume() {
        const uint8_t len=0x01;
        uint8_t cmd[len]={MP3_CMD::GETVOLUME};
        _sendMP3(cmd, len);
        return true;
    }

    void parseRecBuf(Scheduler *pSched, String topic) {
        bool known=false;
        if (recBufPtr==3 && recBuf[0]==3) {
            switch (recBuf[1]) {
                case 0x10: // play state
                    uint8_t state;
                    state=recBuf[2];
                    if (state!=curPlayState) {
                        curPlayState=state;
                        switch (state) {
                            case 0:
                                pSched->publish(topic+"/state", "STOP");
                                break;
                            case 1:
                                pSched->publish(topic+"/state", "PLAY");
                                break;
                            case 2:
                                pSched->publish(topic+"/state", "PAUSE");
                                break;
                            case 3:
                                pSched->publish(topic+"/state", "FASTFORWARD");
                                break;
                            case 4:
                                pSched->publish(topic+"/state", "FASTREWIND");
                                break;
                            case 0x11:
                                pSched->publish(topic+"/state", "PLAYING");
                                break;
                            default:
                                char msg[32];
                                sprintf(msg,"UNKNOWN 0x%02x",state);
                                pSched->publish(topic+"/state", msg);
                                break;
                        }
                    }
                    known=true;
                    break;
                case 0x11: // replay to get volume
                    uint8_t vol;
                    vol=recBuf[2];
                    if (vol!=curVolState) {
                        curVolState=vol;
                        char sval[32];
                        sprintf(sval,"%d",vol);
                        pSched->publish(topic+"/volume", sval);                       
                    }
                    known=true;
                    break;
                case 0x18: // SD card state
                    uint8_t sdcard;
                    sdcard=recBuf[2];
                    if (sdcard!=curSdState) {
                        curSdState=sdcard;
                        switch (sdcard) {
                            case 0:
                                pSched->publish(topic+"/storage", "NONE");
                            break;
                            case 1:
                                pSched->publish(topic+"/storage", "DISK");
                            break;
                            case 2:
                                pSched->publish(topic+"/storage", "TF-CARD");
                            break;
                            case 3:
                                pSched->publish(topic+"/storage", "SPI");
                            break;
                            default:
                                char msg[32];
                                sprintf(msg,"UNKNOWN 0x%02x",sdcard);
                                pSched->publish(topic+"/storage", msg);
                            break;
                        }
                    }
                    known=true;
                    break;
            }
        }
        if (recBufPtr==2 && recBuf[0]==2) {
            // cmd echo
            known=true;
        }
        if (!known) {
            char scratch[256];
            char part[10];
            strcpy(scratch,"");
            for (int i=0; i<recBufPtr; i++) {
                sprintf(part," 0x%02x",recBuf[i]);
                strcat(scratch,part);
            }
            pSched->publish(topic+"/xmessage", scratch);
        }
    }

    virtual bool asyncReceive(Scheduler *pSched, String topic) override {
        while (pSer->available()>0) {
            uint8_t b=pSer->read();
            switch (recState) {
                case recStateType::none:
                    if (b==0x7e) {
                        recBufPtr=0;
                        recState=recStateType::started;
                    } else {
                        recBufPtr=0;
                    }
                    continue;
                break;
                case recStateType::started:
                    if (b==0xef) {
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

class Mp3Player {
  private:
    Scheduler *pSched;
    int tID;

  public:
    Mp3PlayerProtocol *mp3prot;
    enum MP3_PLAYER_TYPE {CATALEX, OPENSMART};
    String name;
    HardwareSerial *pSerial;
    MP3_PLAYER_TYPE mp3type;

    Mp3Player(String name, HardwareSerial *pSerial, MP3_PLAYER_TYPE mp3type=MP3_PLAYER_TYPE::OPENSMART) : name(name), pSerial(pSerial), mp3type(mp3type)  {
        switch (mp3type) {
            case MP3_PLAYER_TYPE::CATALEX:
                mp3prot=(Mp3PlayerProtocol *)new Mp3PlayerCatalex(pSerial);
            break;
            case MP3_PLAYER_TYPE::OPENSMART:
                mp3prot=(Mp3PlayerProtocol *)new Mp3PlayerOpenSmart(pSerial);
            break;
            default:
                mp3prot=nullptr;
            break;   
        }
    }

    ~Mp3Player() {
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;
        
        Serial.begin(9600);

        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 50000);

        /* std::function<void(String, String, String)> */
        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/mp3/#", fnall);

        mp3prot->begin();
    }

    bool setVolume(uint8_t volumePercent) { return mp3prot->setVolume(volumePercent); }
    bool playFolderTrack(uint8_t folder, uint8_t track) { return mp3prot->playFolderTrack(folder, track); }
    bool playIndex(uint16_t index) { return mp3prot->playIndex(index); }
    bool interleaveFolderTrack(uint8_t folder, uint8_t track) { return mp3prot->interleaveFolderTrack(folder, track);}
    bool setRepeatMode(Mp3PlayerProtocol::RepeatMode mode=Mp3PlayerProtocol::RepeatMode::once) {return mp3prot->setRepeatMode(mode); }
    bool pause() { return mp3prot->pause(); }
    bool stopInterleave() { return mp3prot->stopInterleave(); }
    bool play() { return mp3prot->play(); }
    bool stop() { return mp3prot->stop(); }

  private:

    int checker=0;

    void loop() {
        ++checker;
        if (checker>20) {
            checker=0;
            mp3prot->asyncCheckPlayMode();
        }
        mp3prot->asyncReceive(pSched, name+"/mp3");
        mp3prot->asyncSend();
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/mp3/track/set") {
            char buf[32];
            memset(buf,0,32);
            strncpy(buf,msg.c_str(),31);
            char *p=strchr(buf,',');
            int folder=-1;
            int track=-1;
            if (p) {
                *p=0;
                ++p;
                folder=atoi(buf);
                track=atoi(p);
            }
            if ((track!=-1) && (folder!=-1)) {
                //mp3prot->stop();
                mp3prot->playFolderTrack(folder,track);
            }
        }
        if (topic == name+"/mp3/volume/set") {
            uint8_t vol=atoi(msg.c_str());
            mp3prot->setVolume(vol);
        }
        if (topic == name+"/mp3/state/set") {
            if (msg=="stop") mp3prot->stop();
            if (msg=="pause") mp3prot->pause();
            if (msg=="play") mp3prot->play();
        }
    };
};  // Mp3

}  // namespace ustd
