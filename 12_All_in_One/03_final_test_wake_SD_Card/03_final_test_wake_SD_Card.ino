#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <Wire.h>
#include "RTClib.h"
#include <esp_sleep.h>
#include <SPI.h>
#include <SD.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <string.h>
// #include <TinyGsmClient.h>


// wifi connection
  #include <WiFi.h>
  #include <WiFiClientSecure.h>


  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  PubSubClient client;

  const char* ssid = "Innovation Lab";
  const char* password = "beinnovative###";

// ==========================================
// User Configuration
// ==========================================

  // MQTT Broker settings
  const char* mqtt_server = "telemetry.durnibar.tech"; // e.g. "telemetry.durnibar.tech" or "192.168.1.100"

  // Set use_tls to false for plain MQTT (port 1883), or true for MQTTS TLS (port 8883)
  const bool use_tls = false;
  const int mqtt_port = use_tls ? 8883 : 1883;

  // Device credentials from IoT-Telemetry-Hub
  // The user is the device_id. The password was generated when creating the device.
  const char* mqtt_user = "TEST_02"; 
  const char* mqtt_password = "7fq1epkIUQ1vkrs7";
  const char* device_id = "TEST_02";

  // ==========================================

  unsigned long lastMsg = 0;
  int messageCount = 0;

// ==========================================

Preferences pref;

// Component Setup
  // --- SD Card Pin Definitions ---
    #define SD_CS     5
    #define SD_SCK    2
    #define SD_MISO   19
    #define SD_MOSI   15
    SPIClass SD_SPI(VSPI);
    const char* CSV_FILE_PATH = "/sensor_data.csv";
  // Time Module
    // --- Pin Definitions ---
    #define I2C_SDA_PIN    21          // DS3231 SDA
    #define I2C_SCL_PIN    4            // DS3231 SCL
    #define RTC_WAKE_PIN   GPIO_NUM_39  // DS3231 SQW pin connected to GPIO 27 (RTC_GPIO 17)
    #define INTERVAL_30_MIN true
    #define SLEEP_SECONDS  300           // Test interval

    RTC_DS3231 rtc;
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


// Structure
  struct SensorData {
    // Reading system data
    float systemTemp = 0.00f;
    char timeStamp[25]={0};
    long long readCount = 0;

    // Ultrasonic sensor
    float distanceCm = 0.0f;
    bool distanceValid = false;

    // Turbidity module: estimated sensor output voltage
    float turbidityVoltage = 0.0f;
    bool turbidityValid = false;

    // Water temperature from DS18B20
    float waterTemperatureC = 0.0f;
    bool waterTemperatureValid = false;

    // TDS module
    float tdsPpm = 0.0f;
    float tdsVoltage = 0.0f;
    bool tdsValid = false;

    // Soil sensor Modbus registers
    float soilMoisturePercent = 0.0f;
    float soilTemperatureC = 0.0f;
    uint16_t soilEcUsCm = 0;
    float soilPH = 0.0f;
    uint16_t soilNitrogenMgKg = 0;
    uint16_t soilPhosphorusMgKg = 0;
    uint16_t soilPotassiumMgKg = 0;
    bool soilValid = false;
  };

// Functions
  bool readSoilSensor(SensorData &data);
  void enableSoilSensor();
  float getDistance(SensorData &data);
  float getTurbidity(SensorData &data);
  float getWaterTemp(SensorData &data);
  float getTDS(SensorData &data);
  // Loads saved counter and fallback data from Flash NVS
  void loadNVSData(long long &counter, SensorData &fallback);
  // Saves current valid state to Flash NVS
  void saveNVSTelemetry(long long counter, const SensorData &data);
  bool logDataToSD(const SensorData &data);

  void setup_wifi();
  void reconnect();
  bool sendTelemetry(const SensorData &data);     // for gsm
  bool publishTelemetry(const SensorData &data); //for wifi
  void printSensorData(const SensorData& data);
  
