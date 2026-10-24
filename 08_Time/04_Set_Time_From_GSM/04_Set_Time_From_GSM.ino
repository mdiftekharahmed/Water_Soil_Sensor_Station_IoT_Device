#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"

#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_RX_BUFFER 1024
#include <TinyGsmClient.h>

// ---- Pins (same as main sketch) ----
#define I2C_SDA_PIN    21
#define I2C_SCL_PIN    4
#define GSM_RELAY_PIN  18        // active-LOW relay: LOW = modem ON
#define MODEM_TX       33
#define MODEM_RX       32
#define SerialAT       Serial1

// ---- Settings ----
const char GSM_APN[]            = "gpinternet";
const long RTC_UTC_OFFSET_SEC   = 6 * 3600;   // Dhaka (UTC+6). RTC will hold LOCAL time
const int  MAX_ATTEMPTS         = 3;

TinyGsm   modem(SerialAT);
RTC_DS3231 rtc;

void printRtc(const char *label) {
  DateTime n = rtc.now();
  Serial.printf("%s %04d-%02d-%02d %02d:%02d:%02d\n",
                label, n.year(), n.month(), n.day(), n.hour(), n.minute(), n.second());
}

bool modemPowerUp() {
  digitalWrite(GSM_RELAY_PIN, HIGH);   // OFF
  delay(3000);
  digitalWrite(GSM_RELAY_PIN, LOW);    // ON
  delay(3500);
  SerialAT.begin(9600, SERIAL_8N1, MODEM_TX, MODEM_RX);

  unsigned long t = millis();
  while (millis() - t < 15000UL) {
    if (modem.testAT(1000)) {
      modem.sendAT(GF("E0"));      modem.waitResponse(2000);
      modem.sendAT(GF("+CMEE=2")); modem.waitResponse(2000);
      return true;
    }
  }
  return false;
}

void modemPowerDown() {
  SerialAT.end();
  digitalWrite(GSM_RELAY_PIN, HIGH);   // OFF
}

// Returns true and fills utc[] if NTP time was obtained
bool getNetworkUtc(DateTime &utc) {
  modem.sendAT(GF("+SAPBR=0,1")); modem.waitResponse(5000);   // close any old bearer
  modem.sendAT(GF("+SAPBR=3,1,\"Contype\",\"GPRS\""));        modem.waitResponse();
  modem.sendAT(GF("+SAPBR=3,1,\"APN\",\""), GSM_APN, GF("\"")); modem.waitResponse();
  modem.sendAT(GF("+SAPBR=1,1"));
  if (modem.waitResponse(85000L) != 1) {
    Serial.println("[NTP] Bearer open failed");
    return false;
  }

  bool ok = false;
  modem.sendAT(GF("+CNTPCID=1"));                    modem.waitResponse();
  modem.sendAT(GF("+CNTP=\"pool.ntp.org\",0"));      modem.waitResponse();   // 0 = UTC
  modem.sendAT(GF("+CNTP"));
  if (modem.waitResponse(10000L) == 1 &&
      modem.waitResponse(60000L, GF("+CNTP: 1")) == 1) {

    modem.sendAT(GF("+CCLK?"));
    if (modem.waitResponse(2000L, GF("+CCLK: \"")) == 1) {
      String s = SerialAT.readStringUntil('"');      // e.g. 26/10/21,16:54:57+00
      int yy, mo, dd, hh, mi, ss;
      Serial.printf("[NTP] Network time (UTC): %s\n", s.c_str());
      if (sscanf(s.c_str(), "%d/%d/%d,%d:%d:%d", &yy, &mo, &dd, &hh, &mi, &ss) == 6 && yy >= 26) {
        utc = DateTime(2000 + yy, mo, dd, hh, mi, ss);
        ok = true;
      }
    }
    modem.waitResponse(1000L);
  } else {
    Serial.println("[NTP] Sync failed");
  }

  modem.sendAT(GF("+SAPBR=0,1")); modem.waitResponse(5000);
  return ok;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== DS3231 TIME SETTER (via GSM NTP) ===");

  pinMode(GSM_RELAY_PIN, OUTPUT);
  digitalWrite(GSM_RELAY_PIN, HIGH);   // modem OFF to start

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (!rtc.begin()) { Serial.println("DS3231 not found!"); while (1); }

  rtc.disable32K();
  rtc.writeSqwPinMode(DS3231_OFF);
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);

  printRtc("RTC before:");

  // // this block sets the time
  //   bool done = false;
  //   for (int attempt = 1; attempt <= MAX_ATTEMPTS && !done; attempt++) {
  //     Serial.printf("\n--- Attempt %d/%d ---\n", attempt, MAX_ATTEMPTS);

  //     if (!modemPowerUp())                       { Serial.println("Modem not answering"); modemPowerDown(); continue; }
  //     if (!modem.waitForNetwork(60000L))         { Serial.println("No network");           modemPowerDown(); continue; }

  //     DateTime utc;
  //     if (getNetworkUtc(utc)) {
  //       DateTime local = utc + TimeSpan(RTC_UTC_OFFSET_SEC);
  //       rtc.adjust(local);
  //       done = true;
  //     }
  //     modemPowerDown();
  //   }

  //   if (done) {
  //     Serial.println("\n>>> RTC SET SUCCESSFULLY <<<");
  //   } else {
  //     Serial.println("\n>>> FAILED to set RTC. Check SIM, signal, balance, then reset. <<<");
  //   }
  //   printRtc("RTC after: ");
}

void loop() {
  static unsigned long t = 0;
  if (millis() - t > 5000) { t = millis(); printRtc("RTC now:  "); }
}