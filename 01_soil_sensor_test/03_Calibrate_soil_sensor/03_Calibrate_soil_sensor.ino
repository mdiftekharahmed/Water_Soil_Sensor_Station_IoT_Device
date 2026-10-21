#include <Arduino.h>

// =====================================================
// Pin Mapping
// =====================================================
#define RX2_PIN           16  // MAX485 RO (via 10k/20k voltage divider to 3.3V)
#define TX2_PIN           17  // MAX485 DI
#define RS485_RE          23  // MAX485 RE: Receiver Enable (Active LOW)
#define RS485_DE          22  // MAX485 DE: Driver Enable   (Active HIGH)

#define MODBUS_RELAY_PIN  13  // MAX485 / Bus Power Relay (Active LOW)
#define SOIL_RELAY_PIN    14  // Soil Sensor 12V Power Relay (Active LOW)

HardwareSerial RS485Serial(2);

// Standard 7-Register Read Query (0x0000 - 0x0006)
const uint8_t readQuery[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x07, 0x04, 0x08 };

// =====================================================
// Helper Functions
// =====================================================
uint16_t modbusCRC(const uint8_t *buffer, uint8_t length) {
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

void printHex(const uint8_t *data, int length) {
  for (int i = 0; i < length; i++) {
    if (data[i] < 0x10) Serial.print("0");
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

// Separate control for decoupled RE and DE lines
void setRS485Transmit() {
  digitalWrite(RS485_RE, HIGH); // Disable receiver (prevents local echo)
  digitalWrite(RS485_DE, HIGH); // Enable driver
  delayMicroseconds(50);
}

void setRS485Receive() {
  digitalWrite(RS485_DE, LOW);  // Disable driver
  digitalWrite(RS485_RE, LOW);  // Enable receiver
}

// =====================================================
// Modbus Function 0x06: Write Single Register
// =====================================================
bool writeRegister(uint16_t regAddress, uint16_t value) {
  uint8_t frame[8];
  frame[0] = 0x01;                       // Slave ID
  frame[1] = 0x06;                       // Function Code 0x06 (Write Single Register)
  frame[2] = (regAddress >> 8) & 0xFF;   // Address High
  frame[3] = regAddress & 0xFF;          // Address Low
  frame[4] = (value >> 8) & 0xFF;        // Value High
  frame[5] = value & 0xFF;               // Value Low

  uint16_t crc = modbusCRC(frame, 6);
  frame[6] = crc & 0xFF;
  frame[7] = (crc >> 8) & 0xFF;

  Serial.printf("Writing Reg 0x%04X = %d ... TX: ", regAddress, value);
  printHex(frame, 8);

  while (RS485Serial.available()) RS485Serial.read();

  // Transmit
  setRS485Transmit();
  RS485Serial.write(frame, 8);
  RS485Serial.flush(); // Wait for full frame transmission
  setRS485Receive();

  // Listen for echo response (8 bytes)
  uint8_t rx[16];
  int count = 0;
  unsigned long start = millis();
  while (millis() - start < 1000 && count < 8) {
    if (RS485Serial.available()) {
      rx[count++] = RS485Serial.read();
    }
  }

  Serial.print("Response: ");
  if (count == 0) {
    Serial.println("TIMEOUT (No reply)");
    return false;
  }
  printHex(rx, count);

  // Sensor echoes back the exact frame on success
  if (count == 8 && memcmp(frame, rx, 8) == 0) {
    Serial.println(">>> SUCCESS: Stored in EEPROM.\n");
    return true;
  } else {
    Serial.println(">>> ERROR: Sensor rejected calibration write.\n");
    return false;
  }
}

// =====================================================
// Modbus Function 0x03: Read All 7 Registers
// =====================================================
void verifyReadings() {
  while (RS485Serial.available()) RS485Serial.read();

  setRS485Transmit();
  RS485Serial.write(readQuery, sizeof(readQuery));
  RS485Serial.flush();
  setRS485Receive();

  uint8_t rx[32];
  int len = 0;
  unsigned long start = millis();
  while (millis() - start < 1500 && len < 19) {
    if (RS485Serial.available()) {
      rx[len++] = RS485Serial.read();
    }
  }

  if (len >= 19 && rx[0] == 0x01 && rx[1] == 0x03) {
    float moisture    = ((rx[3] << 8) | rx[4]) * 0.1f;
    float temperature = ((rx[5] << 8) | rx[6]) > 0x7FFF 
                        ? -((0xFFFF - ((rx[5] << 8) | rx[6]) + 1) * 0.1f) 
                        : ((rx[5] << 8) | rx[6]) * 0.1f;
    uint16_t ec       = (rx[7] << 8) | rx[8];
    float pH          = ((rx[9] << 8) | rx[10]) * 0.1f;
    uint16_t n        = (rx[11] << 8) | rx[12];
    uint16_t p        = (rx[13] << 8) | rx[14];
    uint16_t k        = (rx[15] << 8) | rx[16];

    Serial.println("========= VERIFIED SENSOR VALUES =========");
    Serial.printf("Moisture    : %.1f %%\n", moisture);
    Serial.printf("Temperature : %.1f C\n", temperature);
    Serial.printf("EC          : %d uS/cm\n", ec);
    Serial.printf("pH          : %.1f\n", pH);
    Serial.printf("Nitrogen    : %d mg/kg\n", n);
    Serial.printf("Phosphorus  : %d mg/kg\n", p);
    Serial.printf("Potassium   : %d mg/kg\n", k);
    Serial.println("==========================================");
  } else {
    Serial.print("Raw Bytes Received (");
    Serial.print(len);
    Serial.print(" bytes): ");
    printHex(rx, len);
    Serial.println("[ERROR] Failed to read updated registers.");
  }
}

// =====================================================
// Setup
// =====================================================
void setup() {
  Serial.begin(115200);

  // 1. Initialize and energize relays (Active LOW)
  pinMode(MODBUS_RELAY_PIN, OUTPUT);
  pinMode(SOIL_RELAY_PIN, OUTPUT);
  digitalWrite(MODBUS_RELAY_PIN, LOW); // Relay ON
  digitalWrite(SOIL_RELAY_PIN, LOW);   // Relay ON
  Serial.println("\n[POWER] Modbus & Soil Relays Energized (LOW)");

  // 2. Configure separate RE and DE pins
  pinMode(RS485_RE, OUTPUT);
  pinMode(RS485_DE, OUTPUT);
  setRS485Receive(); // Default to listening mode

  // 3. Start HardwareSerial2
  RS485Serial.begin(4800, SERIAL_8N1, RX2_PIN, TX2_PIN);

  // 4. Critical boot delay for the sensor's internal MCU
  Serial.println("[WAIT] Waiting 3.5 seconds for sensor electronics to boot...");
  delay(3500);

  Serial.println("--- RS485 RBD-2300 NPK CALIBRATION TOOL ---");

  // Baseline calibration writes (Registers 0x0004, 0x0005, 0x0006)
  writeRegister(0x0004, 45);   // Nitrogen: 45 mg/kg
  delay(250);
  writeRegister(0x0005, 25);   // Phosphorus: 25 mg/kg
  delay(250);
  writeRegister(0x0006, 80);   // Potassium: 80 mg/kg
  delay(500);

  Serial.println("Verifying stored registers via Function 0x03 read...");
  verifyReadings();
}

void loop() {
  // Idle after one-time setup run
  delay(10000);
}