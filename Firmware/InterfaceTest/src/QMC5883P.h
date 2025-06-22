#pragma once

#include "Arduino.h"
#include "i2cdev/I2Cdev.h"


// # Compass base class
// class mag_base:
//     def __init__(self, i2c):
//         self.i2c = i2c

//     def read_scaled(self):
//         return (0.0, 0.0, 0.0, 0.0)

//     def read_raw(self):
//         return (0, 0, 0)

//     def set_sampling_rate(self, rate):
//         pass

//     def set_range(self, val):
//         pass

//     def set_oversampling(self, val):
//         pass

//     def reset(self):
//         pass


#define QMC5883P_ADDRESS 0x2C

#define CONFIG_2GAUSS (3)
#define CONFIG_8GAUSS (2)
#define CONFIG_12GAUSS (1)
#define CONFIG_30GAUSS (0)
#define CR2_SOFT_RESET (1 << 7)
#define CR1_MODE_SUSPEND (0)
#define CR1_MODE_NORMAL (1)
#define CR1_MODE_SINGLE (2)
#define CR1_MODE_CONT (3)
#define CR1_ODR_10HZ (0)
#define CR1_ODR_50HZ (1 << 2)
#define CR1_ODR_100HZ (2 << 2)
#define CR1_ODR_200HZ (3 << 2)
#define CR1_OVR_SMPL8 (0)
#define CR1_OVR_SMPL4 (1 << 4)
#define CR1_OVR_SMPL2 (2 << 4)
#define CR1_OVR_SMPL1 (3 << 4)
#define CR1_DOWN_SMPL1 (0)
#define CR1_DOWN_SMPL2 (1 << 6)
#define CR1_DOWN_SMPL4 (2 << 6)
#define CR1_DOWN_SMPL8 (3 << 6)


//for the QMC5883P, a different version of the QMC5883L, see:
//https://datasheet.lcsc.com/lcsc/2108072330_QST-QMC5883P_C2847467.pdf

class QMC5883P {

    private:


    //least significant bits per Gauss, used to scale with the range setting
    const uint16_t lsb_per_G[4] = {1000, 2500, 3750, 15000};
    uint8_t range = CONFIG_2GAUSS;

    //buffer for raw data output
    uint16_t xyz[3] = {};



    void _set_config(uint8_t value, uint8_t offset, uint8_t size) {
        uint8_t current = 0;
        I2Cdev::readByte(QMC5883P_ADDRESS, 0x0A, &current);
        current &= ~(((1 << size) - 1) << offset);
        current |= (value << offset);
        I2Cdev::writeByte(QMC5883P_ADDRESS, 0x0A, current);
    }


    public:

    QMC5883P() {

        //define sign for syz axis
        I2Cdev::writeByte(QMC5883P_ADDRESS, 0x29, 0x06);

        //2 gauss, most sensitive
        set_range(0);

        //initial config
        I2Cdev::writeByte(QMC5883P_ADDRESS, 0x0A, CR1_DOWN_SMPL8 | CR1_OVR_SMPL8 | CR1_ODR_200HZ | CR1_MODE_NORMAL);

        //wait until data is ready
        data_ready();

        set_range(0);
    }


    //python translated: https://docs.micropython.org/en/latest/library/machine.I2C.html
    //void i2c_writereg(int address, char* bytes)
    
    //char* bytes i2c_readregs(int address, int byte_count) //read a register set (almost always a single byte in the python version)
    //the only time it's not is when we get all 6 xyz bytes out

    //void range_select() (puts data into range variable) //get current gauss range?
    //void _set_config(int value, int offset, int shift); //internal config setter (set bit)
    //void set_sample_rate(int rate); //set sample rate
    //void set_oversampling_rate(int rate); //set oversampling rate
    //void set_range(int range); //set gauss range
    //void reset(); //reset magnetometer
    //void ready(); //wait unit ready



    void range_select() {
        range = CONFIG_2GAUSS;

        uint8_t status = 0;
        I2Cdev::readByte(QMC5883P_ADDRESS, 0x0B, &status);
        if(status & 0x02 != 0x00) {
            if(range == 0) {
                return;
            } else {
                range -= 1;
                I2Cdev::writeByte(QMC5883P_ADDRESS, 0x0B, range << 2);
            }
        }
    }


    void set_sample_rate(uint8_t rate) {
        _set_config(rate, 6, 2);
    }

    void set_oversampling_rate(uint8_t rate) {
        _set_config(rate, 4, 2);
    }


    void set_range(uint8_t value) {
        range = CONFIG_2GAUSS - value;
        I2Cdev::writeByte(QMC5883P_ADDRESS, 0x0B, range << 2);
    }


    //wait until the ready bit is set in the register
    int data_ready() {
        uint8_t status = 0x00;
        int i = 0;

        //wait until ready register
        while((status & 0x01) == 0x00) {
            ++i;
            delay(1);
            I2Cdev::readByte(QMC5883P_ADDRESS, 0x09, &status);
            if(i > 100) {
                return -1;
            }
        }
        return 0;
    }

    //reads the most recent magnetometer values into the local buffer, returning an immutable refrence to the result
    const uint16_t* read_raw() {
        //read from the first 6 addresses into the local buffer
        I2Cdev::readBytes(QMC5883P_ADDRESS, 0x01, 6, (uint8_t*)xyz);
        return xyz; //little endian format
    }

    //read the magnetometer values and scale them accordingly, returning an immutable refrence to the result
    const uint16_t* read_scaled() {
        read_raw();
        uint16_t scale = lsb_per_G[range];

        //re-scale the values
        xyz[0] /= scale;
        xyz[1] /= scale;
        xyz[2] /= scale;


        return xyz;

    }



};









