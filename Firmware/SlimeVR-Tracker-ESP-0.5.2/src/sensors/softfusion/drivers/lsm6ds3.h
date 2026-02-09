/*
	SlimeVR Code is placed under the MIT license
	Copyright (c) 2024 Tailsy13 & SlimeVR Contributors

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
*/

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include <hmc5883l.h>
#include <qmc5883l.h>
#include <qmc5883p.h>


//just throw these in here for now. I'll format them properly once I get it working
///////////////////////////////////////////////////////////////////////////
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
///////////////////////////////////////////////////////////////////////////




namespace SlimeVR::Sensors::SoftFusion::Drivers {

// Driver uses acceleration range at 8g
// and gyroscope range at 1000dps
// Gyroscope ODR = 416Hz, accel ODR = 416Hz

template <typename I2CImpl>
struct LSM6DS3 {
	static constexpr uint8_t Address = 0x6a;
	static constexpr auto Name = "LSM6DS3";
	static constexpr auto Type = ImuID::LSM6DS3;

	static constexpr float Freq = 416;

	static constexpr float GyrTs = 1.0 / Freq;
	static constexpr float AccTs = 1.0 / Freq;
	static constexpr float MagTs = 1.0 / Freq;

	static constexpr float GyroSensitivity = 1000.0 / 35.0f;;
	static constexpr float AccelSensitivity = 1000.0 / 0.244f;

	I2CImpl i2c;
	SlimeVR::Logging::Logger logger;
	LSM6DS3(I2CImpl i2c, SlimeVR::Logging::Logger& logger)
		: i2c(i2c)
		, logger(logger) {}

	struct Regs {
		struct WhoAmI {
			static constexpr uint8_t reg = 0x0f;
			static constexpr uint8_t value = 0x69; //edited
		};
		static constexpr uint8_t OutTemp = 0x20; //compatible
		struct Ctrl1XL { //compatible
			static constexpr uint8_t reg = 0x10;
			static constexpr uint8_t value = (0b11 << 2) | (0b0110 << 4);  // 8g, 416Hz
		};
		struct Ctrl2G { //compatible
			static constexpr uint8_t reg = 0x11;
			static constexpr uint8_t value
				= (0b10 << 2) | (0b0110 << 4);  // 1000dps, 416Hz
		};
		struct Ctrl3C { //compatible
			static constexpr uint8_t reg = 0x12;
			static constexpr uint8_t valueSwReset = 1;
			static constexpr uint8_t value = (1 << 6) | (1 << 2);  // BDU = 1, IF_INC =
																   // 1
		};
		struct FifoCtrl3 { //compatible
			static constexpr uint8_t reg = 0x08;
			static constexpr uint8_t value
				= 0b001 | (0b001 << 3);  // accel no decimation, gyro no decimation
		};
		struct FifoCtrl5 {
			static constexpr uint8_t reg = 0x0a;
			static constexpr uint8_t value
				= 0b110 | (0b0111 << 3);  // continuous mode, odr = 833Hz
		};

		static constexpr uint8_t FifoStatus = 0x3a; //compatible
		static constexpr uint8_t FifoData = 0x3e; //compatible
	};

	bool initialize() {
		// perform initialization step

		//reset
		i2c.writeReg(Regs::Ctrl3C::reg, Regs::Ctrl3C::valueSwReset);
		delay(20);


		//whoami has already been determined by this point


		//configure the QMP sensor
		//todo: make this other mags
		if (1) {
			//disable accelerometer
			i2c.writeReg(LSM6DS3_CTRL1_XL, 0b00000000);

			//enable internal functions
			uint8_t old_val = i2c.readReg(LSM6DS3_CTRL10_C);
			// while(1) {
			// 	Serial.printf("%02X", old_val);
			// 	delay(1000);
			// }
			i2c.writeReg(LSM6DS3_CTRL10_C, old_val | (1 << 2));

			//enable master passthrough mode
			i2c.writeReg(LSM6DS3_MASTER_CONFIG, 1 << 2);


			//reset QMP. it doesn't like this for some reason... I'll remove it for now
			//I2Cdev::writeByte(QMP_DEVADDR, QMP_RA_CONTROL2, QMP_CFG_SOFT_RESET);
			//delay(20);


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
			// 	uint8_t data[6] = {};
			// 	I2Cdev::readBytes(QMP_DEVADDR, 0x01, 6, data);
			// 	Serial.printf("%x|%x|%x|%x|%x|%x\n", data[0], data[1], data[2], data[3], data[4], data[5]);
			// 	delay(100);
			// }


			//disable accelerometer
			i2c.writeReg(LSM6DS3_CTRL1_XL, 0b00000000);
			//disable master mode
			i2c.writeReg(LSM6DS3_MASTER_CONFIG, 0);

		}


		//set up sensor hub
		if (1) {
			//enable function bank
			i2c.writeReg(LSM6DS3_FUNC_CFG_ACCESS, (1 << 7));

			//device address + r/w mode
			i2c.writeReg(LSM6DS3_SLV0_ADD, (QMP_DEVADDR << 1) | 1);

			//write sub-address (data starts at 0x01)
			i2c.writeReg(LSM6DS3_SLV0_SUBADD, 0x01);

			//set byte length
			uint8_t slave0_cfg = i2c.readReg(LSM6DS3_SLAVE0_CONFIG);
			slave0_cfg |= 6; //set to read 6 bytes
			i2c.writeReg(LSM6DS3_SLAVE0_CONFIG, slave0_cfg);

			//disable function bank
			i2c.writeReg(LSM6DS3_FUNC_CFG_ACCESS, 0);

			//enable internal functions
			uint8_t old_val = i2c.readReg(LSM6DS3_CTRL10_C);
			i2c.writeReg(LSM6DS3_CTRL10_C, old_val | (1 << 2));

			//enable master
			i2c.writeReg(LSM6DS3_MASTER_CONFIG, 1 | (1 << 3));

		}



		//set output data rate to 416 hz, range is 8g
		i2c.writeReg(Regs::Ctrl1XL::reg, Regs::Ctrl1XL::value);

		//set gyro to 416hz with 1000 dps
		i2c.writeReg(Regs::Ctrl2G::reg, Regs::Ctrl2G::value);

		//enable block data updating (new data doesn't go in until all old data is read out) and auto-increment (i2c doesn't have to specify addressing, default is on)
		i2c.writeReg(Regs::Ctrl3C::reg, Regs::Ctrl3C::value);

		//buffer config: no decimation for either accelerometer or gyroscope
		i2c.writeReg(Regs::FifoCtrl3::reg, Regs::FifoCtrl3::value);

		//buffer config: ODR speed is 833hz in continuous mode
		i2c.writeReg(Regs::FifoCtrl5::reg, Regs::FifoCtrl5::value);
		return true;
	}

