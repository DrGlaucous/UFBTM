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
#include <defines_bmi160.h>

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
		
		struct Ctrl10C {
			static constexpr uint8_t reg = 0x19;
			static constexpr uint8_t value
				= (1 << 2);  //enable enteral functions (for sensorhub)
		};
		struct MasterConfig {
			static constexpr uint8_t reg = 0x1A;
			static constexpr uint8_t valuePassthrough
				= (1 << 2);  //enable master passthrough for configuring attached devices
			static constexpr uint8_t value
				= 1 | (1 << 3);  //enable the master sensorhub and internal pullups
		};
		struct FunctionCfgAccess {
			static constexpr uint8_t reg = 0x01;
			static constexpr uint8_t value
				= (1 << 7);  //enable function bank
		};
		//value is determined by the slave. we cannot preset it.
		struct Slave0Address {
			static constexpr uint8_t reg = 0x02;
		};
		struct Slave0SubAddress {
			static constexpr uint8_t reg = 0x03; //what register from slave0 to read from
		};
		struct Slave0Config { //general slave configuration register (how much data, rate, etc)
			static constexpr uint8_t reg = 0x04;
		};
		struct Sensorhub1Reg { //address where the start of the sensorhub data is stored (where the magnetometer data is retrieved)
			static constexpr uint8_t reg = 0x2E;
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

	void initHMC() {

		//disable accelerometer
		i2c.writeReg(Regs::Ctrl1XL::reg, 0b00000000);

		//enable internal functions
		uint8_t old_val = i2c.readReg(Regs::Ctrl10C::reg);
		i2c.writeReg(Regs::Ctrl10C::reg, old_val | Regs::Ctrl10C::value);

		//enable master passthrough mode
		i2c.writeReg(Regs::MasterConfig::reg, Regs::MasterConfig::valuePassthrough);

		//set up the QMC sensor
		i2c.writeRegAddr(HMC_DEVADDR, HMC_RA_CFGA, 
			HMC_CFGA_DATA_RATE_75 | HMC_CFGA_AVG_SAMPLES_8 | HMC_CFGA_BIAS_NORMAL
		);
		i2c.writeRegAddr(HMC_DEVADDR, HMC_RA_CFGB, HMC_CFGB_GAIN_1_30);
		i2c.writeRegAddr(HMC_DEVADDR, HMC_RA_MODE, HMC_MODE_HIGHSPEED | HMC_MODE_READ_CONTINUOUS);
		delay(3);

		//disable master mode
		i2c.writeReg(Regs::MasterConfig::reg, 0);

		//enable function bank
		i2c.writeReg(Regs::FunctionCfgAccess::reg, Regs::FunctionCfgAccess::value);

		//device address + r/w mode
		i2c.writeReg(Regs::Slave0Address::reg, (HMC_DEVADDR << 1) | 1);

		//write sub-address (data starts at 0x00)
		i2c.writeReg(Regs::Slave0SubAddress::reg, HMC_RA_DATA);

		//set byte length, max is 0b111, everything else left at default
		i2c.writeReg(Regs::Slave0Config::reg, 6);

		//disable function bank
		i2c.writeReg(Regs::FunctionCfgAccess::reg, 0);

		//enable master
		i2c.writeReg(Regs::MasterConfig::reg, Regs::MasterConfig::value);

	}
	void initQMC() {

		//disable accelerometer
		i2c.writeReg(Regs::Ctrl1XL::reg, 0b00000000);

		//enable internal functions
		uint8_t old_val = i2c.readReg(Regs::Ctrl10C::reg);
		i2c.writeReg(Regs::Ctrl10C::reg, old_val | Regs::Ctrl10C::value);

		//enable master passthrough mode
		i2c.writeReg(Regs::MasterConfig::reg, Regs::MasterConfig::valuePassthrough);

		//set up the QMC sensor
		i2c.writeRegAddr(QMC_DEVADDR, QMC_RA_RESET, 1);
		delay(3);
		i2c.writeRegAddr(QMC_DEVADDR, QMC_RA_CONTROL, 
			QMC_CFG_MODE_CONTINUOUS | QMC_CFG_ODR_200HZ | QMC_CFG_RNG_8G | QMC_CFG_OSR_512
		);
		delay(3);

		//disable master mode
		i2c.writeReg(Regs::MasterConfig::reg, 0);

		//enable function bank
		i2c.writeReg(Regs::FunctionCfgAccess::reg, Regs::FunctionCfgAccess::value);

		//device address + r/w mode
		i2c.writeReg(Regs::Slave0Address::reg, (QMC_DEVADDR << 1) | 1);

		//write sub-address (data starts at 0x00)
		i2c.writeReg(Regs::Slave0SubAddress::reg, QMC_RA_DATA);

		//set byte length, max is 0b111, everything else left at default
		i2c.writeReg(Regs::Slave0Config::reg, 6);

		//disable function bank
		i2c.writeReg(Regs::FunctionCfgAccess::reg, 0);

		//enable master
		i2c.writeReg(Regs::MasterConfig::reg, Regs::MasterConfig::value);

	}
	void initQMP() {

		//configure the QMP sensor
		//todo: make this other mags

		//disable accelerometer
		i2c.writeReg(Regs::Ctrl1XL::reg, 0b00000000);


		//enable internal functions
		//starting value is 0x38 for some reason... setting it to 0 or just our new value breaks the fifo, so we have to append our values
		uint8_t old_val = i2c.readReg(Regs::Ctrl10C::reg);
		i2c.writeReg(Regs::Ctrl10C::reg, old_val | Regs::Ctrl10C::value);


		//enable master passthrough mode
		i2c.writeReg(Regs::MasterConfig::reg, Regs::MasterConfig::valuePassthrough);


		//reset QMP. it doesn't like this for some reason... I'll remove it for now
		//I2Cdev::writeByte(QMP_DEVADDR, QMP_RA_CONTROL2, QMP_CFG_SOFT_RESET);
		//delay(20);


		//svr configures the mag to run at 200hz, but grabs data at 50hz
		//set up the QMP sensor
		//put in continuous operation mode with highest speeds
		i2c.writeRegAddr(QMP_DEVADDR, QMP_RA_CONTROL, 
			QMP_CFG_MODE_CONT | QMP_CFG_ODR_200HZ | QMP_CFG_OVR_SMPL8 | QMP_CFG_DOWN_SMPL8
		);
		delay(3);
		//set gauss range
		i2c.writeRegAddr(QMP_DEVADDR,
			QMP_RA_CONTROL2,
			QMP_CFG_RNG_8G
		);
		delay(3);

		//test to see if the magnetometer is indeed set up to run
		// while(1) {
		// 	uint8_t data[6] = {};
		// 	I2Cdev::readBytes(QMP_DEVADDR, 0x01, 6, data);
		// 	Serial.printf("%x|%x|%x|%x|%x|%x\n", data[0], data[1], data[2], data[3], data[4], data[5]);
		// 	delay(100);
		// }


		//disable accelerometer again (probably redundant)
		//i2c.writeReg(Regs::Ctrl1XL::reg, 0b00000000);
		//disable master mode
		i2c.writeReg(Regs::MasterConfig::reg, 0);

		////////////////////////////////////////////////////
		//set up sensor hub

		//enable function bank
		i2c.writeReg(Regs::FunctionCfgAccess::reg, Regs::FunctionCfgAccess::value);

		//device address + r/w mode
		i2c.writeReg(Regs::Slave0Address::reg, (QMP_DEVADDR << 1) | 1);

		//write sub-address (data starts at 0x01)
		i2c.writeReg(Regs::Slave0SubAddress::reg, QMP_RA_DATA);

		//set byte length, max is 0b111, everything else left at default
		i2c.writeReg(Regs::Slave0Config::reg, 6);

		//disable function bank
		i2c.writeReg(Regs::FunctionCfgAccess::reg, 0);

		//enable internal functions (probably redundant)
		//uint8_t old_val = i2c.readReg(Regs::Ctrl10C::reg);
		//i2c.writeReg(Regs::Ctrl10C::reg, old_val | Regs::Ctrl10C::value);

		//enable master
		i2c.writeReg(Regs::MasterConfig::reg, Regs::MasterConfig::value);



	}

	bool initialize(MagnetometerStatus magStatus) {
		// perform initialization step

		//whoami has already been determined by this point

		//reset
		i2c.writeReg(Regs::Ctrl3C::reg, Regs::Ctrl3C::valueSwReset);
		delay(20);

		//Serial.printf("Mag status: %d\n", magStatus);

		//initialize the magnetometer if enabled
		if(magStatus == MagnetometerStatus::MAG_ENABLED) {
			#if BMI160_MAG_TYPE == BMI160_MAG_TYPE_HMC
				initHMC();
			#elif BMI160_MAG_TYPE == BMI160_MAG_TYPE_QMC
				initQMC();
			#elif BMI160_MAG_TYPE == BMI160_MAG_TYPE_QMP
				initQMP();
			#else
				static_assert(false, "Mag is enabled but BMI160_MAG_TYPE not set in defines");
			#endif
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
			i2c.readBytes(Regs::Sensorhub1Reg::reg, sizeof(data), (uint8_t *)data);
			//i2c.readBytes(LSM6DS3_OUTX_L_XL, sizeof(data), (uint8_t *)data);

			//remap according to magnetometer type
			int16_t remappedxyz[3] = {};
			getMagnetometerXYZFromBuffer((uint8_t *)data, &remappedxyz[0], &remappedxyz[1], &remappedxyz[2]);


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

	//remap magnetometer data depending on type
	void getMagnetometerXYZFromBuffer(
		uint8_t* data,
		int16_t* x,
		int16_t* y,
		int16_t* z
	) {
	#if BMI160_MAG_TYPE == BMI160_MAG_TYPE_HMC
		// hmc5883l -> 0 msb 1 lsb
		// XZY order
		*x = ((int16_t)data[0] << 8) | data[1];
		*z = ((int16_t)data[2] << 8) | data[3];
		*y = ((int16_t)data[4] << 8) | data[5];
	#elif (BMI160_MAG_TYPE == BMI160_MAG_TYPE_QMC)
		// qmc5883l -> 0 lsb 1 msb
		// XYZ order
		*x = ((int16_t)data[1] << 8) | data[0];
		*y = ((int16_t)data[3] << 8) | data[2];
		*z = ((int16_t)data[5] << 8) | data[4];
	#elif (BMI160_MAG_TYPE == BMI160_MAG_TYPE_QMP)
		// qmc5883p -> 0 lsb 1 msb
		// XYZ order, but chip's axis itself is different
		//I'm only going by the datasheets here, I haven't physically tested a QMC or HMC unfortunately... :(
		//to perfectly convert the QMP to HMC, we need to:
		//leave z alone
		//send out Y as X
		//invert X and send it out as Y
		
		*y = (((int16_t)data[1] << 8) | data[0]) * -1; //raw x
		*x = ((int16_t)data[3] << 8) | data[2]; //raw y
		*z = ((int16_t)data[5] << 8) | data[4]; //raw z

	#endif
	}

};

}  // namespace SlimeVR::Sensors::SoftFusion::Drivers