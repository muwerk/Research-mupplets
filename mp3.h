// specific Chinese MP3 player hardware
// WARNING: there are several similar but different ones.
// Different hardware almost certainly use slightly (or not so slightly) different protocol variations.

#pragma once

#include "scheduler.h"

// TODO: this won't hold for API configurable serials:
#ifdef USE_SERIAL_DBG
#error You cannot use define USE_SERIAL_DBG with MP3 mupplet, since the serial port is needed for the communication with the MP3 hardware!
#endif

class Mp3PlayerProtocol {
  public:
    enum RepeatMode {once, loop};

    virtual ~Mp3PlayerProtocol() {}; // Otherwise destructor of derived classes is never called!
    virtual bool begin { return false;}
    virtual bool setVolume(uint8_t volumePercent) { return false;}
    virtual bool playFolderTrack(uint8_t folder, uint8_t track) { return false;}
    virtual bool repeatLoopFolderTrack(uint8_t folder, uint8_t track) { return false;}
    virtual bool playIndex(uint16_t index) { return false;}
    virtual bool interleaveFolderTrack(uint8_t folder, uint8_t track) { return false;}
    virtual bool setRepeatMode(RepeatMode mode=RepeatMode::once) {return false;}
    virtual bool pause() { return false;}
    virtual bool play() { return false; }
}


// This: http://geekmatic.in.ua/pdf/Catalex_MP3_board.pdf
class MP3PlayerCatalex : Mp3PlayerProtocol { // Untested!
  public:
    enum MP3_CMD {
            PLAYINDEX=0x03,
            VOLUME=0x06,
            SELECT_SD=0x09,
            PLAY=0x0d,
            PAUSE=0x0e,
            PLAYFOLDERTRACK=0x0f
        }
    enum MP3_SUBCMD {
            SELECT_SD_TF=0x02
        }

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
        sendMP3(MP3_CMD::SELECT_SD, 0, MP3_SUBCMD::SELECT_SD_TF);
    }

  public:
    Stream *pSer
    Mp3Player(Stream *pSer) : pSer(pSer) {}

    virtual bool begin() override {
        _selectSD();
        return true;
    }
    virtual bool playFolderTrack(uint8_t folder = 0, uint8_t track = 0) override {
        sendMP3(MP3_CMD::PLAYFOLDERTRACK, folder, track);
        return true;
    }

    virtual bool playIndex(uint16_t index = 1) override {
        sendMP3(MP3_CMD::PLAYINDEX, index/256, index%256);
        return true;
    }

    virtual bool pause() override {
        sendMP3(MP3_CMD::PAUSE, 0, 0);
        return true;
    }
    virtual bool play() override {
        sendMP3(MP3_CMD::PLAY, 0, 0);
        return true;
    }

    virtual bool setVolume(uint8_t vol) override {
        if (vol > 30)
            vol = 30;
        sendMP3(MP3_CMD_VOL, 0, vol);
        return true;
    }   
}


// Other: http://ioxhop.info/files/OPEN-SMART-Serial-MP3-Player-A/Serial%20MP3%20Player%20A%20v1.1%20Manual.pdf
class Mp3PlayerOpenSmart : Mp3PlayerProtocol {
  public:
    enum MP3_CMD {
            PLAY=0x01,
            PAUSE=0x02,
            NEXT=0x03,
            PREVIOUS=0x04,
            PLAYINDEX=0x03,
            STOPPLAY=0x0e,
            STOPINJECT=0x0f,

            VOLUME=0x31,
            REPEATMODE=0x33,
            SELECT_SD=0x35,
            PLAYINDEX=0x41,
            PLAYFOLDERTRACK=0x42,
            INJECTFOLDERTRACK=0x44
        }
    enum MP3_SUBCMD {
            REPEAT_LOOP=0x00,
            REPEAT_SINGLE=0x01,
            SELECT_SD_TF=0x01
        }

  private:
    void _sendMP3(uint8_t *pData, uint8_t dataLen=0) {
        pSer->write(0x7e);
        pSer->write(dataLen+1); // dataLength + len-field
        pSer->write(pData, dataLen);
        pSer->write(0xef);
    }

    void _selectSD() {
        uint8_t len=0x02;
        uint8_t cmd[len] = {MP3_CMD::SELECT_SD, MP3_SUBCMD::SELECT_SD_TF};
        _sendMP3(cmd, len);
    }

