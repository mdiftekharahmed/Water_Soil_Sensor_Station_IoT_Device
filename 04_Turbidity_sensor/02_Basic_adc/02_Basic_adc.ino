

// Turbidity
#define OTHER_RELAY_PIN 25
    #define TRB_PIN 34
    #define num_of_read 10
  
  float getTurbidity(){
    float arr[num_of_read];
    for(int  i=0;i<num_of_read;i++){
      int raw = analogRead(TRB_PIN);
      uint32_t pinMilliVolts = analogReadMilliVolts(TRB_PIN);

      // Divider: 10 kΩ from sensor AO to GPIO, 20 kΩ from GPIO to GND.
      // GPIO voltage = AO × 20 / (10 + 20), so AO = GPIO × 1.5.
      float pinVoltage = pinMilliVolts / 1000.0f;
      float sensorVoltage = pinVoltage * 1.3413f;

      Serial.print("ADC raw: ");
      Serial.print(raw);
      Serial.print(" | ADC pin: ");
      Serial.print(pinVoltage, 3);
      Serial.print(" V | Sensor AO estimate: ");
      Serial.print(sensorVoltage, 3);
      Serial.println(" V");

      arr[i] = sensorVoltage;
      delay(500);
    }
    for (int i = 0; i < num_of_read - 1; i++) {
      bool swapped = false;
      
      for (int j = 0; j < num_of_read - i - 1; j++) {
        if (arr[j] > arr[j + 1]) {
          // Swap the elements
          float temp = arr[j];
          arr[j] = arr[j + 1];
          arr[j + 1] = temp;
          swapped = true;
        }
      }
      if (!swapped) {
        break;
      }
    }
    float val = 0;
    for(int i=3;i<7;i++){
      val+= arr[i];
    }
    return val/4;
  }


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
 pinMode(OTHER_RELAY_PIN, OUTPUT);
digitalWrite(OTHER_RELAY_PIN, LOW);

  // Turbidity
    analogReadResolution(12);  // ESP32 raw range: 0–4095
    analogSetPinAttenuation(TRB_PIN, ADC_11db);
  
  Serial.println("Setup complete");
}

void loop() {
  // put your main code here, to run repeatedly:
  getTurbidity();
  delay(2000);
}


