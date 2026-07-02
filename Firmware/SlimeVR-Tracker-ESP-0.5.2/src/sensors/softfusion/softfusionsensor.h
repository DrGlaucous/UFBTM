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

#include "../SensorFusionRestDetect.h"
#include "../sensor.h"
#include "GlobalVars.h"

namespace SlimeVR::Sensors {

template <template <typename I2CImpl> typename T, typename I2CImpl>
class SoftFusionSensor : public Sensor {
	using imu = T<I2CImpl>;
	using RawVectorT = std::array<int16_t, 3>;
	static constexpr auto UpsideDownCalibrationInit = true;
	static constexpr auto GyroCalibDelaySeconds = 5;
	static constexpr auto GyroCalibSeconds = 5;
	static constexpr auto SampleRateCalibDelaySeconds = 1;
	static constexpr auto SampleRateCalibSeconds = 5;

	static constexpr auto AccelCalibDelaySeconds = 3;
	static constexpr auto AccelCalibRestSeconds = 3;

	static constexpr double GScale
		= ((32768. / imu::GyroSensitivity) / 32768.) * (PI / 180.0);
	static constexpr double AScale = CONST_EARTH_GRAVITY / imu::AccelSensitivity;

	static constexpr bool HasMotionlessCalib
		= requires(imu& i) { typename imu::MotionlessCalibrationData; };
	static constexpr size_t MotionlessCalibDataSize() {
		if constexpr (HasMotionlessCalib) {
			return sizeof(typename imu::MotionlessCalibrationData);
		} else {
			return 0;
		}
	}

	//doesn't need to be in the sub-class
	int axisRemap;

	//check the sensor status at the whoami register
	bool detected() const {
		const auto value = m_sensor.i2c.readReg(imu::Regs::WhoAmI::reg);
		if (imu::Regs::WhoAmI::value != value) {
			m_Logger.error(
				"Sensor not detected, expected reg 0x%02x = 0x%02x but got 0x%02x",
				imu::Regs::WhoAmI::reg,
				imu::Regs::WhoAmI::value,
				value
			);
			return false;
		}

		return true;
	}

	void sendTempIfNeeded() {
		uint32_t now = micros();
		constexpr float maxSendRateHz = 2.0f;
		constexpr uint32_t sendInterval = 1.0f / maxSendRateHz * 1e6;
		uint32_t elapsed = now - m_lastTemperaturePacketSent;
		if (elapsed >= sendInterval) {
			const float temperature = m_sensor.getDirectTemp();
			m_lastTemperaturePacketSent = now - (elapsed - sendInterval);
			networkConnection.sendTemperature(sensorId, temperature);
		}
	}

	//tell the gyro accelerometer magnetometer combiner to... create a new fusion object?
	void recalcFusion() {
		m_fusion = SensorFusionRestDetect(
			m_calibration.G_Ts,
			m_calibration.A_Ts,
			m_calibration.M_Ts
		);
	}

	//take our axis values, apply temperature offset to them, and then update the m_fusion with that.
	void processAccelSample(const int16_t xyz[3], const sensor_real_t timeDelta) {
		sensor_real_t accelData[]
			= {static_cast<sensor_real_t>(xyz[0]),
			   static_cast<sensor_real_t>(xyz[1]),
			   static_cast<sensor_real_t>(xyz[2])};

		float tmp[3];
		for (uint8_t i = 0; i < 3; i++) {
			tmp[i] = (accelData[i] - m_calibration.A_B[i]);
		}

		accelData[0]
			= (m_calibration.A_Ainv[0][0] * tmp[0] + m_calibration.A_Ainv[0][1] * tmp[1]
			   + m_calibration.A_Ainv[0][2] * tmp[2])
			* AScale;
		accelData[1]
			= (m_calibration.A_Ainv[1][0] * tmp[0] + m_calibration.A_Ainv[1][1] * tmp[1]
			   + m_calibration.A_Ainv[1][2] * tmp[2])
			* AScale;
		accelData[2]
			= (m_calibration.A_Ainv[2][0] * tmp[0] + m_calibration.A_Ainv[2][1] * tmp[1]
			   + m_calibration.A_Ainv[2][2] * tmp[2])
			* AScale;

		//remap
		remapAllAxis(AXIS_REMAP_GET_ALL_IMU(axisRemap), &accelData[0], &accelData[1], &accelData[2]);


		m_fusion.updateAcc(accelData, m_calibration.A_Ts);
	}

