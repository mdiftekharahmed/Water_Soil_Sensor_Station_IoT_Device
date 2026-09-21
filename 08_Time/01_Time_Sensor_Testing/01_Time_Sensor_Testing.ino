#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"

// I2C Pin Configuration
#define I2C_SDA 15
#define I2C_SCL 4

RTC_DS3231 rtc;

// Day of week strings
const char daysOfTheWeek[7][12] = {
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Initialize I2C bus on GPIO 15 (SDA) and GPIO 4 (SCL)
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!rtc.begin()) {
    Serial.println("[ERROR] Couldn't find DS3231 RTC on GPIO 15 / 4!");
    while (1);
  }

  // Adjust time if battery was removed or clock lost power
  if (rtc.lostPower()) {
    Serial.println("[WARN] RTC lost power! Synchronizing to sketch compilation time...");
    // Sets RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Uncomment the line below ONCE to manually set a specific timestamp:
  // rtc.adjust(DateTime(2026, 9, 14, 15, 30, 0)); // (YYYY, MM, DD, HH, MM, SS)

  Serial.println("[OK] DS3231 RTC initialized successfully.\n");
}

void loop() {
  DateTime now = rtc.now();

  // Format Date and Time
  char dateBuffer[16];
  char timeBuffer[16];
  snprintf(dateBuffer, sizeof(dateBuffer), "%04d-%02d-%02d", now.year(), now.month(), now.day());
  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());

  // Read internal temperature sensor
  float tempC = rtc.getTemperature();

  // Print output
  Serial.print("[RTC] ");
  Serial.print(dateBuffer);
  Serial.print(" (");
  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  Serial.print(") ");
  Serial.print(timeBuffer);
  Serial.print(" | Die Temp: ");
  Serial.print(tempC, 2);
  Serial.println(" °C");

  delay(2000);
}