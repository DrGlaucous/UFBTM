#include <Wire.h>
#include <Arduino.h>
#include "QMC5883P.h"
#include "LSM6DS3.h"

#define PIN_IMU_SDA 2
#define PIN_IMU_SCL 3
// Wire.begin(static_cast<int>(PIN_IMU_SDA), static_cast<int>(PIN_IMU_SCL));
// address: 0x2C

#define SERL Serial1

void setup() {
	Wire.begin(PIN_IMU_SDA, PIN_IMU_SCL);
	//Wire.begin();

	delay(1000);

	SERL.begin(115200, 134217756UL, 20, 21);
	SERL.print("Magnetometer\n");
}

void loop() {


	QMC5883P magneto = QMC5883P();
	LSM6DS3 gyro = LSM6DS3(LSM6DS3_ADDRESS_LOW);
	magneto.set_range(CONFIG_8GAUSS);
	
	SERL.print("Initialized\n");

	while(1) {
		if(magneto.data_ready() == 0) {
			const uint16_t* data = magneto.read_raw();

			SERL.print("");
			SERL.print((int16_t)data[0]);
			SERL.print("\t");
			SERL.print((int16_t)data[1]);
			SERL.print("\t");
			SERL.print((int16_t)data[2]);
			SERL.print("\n");

			//SERL.printf("X:%d Y:%d Z:%d\n", data[0], data[1], data[2]);
			delay(50);
		}
	}




}