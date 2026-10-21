#include <SoftwareSerial.h>

// MAX485 connections
const uint8_t RS485_RX = 10;  // RO
const uint8_t RS485_TX = 11;  // DI
const uint8_t RS485_RE = 7;   // RE, active LOW
const uint8_t RS485_DE = 8;   // DE, active HIGH

SoftwareSerial RS485Serial(RS485_RX, RS485_TX);

const uint8_t modbusQuery[] = {
  0x01, 0x03, 0x00, 0x00, 0x00, 0x07, 0x04, 0x08
};

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
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}

void readSoilSensor() {
  while (RS485Serial.available()) {
    RS485Serial.read();
  }

  Serial.println(F("--------------------------------"));
  Serial.println(F("TX:"));
  printHex(modbusQuery, sizeof(modbusQuery));

  // Transmit
  digitalWrite(RS485_RE, HIGH);  // Disable receiver
  digitalWrite(RS485_DE, HIGH);  // Enable driver
  delay(2);

  RS485Serial.write(modbusQuery, sizeof(modbusQuery));
  RS485Serial.flush();           // Wait for transmission to finish
  delay(2);

  // Receive
  digitalWrite(RS485_DE, LOW);   // Disable driver
  digitalWrite(RS485_RE, LOW);   // Enable receiver

  Serial.println(F("Listening..."));

  uint8_t rxBuffer[128];
  int rxLength = 0;
  unsigned long startTime = millis();

  while (millis() - startTime < 1500) {
    while (RS485Serial.available()) {
      int value = RS485Serial.read();

      if (rxLength < sizeof(rxBuffer)) {
        rxBuffer[rxLength++] = (uint8_t)value;
      }
    }
  }

  Serial.print(F("RX: "));

  if (rxLength == 0) {
    Serial.println(F("NOTHING"));
    Serial.println(F("Received: 0 bytes"));
    return;
  }

  printHex(rxBuffer, rxLength);
  Serial.print(F("Received: "));
  Serial.print(rxLength);
  Serial.println(F(" bytes"));

  // Response to 7 registers: 1 address + 1 function + 1 byte count
  // + 14 data bytes + 2 CRC bytes = 19 bytes.
  int frameStart = -1;

  for (int i = 0; i <= rxLength - 19; i++) {
    if (rxBuffer[i] == 0x01 &&
        rxBuffer[i + 1] == 0x03 &&
        rxBuffer[i + 2] == 0x0E) {
      frameStart = i;
      break;
    }
  }

  if (frameStart < 0) {
    Serial.println(F("NO VALID MODBUS FRAME"));
    return;
  }

  uint8_t frame[19];

  for (int i = 0; i < 19; i++) {
    frame[i] = rxBuffer[frameStart + i];
  }

  Serial.println(F("\nVALID MODBUS FRAME:"));
  printHex(frame, 19);

  uint16_t calculatedCRC = modbusCRC(frame, 17);
  uint16_t receivedCRC = frame[17] | ((uint16_t)frame[18] << 8);

  Serial.print(F("Calculated CRC: 0x"));
  Serial.println(calculatedCRC, HEX);

  Serial.print(F("Received CRC:   0x"));
  Serial.println(receivedCRC, HEX);

  if (calculatedCRC != receivedCRC) {
    Serial.println(F("CRC ERROR"));
    return;
  }

  Serial.println(F("CRC OK"));

  uint16_t reg[7];

  for (int i = 0; i < 7; i++) {
    int index = 3 + (i * 2);
    reg[i] = ((uint16_t)frame[index] << 8) | frame[index + 1];
  }

  float moisture = reg[0] * 0.1;
  float temperature =
      (reg[1] > 0x7FFF)
          ? -((0xFFFF - reg[1] + 1) * 0.1)
          : reg[1] * 0.1;
  uint16_t ec = reg[2];
  float pH = reg[3] * 0.1;
  uint16_t nitrogen = reg[4];
  uint16_t phosphorus = reg[5];
  uint16_t potassium = reg[6];

  Serial.println(F("\n========= SENSOR VALUES ========="));
  Serial.print(F("Moisture    : "));
  Serial.print(moisture, 1);
  Serial.println(F(" %"));

  Serial.print(F("Temperature : "));
  Serial.print(temperature, 1);
  Serial.println(F(" C"));

  Serial.print(F("EC          : "));
  Serial.print(ec);
  Serial.println(F(" uS/cm"));

  Serial.print(F("pH          : "));
  Serial.println(pH, 1);

  Serial.print(F("Nitrogen    : "));
  Serial.print(nitrogen);
  Serial.println(F(" mg/kg"));

  Serial.print(F("Phosphorus  : "));
  Serial.print(phosphorus);
  Serial.println(F(" mg/kg"));

  Serial.print(F("Potassium   : "));
  Serial.print(potassium);
  Serial.println(F(" mg/kg"));
  Serial.println(F("================================"));
}

void setup() {
  Serial.begin(115200);
  RS485Serial.begin(4800);

  pinMode(RS485_RE, OUTPUT);
  pinMode(RS485_DE, OUTPUT);

  // Start in receive mode
  digitalWrite(RS485_RE, LOW);
  digitalWrite(RS485_DE, LOW);

  delay(1000);

  Serial.println();
  Serial.println(F("================================"));
  Serial.println(F(" ARDUINO UNO SOIL SENSOR MODBUS TEST"));
  Serial.println(F("================================"));
  Serial.println(F("RO: pin 10"));
  Serial.println(F("DI: pin 11"));
  Serial.println(F("RE: pin 7"));
  Serial.println(F("DE: pin 8"));
  Serial.println(F("Baud: 4800"));
  Serial.println(F("Slave ID: 1"));
}

void loop() {
  readSoilSensor();
  delay(3000);
}


