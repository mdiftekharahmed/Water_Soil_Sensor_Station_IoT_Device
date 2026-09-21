#include <Arduino.h>

// Relay Control Pins
const uint8_t relayPins[] = {5, 18,19, 21};
const uint8_t numRelays = sizeof(relayPins) / sizeof(relayPins[0]);

// Active-LOW logic definitions
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

void setup() {
  Serial.begin(115200);
  // Initialize each pin as OUTPUT and set initial state to OFF
  for (uint8_t i = 0; i < numRelays; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], RELAY_OFF);
  }

  Serial.println("System ready!");

    for (uint8_t i = 0; i < numRelays; i++) {
      digitalWrite(relayPins[i], RELAY_ON);
    }
    Serial.println("Turned ON");
    delay(3000);
}

void loop() {
  // Serial.println("Turned OFF");
  // // 1. Turn ALL relays OFF for 3 seconds
  // for (uint8_t i = 0; i < numRelays; i++) {
  //   digitalWrite(relayPins[i], RELAY_OFF);
  // }
  // Serial.println("Turned OFF");
  // delay(3000);

  // // 2. Turn ALL relays ON for 5 seconds
  // for (uint8_t i = 0; i < numRelays; i++) {
  //   digitalWrite(relayPins[i], RELAY_ON);
  // }
  // Serial.println("Turned ON");
  // delay(3000);
}