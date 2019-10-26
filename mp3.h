// specific Chinese MP3 player hardware
// WARNING: there are several similar but different ones.
// These MP3 player protocols are a complete mess.
// This: http://geekmatic.in.ua/pdf/Catalex_MP3_board.pdf
// Others: http://ioxhop.info/files/OPEN-SMART-Serial-MP3-Player-A/Serial%20MP3%20Player%20A%20v1.1%20Manual.pdf

#pragma once

#include "scheduler.h"

#define MP3_CMD_PLAYINDEX 0x03
#define MP3_CMD_PLAYFOLDERTRACK 0x0f
#define MP3_CMD_SELSD 0x09
#define MP3_DATA_SELDSD_TF 0x02
#define MP3_CMD_VOL 0x06

class otherMP3 { // Unused!
    void sendMP3(uint8_t cmd, uint8_t d1 = 0, uint8_t d2 = 0) {
        uint8_t buf[8] = {0x7e, 0xff, 0x06, 0x00, 0x00, 0x00, 0x00, 0xef};
        buf[3] = cmd;
        buf[5] = d1;
        buf[6] = d2;
        for (int i = 0; i < 8; i++)
            Serial.write(buf[i]);
        // Serial.write(buf, 8);
    }

    void selectSD() {
        sendMP3(MP3_CMD_SELSD, 0, MP3_DATA_SELDSD_TF);
    }

    void playFolderTrack(uint8_t folder = 0, uint8_t track = 0) {
        sendMP3(MP3_CMD_PLAYFOLDERTRACK, folder, track);
    }

    void playIndex(uint8_t index = 1) {
        sendMP3(MP3_CMD_PLAYINDEX, 0, index);
    }

    void pause() {
        sendMP3(0x0e, 0, 0);
    }
    void resume() {
        sendMP3(0x0d, 0, 0);
    }

    void volume(uint8_t vol) {
        if (vol > 30)
            vol = 30;
        sendMP3(MP3_CMD_VOL, 0, vol);
    }   
}

#ifdef USE_SERIAL_DBG
#error You cannot use define USE_SERIAL_DBG with MP3 mupplet, since the serial port is needed for the communication with the MP3 hardware!
#endif


class Mp3 {
  private:
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t serialPortNo;

  public:
    Mp3(String name, uint8_t serialPortNo) : name(name), serialPortNo(serialPortNo) {
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

        delay(20);
        sel();
        delay(20);
        vol(10);
        delay(20);
    }

    void sel() {
        uint8_t cmd[5] = {0x7e, 0x03, 0x35, 0x01, 0xef};
        Serial.write(cmd, 5);
    }

    void play() {
        uint8_t cmd[4] = {0x7e, 0x02, 0x01, 0xef};
        Serial.write(cmd, 4);
    }
    void playf(uint8_t folder, uint8_t track) {
        uint8_t cmd[6] = {0x7e, 0x04, 0x42, folder, track, 0xef};
        Serial.write(cmd, 6);
    }

    void inject(uint8_t folder, uint8_t track) {
        uint8_t cmd[6] = {0x7e, 0x04, 0x44, folder, track, 0xef};
        Serial.write(cmd, 6);
    }

    void vol(uint8_t vol) {
        if (vol > 30)
            vol = 30;
        uint8_t cmd[5] = {0x7e, 0x03, 0x31, vol, 0xef};
        Serial.write(cmd, 5);
    }

    void repeat(uint8_t folder, uint8_t track) {
        uint8_t cmd[6] = {0x7e, 0x04, 0x33, folder, track, 0xef};
        Serial.write(cmd, 6);
    }

    // TODO: test track:
    void bim() {
        inject(1, 3);
    }

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
                playFolderTrack(folder,track);
            }
        }
    };
};  // Mp3

}  // namespace ustd