// ======================== Setup =============================
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(512);

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
    digitalWrite(OTHER_RELAY_PIN, LOW);

  
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


  // // // Time module
  //   Serial.println("\n==========================================");
  //   Serial.println("[STEP 1] ESP32 Woke Up / Booted");

  //   // Check how the ESP32 was triggered
  //   esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  //   if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
  //     Serial.println("[STATUS] Wakeup Cause: DS3231 Alarm Interrupt (GPIO 27 / EXT0)");
  //   } else {
  //     Serial.println("[STATUS] Wakeup Cause: Power-on or Manual Reset");
  //   }

  //   // 2. Initialize I2C and DS3231
  //   Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  //   if (!rtc.begin()) {
  //     Serial.println("[ERROR] DS3231 not found on GPIO 15/4! Halting.");
  //     while (1);
  //   }

  //   if (rtc.lostPower()) {
  //     Serial.println("[WARN] RTC lost power, syncing to compile time...");
  //     rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  //   }

  //   DateTime now = rtc.now();
  //   Serial.printf("[TIME] Current RTC Time: %04d-%02d-%02d %02d:%02d:%02d\n",
  //                 now.year(), now.month(), now.day(),
  //                 now.hour(), now.minute(), now.second());

  //   // 3. Blink LED 10 Times
  //   Serial.println("\n[STEP 2] Starting 10-Blink Sequence...");
  //   Serial.println("[STATUS] 10 Blinks completed.");

  //   // 4. Configure DS3231 SQW for Alarm Interrupt Mode
  //   Serial.println("\n[STEP 3] Arming DS3231 Alarm 1 for 20 Seconds Ahead...");
  //   rtc.disable32K();
  //   rtc.writeSqwPinMode(DS3231_OFF); // Puts SQW line in alarm interrupt mode

  //   // Clear existing alarm flags to ensure SQW line is released HIGH (3.3V)
  //   rtc.clearAlarm(1);
  //   rtc.clearAlarm(2);
  //   rtc.disableAlarm(2);

  //   // Set Alarm 1 for 20 seconds into the future
  //   now = rtc.now();
  //   DateTime alarmTime = now + TimeSpan(SLEEP_SECONDS);
  //   rtc.setAlarm1(alarmTime, DS3231_A1_Date); // Exact match: date, hour, minute, second

  //   Serial.printf("[ALARM] Armed for %02d:%02d:%02d (Wait duration: %d seconds)\n",
  //                 alarmTime.hour(), alarmTime.minute(), alarmTime.second(), SLEEP_SECONDS);

  //   // Clear alarm flag once more so the pin remains HIGH before entering sleep
  //   rtc.clearAlarm(1);

  //   // 5. Configure ESP32 EXT0 Wakeup
  //   Serial.println("\n[STEP 4] Arming ESP32 EXT0 Wakeup on GPIO 27 (Trigger: LOW)...");
  //   esp_sleep_enable_ext0_wakeup(RTC_WAKE_PIN, 0); // 0 = wake when SQW pulls pin to 0V

  //   // 6. Enter Deep Sleep
  //   Serial.println("[STEP 5] Entering Deep Sleep. Waiting for DS3231 alarm...");
  //   Serial.println("==========================================\n");
  //   Serial.flush();

  //   // esp_deep_sleep_start();

  // // Time module wakes on xx:00:00 and xx:30:00

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
  digitalWrite(OTHER_RELAY_PIN, HIGH);
}