	//ditto, but for gyroscope
	void processGyroSample(const int16_t xyz[3], const sensor_real_t timeDelta) {

		//apply scale
		sensor_real_t gyroData[] = {
			static_cast<sensor_real_t>(
				GScale * (static_cast<sensor_real_t>(xyz[0]) - m_calibration.G_off[0])
			),
			static_cast<sensor_real_t>(
				GScale * (static_cast<sensor_real_t>(xyz[1]) - m_calibration.G_off[1])
			),
			static_cast<sensor_real_t>(
				GScale * (static_cast<sensor_real_t>(xyz[2]) - m_calibration.G_off[2])
			)
		};

		//apply tempcal (todo: add this, see: bmi160sensor.cpp)


		//remap (why wasn't this already here?!)
		remapAllAxis(AXIS_REMAP_GET_ALL_IMU(axisRemap), &gyroData[0], &gyroData[1], &gyroData[2]);


		m_fusion.updateGyro(gyroData, m_calibration.G_Ts);
	}

	//new: process magnetometer data now
	void processMagSample(const int16_t xyz[3], const sensor_real_t timeDelta) {

		if(m_calibration.magEnabled == false) {
			return;
		}


		//apply calibration offsets
		sensor_real_t magData[]
			= {static_cast<sensor_real_t>(xyz[0]),
			   static_cast<sensor_real_t>(xyz[1]),
			   static_cast<sensor_real_t>(xyz[2])};

		

		//Serial.printf("  Pre Mag : %6f, %6f, %6f\n", magData[0 + 0], magData[0 + 1], magData[0 + 2]);

		float tmp[3];
		for (uint8_t i = 0; i < 3; i++) {
			tmp[i] = (magData[i] - m_calibration.M_B[i]);
		}

		magData[0] = m_calibration.M_Ainv[0][0] * tmp[0] + m_calibration.M_Ainv[0][1] * tmp[1]
			       + m_calibration.M_Ainv[0][2] * tmp[2];
		magData[1] = m_calibration.M_Ainv[1][0] * tmp[0] + m_calibration.M_Ainv[1][1] * tmp[1]
			       + m_calibration.M_Ainv[1][2] * tmp[2];
		magData[2] = m_calibration.M_Ainv[2][0] * tmp[0] + m_calibration.M_Ainv[2][1] * tmp[1]
			       + m_calibration.M_Ainv[2][2] * tmp[2];

		//remap
		remapAllAxis(AXIS_REMAP_GET_ALL_MAG(axisRemap), &magData[0], &magData[1], &magData[2]);

		//Serial.printf("Mag: %6f, %6f, %6f\n", magData[0 + 0], magData[0 + 1], magData[0 + 2]);

		//update the sensor fusion
		m_fusion.updateMag(magData, m_calibration.M_Ts);
	}



	//block for X seconds and pull all the sample data into dummy variables
	void eatSamplesForSeconds(const uint32_t seconds) {
		const auto targetDelay = millis() + 1000 * seconds;
		auto lastSecondsRemaining = seconds;
		while (millis() < targetDelay) {
#ifdef ESP8266
			ESP.wdtFeed(); //ensure the dog is fed while we're stuck in this while loop
#endif
			//log every second passed
			auto currentSecondsRemaining = (targetDelay - millis()) / 1000;
			if (currentSecondsRemaining != lastSecondsRemaining) {
				m_Logger.info("%d...", currentSecondsRemaining + 1);
				lastSecondsRemaining = currentSecondsRemaining;
			}
			m_sensor.bulkRead(
				[](const int16_t xyz[3], const sensor_real_t timeDelta) {},
				[](const int16_t xyz[3], const sensor_real_t timeDelta) {},
				[](const int16_t xyz[3], const sensor_real_t timeDelta) {}
			);
		}
	}

