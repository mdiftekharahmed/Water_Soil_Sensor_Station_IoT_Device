#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"
#include <esp_sleep.h>

// Set to true for every 30 mins (:00, :30), or false for every 60 mins (:00)
#define INTERVAL_30_MIN true

// --- Pin Definitions ---
#define LED_PIN        2            // Onboard LED
#define I2C_SDA_PIN    21           // DS3231 SDA
#define I2C_SCL_PIN    4            // DS3231 SCL
#define RTC_WAKE_PIN   GPIO_NUM_39  // DS3231 SQW interrupt pin

RTC_DS3231 rtc;

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(RTC_WAKE_PIN, INPUT_PULLUP);

  Serial.println("\n==========================================");
  
  // 1. Identify Wake Reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("[STATUS] Woken up by DS3231 Hardware Clock Alarm!");
  } else {
    Serial.println("[STATUS] Boot Cause: Power-on or Manual Reset");
  }

  // 2. Initialize I2C Bus
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (!rtc.begin()) {
    Serial.println("[ERROR] DS3231 not found! Halting.");
    while (1);
  }

  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  DateTime now = rtc.now();
  Serial.printf("[TIME] RTC Clock: %02d:%02d:%02d\n",
                now.hour(), now.minute(), now.second());

  // 3. Execute Job (10 Blinks)
  Serial.println("\n[JOB] Starting 10-Blink Sequence...");
  for (int i = 1; i <= 10; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
  Serial.println("[JOB] Completed.");

  // 4. Configure DS3231 SQW Pin for Interrupt Mode
  rtc.disable32K();
  rtc.writeSqwPinMode(DS3231_OFF); // SQW pin driven by internal hardware alarms

  // Clear existing alarm flags so SQW line releases HIGH (3.3V)
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);

  // 5. Program Hardware Wall-Clock Alarms (No math/counting needed)
  // Alarm 1 matches when Minute == 00 and Second == 00
  DateTime match00 = DateTime(2026, 1, 1, 0, 0, 0);
  rtc.setAlarm1(match00, DS3231_A1_Minute);

  if (INTERVAL_30_MIN) {
    // Alarm 2 matches when Minute == 30 (Alarm 2 fires at 00 seconds automatically)
    DateTime match30 = DateTime(2026, 1, 1, 0, 30, 0);
    rtc.setAlarm2(match30, DS3231_A2_Minute);
    Serial.println("[ALARM] Armed DS3231 Hardware for :00 and :30 marks.");
  } else {
    rtc.disableAlarm(2);
    Serial.println("[ALARM] Armed DS3231 Hardware for :00 top-of-hour only.");
  }

  // Ensure flags are cleared right before entering sleep
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);

  // 6. Arm EXT0 Wakeup on GPIO 27 (Triggers when SQW pulls to 0V / LOW)
  esp_sleep_enable_ext0_wakeup(RTC_WAKE_PIN, 0);

  Serial.println("[SLEEP] Entering Deep Sleep. Waiting for DS3231 clock signal...");
  Serial.println("==========================================\n");
  Serial.flush();

  esp_deep_sleep_start();
}

void loop() {
  // Never reached in deep sleep
}