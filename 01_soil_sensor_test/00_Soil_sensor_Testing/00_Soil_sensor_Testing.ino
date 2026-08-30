#include <Arduino.h>

// =====================================================
// ESP32 UART2 + MAX485 (Separate RE and DE Pins)
// =====================================================

#define RX2_PIN     16  // RO: goes to RX2 (goes with 10k/20k ohm voltage divider to convert 5v to 3.3v)
#define TX2_PIN     17  // DI: goes to TX2
#define RS485_RE    23  // RE:  Receiver Enable (Active LOW)
#define RS485_DE    22  // DE:  Driver Enable   (Active HIGH)

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

// =====================================================
// Setup
// =====================================================
void setup() {
  Serial.begin(115200);

  // MAX485 direction pins configuration
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
}

// =====================================================
// Read Sensor
// =====================================================
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

// =====================================================
// Loop
// =====================================================
void loop() {
  readSoilSensor();
  delay(3000);
}