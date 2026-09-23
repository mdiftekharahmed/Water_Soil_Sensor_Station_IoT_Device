#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"
#include <esp_sleep.h>

// --- Pin Definitions ---
#define LED_PIN        2            // Onboard LED (GPIO 2 on most ESP32 DevKit boards)
#define I2C_SDA_PIN    21          // DS3231 SDA
#define I2C_SCL_PIN    4            // DS3231 SCL
#define RTC_WAKE_PIN   GPIO_NUM_39  // DS3231 SQW pin connected to GPIO 27 (RTC_GPIO 17)

#define SLEEP_SECONDS  20           // Test interval

RTC_DS3231 rtc;

void setup() {
  Serial.begin(115200);
  delay(1000); // Allow Serial Monitor to attach

  // 1. Initialize LED Pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("\n==========================================");
  Serial.println("[STEP 1] ESP32 Woke Up / Booted");

  // Check how the ESP32 was triggered
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("[STATUS] Wakeup Cause: DS3231 Alarm Interrupt (GPIO 27 / EXT0)");
  } else {
    Serial.println("[STATUS] Wakeup Cause: Power-on or Manual Reset");
  }

  // 2. Initialize I2C and DS3231
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (!rtc.begin()) {
    Serial.println("[ERROR] DS3231 not found on GPIO 15/4! Halting.");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("[WARN] RTC lost power, syncing to compile time...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  DateTime now = rtc.now();
  Serial.printf("[TIME] Current RTC Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                now.year(), now.month(), now.day(),
                now.hour(), now.minute(), now.second());

  // 3. Blink LED 10 Times
  Serial.println("\n[STEP 2] Starting 10-Blink Sequence...");
  for (int i = 1; i <= 10; i++) {
    digitalWrite(LED_PIN, HIGH);
    Serial.printf("  -> Blink %02d/10: ON\n", i);
    delay(250);

    digitalWrite(LED_PIN, LOW);
    Serial.printf("  -> Blink %02d/10: OFF\n", i);
    delay(250);
  }
  Serial.println("[STATUS] 10 Blinks completed.");

  // 4. Configure DS3231 SQW for Alarm Interrupt Mode
  Serial.println("\n[STEP 3] Arming DS3231 Alarm 1 for 20 Seconds Ahead...");
  rtc.disable32K();
  rtc.writeSqwPinMode(DS3231_OFF); // Puts SQW line in alarm interrupt mode

  // Clear existing alarm flags to ensure SQW line is released HIGH (3.3V)
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  rtc.disableAlarm(2);

  // Set Alarm 1 for 20 seconds into the future
  now = rtc.now();
  DateTime alarmTime = now + TimeSpan(SLEEP_SECONDS);
  rtc.setAlarm1(alarmTime, DS3231_A1_Date); // Exact match: date, hour, minute, second

  Serial.printf("[ALARM] Armed for %02d:%02d:%02d (Wait duration: %d seconds)\n",
                alarmTime.hour(), alarmTime.minute(), alarmTime.second(), SLEEP_SECONDS);

  // Clear alarm flag once more so the pin remains HIGH before entering sleep
  rtc.clearAlarm(1);

  // 5. Configure ESP32 EXT0 Wakeup
  Serial.println("\n[STEP 4] Arming ESP32 EXT0 Wakeup on GPIO 27 (Trigger: LOW)...");
  esp_sleep_enable_ext0_wakeup(RTC_WAKE_PIN, 0); // 0 = wake when SQW pulls pin to 0V

  // 6. Enter Deep Sleep
  Serial.println("[STEP 5] Entering Deep Sleep. Waiting for DS3231 alarm...");
  Serial.println("==========================================\n");
  Serial.flush();

  esp_deep_sleep_start();
}

void loop() {
  // Never reached: Deep sleep resets the core and re-executes setup() on every wake cycle
}