// =====================================================
// Loop
// =====================================================
void loop() {
  SensorData sensorData;
  SensorData fallbackData;

    // Internal Storage
      // 1. Load historical state and counter from flash
      loadNVSData(sensorData.readCount, fallbackData);
      sensorData.readCount =  fallbackData.readCount + 1;

    // Previous Data
      Serial.println("================================");
      Serial.println("Previous Data");
      Serial.println("================================");
      Serial.println("\n========= SENSOR VALUES =========");
      Serial.print("Moisture    : "); Serial.print(fallbackData.soilMoisturePercent, 1);    Serial.println(" %");
      Serial.print("Temperature : "); Serial.print(fallbackData.soilTemperatureC, 1); Serial.println(" C");
      Serial.print("EC          : "); Serial.print(fallbackData.soilEcUsCm);             Serial.println(" uS/cm");
      Serial.print("pH          : "); Serial.print(fallbackData.soilPH, 1);          Serial.println();
      Serial.print("Nitrogen    : "); Serial.print(fallbackData.soilNitrogenMgKg);       Serial.println(" mg/kg");
      Serial.print("Phosphorus  : "); Serial.print(fallbackData.soilPhosphorusMgKg);     Serial.println(" mg/kg");
      Serial.print("Potassium   : "); Serial.print(fallbackData.soilPotassiumMgKg);      Serial.println(" mg/kg");
      Serial.println("================================");


    // System and Reading count data
      // Read DS3231 internal die temperature
        sensorData.systemTemp = rtc.getTemperature();

      // Read current timestamp from DS3231
        // Set to your RTC's offset from UTC in seconds (0 if the RTC already holds UTC)
        const long RTC_UTC_OFFSET_SEC = 6 * 3600;   // e.g. 6 * 3600 if the RTC holds UTC+6

        DateTime now = rtc.now() - TimeSpan(RTC_UTC_OFFSET_SEC);

        // ISO 8601 UTC, e.g. 2026-10-18T09:30:00Z (20 chars, fits timeStamp[25])
        snprintf(sensorData.timeStamp, sizeof(sensorData.timeStamp), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                now.year(), now.month(), now.day(),
                now.hour(), now.minute(), now.second());

        Serial.printf("Timestamp      : %s\n", sensorData.timeStamp);

      // Print System Telemetry to Serial Monitor
          Serial.println("\n========= SYSTEM TELEMETRY =========");
          Serial.printf("Read Count (ID): %lld\n", sensorData.readCount);
          Serial.printf("Timestamp      : %s\n", sensorData.timeStamp);
          Serial.printf("System Temp    : %.2f °C\n", sensorData.systemTemp);
          Serial.println("=====================================");
    // ------------

  //   // ==========Collect Data==========

      // Soil sensor Data
        enableSoilSensor();
        delay(3000);
        bool success = readSoilSensor(sensorData);
        if(!success){
          sensorData.soilMoisturePercent=fallbackData.soilMoisturePercent;
          sensorData.soilTemperatureC=fallbackData.soilTemperatureC;
          sensorData.soilEcUsCm=fallbackData.soilEcUsCm;
          sensorData.soilPH=fallbackData.soilPH;
          sensorData.soilNitrogenMgKg=fallbackData.soilNitrogenMgKg;
          sensorData.soilPhosphorusMgKg=fallbackData.soilPhosphorusMgKg;
          sensorData.soilPotassiumMgKg=fallbackData.soilPotassiumMgKg;
          Serial.println("================================");
        }
        delay(1000);
        disableSoilSensor();
      // Checking Soil Data
        if (success && sensorData.soilValid) {
          Serial.println("========= SOIL DATA OBJECT =========");
          Serial.print("Moisture: ");   Serial.print(sensorData.soilMoisturePercent, 1); Serial.println(" %");
          Serial.print("Temperature: "); Serial.print(sensorData.soilTemperatureC, 1); Serial.println(" C");
          Serial.print("EC: ");         Serial.print(sensorData.soilEcUsCm); Serial.println(" uS/cm");
          Serial.print("pH: ");         Serial.println(sensorData.soilPH, 1);
          Serial.print("Nitrogen: ");   Serial.print(sensorData.soilNitrogenMgKg); Serial.println(" mg/kg");
          Serial.print("Phosphorus: "); Serial.print(sensorData.soilPhosphorusMgKg); Serial.println(" mg/kg");
          Serial.print("Potassium: ");  Serial.print(sensorData.soilPotassiumMgKg); Serial.println(" mg/kg");
          Serial.println("====================================");
        } else {
          Serial.println("Soil sensor read failed.");
        }

      
      enableOtherPins();
      delay(3000);
      // Ultrasonic distance
        float dist = getDistance(sensorData);
        if(dist==-1){sensorData.distanceCm =fallbackData.distanceCm; }
        Serial.print("distance: ------------> ");
        Serial.println(sensorData.distanceCm);
      // Turbidity
        float turbidity = getTurbidity(sensorData);
        if(turbidity==-1){sensorData.turbidityVoltage =fallbackData.turbidityVoltage; }
        Serial.print("Turbidty: ------------> ");
        Serial.println(sensorData.turbidityVoltage);
      // Water temp
        float waterTemp =  getWaterTemp(sensorData);
        if(waterTemp==-1){sensorData.waterTemperatureC =fallbackData.waterTemperatureC; }
        Serial.print("Water Temp: ------------> ");
        Serial.println(sensorData.waterTemperatureC);
      // TDS
        float tds = getTDS(sensorData);
        if(tds==-1){sensorData.tdsPpm =fallbackData.tdsPpm; }
        Serial.print("TDS:-------------------> ");
        Serial.println(sensorData.tdsPpm);
      
      disableOtherPins();
      delay(3000);
    
  sensorData.readCount =  fallbackData.readCount + 1;
  // // //Commit Data to Flash NVS and SD Card
    saveNVSTelemetry(sensorData.readCount, sensorData);
    enableOtherPins();
    delay(1000);
    logDataToSD(sensorData);
    disableOtherPins();


  
  // // sendTelemetry(sensorData);
  
  //WIFI:  Fresh MQTT session right before publishing
      if (WiFi.status() != WL_CONNECTED) setup_wifi();
      client.disconnect();   // drop any stale or half-open session
      reconnect();
      if (!publishTelemetry(sensorData)) {   // one retry on a new session
        client.disconnect();
        reconnect();
        publishTelemetry(sensorData);
      }
      client.loop();
      delay(500);            // let the packet leave before closing
      client.disconnect();

  // // Go to deep sleep
    Serial.println("[SLEEP] Measurement complete. Entering Deep Sleep...");
    Serial.flush();

    client.loop();
    delay(5000);

  printSensorData(sensorData);
  delay(3000);
  esp_deep_sleep_start();
}