	//block for X milliseconds and pull all data except the last one read into dummy variables
	std::tuple<RawVectorT, RawVectorT, RawVectorT> eatSamplesReturnLast(const uint32_t milliseconds
	) {
		RawVectorT accel = {0};
		RawVectorT gyro = {0};
		RawVectorT mag = {0};
		const auto targetDelay = millis() + milliseconds;
		while (millis() < targetDelay) {
			m_sensor.bulkRead(
				[&](const int16_t xyz[3], const sensor_real_t timeDelta) {
					accel[0] = xyz[0];
					accel[1] = xyz[1];
					accel[2] = xyz[2];
				},
				[&](const int16_t xyz[3], const sensor_real_t timeDelta) {
					gyro[0] = xyz[0];
					gyro[1] = xyz[1];
					gyro[2] = xyz[2];
				},
				[&](const int16_t xyz[3], const sensor_real_t timeDelta) {
					mag[0] = xyz[0];
					mag[1] = xyz[1];
					mag[2] = xyz[2];
				}
			);
			yield();
		}
		return std::make_tuple(accel, gyro, mag);
	}

public:
	static constexpr auto TypeID = imu::Type;
	static constexpr uint8_t Address = imu::Address;

	//construct, filling the parent sensor, the IMU interface (as seen in the drivers folder), and the fusion algorithm
	SoftFusionSensor(
		uint8_t id,
		uint8_t addrSuppl,
		float rotation,
		uint8_t sclPin,
		uint8_t sdaPin,
		int axisRemapParam
		//what is this? I think it's a pit for the axisRemapParam since this originally didn't use it.
		//luckily, we can fix that. Goodbye unnamed uint8_t.
		//uint8_t
	)
		: Sensor(
			imu::Name,
			imu::Type,
			id,
			imu::Address + addrSuppl,
			rotation,
			sclPin,
			sdaPin
		)
		, m_fusion(imu::GyrTs, imu::AccTs, imu::MagTs)
		, m_sensor(I2CImpl(imu::Address + addrSuppl), m_Logger) {

			//determine if we use the passed-in remapping
			if (axisRemapParam < 256) {
				//Serial.printf("Using default remap, old: %d\n", axisRemapParam);
				axisRemap = AXIS_REMAP_DEFAULT;
			} else {
				//Serial.printf("Using axis remap: %d\n", axisRemapParam);
				axisRemap = axisRemapParam;
			}
		}

	~SoftFusionSensor() {}

	//motion loop override for sensor fusion thingies; checks for fresh data and updates the fusion algorithm, then sends the data out
	void motionLoop() override final {
		sendTempIfNeeded();

		// read fifo updating fusion
		uint32_t now = micros();
		constexpr uint32_t targetPollIntervalMicros = 6000;
		uint32_t elapsed = now - m_lastPollTime;
		if (elapsed >= targetPollIntervalMicros) {
			m_lastPollTime = now - (elapsed - targetPollIntervalMicros);
			m_sensor.bulkRead(
				[&](const int16_t xyz[3], const sensor_real_t timeDelta) {
					processAccelSample(xyz, timeDelta);
				},
				[&](const int16_t xyz[3], const sensor_real_t timeDelta) {
					processGyroSample(xyz, timeDelta);
				},
				[&](const int16_t xyz[3], const sensor_real_t timeDelta) {
					processMagSample(xyz, timeDelta);
				}
			);
			optimistic_yield(100);
			//don't try to send out new data if the fusion algorithm hasn't changed
			if (!m_fusion.isUpdated()) {
				return;
			}
			hadData = true;
			//set the update variable to false; we've got the newest ones
			m_fusion.clearUpdated();
		}

		// send new fusion values when time is up
		now = micros();
		constexpr float maxSendRateHz = 120.0f;
		constexpr uint32_t sendInterval = 1.0f / maxSendRateHz * 1e6;
		elapsed = now - m_lastRotationPacketSent;
		if (elapsed >= sendInterval) {
			m_lastRotationPacketSent = now - (elapsed - sendInterval);

			setFusedRotation(m_fusion.getQuaternionQuat());
			setAcceleration(m_fusion.getLinearAccVec());
			optimistic_yield(100);
		}
	}

