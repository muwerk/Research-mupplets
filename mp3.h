#pragma once

#include "scheduler.h"

#define MP3_CMD_PLAYINDEX 0x03
#define MP3_CMD_PLAYFOLDERTRACK 0x0f
#define MP3_CMD_SELSD 0x09
#define MP3_DATA_SELDSD_TF 0x02
#define MP3_CMD_VOL 0x06

class Mp3 {
  private:
    Scheduler *pSched;
    int tID;
    String name;

  public:
    Mp3(String name, uint8_t port) : name(name), port(port) {
    }

    ~Mp3() {
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;

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

    // TODO: test stuff:
    void subsMsg(String topic, String msg, String originator) {
        if (topic == name + "/mp3/get") {
            char buf[32];
            sprintf(buf, "%5.3f", ldrvalue);
            pSched->publish(name + "/mp3", buf);
        }
    };
};  // Mp3

}  // namespace ustd