// void loop() {
//   // Wifi code
//     if (!client.connected()) {
//         reconnect();
//       }
//     client.loop();

//   SensorData sensorData;
//   SensorData fallbackData;


//   // 1. Load historical state and counter from flash
//   loadNVSData(sensorData.readCount, fallbackData);

//   fallbackData.readCount =  fallbackData.readCount + 1;
//   printSensorData(fallbackData);
//   publishTelemetry(fallbackData);

//   // Read current timestamp from DS3231
//   // Set to your RTC's offset from UTC in seconds (0 if the RTC already holds UTC)
//   const long RTC_UTC_OFFSET_SEC = 6 * 3600;   // e.g. 6 * 3600 if the RTC holds UTC+6

//   DateTime now = rtc.now() - TimeSpan(RTC_UTC_OFFSET_SEC);

//   // ISO 8601 UTC, e.g. 2026-10-18T09:30:00Z (20 chars, fits timeStamp[25])
//   snprintf(fallbackData.timeStamp, sizeof(sensorData.timeStamp), "%04d-%02d-%02dT%02d:%02d:%02dZ",
//           now.year(), now.month(), now.day(),
//           now.hour(), now.minute(), now.second());

//   Serial.printf("Timestamp      : %s\n", sensorData.timeStamp);

//   saveNVSTelemetry(fallbackData.readCount, fallbackData);

//   // // Go to deep sleep
//     Serial.println("[SLEEP] Measurement complete. Entering Deep Sleep...");
//     Serial.flush();

//     client.loop();
//   //   delay(5000);

//   // // esp_deep_sleep_start();
//   delay(3000);
// }