	//motion setup overrides for sensor fusion thingies; starts the sensor and does calibration as-needed
	void motionSetup() override final {
		//check the whoami register for a valid fusion sensor
		if (!detected()) {
			m_status = SensorStatus::SENSOR_ERROR;
			return;
		}

		SlimeVR::Configuration::SensorConfig sensorCalibration
			= configuration.getSensor(sensorId);


		// If no compatible calibration data is found, the calibration data will just be
		// zeroed out
		if (sensorCalibration.type == SlimeVR::Configuration::SensorConfigType::SFUSION
			&& (sensorCalibration.data.sfusion.ImuType == imu::Type)
			&& (sensorCalibration.data.sfusion.MotionlessDataLen
				== MotionlessCalibDataSize())) {
			m_calibration = sensorCalibration.data.sfusion;

			//m_Logger.debug("Mag status: %d", sensorCalibration.data.sfusion.magEnabled);

			//restore magnetometer status
			magStatus = m_calibration.magEnabled ? MagnetometerStatus::MAG_ENABLED
												: MagnetometerStatus::MAG_DISABLED;

			m_Logger.info("Calibration settings loaded for sensor %d", sensorId);

			recalcFusion();
		} else if (sensorCalibration.type == SlimeVR::Configuration::SensorConfigType::NONE) {
			m_Logger.warn(
				"No calibration data found for sensor %d, ignoring...",
				sensorId
			);
			m_Logger.info("Calibration is advised");

			//shoehorn test for now
			magStatus = m_calibration.magEnabled ? MagnetometerStatus::MAG_ENABLED
												: MagnetometerStatus::MAG_DISABLED;

		} else {
			m_Logger.warn(
				"Incompatible calibration data found for sensor %d, ignoring...",
				sensorId
			);
			m_Logger.info("Please recalibrate");

			//shoehorn test for now
			magStatus = m_calibration.magEnabled ? MagnetometerStatus::MAG_ENABLED
												: MagnetometerStatus::MAG_DISABLED;

		}

		bool initResult = false;

		if constexpr (HasMotionlessCalib) {
			typename imu::MotionlessCalibrationData calibData;
			std::memcpy(&calibData, m_calibration.MotionlessData, sizeof(calibData));
			initResult = m_sensor.initialize(calibData);
		} else {
			initResult = m_sensor.initialize(magStatus);
		}

		if (!initResult) {
			m_Logger.error("Sensor failed to initialize!");
			m_status = SensorStatus::SENSOR_ERROR;
			return;
		}

		m_status = SensorStatus::SENSOR_OK;
		working = true;
		[[maybe_unused]] auto lastRawSample = eatSamplesReturnLast(1000);
		//sensor is inverted: start the calibration routine
		if constexpr (UpsideDownCalibrationInit) {
			auto gravity = static_cast<sensor_real_t>(
				AScale * static_cast<sensor_real_t>(std::get<0>(lastRawSample)[2])
			);
			m_Logger.info(
				"Gravity read: %.1f (need < -7.5 to start calibration)",
				gravity
			);
			if (gravity < -7.5f) {
				ledManager.on();
				m_Logger.info("Flip front in 5 seconds to start calibration");
				lastRawSample = eatSamplesReturnLast(5000);
				gravity = static_cast<sensor_real_t>(
					AScale * static_cast<sensor_real_t>(std::get<0>(lastRawSample)[2])
				);
				if (gravity > 7.5f) {
					m_Logger.debug("Starting calibration...");
					startCalibration(0);
				} else {
					m_Logger.info("Flip not detected. Skipping calibration.");
				}

				ledManager.off();
			}
		}
	}

