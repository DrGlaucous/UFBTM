
#include <Arduino.h>

#include <esp_now.h>
#include <esp_wifi.h>
#include <Wifi.h>

const char payload[] = "Payload\0";


void setup()
{
    Serial.begin(115200);
    Serial.printf("Start");

    pinMode(8, OUTPUT);
}

uint32_t last_millis = 0;

void loop()
{

    if(millis() - last_millis > 2000) {
        Serial.printf("Tick\n");
        last_millis = millis();

        digitalWrite(8, !digitalRead(8));
    }


}





