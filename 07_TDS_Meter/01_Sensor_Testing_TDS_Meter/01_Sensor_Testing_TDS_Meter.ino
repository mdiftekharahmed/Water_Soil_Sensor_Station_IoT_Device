#include <Arduino.h>

#define TDS_PIN          36     // Analog pin connected to TDS module signal pin
#define VREF             3.3    // ESP32 ADC reference voltage (3.3V)
#define ADC_RESOLUTION   4095.0 // 12-bit ADC resolution
#define SAMPLES          30     // Number of samples for smoothing filter

// Default water temperature in Celsius (adjust or read from a DS18B20 sensor)
float temperature = 25.0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(TDS_PIN, INPUT);
  analogReadResolution(12); // Ensure 12-bit ADC resolution
  
  Serial.println("==================================");
  Serial.println("   Gravity TDS Meter V1.0 Test   ");
  Serial.println("==================================");
}

void loop() {
  // 1. Take multi-sampled analog readings to suppress electrical noise
  long rawSum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    rawSum += analogRead(TDS_PIN);
    delay(4);
  }
  float averageRaw = (float)rawSum / SAMPLES;

  // 2. Convert raw ADC reading to voltage (0.0V - 2.3V range)
  float voltage = (averageRaw / ADC_RESOLUTION) * VREF;

  // 3. Apply temperature compensation (reference is 25.0 °C)
  float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
  float compensationVoltage = voltage / compensationCoefficient;

  // 4. Convert voltage to TDS value (in ppm) using the standard polynomial curve
  float tdsValue = (133.42 * pow(compensationVoltage, 3) 
                  - 255.86 * pow(compensationVoltage, 2) 
                  + 857.39 * compensationVoltage) * 0.5;

  if (tdsValue < 0) tdsValue = 0; // Prevent negative readings in air

  // 5. Print results to Serial Monitor
  Serial.print("Raw ADC: ");
  Serial.print((int)averageRaw);
  Serial.print(" | Voltage: ");
  Serial.print(voltage, 2);
  Serial.print(" V | TDS: ");
  Serial.print(tdsValue, 0);
  Serial.println(" ppm");

  delay(1500);
}