	//set flag (enable/disable magnetometer)
	void setFlag(uint16_t flagId, bool state) {
		if (flagId == FLAG_SENSOR_BMI160_MAG_ENABLED) {
			m_calibration.magEnabled = state;
			magStatus = state ? MagnetometerStatus::MAG_ENABLED
							: MagnetometerStatus::MAG_DISABLED;

			//update backend settings
			SlimeVR::Configuration::SensorConfig config;
			config.type = SlimeVR::Configuration::SensorConfigType::SFUSION;
			config.data.sfusion = m_calibration;
			configuration.setSensor(sensorId, config);

			// Reinitialize the sensor
			motionSetup();
		}

	}

	//run the calibration routine for the IMU, 
	void startCalibration(int calibrationType) override final {
		
		//Serial.printf("Requested calibration of type: %d\n", calibrationType);

		//should be a switch statement, but I'm too lazy to change it right now; they've probably already updated it with the latest SVR firmware.
		if (calibrationType == 0) {
			// ALL
			calibrateSampleRate();
			if constexpr (HasMotionlessCalib) {
				typename imu::MotionlessCalibrationData calibData;
				m_sensor.motionlessCalibration(calibData);
				std::memcpy(
					m_calibration.MotionlessData,
					&calibData,
					sizeof(calibData)
				);
			}
			// Gryoscope offset calibration can only happen after any motionless
			// gyroscope calibration, otherwise we are calculating the offset based
			// on an incorrect starting point
			calibrateGyroOffset();
			calibrateAccel();
			calibrateMag();
		} else if (calibrationType == 1) {
			calibrateSampleRate();
		} else if (calibrationType == 2) {
			calibrateGyroOffset();
		} else if (calibrationType == 3) {
			calibrateAccel();
		} else if (calibrationType == 4) {
			if constexpr (HasMotionlessCalib) {
				typename imu::MotionlessCalibrationData calibData;
				m_sensor.motionlessCalibration(calibData);
				std::memcpy(
					m_calibration.MotionlessData,
					&calibData,
					sizeof(calibData)
				);
			} else {
				m_Logger.info("Sensor doesn't provide any custom motionless calibration"
				);
			}
		} else if (calibrationType == 5) {
			calibrateMag();
		}

		saveCalibration();
	}

	void saveCalibration() {
		m_Logger.debug("Saving the calibration data");
		SlimeVR::Configuration::SensorConfig calibration;
		calibration.type = SlimeVR::Configuration::SensorConfigType::SFUSION;
		calibration.data.sfusion = m_calibration;
		configuration.setSensor(sensorId, calibration);
		configuration.save();
	}

	//calibrate gyroscope
	void calibrateGyroOffset() {
		// Wait for sensor to calm down before calibration
		m_Logger.info(
			"Put down the device and wait for baseline gyro reading calibration (%d "
			"seconds)",
			GyroCalibDelaySeconds
		);
		ledManager.on();
		eatSamplesForSeconds(GyroCalibDelaySeconds);
		ledManager.off();

		m_calibration.temperature = m_sensor.getDirectTemp();
		m_Logger.trace("Calibration temperature: %f", m_calibration.temperature);

		ledManager.pattern(100, 100, 3);
		ledManager.on();
		m_Logger.info("Gyro calibration started...");

		int32_t sumXYZ[3] = {0};
		const auto targetCalib = millis() + 1000 * GyroCalibSeconds;
		uint32_t sampleCount = 0;

		while (millis() < targetCalib) {
#ifdef ESP8266
			ESP.wdtFeed();
#endif
			m_sensor.bulkRead(
				[](const int16_t xyz[3], const sensor_real_t timeDelta) {}, //dummy
				[&sumXYZ,
				 &sampleCount](const int16_t xyz[3], const sensor_real_t timeDelta) {
					sumXYZ[0] += xyz[0];
					sumXYZ[1] += xyz[1];
					sumXYZ[2] += xyz[2];
					++sampleCount;
				},
				[](const int16_t xyz[3], const sensor_real_t timeDelta) {}
			);
		}

		ledManager.off();
		m_calibration.G_off[0] = ((double)sumXYZ[0]) / sampleCount;
		m_calibration.G_off[1] = ((double)sumXYZ[1]) / sampleCount;
		m_calibration.G_off[2] = ((double)sumXYZ[2]) / sampleCount;

		m_Logger.info(
			"Gyro offset after %d samples: %f %f %f",
			sampleCount,
			UNPACK_VECTOR_ARRAY(m_calibration.G_off)
		);
	}

