#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Relay Switch
  #define GSM_RELAY_PIN 18
  #define MODBUS_RELAY_PIN 13
  #define SOIL_RELAY_PIN 14
  #define OTHER_RELAY_PIN 25

// Ultrasonic sensor
  const int num_of_read = 10;
  const int trigPin = 26;
  const int echoPin = 35;
  const float speedOfSound = 0.0343;

// Turbidity
  #define TRB_PIN 34
// TDS Meter
  constexpr int TDS_PIN = 36;
  constexpr float WATER_TEMP_C = 25.0f;  // Replace with DS18B20 reading

  // ESP32 ADC calibration returns millivolts. GPIO 36 is ADC1 on classic ESP32.
  constexpr int SAMPLE_COUNT = 30;
// Water Temp
  #define Water_PIN 27
  OneWire oneWire(Water_PIN);
  DallasTemperature sensors(&oneWire);

// GSM

// SD CARD

// Soil Sensor

  // =====================================================
  // ESP32 UART2 + MAX485 (Separate RE and DE Pins)
  // =====================================================

    #define RX2_PIN     16  // RO: goes to RX2 (goes with 10k/20k ohm voltage divider to convert 5v to 3.3v)
    #define TX2_PIN     17  // DI: goes to TX2
    #define RS485_RE    23// RE:  Receiver Enable (Active LOW)
    #define RS485_DE    22 // DE:  Driver Enable   (Active HIGH)




  HardwareSerial RS485Serial(2);

  // =====================================================
  // Modbus Request: Read 7 registers starting at 0x0000
  // =====================================================
  const uint8_t modbusQuery[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x07, 0x04, 0x08};

  // =====================================================
  // Modbus CRC16 Calculation
  // =====================================================
  uint16_t modbusCRC(uint8_t *buffer, uint8_t length) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < length; i++) {
      crc ^= buffer[i];
      for (uint8_t j = 0; j < 8; j++) {
        if (crc & 0x0001) {
          crc >>= 1;
          crc ^= 0xA001;
        } else {
          crc >>= 1;
        }
      }
    }
    return crc;
  }

  // =====================================================
  // Helper: Print Hex Bytes
  // =====================================================
  void printHex(uint8_t *data, int length) {
    for (int i = 0; i < length; i++) {
      if (data[i] < 0x10) Serial.print("0");
      Serial.print(data[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }


// Functions
  void readSoilSensor();  
  void enableSoilSensor();
  float getDistance();
  float getTurbidity();
  float getWaterTemp();
  float getTDS();
  void enableGSMPin();
  void disableGSMPin();

// ======================== Setup =============================
void setup() {
  Serial.begin(115200);
  // Turbidity
    analogReadResolution(12);  // ESP32 raw range: 0–4095
    analogSetPinAttenuation(TRB_PIN, ADC_11db);
  // Relay Pins

    pinMode(GSM_RELAY_PIN, OUTPUT);
    pinMode(MODBUS_RELAY_PIN, OUTPUT);
    pinMode(SOIL_RELAY_PIN, OUTPUT);
    pinMode(OTHER_RELAY_PIN, OUTPUT);

    // active-LOW relay board
    digitalWrite(GSM_RELAY_PIN, HIGH);
    digitalWrite(MODBUS_RELAY_PIN, HIGH);
    digitalWrite(SOIL_RELAY_PIN, HIGH);
    digitalWrite(OTHER_RELAY_PIN, HIGH);

  
  // MAX485 and Soil Sensor

    pinMode(RS485_RE, OUTPUT);
    pinMode(RS485_DE, OUTPUT);

    // Set initial state to RECEIVE mode
    // RE = LOW  (Receiver Enabled)
    // DE = LOW  (Driver Disabled)
    digitalWrite(RS485_RE, LOW);
    digitalWrite(RS485_DE, LOW);

    // HardwareSerial 2 setup
    RS485Serial.begin(4800, SERIAL_8N1, RX2_PIN, TX2_PIN);

    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.println(" ESP32 SOIL SENSOR MODBUS TEST");
    Serial.println("================================");
    Serial.println("RX2      : GPIO16");
    Serial.println("TX2      : GPIO17");
    Serial.println("RE       : GPIO23");
    Serial.println("DE       : GPIO22");
    Serial.println("Baud     : 4800");
    Serial.println("Slave ID : 1");
    Serial.println();


  // Ultrasonic
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    digitalWrite(trigPin, LOW);
    Serial.printf("Ultrasonic Sensor Ready\n");
  
  // Water Temp
    Serial.println("Starting Water temp wire!");
    sensors.begin();
    Serial.print("Sensors detected: ");
    Serial.println(sensors.getDeviceCount());

    if (sensors.getDeviceCount() == 0) {
      Serial.println("ERROR: No DS18B20 detected!");
    } else {
      Serial.println("DS18B20 detected successfully.");
    }

  // TDS METER
    analogReadResolution(12);
    analogSetPinAttenuation(TDS_PIN, ADC_11db);
}


// =====================================================
// Loop
// =====================================================
void loop() {

    // ==========Collect Data==========

      
      enableSoilSensor();
      delay(3000);
      readSoilSensor();
      delay(3000);
      disableSoilSensor();

      // enableOtherPins();
      // float dist = getDistance();
      // Serial.print("distance: ------------> ");
      // Serial.println(dist);

  // float turbidity = getTurbidity();
  // Serial.print("Turbidty: ------------> ");
  // Serial.println(turbidity);

  // float waterTemp =  getWaterTemp();
  // Serial.print("Water Temp: ------------> ");
  // Serial.println(waterTemp);

  // float tds = getTDS();
  // Serial.print("TDS:-------------------> ");
  // Serial.println(tds);
  // disableOtherPins();

  // enableGSMPin();
  
}



// Function Definition

  void readSoilSensor() {
    // Clear lingering serial input
    while (RS485Serial.available()) {RS485Serial.read();
    }

    Serial.println("--------------------------------");
    Serial.println("TX:");
    printHex((uint8_t*)modbusQuery, sizeof(modbusQuery));

    // ---------------------------------------------------
    // TRANSMIT MODE
    // DE = HIGH (Driver Enabled)
    // RE = HIGH (Receiver Disabled - disables hardware loopback)
    // ---------------------------------------------------
    digitalWrite(RS485_RE, HIGH);
    digitalWrite(RS485_DE, HIGH);

    delay(2);

    RS485Serial.write(modbusQuery, sizeof(modbusQuery));
    RS485Serial.flush(); // Wait until transmission completes

    delay(2);

    // ---------------------------------------------------
    // RECEIVE MODE
    // DE = LOW  (Driver Disabled)
    // RE = LOW  (Receiver Enabled)
    // ---------------------------------------------------
    digitalWrite(RS485_DE, LOW);
    digitalWrite(RS485_RE, LOW);

    Serial.println("Listening...");

    uint8_t rxBuffer[128];
    int rxLength = 0;
    unsigned long startTime = millis();

    // Listen for up to 1500 ms
    while (millis() - startTime < 1500) {
      while (RS485Serial.available()) {
        if (rxLength < sizeof(rxBuffer)) {
          rxBuffer[rxLength++] = RS485Serial.read();
        } else {
          RS485Serial.read();
        }
      }
      delay(1);
    }

    // ---------------------------------------------------
    // PROCESS RAW DATA
    // ---------------------------------------------------
    Serial.print("RX: ");

    if (rxLength == 0) {
      Serial.println("NOTHING");
      Serial.println("Received: 0 bytes");
      return;
    }

    printHex(rxBuffer, rxLength);
    Serial.print("Received: ");
    Serial.print(rxLength);
    Serial.println(" bytes");

    // ---------------------------------------------------
    // SEARCH FOR VALID MODBUS FRAME (19 bytes minimum)
    // ---------------------------------------------------
    int frameStart = -1;
    for (int i = 0; i <= rxLength - 19; i++) {
      if (rxBuffer[i] == 0x01 && rxBuffer[i + 1] == 0x03 && rxBuffer[i + 2] == 0x0E) {
        frameStart = i;
        break;
      }
    }

    if (frameStart < 0) {
      Serial.println("NO VALID MODBUS FRAME");
      return;
    }

    // Extract 19-byte frame
    uint8_t frame[19];
    for (int i = 0; i < 19; i++) {
      frame[i] = rxBuffer[frameStart + i];
    }

    Serial.println("\nVALID MODBUS FRAME:");
    printHex(frame, 19);

    // ---------------------------------------------------
    // CRC CHECK
    // ---------------------------------------------------
    uint16_t calculatedCRC = modbusCRC(frame, 17);
    uint16_t receivedCRC   = frame[17] | ((uint16_t)frame[18] << 8);

    Serial.print("Calculated CRC: 0x");
    if (calculatedCRC < 0x1000) Serial.print("0");
    Serial.println(calculatedCRC, HEX);

    Serial.print("Received CRC:   0x");
    if (receivedCRC < 0x1000) Serial.print("0");
    Serial.println(receivedCRC, HEX);

    if (calculatedCRC != receivedCRC) {
      Serial.println("CRC ERROR");
      return;
    }

    Serial.println("CRC OK");

    // ---------------------------------------------------
    // EXTRACT SENSOR REGISTERS
    // ---------------------------------------------------
    uint16_t reg[7];
    for (int i = 0; i < 7; i++) {
      int index = 3 + (i * 2);
      reg[i] = ((uint16_t)frame[index] << 8) | frame[index + 1];
    }

    float moisture   = reg[0] * 0.1;
    float temperature = (reg[1] > 0x7FFF) ? -((0xFFFF - reg[1] + 1) * 0.1) : reg[1] * 0.1;
    uint16_t ec      = reg[2];
    float pH         = reg[3] * 0.1;
    uint16_t nitrogen   = reg[4];
    uint16_t phosphorus = reg[5];
    uint16_t potassium  = reg[6];

    // ---------------------------------------------------
    // DISPLAY RESULTS
    // ---------------------------------------------------
    Serial.println("\n========= SENSOR VALUES =========");
    Serial.print("Moisture    : "); Serial.print(moisture, 1);    Serial.println(" %");
    Serial.print("Temperature : "); Serial.print(temperature, 1); Serial.println(" C");
    Serial.print("EC          : "); Serial.print(ec);             Serial.println(" uS/cm");
    Serial.print("pH          : "); Serial.print(pH, 1);          Serial.println();
    Serial.print("Nitrogen    : "); Serial.print(nitrogen);       Serial.println(" mg/kg");
    Serial.print("Phosphorus  : "); Serial.print(phosphorus);     Serial.println(" mg/kg");
    Serial.print("Potassium   : "); Serial.print(potassium);      Serial.println(" mg/kg");
    Serial.println("================================");
    
  }
  void enableSoilSensor(){
    Serial.println("Relay SOIL ON");
    digitalWrite(MODBUS_RELAY_PIN, LOW);
    Serial.println("Relay 3 ON");
    digitalWrite(SOIL_RELAY_PIN, LOW);
    delay(1000);
  }
  void disableSoilSensor(){
    Serial.println("Relay Soil OFF");
    digitalWrite(MODBUS_RELAY_PIN, HIGH);
    Serial.println("Relay 3 ON");
    digitalWrite(SOIL_RELAY_PIN, HIGH);
    delay(1000);
  }
  void enableOtherPins(){
    Serial.println("Relay Other ON");
    digitalWrite(OTHER_RELAY_PIN, LOW);
    delay(1000);
  }
  void disableOtherPins(){
    Serial.println("Relay Other OFF");
    digitalWrite(OTHER_RELAY_PIN, HIGH);
    delay(1000);
  }

  void enableGSMPin(){
      Serial.println("Relay GSM ON");
      digitalWrite(GSM_RELAY_PIN, LOW);
      delay(3000);
  }
    void disableGSMPin(){
      Serial.println("Relay GSM OFF");
      digitalWrite(GSM_RELAY_PIN, HIGH);
      delay(3000);
  }

  float getDistance(){

    float arr[num_of_read];
    for(int  i=0;i<num_of_read;i++){
      digitalWrite(trigPin, LOW);
      delayMicroseconds(5);

      digitalWrite(trigPin, HIGH);
      delayMicroseconds(20);
      digitalWrite(trigPin, LOW);

      long duration = pulseIn(echoPin, HIGH, 30000);

      if (duration == 0) {
        Serial.printf("Out of range or no echo received\n");
        arr[i]=-1;
      } else {
        float distance = (duration * speedOfSound) / 2.0;

        if (distance < 20.0 || distance > 600.0) {
          Serial.printf("Unreliable reading: %.1f cm (valid range: 20-600 cm)\n", distance);
        } else {
          Serial.printf("Distance: %.1f cm\n", distance);
        }
        arr[i]=distance;
      }
      delay(200);
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

  float getTurbidity(){
    float arr[num_of_read];
    for(int  i=0;i<num_of_read;i++){
      int raw = analogRead(TRB_PIN);
      uint32_t pinMilliVolts = analogReadMilliVolts(TRB_PIN);

      // Divider: 10 kΩ from sensor AO to GPIO, 20 kΩ from GPIO to GND.
      // GPIO voltage = AO × 20 / (10 + 20), so AO = GPIO × 1.5.
      float pinVoltage = pinMilliVolts / 1000.0f;
      float sensorVoltage = pinVoltage * 1.5f;

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

  float getWaterTemp(){
    float arr[num_of_read];
    for(int  i=0;i<num_of_read;i++){
      sensors.requestTemperatures();

      float tempC = sensors.getTempCByIndex(0);

      if (tempC == DEVICE_DISCONNECTED_C) {
        Serial.println("ERROR: Sensor disconnected!");
        arr[i] = -1;
      } else {
        Serial.print("Temperature: ");
        Serial.print(tempC, 2);
        Serial.print(" °C | ");
        Serial.print(DallasTemperature::toFahrenheit(tempC), 2);
        Serial.println(" °F");
        arr[i] = tempC; 
      }
      delay(1000);
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

  float getTDS(){
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

    return tdsPpm;
  }
