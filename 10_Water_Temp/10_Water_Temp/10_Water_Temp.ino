#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define DS18B20_PIN 27  

#define OTHER_RELAY_PIN 25

OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  delay(1000);

    pinMode(OTHER_RELAY_PIN, OUTPUT);
    digitalWrite(OTHER_RELAY_PIN, LOW);
  delay(1000);
  Serial.println();
  Serial.println("==============================");
  Serial.println("DS18B20 TEMPERATURE TEST");
  Serial.println("==============================");

  sensors.begin();

  Serial.print("Sensors detected: ");
  Serial.println(sensors.getDeviceCount());

  if (sensors.getDeviceCount() == 0) {
    Serial.println("ERROR: No DS18B20 detected!");
  } else {
    Serial.println("DS18B20 detected successfully.");
  }

  digitalWrite(OTHER_RELAY_PIN, HIGH);
  delay(1000);
}

void loop() {

  digitalWrite(OTHER_RELAY_PIN, LOW);
  delay(10000);

  Serial.println();
  Serial.println("Requesting temperature...");

  sensors.requestTemperatures();

  float tempC = sensors.getTempCByIndex(0);

  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("ERROR: Sensor disconnected!");
  } else {
    Serial.print("Temperature: ");
    Serial.print(tempC, 2);
    Serial.println(" °C");

    Serial.print("Temperature: ");
    Serial.print(DallasTemperature::toFahrenheit(tempC), 2);
    Serial.println(" °F");
  }

  delay(1000);
  digitalWrite(OTHER_RELAY_PIN, HIGH);
  delay(1000);
}