	//calibrate accelerometer
	void calibrateAccel() {
		//use the magnetometer ironing algorithm to calibrate the accelerometer, just like the BMI160 does
		auto magneto = std::make_unique<MagnetoCalibration>();
		m_Logger.info(
			"Put the device into 6 unique orientations (all sides), leave it still and "
			"do not hold/touch for %d seconds each",
			AccelCalibRestSeconds
		);
		//wait for 3 seconds while the user positions it properly (note: we don't have to do 6-sided calibration if we're using the mag algorithm)
		ledManager.on();
		eatSamplesForSeconds(AccelCalibDelaySeconds);
		ledManager.off();

		RestDetectionParams calibrationRestDetectionParams;
		calibrationRestDetectionParams.restMinTime = AccelCalibRestSeconds;
		calibrationRestDetectionParams.restThAcc = 0.25f;

		RestDetection calibrationRestDetection(
			calibrationRestDetectionParams,
			imu::GyrTs,
			imu::AccTs
		);

		constexpr uint16_t expectedPositions = 6;
		constexpr uint16_t numSamplesPerPosition = 96;

		uint16_t numPositionsRecorded = 0;
		uint16_t numCurrentPositionSamples = 0;
		bool waitForMotion = true;

		auto accelCalibrationChunk
			= std::make_unique<float[]>(numSamplesPerPosition * 3);
		ledManager.pattern(100, 100, 6);
		ledManager.on();
		m_Logger.info("Gathering accelerometer data...");
		m_Logger.info(
			"Waiting for position %i, you can leave the device as is...",
			numPositionsRecorded + 1
		);
		bool samplesGathered = false;
		while (!samplesGathered) {
#ifdef ESP8266
			ESP.wdtFeed();
#endif
			m_sensor.bulkRead(
				[&](const int16_t xyz[3], const sensor_real_t timeDelta) {
					const sensor_real_t scaledData[]
						= {static_cast<sensor_real_t>(
							   AScale * static_cast<sensor_real_t>(xyz[0])
						   ),
						   static_cast<sensor_real_t>(
							   AScale * static_cast<sensor_real_t>(xyz[1])
						   ),
						   static_cast<sensor_real_t>(
							   AScale * static_cast<sensor_real_t>(xyz[2])
						   )};

					calibrationRestDetection.updateAcc(imu::AccTs, scaledData);

					//makes sure you're not jiggling the IMU before it starts collecting more data
					if (waitForMotion) {
						if (!calibrationRestDetection.getRestDetected()) {
							waitForMotion = false;
						}
						return;
					}

					if (calibrationRestDetection.getRestDetected()) {
						const uint16_t i = numCurrentPositionSamples * 3;
						accelCalibrationChunk[i + 0] = xyz[0];
						accelCalibrationChunk[i + 1] = xyz[1];
						accelCalibrationChunk[i + 2] = xyz[2];
						numCurrentPositionSamples++;

						if (numCurrentPositionSamples >= numSamplesPerPosition) {
							for (int i = 0; i < numSamplesPerPosition; i++) {
								magneto->sample(
									accelCalibrationChunk[i * 3 + 0],
									accelCalibrationChunk[i * 3 + 1],
									accelCalibrationChunk[i * 3 + 2]
								);
							}
							numPositionsRecorded++;
							numCurrentPositionSamples = 0;
							if (numPositionsRecorded < expectedPositions) {
								ledManager.pattern(50, 50, 2);
								ledManager.on();
								m_Logger.info(
									"Recorded, waiting for position %i...",
									numPositionsRecorded + 1
								);
								waitForMotion = true;
							}
						}
					} else {
						numCurrentPositionSamples = 0;
					}

					if (numPositionsRecorded >= expectedPositions) {
						samplesGathered = true;
					}
				},
				[](const int16_t xyz[3], const sensor_real_t timeDelta) {},
				[](const int16_t xyz[3], const sensor_real_t timeDelta) {}
			);
		}
		ledManager.off();
		m_Logger.debug("Calculating accelerometer calibration data...");
		accelCalibrationChunk.reset();

		float A_BAinv[4][3];
		magneto->current_calibration(A_BAinv);

		m_Logger.debug("Finished calculating accelerometer calibration");
		m_Logger.debug("Accelerometer calibration matrix:");
		m_Logger.debug("{");
		for (int i = 0; i < 3; i++) {
			m_calibration.A_B[i] = A_BAinv[0][i];
			m_calibration.A_Ainv[0][i] = A_BAinv[1][i];
			m_calibration.A_Ainv[1][i] = A_BAinv[2][i];
			m_calibration.A_Ainv[2][i] = A_BAinv[3][i];
			m_Logger.debug(
				"  %f, %f, %f, %f",
				A_BAinv[0][i],
				A_BAinv[1][i],
				A_BAinv[2][i],
				A_BAinv[3][i]
			);
		}
		m_Logger.debug("}");
	}

