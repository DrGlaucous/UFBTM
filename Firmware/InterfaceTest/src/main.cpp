#include <Wire.h>
#include <Arduino.h>
#include "QMC5883P.h"

#define PIN_IMU_SDA 3
#define PIN_IMU_SCL 2
// Wire.begin(static_cast<int>(PIN_IMU_SDA), static_cast<int>(PIN_IMU_SCL));
// address: 0x2C

void setup() {
	//Wire.begin(static_cast<int>(PIN_IMU_SDA), static_cast<int>(PIN_IMU_SCL));
	Wire.begin();

	delay(1000);

	Serial.begin(115200);
	Serial.print("Magnetometer\n");
}

void loop() {


	QMC5883P magneto = QMC5883P();
	
	Serial.print("Initialized\n");

	while(1) {
		if(magneto.data_ready() == 0) {
			const uint16_t* data = magneto.read_raw();

			Serial.print("");
			Serial.print(data[0]);
			Serial.print("\t");
			Serial.print(data[1]);
			Serial.print("\t");
			Serial.print(data[2]);
			Serial.print("\n");

			//Serial.printf("X:%d Y:%d Z:%d\n", data[0], data[1], data[2]);
			delay(50);
		}
	}




}