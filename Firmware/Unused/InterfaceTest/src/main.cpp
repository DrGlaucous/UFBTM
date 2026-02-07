#include <Wire.h>
#include <Arduino.h>
#include "QMC5883P.h"
#include "LSM6DS3.h"

#define PIN_IMU_SDA 2
#define PIN_IMU_SCL 3
// Wire.begin(static_cast<int>(PIN_IMU_SDA), static_cast<int>(PIN_IMU_SCL));
// address: 0x2C

//#define SERL Serial1

void setup() {
	Wire.begin(PIN_IMU_SDA, PIN_IMU_SCL);
	//Wire.begin();

	Serial.begin(115200);
	Serial.print("Magnetometer\n");

	//simple i2c scanner. We don't need that right now.
	/*
	delay(1000);
	while(1) {
		for(int address = 1; address < 127; address++ )
		{
			// The i2c_scanner uses the return value of
			// the Write.endTransmisstion to see if
			// a device did acknowledge to the address.
			Wire.beginTransmission(address);
			int error = Wire.endTransmission();
		
			if (error == 0)
			{
				Serial.print("I2C device found at address 0x");
				if (address < 16)
					Serial.print("0");
				Serial.print(address,HEX);
				Serial.println("  !");
			}
			else if (error==4)
			{
			Serial.print("Unknown error at address 0x");
			if (address<16)
				Serial.print("0");
			Serial.println(address,HEX);
			}
		}
		Serial.print("Scan complete.");
		delay(1000);

	}
	*/


}

void loop() {

	LSM6DS3 gyro = LSM6DS3(LSM6DS3_ADDRESS_LOW);
	gyro.begin();

	// while(1) {
	// 	Serial.printf("done");
	// 	delay(1000);
	// }

	while(1) {
		float x, y, z = 0;

		if (gyro.accelerationAvailable()) {
			//gyro.readAcceleration(x, y, z);
			gyro.readMagneto(x, y, z);
			Serial.printf("Data: %.2f, %.2f, %.2f\n", x, y, z);
		}

		delay(70);
		

	}




	QMC5883P magneto = QMC5883P();
	magneto.set_range(CONFIG_8GAUSS);
	
	Serial.print("Initialized\n");

	while(1) {
		if(magneto.data_ready() == 0) {
			const int16_t* data = magneto.read_mapped();

			//in microteslas
			float mtx = data[0] / ((float)magneto.lsb_per_G[CONFIG_8GAUSS]) * 100.0;
			float mty = data[1] / ((float)magneto.lsb_per_G[CONFIG_8GAUSS]) * 100.0;
			float mtz = data[2] / ((float)magneto.lsb_per_G[CONFIG_8GAUSS]) * 100.0;


			Serial.print("");
			Serial.print(mtx);
			Serial.print("\t");
			Serial.print(mty);
			Serial.print("\t");
			Serial.print(mtz);
			Serial.print("\n");

			//SERL.printf("X:%d Y:%d Z:%d\n", data[0], data[1], data[2]);
			delay(50);
		}
	}




}