	//calibrate magnetometer (ripped from BMI160)
	void calibrateMag() {
		if(magStatus == MagnetometerStatus::MAG_ENABLED) {

#ifndef BMI160_CALIBRATION_MAG_SECONDS
			static_assert(false, "BMI160_CALIBRATION_MAG_SECONDS not set in defines");
#endif

#if BMI160_CALIBRATION_MAG_SECONDS == 0
			m_Logger.debug("Skipping magnetometer calibration");
			return;
#endif

			MagnetoCalibration* magneto = new MagnetoCalibration();

			constexpr uint8_t MAG_CALIBRATION_DELAY_SEC = 3;
			constexpr float MAG_CALIBRATION_DURATION_SEC = BMI160_CALIBRATION_MAG_SECONDS;
			m_Logger.info(
				"After 3 seconds, rotate the device in figure 8 pattern while it's gathering "
				"data (%.1f seconds)",
				MAG_CALIBRATION_DURATION_SEC
			);
			eatSamplesForSeconds(MAG_CALIBRATION_DELAY_SEC);
			
			//ledManager.pattern(100, 100, 9);
			//delay(100);
			ledManager.on();
			m_Logger.debug("Gathering magnetometer data...");

			constexpr float SAMPLE_DELAY_MS = 100.0f;
			constexpr uint16_t magCalibrationSamples
				= MAG_CALIBRATION_DURATION_SEC / (SAMPLE_DELAY_MS / 1e3f);
			uint32_t last_time = millis();

			uint8_t magdata[6];
			for (int i = 0; i < magCalibrationSamples;) {
				ledManager.on();

				//int16_t mx, my, mz;
				
				//todo: read in fresh data
				//imu.getMagnetometerXYZBuffer(magdata);
				//getMagnetometerXYZFromBuffer(magdata, &mx, &my, &mz);

				m_sensor.bulkRead(
					[](const int16_t xyz[3], const sensor_real_t timeDelta) {},
					[](const int16_t xyz[3], const sensor_real_t timeDelta) {},
					[&](const int16_t xyz[3], const sensor_real_t timeDelta) {

						//wait for the delay, then grab a new sample
						if(last_time + (uint32_t)SAMPLE_DELAY_MS < millis()) {

							ledManager.on();
							last_time = millis();							
							magneto->sample(xyz[0], xyz[1], xyz[2]);
							i += 1;
							ledManager.off();

						}
					}
				);
				
				

				//ledManager.off();
				//delay(SAMPLE_DELAY_MS);
			}
			ledManager.off();
			m_Logger.debug("Calculating magnetometer calibration data...");

			float M_BAinv[4][3];
			magneto->current_calibration(M_BAinv);
			delete magneto;

			m_Logger.debug("[INFO] Magnetometer calibration matrix:");
			m_Logger.debug("{");
			for (int i = 0; i < 3; i++) {
				m_calibration.M_B[i] = M_BAinv[0][i];
				m_calibration.M_Ainv[0][i] = M_BAinv[1][i];
				m_calibration.M_Ainv[1][i] = M_BAinv[2][i];
				m_calibration.M_Ainv[2][i] = M_BAinv[3][i];
				m_Logger.debug(
					"  %f, %f, %f, %f",
					M_BAinv[0][i],
					M_BAinv[1][i],
					M_BAinv[2][i],
					M_BAinv[3][i]
				);
			}
			m_Logger.debug("}");

		} else {
			m_Logger.debug("[INFO] Magnetometer disabled. Skipping calibration.");
		}

	}

