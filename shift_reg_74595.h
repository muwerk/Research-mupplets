// shift_reg_74595.h
// Data output on TTL chip 74HC595
// port_data_mosi: connection to 74HC595 Pin 14 [DS]
// port_clock_sck: connection to 74HC595 Pin 11 [SHCP]
// port_latch: connection to 74HC595 Pin 12 [STCP]

//             74HC595
//            +--v--+
//          Q1|1  16| Vcc
//          Q2|2  15| Q0
//          Q3|3  14| DS    <-- Data
//          Q4|4  13| OEi
//          Q5|5  12| ST_CP <-- Latch
//          Q6|6  11| SH_CP <-- Clocks
//          Q7|7  10| MRi
//         Gnd|8   9| Q7'
//            +-----+


#pragma once

#include "scheduler.h"
#include "mup_util.h"

namespace ustd {

class ShiftReg {
  public:
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t port_data_mosi;
    uint8_t port_clock_sck;
    uint8_t port_latch;
    uint8_t cur_data;
    bool useSPI;

    ShiftReg(String name, uint8_t port_data_mosi, uint8_t port_clock_sck, uint8_t port_latch, bool useSPI=true )
        : name(name), port_data_mosi(port_data_mosi), port_clock_sck(port_clock_sck), port_latch(port_latch), useSPI(useSPI) {
    }

    ~ShiftReg() {
    }

    void begin(Scheduler *_pSched) {
        pSched = _pSched;
        digitalWrite(port_latch, HIGH);
        pinMode(port_latch, OUTPUT);
        if (useSPI) {
            //SPI.pins(mosi, miso, clk, hwcs);
            SPI.begin();
        } else {
            digitalWrite(port_data_mosi, HIGH);  // first writing, then setting output-mode
            digitalWrite(port_clock_sck, HIGH); // generates safe start state
            pinMode(port_data_mosi, OUTPUT);
            pinMode(port_clock_sck, OUTPUT);
        }
        writeShiftReg(0);
        auto ft = [=]() { this->loop(); };
        tID = pSched->add(ft, name, 50000);

        auto fnall = [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/shiftreg/#", fnall);
    }

    void writeShiftReg(uint8_t data, uint8_t mask=0xff) {
        uint8_t abs_data=data;
        if (mask!=0xff) {
            abs_data = (cur_data & (!mask)) | (data & mask);
        }
        cur_data=abs_data;
        digitalWrite(port_latch, LOW); // Pulse latch to put data on output ports
        delayMicroseconds(2);
        if (useSPI) SPI.transfer(abs_data);
        else shiftOut(port_data_mosi, port_clock_sck, MSBFIRST, abs_data);
        delayMicroseconds(2);
        digitalWrite(port_latch, HIGH);
    }

    void set(uint8_t data) {
        writeShiftReg(data);
        char buf[32];
        sprintf(buf,"%d",data);
        pSched->publish(name + "/shiftreg/data", buf);
    }

    void loop() {
    }

    void subsMsg(String topic, String msg, String originator) {
        char msgbuf[128];
        memset(msgbuf,0,128);
        strncpy(msgbuf,msg.c_str(),127);
        // TODO: allow other data-formats than decimal
        if (topic == name + "/shiftreg/data/set") {
            uint8_t data=atoi(msgbuf);
            set(data);
        }
    };
};  // ShiftReg

}  // namespace ustd
