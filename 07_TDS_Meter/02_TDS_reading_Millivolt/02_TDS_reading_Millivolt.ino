#include <Arduino.h>
#include <math.h>

#define OTHER_RELAY_PIN 25

constexpr int TDS_PIN = 36;
constexpr float WATER_TEMP_C = 25.0f;  // Replace with DS18B20 reading

// ESP32 ADC calibration returns millivolts. GPIO 36 is ADC1 on classic ESP32.
constexpr int SAMPLE_COUNT = 30;

void setup() {
  Serial.begin(115200);

  pinMode(OTHER_RELAY_PIN, OUTPUT);
  digitalWrite(OTHER_RELAY_PIN, LOW);
  
  analogReadResolution(12);
  analogSetPinAttenuation(TDS_PIN, ADC_11db);

  delay(500);
  Serial.println("TDS meter");
}

void loop() {
  uint32_t millivoltSum = 0;

  // Discard one initial reading, then collect calibrated readings.
  analogReadMilliVolts(TDS_PIN);
  for (int i = 0; i < SAMPLE_COUNT; ++i) {
    millivoltSum += analogReadMilliVolts(TDS_PIN);
    delay(10);
  }

  const float voltage = (millivoltSum / float(SAMPLE_COUNT)) / 1000.0f;

  // Temperature compensation referenced to 25 °C.
  const float compensationCoefficient =
      1.0f + 0.02f * (WATER_TEMP_C - 25.0f);
  const float compensatedVoltage = voltage / compensationCoefficient;

  float tdsPpm =
      (133.42f * powf(compensatedVoltage, 3)
       - 255.86f * powf(compensatedVoltage, 2)
       + 857.39f * compensatedVoltage) * 0.5f;

  if (!isfinite(tdsPpm) || tdsPpm < 0.0f) {
    tdsPpm = 0.0f;
  }

  Serial.print("ADC: ");
  Serial.print(analogRead(TDS_PIN));
  Serial.print(" | Voltage: ");
  Serial.print(voltage, 3);
  Serial.print(" V | Temperature: ");
  Serial.print(WATER_TEMP_C, 1);
  Serial.print(" C | TDS: ");
  Serial.print(tdsPpm, 0);
  Serial.println(" ppm");

  delay(1000);
}