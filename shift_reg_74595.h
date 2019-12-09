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
//          Q6|6  11| SH_CP <-- Clock
//          Q7|7  10| MRi
//         Gnd|8   9| Q7'
//            +-----+


#pragma once

#include "scheduler.h"
#include "mup_util.h"

namespace ustd {

class ShiftReg {
    /*! Instantiate a 74HC595 shift register mupplet.
    *
    * This mupplet uses three IOs to fill a 74HC595 shift register of 8 output bits with data.
    * It can either use the hardware SPI or 3 arbitrary IOs. Through cascading shift
    * registers, a large number of outputs can be driven.
    */
 
  public:
    String SHIFTREG74595_VERSION="0.1.0";
    Scheduler *pSched;
    int tID;
    String name;
    uint8_t port_data_mosi;
    uint8_t port_clock_sck;
    uint8_t port_latch;
    uint8_t cur_data;
    bool useSPI;
    unsigned long bitPulseTimer[8]={0,0,0,0,0,0,0,0};
    unsigned long bitPulseDelta[8]={0,0,0,0,0,0,0,0};


    ShiftReg(String name, uint8_t port_data_mosi, uint8_t port_clock_sck, uint8_t port_latch, bool useSPI=true )
        : name(name), port_data_mosi(port_data_mosi), port_clock_sck(port_clock_sck), port_latch(port_latch), useSPI(useSPI) {
        /*! Create 74HC595 mupplet
         *
         * This either use hardware SPI or 3 GPIOs. Functionality is equivalent.
         *
         * @param name Name of this mupplet.
         * @param port_data_mosi MOSI hardware pin for SPI mode or any GPIO for non-SPI mode.
         * @param port_clock_sck SCK hardware pin for SPI mode or any GPIO for non-SPI mode.
         * @param port_latch A GPIO used to latch the outputs during programming.
         * @param useSPI true: hardware SPI mode, false: GPIO bitbang mode.
         */    
    }

    ~ShiftReg() {
    }

    void begin(Scheduler *_pSched, unsigned long scheduleIntervalUsec=50000) {
        /*! Start mupplet
         *
         * @param pSched Pointer to scheduler object
         * @param scheduleIntervalUsec Period for pulse-timer scheduling, default 50ms
         */
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
        tID = pSched->add(ft, name, scheduleIntervalUsec);

        auto fnall = [=](String topic, String msg, String originator) {
                this->subsMsg(topic, msg, originator);
            };
        pSched->subscribe(tID, name + "/shiftreg/#", fnall);
    }

  private:
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

  public:
    void set(uint8_t data, uint8_t mask=0xff) {
        /*! Output to 74HC595
         *
         * Write complete byte or masked part to 74HC595 outputs and send MQTT update message.
         * 
         * @param data data byte for output.
         * @param mask masks that shows which bits should be changed to content of data. 
         * If a bit in mask is zero, then the old content of the shift register is maintained.
         */
        writeShiftReg(data,mask);
        char buf[32];
        sprintf(buf,"%d",cur_data);
        pSched->publish(name + "/shiftreg", buf);
    }

    void setBit(uint8_t bit, bool val) {
        /*! Change a single bit of 74HC595
         *
         * Set or reset a bit in 74HC595 shift register.
         * 
         * @param bit bit to change.
         * @param val true: set bit to 1, false: set bit to zero.
         */
        uint8_t cw=1<<bit;
        if (val) {
            set(cw,cw);
        } else {
            set(0,cw);
        }
    }

    void pulseBit(uint8_t bit, unsigned long ms=1000) {
        /*! Pulse a single bit of 74HC595 shift register for duration of ms milliseconds.
         *
         * Set a bit in 74HC595 shift register for a time perios of ms milliseconds (one-time).
         * 
         * @param bit bit to pulse.
         * @param ms high signal duration in milliseconds. Note: default timer for this mupplet 
         * is 50ms, which therefore is the minimum resolution for a pulse. Change parameter
         * scheduleIntervalUsec of the begin() method to increase the resolution. (Shorter 
         * schedule intervals allow for shorter pulses.)
         */
        if (bit>=0 and bit<8) {
            setBit(bit, true);
            bitPulseTimer[bit]=millis();
            bitPulseDelta[bit]=ms;
        }
    }

  private:
    void loop() {
        for (uint8_t bit=0; bit<8; bit++) {
            if (bitPulseTimer[bit]) {
                if (timeDiff(bitPulseTimer[bit],millis())>bitPulseDelta[bit]) {
                    bitPulseTimer[bit]=0;
                    bitPulseDelta[bit]=0;
                    setBit(bit,false);
                }
            }
        }
    }

    void subsMsg(String topic, String msg, String originator) {
        char msgbuf[128];
        memset(msgbuf,0,128);
        strncpy(msgbuf,msg.c_str(),127);
        // TODO: allow other data-formats than decimal
        if (topic == name + "/shiftreg/set/all") {
            uint8_t mask=0xff;
            char *p=strchr(msgbuf,',');
            if (p!=nullptr) {
                *p=0;
                ++p;
                mask=atoi(p);
            }
            uint8_t data=atoi(msgbuf);
            set(data,mask);
        } else {
            String wct=name+"/shiftreg/set/*";   // 8Bits:  /shiftreg/set/[0..7]
            int bit;
            if (pSched->mqttmatch(topic,wct )) {
                if (topic.length() >= wct.length()) {
                    const char *p=topic.c_str();
                    bit=atoi(&p[name.length()+strlen("/i2cpwm/")]);
                    if (bit>=0 and bit<=7) {
                        double level=parseUnitLevel(msg);
                        if (level==0.0) setBit(bit, false);
                        else setBit(bit, true);
                    }
                }
            }
        }
        String wct=name+"/shiftreg/pulse/*";   // 8Bits:  /shiftreg/set/[0..7]
        int bit;
        if (pSched->mqttmatch(topic,wct )) {
            if (topic.length() >= wct.length()) {
                const char *p=topic.c_str();
                bit=atoi(&p[name.length()+strlen("/i2cpwm/")]);
                if (bit>=0 and bit<=7) {
                    unsigned long ms=1000;
                    char *p=strchr(msgbuf,',');
                    if (p!=nullptr) {
                        *p=0;
                        ++p;
                        ms=atol(p);
                    }
                    double level=parseUnitLevel(msgbuf);
                    if (level==0.0) setBit(bit, false);
                    else {
                        pulseBit(bit, ms);
                    }
                }
            }
        }
        
    };
};  // ShiftReg

}  // namespace ustd