	//figure out how fast our IMUs can go.
	void calibrateSampleRate() {
		m_Logger.debug(
			"Calibrating IMU sample rate in %d second(s)...",
			SampleRateCalibDelaySeconds
		);
		ledManager.on();
		//clear queue for the initial second delay
		eatSamplesForSeconds(SampleRateCalibDelaySeconds);

		uint32_t accelSamples = 0;
		uint32_t gyroSamples = 0;
		uint32_t magSamples = 0;

		const auto calibTarget = millis() + 1000 * SampleRateCalibSeconds;
		m_Logger.debug("Counting samples now...");
		uint32_t currentTime;
		while ((currentTime = millis()) < calibTarget) {
			m_sensor.bulkRead(
				[&accelSamples](const int16_t xyz[3], const sensor_real_t timeDelta) {
					accelSamples++;
				},
				[&gyroSamples](const int16_t xyz[3], const sensor_real_t timeDelta) {
					gyroSamples++;
				},
				[&magSamples](const int16_t xyz[3], const sensor_real_t timeDelta) {
					magSamples++;
				}
			);
			yield();
		}

		const auto millisFromStart
			= currentTime - (calibTarget - 1000 * SampleRateCalibSeconds);
		m_Logger.debug(
			"Collected %d gyro, %d acc, $d mag samples during %d ms",
			gyroSamples,
			accelSamples,
			magSamples,
			millisFromStart
		);
		m_calibration.A_Ts = millisFromStart / (accelSamples * 1000.0);
		m_calibration.G_Ts = millisFromStart / (gyroSamples * 1000.0);
		m_calibration.M_Ts = millisFromStart / (magSamples * 1000.0);

		m_Logger.debug(
			"Gyro frequency %fHz, accel frequency: %fHz, mag frequency: %fHz",
			1.0 / m_calibration.G_Ts,
			1.0 / m_calibration.A_Ts,
			1.0 / m_calibration.M_Ts,
		);
		ledManager.off();

		// fusion needs to be recalculated
		recalcFusion();
	}

	SensorStatus getSensorState() override final { return m_status; }

	SensorFusionRestDetect m_fusion;
	T<I2CImpl> m_sensor;

	// dummy initial calibration. Doesn't affect input data in this state
	SlimeVR::Configuration::SoftFusionSensorConfig m_calibration
		= {
		   .ImuType = {imu::Type},
		   .MotionlessDataLen = {MotionlessCalibDataSize()},
		   .A_B = {0.0, 0.0, 0.0},
		   .A_Ainv = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}},
		   .M_B = {0.0, 0.0, 0.0},
		   .M_Ainv = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}},
		   .G_off = {0.0, 0.0, 0.0},
		   .temperature = 0.0,
		   .A_Ts = imu::AccTs,
		   .G_Ts = imu::GyrTs,
		   .M_Ts = imu::MagTs,
		   .G_Sens = {1.0, 1.0, 1.0},
		   .MotionlessData = {}
		};

	SensorStatus m_status = SensorStatus::SENSOR_OFFLINE;
	uint32_t m_lastPollTime = micros();
	uint32_t m_lastRotationPacketSent = 0;
	uint32_t m_lastTemperaturePacketSent = 0;
};

}  // namespace SlimeVR::Sensors