	float getDirectTemp() const {
		const auto value = static_cast<int16_t>(i2c.readReg16(Regs::OutTemp));
		float result = ((float)value / 256.0f) + 25.0f;

		return result;
	}

	//weird nasty c++ casting. gross.
	//this is the only read method called by the sensorfusion class
	template <typename AccelCall, typename GyroCall, typename MagCall>
	void bulkRead(AccelCall&& processAccelSample, GyroCall&& processGyroSample, MagCall&& processMagSample) {

		//get fifo status1 and fifo status2
		const auto read_result = i2c.readReg16(Regs::FifoStatus);
		if (read_result & 0x4000) {  // overrun!
			// disable and re-enable fifo to clear it
			logger.debug("Fifo overrun, resetting...");
			i2c.writeReg(Regs::FifoCtrl5::reg, 0);
			i2c.writeReg(Regs::FifoCtrl5::reg, Regs::FifoCtrl5::value);
			return;
		}
		const auto unread_entries = read_result & 0x7ff;
		constexpr auto single_measurement_words = 6;
		constexpr auto single_measurement_bytes
			= sizeof(uint16_t) * single_measurement_words;

		//also adding mag values in here, so make that 9 of them
		std::array<int16_t, 60>
			read_buffer;  // max 10 packages of 6 16bit values of data form fifo
		const auto bytes_to_read = std::min(
									   static_cast<size_t>(read_buffer.size()),
									   static_cast<size_t>(unread_entries)
								   )
								 * sizeof(uint16_t) / single_measurement_bytes
								 * single_measurement_bytes;

		i2c.readBytes(
			Regs::FifoData,
			bytes_to_read,
			reinterpret_cast<uint8_t*>(read_buffer.data())
		);


		//in theory, we could probably use the FIFO to store mag samples too, but reading them direct seems to work ok
		// for(uint16_t i = 0; i < bytes_to_read / sizeof(uint16_t); ++i) {
		// 	Serial.printf("%04X", read_buffer[i]);
		// }

		//if (i2c.readReg(LSM6DS3_STATUS_REG) & 0x01)
		if(bytes_to_read > 0)
		{
			//Serial.println();
			//Serial.printf("Gyro: %6d, %6d, %6d ", read_buffer[0], read_buffer[0 + 1], read_buffer[0 + 2]);
			//Serial.printf("Accel : %6d, %6d, %6d", read_buffer[0 + 3], read_buffer[0 + 4], read_buffer[0 + 5]);
			
			int16_t data[3];
			i2c.readBytes(LSM6DS3_SENSORHUB1_REG, sizeof(data), (uint8_t *)data);
			//i2c.readBytes(LSM6DS3_OUTX_L_XL, sizeof(data), (uint8_t *)data);

			//Serial.printf("Start Mag : %6d, %6d, %6d ||| ", data[0 + 0], data[0 + 1], data[0 + 2]);
			processMagSample(data, MagTs);
		}
		

		


		for (uint16_t i = 0; i < bytes_to_read / sizeof(uint16_t);
			 i += single_measurement_words) {
			processGyroSample(reinterpret_cast<const int16_t*>(&read_buffer[i]), GyrTs);
			processAccelSample(
				reinterpret_cast<const int16_t*>(&read_buffer[i + 3]),
				AccTs
			);
		}
	}
};

}  // namespace SlimeVR::Sensors::SoftFusion::Drivers