  public:
    Stream *pSer
    Mp3Player(Stream *pSer) : pSer(pSer) {}

    virtual bool begin() override {
        _selectSD();
        return true;
    }

    virtual bool playFolderTrack(uint8_t folder = 0, uint8_t track = 0) override {
        uint8_t len=0x03;
        uint8_t cmd[len] = {MP3_CMD::PLAYFOLDERTRACK, folder, track};
        _sendMP3(cmd, len);
        return true;
    }

    virtual bool playIndex(uint16_t index = 1) override {
        uint8_t len0x03;
        uint8_t cmd={MP3_CMD::PLAYINDEX, index/256, index%256};
        _sendMP3(cmd, len);
        return true;
    }

    virtual bool pause() override {
        uint8_t len0x01;
        uint8_t cmd={MP3_CMD::PAUSE};
        _sendMP3(cmd, len);
        return true;
    }
    
    virtual bool play() override {
        uint8_t len0x01;
        uint8_t cmd={MP3_CMD::PLAY};
        _sendMP3(cmd, len);
        return true;
    }

    virtual bool setVolume(uint8_t vol) override {
        if (vol > 30)
            vol = 30;
        uint8_t len=0x02;
        uint8_t cmd[len] = {MP3_CMD::VOLUME, vol};
        _sendMP3(cmd, len);
        return true;
    }   

    void interleaveFolderTrack(uint8_t folder, uint8_t track) {
        uint8_t len=0x03;
        uint8_t cmd[len] = {MP3_CMD::INJECTFOLDERTRACK, folder, track};
        _sendMP3(cmd, len);
        return true;
    }

    void repeatLoopFolderTrack(uint8_t folder, uint8_t track) {
        uint8_t len=0x03;
        uint8_t cmd[len] = {MP3_CMD::REPEATMODE, folder, track};
        _sendMP3(cmd, len);
        return true;
    }
}

class Mp3Player {
  private:
    Scheduler *pSched;
    int tID;
    String name;
    MP3_PLAYER_TYPE mp3type;
    Stream *pSerial;

  public:
    Mp3PlayerProtocol *mp3prot;
    enum MP3_PLAYER_TYPE {CATALEX, OPENSMART};

    Mp3(String name, Stream *pSerial, MP3_PLAYER_TYPE mp3type=MP3_PLAYER_TYPE::OPENSMART) : name(name), mp3type(mp3type), pSerial(pSerial)  {
        switch (mp3type) {
            case MP3_PLAYER_TYPE::CATALEX:
                mp3prot=Mp3PlayerCatalex(pSerial);
            break;
            case MP3_PLAYER_TYPE::OPENSMART:
                mp3prot=Mp3PlayerOpenSmart(pSerial);
            break;
            defaut:
                mp3prot=nullptr;
            break;   
        }
    }

    ~Mp3() {
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;
        
        Serial.begin(9600);

        // give a c++11 lambda as callback scheduler task registration of
        // this.loop():
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 200000);

        /* std::function<void(String, String, String)> */
        auto fnall = [=](String topic, String msg, String originator) {
            this->subsMsg(topic, msg, originator);
        };
        pSched->subscribe(tID, name + "/mp3/#", fnall);

        mp3prot->begin();
    }

    bool setVolume(uint8_t volumePercent) { return mp3Prot->setVolume(volumePercent); }
    bool playFolderTrack(uint8_t folder, uint8_t track) { return mp3Prot->playFolderTrack(uint8_t folder, uint8_t track); }
    bool repeatLoopFolderTrack(uint8_t folder, uint8_t track) { return mp3Prot->repeatLoopFolderTrack(uint8_t folder, uint8_t track); }
    bool playIndex(uint16_t index) { return mp3prot->playIndex(uint16_t index); }
    bool interleaveFolderTrack(uint8_t folder, uint8_t track) { return mp3Prot->interleaveFolderTrack(uint8_t folder, uint8_t track);}
    bool setRepeatMode(RepeatMode mode=RepeatMode::once) {return setRepeatMode(RepeatMode mode=RepeatMode::once); }
    bool pause() { return mp3prot->pause(); }
    bool play() { return mp3prot->play(); }

  private:
    void loop() {
    }

    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/mp3/track/set") {
            char buf[32];
            memset(buf,0,32);
            strncmp(buf,msg.c_str(),31);
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
                mp3prot->playFolderTrack(folder,track);
            }
        }
    };
};  // Mp3

}  // namespace ustd
