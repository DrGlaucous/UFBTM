/*
  This file is part of the Arduino_LSM6DS3 library.
  Copyright (c) 2019 Arduino SA. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <Arduino.h>
//#include <Wire.h>
//#include <SPI.h>
#include "i2cdev/I2Cdev.h"
#include "qmp_regs.h"

//this is equivalent to LSM6DS3_ADDRESS_LOW
#define LSM6DS3_ADDRESS 0x6A

//a, b
#define LSM6DS3_ADDRESS_LOW 0b1101010
#define LSM6DS3_ADDRESS_HIGH 0b1101011

//external sensor config registers
#define LSM6DS3_FUNC_CFG_ACCESS 0x01
#define LSM6DS3_SLV0_ADD 0x02 //external sensor address + r/w
#define LSM6DS3_SLV0_SUBADD 0x03 //starting register address on external sensor
#define LSM6DS3_SLAVE0_CONFIG 0x04 //configuration flags for the external sensor



#define LSM6DS3_WHO_AM_I_REG 0X0F
#define LSM6DS3_CTRL1_XL 0X10
#define LSM6DS3_CTRL2_G 0X11
#define LSM6DS3_CTRL3_C 0x12

#define LSM6DS3_STATUS_REG 0X1E

#define LSM6DS3_CTRL6_C 0X15
#define LSM6DS3_CTRL7_G 0X16
#define LSM6DS3_CTRL8_XL 0X17

//for functions and i2c master mode
#define LSM6DS3_CTRL10_C 0x19
#define LSM6DS3_MASTER_CONFIG 0x1A

#define LSM6DS3_OUT_TEMP_L 0X20

#define LSM6DS3_OUTX_L_G 0X22
#define LSM6DS3_OUTX_H_G 0X23
#define LSM6DS3_OUTY_L_G 0X24
#define LSM6DS3_OUTY_H_G 0X25
#define LSM6DS3_OUTZ_L_G 0X26
#define LSM6DS3_OUTZ_H_G 0X27

#define LSM6DS3_OUTX_L_XL 0X28
#define LSM6DS3_OUTX_H_XL 0X29
#define LSM6DS3_OUTY_L_XL 0X2A
#define LSM6DS3_OUTY_H_XL 0X2B
#define LSM6DS3_OUTZ_L_XL 0X2C
#define LSM6DS3_OUTZ_H_XL 0X2D

#define LSM6DS3_SENSORHUB1_REG 0x2E
#define LSM6DS3_SENSORHUB2_REG 0x2F
#define LSM6DS3_SENSORHUB3_REG 0x30
#define LSM6DS3_SENSORHUB4_REG 0x31
#define LSM6DS3_SENSORHUB5_REG 0x32
#define LSM6DS3_SENSORHUB6_REG 0x33
#define LSM6DS3_SENSORHUB7_REG 0x34
#define LSM6DS3_SENSORHUB8_REG 0x35
#define LSM6DS3_SENSORHUB9_REG 0x36
#define LSM6DS3_SENSORHUB10_REG 0x37
#define LSM6DS3_SENSORHUB11_REG 0x38
#define LSM6DS3_SENSORHUB12_REG 0x39

#define LSM6DS3_FIFO_STATUS1 0x3A
#define LSM6DS3_FIFO_STATUS2 0x3B
#define LSM6DS3_FIFO_STATUS3 0x3C
#define LSM6DS3_FIFO_STATUS4 0x3D

//check if data is ready on the sensor hub
#define LSM6DS3_FUNC_SRC 0x53

class LSM6DS3
{
public:

	LSM6DS3(uint8_t slaveAddress) :	_slaveAddress(slaveAddress)
	{
	}


	// recall: destructors should be virtual
	virtual ~LSM6DS3()
	{
	}

	int begin() {


		if (!(readRegister(LSM6DS3_WHO_AM_I_REG) == 0x6C || readRegister(LSM6DS3_WHO_AM_I_REG) == 0x69))
		{
			end();
			return 0;
		}


		//reset and block until reset is finished
		writeRegister(LSM6DS3_CTRL3_C, 1);
		uint8_t result = 0;
		do {
			result = readRegister(LSM6DS3_CTRL3_C);
			Serial.printf("Result: %d\n", result);
		} while(result & 1);


		//write to peripheral (leaving unfinished for now, we'll come back to it later)
		if (0) {
			//enable internal functions
			writeRegister(LSM6DS3_CTRL10_C, (1 << 2));
		
			//enable master mode
			writeRegister(LSM6DS3_MASTER_CONFIG, 1 << 2);
		}


		//set up sensor hub to read data
		
		//enable function bank
		writeRegister(LSM6DS3_FUNC_CFG_ACCESS, (1 << 7));

		//device address + r/w mode
		writeRegister(LSM6DS3_SLV0_ADD, (QMP_DEVADDR << 1) | 1);

		//set byte length
		uint8_t slave0_cfg = readRegister(LSM6DS3_SLAVE0_CONFIG);
		slave0_cfg |= 6; //set to read 6 bytes
		writeRegister(LSM6DS3_SLAVE0_CONFIG, slave0_cfg);

		//disable function bank
		writeRegister(LSM6DS3_FUNC_CFG_ACCESS, 0);


		//set device count (no need: 00 = 1 device)


		//enable internal functions
		writeRegister(LSM6DS3_CTRL10_C, 1 << 2);

		//enable master
		writeRegister(LSM6DS3_MASTER_CONFIG, 1 | (1 << 3));

		//set output data rate to 52 hz
		writeRegister(LSM6DS3_CTRL1_XL, 0b00110000);




		return 1;
	}

	int begin2() {

		if (!(readRegister(LSM6DS3_WHO_AM_I_REG) == 0x6C || readRegister(LSM6DS3_WHO_AM_I_REG) == 0x69))
		{
			end();
			return 0;
		}

		//reset device
		writeRegister(LSM6DS3_CTRL3_C, 1);
		delay(1000);



		//writeRegister(LSM6DS3_CTRL10_C, (1 << 2)); //FUNC_EN = on

		//configure mag via passthrough
		{
			writeRegister(LSM6DS3_MASTER_CONFIG, 1 << 2); //enable passthrough
			//svr configures the mag to run at 200hz, but grabs data at 50hz
			//set up the QMP sensor
			//put in continuous operation mode with highest speeds
			I2Cdev::writeByte(QMP_DEVADDR, QMP_RA_CONTROL, 
				QMP_CFG_MODE_CONT | QMP_CFG_ODR_200HZ | QMP_CFG_OVR_SMPL8 | QMP_CFG_DOWN_SMPL8
			);
			delay(3);
			//set gauss range
			I2Cdev::writeByte(QMP_DEVADDR,
				QMP_RA_CONTROL2,
				QMP_CFG_RNG_8G
			);
			delay(3);

			// while(1) {
			// 	int16_t xyz[3];
			// 	I2Cdev::readBytes(QMC5883P_ADDRESS, 0x01, 6, (uint8_t*)xyz);
			// 	printf("XYZ: %d, %d, %d\n", xyz[0], xyz[1], xyz[2]);
			// }

			//disable the i2c passthrough for the rest of the configuration
			writeRegister(LSM6DS3_MASTER_CONFIG, 0);
			writeRegister(LSM6DS3_CTRL10_C, 0); //FUNC_EN = off
			delay(20);
		}

		//writeRegister(LSM6DS3_FUNC_CFG_ACCESS, 1 << 7);

		writeRegister(LSM6DS3_MASTER_CONFIG, (1 << 3) | (1 << 0)); //MASTER_ON and PULL_UP_EN (1 << 3) optional
		
		writeRegister(LSM6DS3_SLV0_ADD, (QMP_DEVADDR << 1) | 0); //device address + r/w mode

		writeRegister(LSM6DS3_SLV0_SUBADD, QMP_RA_DATA); //data address

		writeRegister(LSM6DS3_SLAVE0_CONFIG, 0x6);

		//writeRegister(LSM6DS3_FUNC_CFG_ACCESS, 0);

		writeRegister(LSM6DS3_CTRL10_C, (1 << 2) | 0x20); //FUNC_EN = on


		//enable mag and gyro
		// set the gyroscope control register to work at 104 Hz, 2000 dps and in bypass mode
		writeRegister(LSM6DS3_CTRL2_G, 0x4C);
		// Set the Accelerometer control register to work at 104 Hz, 4 g,and in bypass mode and enable ODR/4
		// low pass filter (check figure9 of LSM6DS3's datasheet)
		writeRegister(LSM6DS3_CTRL1_XL, 
			0x40
		);
		// set gyroscope power mode to high performance and bandwidth to 16 MHz
		writeRegister(LSM6DS3_CTRL7_G, 0x00);
		// Set the ODR config register to ODR/4
		writeRegister(LSM6DS3_CTRL8_XL, 0x09);



		return 1;
	}

	int begin3()
	{

		if (!(readRegister(LSM6DS3_WHO_AM_I_REG) == 0x6C || readRegister(LSM6DS3_WHO_AM_I_REG) == 0x69))
		{
			end();
			return 0;
		}

		//reset device
		writeRegister(LSM6DS3_CTRL3_C, 1);
		delay(1000);

		//In the docs the MSB is on the left.
		//set up the sensor hub and magnetometer (it might be required to do this before starting the accelerometer itself)

		writeRegister(LSM6DS3_MASTER_CONFIG, (1 << 2) | (1 << 3)); //enable passthrough
		//svr configures the mag to run at 200hz, but grabs data at 50hz
		//set up the QMP sensor
		//put in continuous operation mode with highest speeds
		I2Cdev::writeByte(QMP_DEVADDR, QMP_RA_CONTROL, 
			QMP_CFG_MODE_CONT | QMP_CFG_ODR_200HZ | QMP_CFG_OVR_SMPL8 | QMP_CFG_DOWN_SMPL8
		);
		delay(3);
		//set gauss range
		I2Cdev::writeByte(QMP_DEVADDR,
			QMP_RA_CONTROL2,
			QMP_CFG_RNG_8G
		);
		delay(3);

		// while(1) {
		// 	int16_t xyz[3];
		// 	I2Cdev::readBytes(QMC5883P_ADDRESS, 0x01, 6, (uint8_t*)xyz);
		// 	printf("XYZ: %d, %d, %d\n", xyz[0], xyz[1], xyz[2]);
		// }

		//disable the i2c passthrough for the rest of the configuration
		writeRegister(LSM6DS3_MASTER_CONFIG, 0);
		delay(3);
		writeRegister(LSM6DS3_CTRL10_C, (1 << 2)); //FUNC_EN = on
		writeRegister(LSM6DS3_FUNC_CFG_ACCESS, 1 << 7);
		//set up the sensor hub for auto-reading
		writeRegister(LSM6DS3_SLV0_ADD, (QMP_DEVADDR << 1) | 1); //device address + r/w mode
		delay(3);
		writeRegister(LSM6DS3_SLV0_SUBADD, QMP_RA_DATA); //data address
		delay(3);

		writeRegister(LSM6DS3_SLAVE0_CONFIG, 6);

		// writeRegister(LSM6DS3_SLAVE0_CONFIG, 
		// 	0b00 << 6 //no decimation rate (fastest)
		// 	| 0b00 << 4 //one sensor
		// 	| 0b0 << 3 //source mode read disabled
		// 	| 6 //read 6 bytes from sensor 1
		// );
		delay(3);
		

		writeRegister(LSM6DS3_MASTER_CONFIG, (1 << 3) | (1 << 0)); //MASTER_ON and PULL_UP_EN (1 << 3) optional
		


		return 1;
	}

	void end()
	{

		writeRegister(LSM6DS3_CTRL2_G, 0x00);
		writeRegister(LSM6DS3_CTRL1_XL, 0x00);

	}

	////////////////////////// Accelerometer

	// Results are in g (earth gravity).
	virtual int readAcceleration(float &x, float &y, float &z)
	{
		int16_t data[3];

		if (!readRegisters(LSM6DS3_OUTX_L_XL, (uint8_t *)data, sizeof(data)))
		{
			x = NAN;
			y = NAN;
			z = NAN;

			return 0;
		}

		x = data[0] * 4.0 / 32768.0;
		y = data[1] * 4.0 / 32768.0;
		z = data[2] * 4.0 / 32768.0;

		return 1;
	}

	// Sampling rate of the sensor, is a constant for now.
	virtual float accelerationSampleRate()
	{
		return 104.0F;
	}

	// Check for available data from accelerometer
	virtual int accelerationAvailable()
	{
		if (readRegister(LSM6DS3_STATUS_REG) & 0x01)
		{
			return 1;
		}

		return 0;
	}

	////////////////////////// Gyroscope

	// Results are in degrees/second.
	virtual int readGyroscope(float &x, float &y, float &z)
	{
		int16_t data[3];

		if (!readRegisters(LSM6DS3_OUTX_L_G, (uint8_t *)data, sizeof(data)))
		{
			x = NAN;
			y = NAN;
			z = NAN;

			return 0;
		}

		x = data[0] * 2000.0 / 32768.0;
		y = data[1] * 2000.0 / 32768.0;
		z = data[2] * 2000.0 / 32768.0;

		return 1;
	}

	// Sampling rate of the sensor.
	virtual float gyroscopeSampleRate()
	{
		return 104.0F;
	}

	// Check for available data from gyroscope
	virtual int gyroscopeAvailable()
	{
		if (readRegister(LSM6DS3_STATUS_REG) & 0x02)
		{
			return 1;
		}

		return 0;
	}

	////////////////////////// Temperature Sensor

	// Results are in deg. C
	virtual int readTemperature(float &t)
	{
		int16_t data[1];

		if (!readRegisters(LSM6DS3_OUT_TEMP_L, (uint8_t *)data, sizeof(data)))
		{
			t = NAN;

			return 0;
		}

		t = data[0] / 16.0 + 25;

		return 1;
	}

	// Sampling rate of the sensor.
	virtual float temperatureSampleRate()
	{
		return 52.0F;
	}

	// Check for available data from temperature sensor
	virtual int temperatureAvailable()
	{
		if (readRegister(LSM6DS3_STATUS_REG) & 0x04)
		{
			return 1;
		}

		return 0;
	}


	/////////////////////////// Magnetometer sensor

	// Results are in uh... numbers
	virtual int readMagneto(float &x, float &y, float &z) {
		int16_t data[3];

		if (!readRegisters(LSM6DS3_SENSORHUB1_REG, (uint8_t *)data, sizeof(data)))
		{
			x = NAN;
			y = NAN;
			z = NAN;

			return 0;
		}
		
		x = data[0];
		y = data[1];
		z = data[2];

		return 1;

		
	}

	// Sampling rate of the sensor, is a constant for now.
	virtual float magnetoSampleRate()
	{
		return 104.0F;
	}

	// Check for available data from the magneto sensor
	virtual int magnetoAvailable()
	{
		return accelerationAvailable();
	}





protected:
	int readRegister(uint8_t address)
	{
		uint8_t value;

		if (readRegisters(address, &value, sizeof(value)) != 1)
		{
			return -1;
		}

		return value;
	}

	int readRegisters(uint8_t address, uint8_t *data, size_t length)
	{

		I2Cdev::readBytes(_slaveAddress, address, length, data);
		return 1;
	}

	int writeRegister(uint8_t address, uint8_t value)
	{

		I2Cdev::writeByte(_slaveAddress, address, value);

		return 1;
	}

private:
	uint8_t _slaveAddress;
	int _csPin;
	int _irqPin;

};




