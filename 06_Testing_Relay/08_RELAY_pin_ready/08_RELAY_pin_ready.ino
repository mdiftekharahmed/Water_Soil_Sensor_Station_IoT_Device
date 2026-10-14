#include <Arduino.h>

#define GSM_RELAY_PIN 18
#define MODBUS_RELAY_PIN 13
#define SOIL_RELAY_PIN 14
#define OTHER_RELAY_PIN 25

void setup() {
  Serial.begin(115200);

  pinMode(GSM_RELAY_PIN, OUTPUT);
  pinMode(MODBUS_RELAY_PIN, OUTPUT);
  pinMode(SOIL_RELAY_PIN, OUTPUT);
  pinMode(OTHER_RELAY_PIN, OUTPUT);

  // active-LOW relay board
  digitalWrite(GSM_RELAY_PIN, HIGH);
  digitalWrite(MODBUS_RELAY_PIN, HIGH);
  digitalWrite(SOIL_RELAY_PIN, HIGH);
  digitalWrite(OTHER_RELAY_PIN, HIGH);

  Serial.println("Starting 4-channel relay test...");
}

void loop() {
  Serial.println("Relay 1 ON");
  digitalWrite(GSM_RELAY_PIN, LOW);
  delay(3000);

  Serial.println("Relay 1 OFF");
  digitalWrite(GSM_RELAY_PIN, HIGH);
  delay(1000);

  Serial.println("Relay 2 ON");
  digitalWrite(MODBUS_RELAY_PIN, LOW);
  delay(3000);

  Serial.println("Relay 2 OFF");
  digitalWrite(MODBUS_RELAY_PIN, HIGH);
  delay(1000);

  Serial.println("Relay 3 ON");
  digitalWrite(SOIL_RELAY_PIN, LOW);
  delay(3000);

  Serial.println("Relay 3 OFF");
  digitalWrite(SOIL_RELAY_PIN, HIGH);
  delay(1000);

  Serial.println("Relay 4 ON");
  digitalWrite(OTHER_RELAY_PIN, LOW);
  delay(3000);

  Serial.println("Relay 4 OFF");
  digitalWrite(OTHER_RELAY_PIN, HIGH);
  delay(3000);
}