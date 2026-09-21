#include <Arduino.h>
#include <Wire.h>

#define I2C_SDA 15
#define I2C_SCL 4

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Initialize I2C with SDA on GPIO 15 and SCL on GPIO 4
  Wire.begin(I2C_SDA, I2C_SCL);

  Serial.println("\n--- ESP32 I2C Scanner ---");
  Serial.printf("Bus configured: SDA = GPIO %d, SCL = GPIO %d\n", I2C_SDA, I2C_SCL);
}

void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println("\nScanning I2C bus...");

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("  [OK] Device found at address 0x%02X", address);

      // Identify common module addresses
      if (address == 0x68) {
        Serial.print(" (DS3231 RTC)");
      } else if (address >= 0x50 && address <= 0x57) {
        Serial.print(" (AT24C32 EEPROM)");
      }
      Serial.println();
      nDevices++;
    } else if (error == 4) {
      Serial.printf("  [ERR] Unknown error at address 0x%02X\n", address);
    }
  }

  if (nDevices == 0) {
    Serial.println("No I2C devices found. Check wiring, pull-ups, and power.");
  } else {
    Serial.printf("Scan complete. Found %d device(s).\n", nDevices);
  }

  delay(5000); // Repeat scan every 5 seconds
}