// Function Definition

  bool readSoilSensor(SensorData &data) {
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
      return false;
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
      return false;
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
      return false;
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
    float temperature = (reg[1] > 0x7FFF) ? -((0xFFFF - reg[1] + 1) * 0.1f) : reg[1] * 0.1f;
    uint16_t ec      = reg[2];
    float pH         = reg[3] * 0.1;
    uint16_t nitrogen   = reg[4];
    uint16_t phosphorus = reg[5];
    uint16_t potassium  = reg[6];

    data.soilMoisturePercent = moisture;
    data.soilTemperatureC = temperature;
    data.soilEcUsCm = ec;
    data.soilPH = pH;
    data.soilNitrogenMgKg = nitrogen;
    data.soilPhosphorusMgKg = phosphorus;
    data.soilPotassiumMgKg = potassium;
    data.soilValid = true;

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
    
    return true;
  }

  void enableSoilSensor(){
    Serial.println("Relay 2 ON");
    digitalWrite(MODBUS_RELAY_PIN, LOW);
    Serial.println("Relay 3 ON");
    digitalWrite(SOIL_RELAY_PIN, LOW);
    delay(1000);
  }
  void disableSoilSensor(){
    Serial.println("Relay 2 ON");
    digitalWrite(MODBUS_RELAY_PIN, HIGH);
    Serial.println("Relay 3 ON");
    digitalWrite(SOIL_RELAY_PIN, HIGH);
    delay(1000);
  }
  void enableOtherPins(){
    Serial.println("Relay 4 ON");
    digitalWrite(OTHER_RELAY_PIN, LOW);
    delay(1000);
  }
  void disableOtherPins(){
    Serial.println("Relay 4 OFF");
    digitalWrite(OTHER_RELAY_PIN, HIGH);
    delay(1000);
  }

  float getDistance(SensorData &data){

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
    data.distanceCm = val/4;
    if(arr[5]==-1)data.distanceValid = false;
    else data.distanceValid = true;

    return val/4;
  }

  float getTurbidity(SensorData &data){
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

    data.turbidityVoltage = val/4;
    if(arr[5]==-1)data.turbidityValid = false;
    else data.turbidityValid = true;
    return val/4;
  }

  float getWaterTemp(SensorData &data){
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

    data.waterTemperatureC = val/4;
    if(arr[5]==-1)data.waterTemperatureValid = false;
    else data.waterTemperatureValid = true;
    return val/4;
  }

  float getTDS(SensorData &data){
    float arr[num_of_read];
    float vrr[num_of_read];
    for(int  i=0;i<num_of_read;i++){
          uint32_t millivoltSum = 0;

          // Discard one initial reading, then collect calibrated readings.
          analogReadMilliVolts(TDS_PIN);
          for (int i = 0; i < SAMPLE_COUNT; ++i) {
            millivoltSum += analogReadMilliVolts(TDS_PIN);
            delay(10);
          }

          const float voltage = (millivoltSum / float(SAMPLE_COUNT)) / 1000.0f;
          vrr[i]= voltage;
          // Temperature compensation referenced to 25 °C.
          const float compensationCoefficient =
              1.0f + 0.02f * (data.waterTemperatureC - 25.0f);
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
          Serial.print(data.waterTemperatureC, 1);
          Serial.print(" C | TDS: ");
          Serial.print(tdsPpm, 0);
          Serial.println(" ppm");

          arr[i] = tdsPpm;
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
      for (int i = 0; i < num_of_read - 1; i++) {
        bool swapped = false;
        
        for (int j = 0; j < num_of_read - i - 1; j++) {
          if (vrr[j] > vrr[j + 1]) {
            // Swap the elements
            float temp = vrr[j];
            vrr[j] = vrr[j + 1];
            vrr[j + 1] = temp;
            swapped = true;
          }
        }
        if (!swapped) {
          break;
        }
      }

      float valp = 0, valv=0;
      for(int i=3;i<7;i++){
        valp+= arr[i];
        valv+= vrr[i];
      }


      data.tdsPpm = valp/4;
      data.tdsVoltage = valv/4;
      if(arr[5]==-1)data.tdsValid = false;
      else data.tdsValid = true;
      return data.tdsPpm;
  }

  // Loads saved counter and fallback data from Flash NVS
  void loadNVSData(long long &counter, SensorData &fallback) {
    pref.begin("telemetry", true); // Open namespace in read-only mode

    // Load persistent counter (default to 0 if never set or power was lost)
    counter = pref.getLong64("readCount", 0);

    // Load last known good sensor payload
    size_t bytesRead = pref.getBytes("lastData", &fallback, sizeof(SensorData));
    if (bytesRead == sizeof(SensorData)) {
      Serial.println("[NVS] Loaded previous valid sensor readings for fallback.");
    } else {
      Serial.println("[NVS] No previous telemetry found in flash. Starting fresh.");
      memset(&fallback, 0, sizeof(SensorData));
    }

    pref.end();
  }

  // Saves current valid state to Flash NVS
  void saveNVSTelemetry(long long counter, const SensorData &data) {
    pref.begin("telemetry", false); // Open namespace in read-write mode

    pref.putLong64("readCount", counter);
    pref.putBytes("lastData", &data, sizeof(SensorData));

    pref.end();
    Serial.printf("[NVS] Successfully committed ID #%lld and data to Flash.\n", counter);
  }

  // SD CARD 
  bool logDataToSD(const SensorData &data) {
    // 1. Initialize SPI bus and SD module
    SD_SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS, SD_SPI, 20000000)) {
      Serial.println("[SD ERROR] Mount Failed! Skipping SD write.");
      return false;
    }

    // 2. Check if file already exists to decide whether to write the header
    bool fileExists = SD.exists(CSV_FILE_PATH);

    // 3. Open in append mode (creates file if it does not exist)
    File file = SD.open(CSV_FILE_PATH, FILE_APPEND);
    if (!file) {
      Serial.println("[SD ERROR] Failed to open CSV file for appending!");
      SD.end();
      return false;
    }

    // 4. Write CSV header only on the very first entry
    if (!fileExists) {
      file.println("ReadCount,Timestamp,SystemTemp_C,"
                  "Distance_cm,DistanceValid,"
                  "Turbidity_V,TurbidityValid,"
                  "WaterTemp_C,WaterTempValid,"
                  "TDS_ppm,"
                  "SoilMoisture_pct,SoilTemp_C,SoilEC_uS_cm,SoilPH,"
                  "Nitrogen_mg_kg,Phosphorus_mg_kg,Potassium_mg_kg,SoilValid");
    }

    // 5. Append telemetry row
    file.printf("%lld,%s,%.2f,%.1f,%d,%.3f,%d,%.2f,%d,%.1f,%.1f,%.1f,%u,%.1f,%u,%u,%u,%d\n",
                data.readCount,
                data.timeStamp,
                data.systemTemp,
                data.distanceCm,
                data.distanceValid ? 1 : 0,
                data.turbidityVoltage,
                data.turbidityValid ? 1 : 0,
                data.waterTemperatureC,
                data.waterTemperatureValid ? 1 : 0,
                data.tdsPpm,
                data.soilMoisturePercent,
                data.soilTemperatureC,
                data.soilEcUsCm,
                data.soilPH,
                data.soilNitrogenMgKg,
                data.soilPhosphorusMgKg,
                data.soilPotassiumMgKg,
                data.soilValid ? 1 : 0);

    file.flush();
    file.close();
    SD.end(); // Unmount to prevent SPI bus power leakage during deep sleep

    Serial.printf("[SD] Successfully appended Cycle ID #%lld to %s\n", data.readCount, CSV_FILE_PATH);
    return true;
  }

  void setup_wifi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    if (use_tls) {
      secureClient.setInsecure();
      client.setClient(secureClient);
      Serial.println("Configured for MQTTS (TLS) on port 8883");
    } else {
      client.setClient(plainClient);
      Serial.println("Configured for plain MQTT on port 1883");
    }
  }

  void reconnect() {
    while (!client.connected()) {
      Serial.print("Attempting MQTT connection...");
      
      String clientId = "ESP32Client-";
      clientId += String(random(0xffff), HEX);
      
      if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
        Serial.println("connected");
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        delay(5000);
      }
    }
  }

  bool sendTelemetry(const SensorData &data) {
    client.setBufferSize(512);

    JsonDocument doc;

    String messageId = "msg-" + String(device_id) + "-" + String((uint64_t)data.readCount);
    doc["message_id"] = messageId;

    JsonObject values = doc["values"].to<JsonObject>();
    values["systemTemp"]      = data.systemTemp;
    values["timestamp"]       = data.timeStamp;
    values["Moisture"]        = data.soilMoisturePercent;
    values["SoilTemperature"] = data.soilTemperatureC;
    values["EC"]              = data.soilEcUsCm;
    values["N"]               = data.soilNitrogenMgKg;
    values["P"]               = data.soilPhosphorusMgKg;
    values["K"]               = data.soilPotassiumMgKg;

    values["Distance"]        = data.distanceCm;
    values["TDS"]             = data.tdsPpm;
    values["pH"]              = data.soilPH;
    values["Turbidity"]       = data.turbidityVoltage;
    values["WaterTemperature"]= data.waterTemperatureC;

    char payload[512];
    serializeJson(doc, payload);

    String topic = "devices/" + String(device_id) + "/telemetry";

    Serial.print("Publishing message to topic: ");
    Serial.println(topic);
    Serial.print("Payload: ");
    Serial.println(payload);

    if (client.publish(topic.c_str(), payload)) {
      Serial.println("Publish successful!");
      Serial.println("-------------------------");
      return true;
    } else {
      Serial.println("Publish failed!");
      Serial.println("-------------------------");
      return false;
    }
  }


  void printSensorData(const SensorData& data) {
    Serial.println("\n========= SENSOR DATA =========");

    Serial.print("System temperature: ");
    Serial.print(data.systemTemp, 2);
    Serial.println(" C");

    Serial.print("Timestamp: ");
    Serial.println(data.timeStamp);

    Serial.print("Read count: ");
    Serial.println(data.readCount);

    Serial.print("Distance: ");
    Serial.print(data.distanceCm, 2);
    Serial.print(" cm | Valid: ");
    Serial.println(data.distanceValid ? "Yes" : "No");

    Serial.print("Turbidity voltage: ");
    Serial.print(data.turbidityVoltage, 3);
    Serial.print(" V | Valid: ");
    Serial.println(data.turbidityValid ? "Yes" : "No");

    Serial.print("Water temperature: ");
    Serial.print(data.waterTemperatureC, 2);
    Serial.print(" C | Valid: ");
    Serial.println(data.waterTemperatureValid ? "Yes" : "No");

    Serial.print("TDS: ");
    Serial.print(data.tdsPpm, 0);
    Serial.print(" ppm | Voltage: ");
    Serial.print(data.tdsVoltage, 3);
    Serial.print(" V | Valid: ");
    Serial.println(data.tdsValid ? "Yes" : "No");

    Serial.print("Soil moisture: ");
    Serial.print(data.soilMoisturePercent, 1);
    Serial.println(" %");

    Serial.print("Soil temperature: ");
    Serial.print(data.soilTemperatureC, 1);
    Serial.println(" C");

    Serial.print("Soil EC: ");
    Serial.print(data.soilEcUsCm);
    Serial.println(" uS/cm");

    Serial.print("Soil pH: ");
    Serial.println(data.soilPH, 1);

    Serial.print("Soil nitrogen: ");
    Serial.print(data.soilNitrogenMgKg);
    Serial.println(" mg/kg");

    Serial.print("Soil phosphorus: ");
    Serial.print(data.soilPhosphorusMgKg);
    Serial.println(" mg/kg");

    Serial.print("Soil potassium: ");
    Serial.print(data.soilPotassiumMgKg);
    Serial.println(" mg/kg");

    Serial.print("Soil data valid: ");
    Serial.println(data.soilValid ? "Yes" : "No");

    Serial.println("===============================");
  }





  
  //Publish TElemetry
  bool publishTelemetry(const SensorData &data) {
        client.setBufferSize(512);

    JsonDocument doc;

    String messageId = "msg-" + String(device_id) + "-" + String((uint64_t)data.readCount);
    doc["message_id"] = messageId;
    // doc["timestamp"]       = data.timeStamp;

     
    // data.timeStamp = "YYYY-MM-DDTHH:MM:SSZ" -> same format, +6 hours
      char localTs[25];
      int y, mo, d, h, mi, s;
      if (sscanf(data.timeStamp, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) == 6) {
        DateTime t = DateTime(y, mo, d, h, mi, s) + TimeSpan(6 * 3600);
        snprintf(localTs, sizeof(localTs), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                t.year(), t.month(), t.day(), t.hour(), t.minute(), t.second());
      } else {
        strlcpy(localTs, data.timeStamp, sizeof(localTs));
      }
      doc["timestamp"] = localTs;

    JsonObject values = doc["values"].to<JsonObject>();
    values["systemTemp"]      = data.systemTemp;
    values["S_Moisture"]        = data.soilMoisturePercent;
    values["S_Temp"] = data.soilTemperatureC;
    values["S_EC"]              = data.soilEcUsCm;
    values["S_N"]               = data.soilNitrogenMgKg;
    values["S_P"]               = data.soilPhosphorusMgKg;
    values["S_K"]               = data.soilPotassiumMgKg;
    values["S_pH"]              = data.soilPH;

    values["Distance"]        = data.distanceCm;
    values["W_TDS"]             = data.tdsPpm;
    values["W_TDS_Volt"]     = data.tdsVoltage;
    values["W_Turb"]       = data.turbidityVoltage;
    values["W_Temp"]= data.waterTemperatureC;

    char payload[512];
    serializeJson(doc, payload);

    String topic = "devices/" + String(device_id) + "/telemetry";

    Serial.print("Publishing message to topic: ");
    Serial.println(topic);
    Serial.print("Payload: ");
    Serial.println(payload);

    if (client.publish(topic.c_str(), payload)) {
      Serial.println("Publish successful!");
      Serial.println("-------------------------");
      return true;
    } else {
      Serial.println("Publish failed!");
      Serial.println("-------------------------");
      return false;
    }
  } 