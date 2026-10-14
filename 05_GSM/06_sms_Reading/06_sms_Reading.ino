#include <Arduino.h>

#define MODEM_TX 335
#define MODEM_RX 32

HardwareSerial SerialAT(1);

void sendCommand(const char *cmd) {
  Serial.print(">> ");
  Serial.println(cmd);

  SerialAT.print(cmd);
  SerialAT.print("\r");

  delay(500);

  while (SerialAT.available()) {
    Serial.write(SerialAT.read());
  }

  Serial.println();
}

void setup() {
  Serial.begin(115200);

  // Keeping the UART order that works in your current hardware
  SerialAT.begin(
    9600,
    SERIAL_8N1,
    MODEM_TX,
    MODEM_RX
  );

  delay(3000);

  Serial.println();
  Serial.println("=== SIM800 STORED SMS TEST ===");

  // Text mode
  sendCommand("AT+CMGF=1");

  // Optional: explicitly select SIM message storage
  sendCommand("AT+CPMS=\"SM\",\"SM\",\"SM\"");

  Serial.println();
  Serial.println("=== STORED SMS ===");

  // List all stored SMS
  sendCommand("AT+CMGL=\"ALL\"");
}

void loop() {
}