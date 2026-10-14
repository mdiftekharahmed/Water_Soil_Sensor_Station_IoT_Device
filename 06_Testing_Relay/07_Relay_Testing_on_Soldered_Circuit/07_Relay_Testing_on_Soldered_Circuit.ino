#include <Arduino.h>

const uint8_t relayPins[] = {13, 14, 25};
const uint8_t relayCount = 3;

// Change these if your relay board is active-HIGH instead
const uint8_t RELAY_ON  = LOW;
const uint8_t RELAY_OFF = HIGH;

void setAllRelays(uint8_t state) {
  Serial.println();

  if (state == RELAY_ON) {
    Serial.println("Turning ALL relays ON...");
  } else {
    Serial.println("Turning ALL relays OFF...");
  }

  for (uint8_t i = 0; i < relayCount; i++) {
    digitalWrite(relayPins[i], state);

    Serial.print("Relay control ");
    Serial.print(i + 1);
    Serial.print(" on GPIO ");
    Serial.print(relayPins[i]);

    if (state == RELAY_ON) {
      Serial.println(" -> ON");
    } else {
      Serial.println(" -> OFF");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("=================================");
  Serial.println("ESP32 RELAY TEST");
  Serial.println("=================================");

  Serial.print("Relay control pins: ");

  for (uint8_t i = 0; i < relayCount; i++) {
    Serial.print(relayPins[i]);

    if (i < relayCount - 1) {
      Serial.print(", ");
    }
  }

  Serial.println();
  Serial.println();

  Serial.println("Configuring relay pins...");

  for (uint8_t i = 0; i < relayCount; i++) {
    pinMode(relayPins[i], OUTPUT);

    // Start safely in OFF state
    digitalWrite(relayPins[i], RELAY_OFF);

    Serial.print("GPIO ");
    Serial.print(relayPins[i]);
    Serial.println(" configured as OUTPUT -> OFF");
  }

  Serial.println();
  Serial.println("Relay setup complete.");
  Serial.println("Starting 3-second ON/OFF loop...");
  Serial.println("=================================");
}

void loop() {
  setAllRelays(RELAY_ON);

  Serial.println("Waiting 3 seconds...");
  delay(10000);

  setAllRelays(RELAY_OFF);

  Serial.println("Waiting 3 seconds...");
  delay(10000);
}