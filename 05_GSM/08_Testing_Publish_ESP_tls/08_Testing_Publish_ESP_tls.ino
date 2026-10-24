// ==========================================================
// ESP32 + SIM800L -> MQTT over TLS (encryption done on the ESP32)
// Chain: PubSubClient -> ESP_SSLClient (BearSSL) -> TinyGsmClient -> SIM800L
// ==========================================================

#define TINY_GSM_MODEM_SIM800
#define SerialMon Serial
#define SerialAT Serial1
#define TINY_GSM_DEBUG SerialMon
#define GSM_PIN ""
#define TINY_GSM_RX_BUFFER 1024   // TLS records are large; the default 64 is too small

#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP_SSLClient.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#define GSM_RELAY_PIN 18

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

// ESP32 and SIM800L pins (kept in the order that works on your board)
#define MODEM_TX 33
#define MODEM_RX 32

const char APN[] = "gpinternet";
const char GPRS_USER[] = "";
const char GPRS_PASS[] = "";

// ---------- MQTT over TLS ----------
const char MQTT_HOST[] = "telemetry.durnibar.tech";
const uint16_t MQTT_PORT = 8883;
const char MQTT_USER[] = "TEST_01";
const char MQTT_PASSWORD[] = "Gh8BbWRCQxLzyM4S";
const char DEVICE_ID[] = "TEST_01";
const char MQTT_TOPIC[] = "devices/TEST_01/telemetry";

// 0 = encrypted but server NOT verified (use only to prove the chain works)
// 1 = verify the broker against ROOT_CA below (use this for deployment)
#define USE_CA_VERIFY 0

#if USE_CA_VERIFY
// Root CA that signed the broker's certificate (e.g. ISRG Root X1 for Let's Encrypt)
const char ROOT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIEcDCCAligAwIBAgIQbI8dxyfHEX97r4U6yYD5zTANBgkqhkiG9w0BAQsFADBP
MQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFy
Y2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMTAeFw0yNjA1MTMwMDAwMDBa
Fw0zMjA5MDIyMzU5NTlaME8xCzAJBgNVBAYTAlVTMSkwJwYDVQQKEyBJbnRlcm5l
dCBTZWN1cml0eSBSZXNlYXJjaCBHcm91cDEVMBMGA1UEAxMMSVNSRyBSb290IFgy
MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAEzZvVn4CDCuwJSvMWSj5cz3es3mcFDR0H
ttwW+1qLFNvicWDEukWVEYmO6gbf9yoWHKS5xcUy4APgHoIYOIvXRdgKam7mAHf7
AlF9ItgKbppbd9/w+kHsOdx1ymgHDB/qo4H1MIHyMA4GA1UdDwEB/wQEAwIBBjAd
BgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwDwYDVR0TAQH/BAUwAwEB/zAd
BgNVHQ4EFgQUfEKWrt5LSDv6kviejM9ti6lyN5UwHwYDVR0jBBgwFoAUebRZ5nu2
5eQBc4AIiMgaWPbpm24wMgYIKwYBBQUHAQEEJjAkMCIGCCsGAQUFBzAChhZodHRw
Oi8veDEuaS5sZW5jci5vcmcvMBMGA1UdIAQMMAowCAYGZ4EMAQIBMCcGA1UdHwQg
MB4wHKAaoBiGFmh0dHA6Ly94MS5jLmxlbmNyLm9yZy8wDQYJKoZIhvcNAQELBQAD
ggIBAD2/e9frmMxNpCV03qUHegg+MV2wz9644YoXdqtH8RyWYcBO7xfjjGEXdU1e
/o0OkEFiynUCOSIk/vLLo7ttz6CPAeNlWfC0XNkoGeWgK6jjXvozBaGuGH5n0Ufo
shMeWTuURqNN5G00sSXDTBrpp2+mgvdZQjb8K11TYMA25QA+YHNfbIEL0BniAhKS
2gsnJjSzrdZLI+EZ7SEyqdR2rkjd1KutLDU+n3TFyxjniZVGur4YlhMP3mY/dV95
IruAkkjOZier6hGBdEgZXXvaCz9u9iVEadsIE75pAGL8oHV5vxdARDiotRpul1IN
/UZwzAbrfUFcw1HkAcYD/mlZfnQ2ieCF2MS7j3Vhv7JPDKp45fmykmzYNSrumRW0
upFFKDBOoF7hsOb7oLyHS+Uft6jOUfOrogj8YUx38hKb2K20r42OgsSdDdxdeYWc
MS3Sb6mwJeSZEYxJ2gaXnDSPaKhhrNkYwljyVQyr4Nq+MEJytXNTnHqaAcrNwZlV
pcJL1KBnMrMjP7eanvUwL3FYj3cF17jtboLt7gLoi4+2rWZFvn+w54jmd/FIuhhZ
cEaU/wvU6BUNMtcVquVGHp7itQeDth5j+XL3j4WJ2SABwzUl6OeYdgpIt/ITZa+p
TT0mQ/r5XyA4MEAiabn7XJjvCERlF2dcn2wqJw+CreTkkQ2R
-----END CERTIFICATE-----
)EOF";
#endif

const unsigned long PUBLISH_INTERVAL_MS = 10000UL;

TinyGsmClient testClient(modem);
TinyGsmClient mqttTransport(modem);   // raw TCP over the SIM800L
ESP_SSLClient sslClient;              // TLS on the ESP32
PubSubClient  mqtt(sslClient);

bool networkReady = false;
unsigned long lastPublish = 0;
unsigned long messageCount = 0;

void printSimInfo();
bool pingGoogleTcp();
bool syncClock(char *iso, size_t len);
bool connectMqtt();
bool publishTelemetry();

void setup() {
  SerialMon.begin(115200);
  delay(1000);

  // Active-LOW relay: LOW powers the modem
  pinMode(GSM_RELAY_PIN, OUTPUT);
  digitalWrite(GSM_RELAY_PIN, LOW);

  SerialMon.println("Wait ...");

  // Argument order kept as on your working setup
  SerialAT.begin(9600, SERIAL_8N1, MODEM_TX, MODEM_RX);

  delay(3000);
  SerialMon.println("Initializing modem ...");
  modem.restart();

  // Ask the network for the time (needed for the timestamp and certificate checks).
  // Takes effect on the next network registration; harmless if already enabled.
  modem.sendAT(GF("+CLTS=1"));
  modem.waitResponse();
  modem.sendAT(GF("&W"));
  modem.waitResponse();

  if (strlen(GSM_PIN) > 0 && modem.getSimStatus() == SIM_LOCKED) {
    modem.simUnlock(GSM_PIN);
  }

  SerialMon.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    SerialMon.println(" fail");
    delay(10000);
    ESP.restart();   // retry from scratch instead of idling forever
  }
  SerialMon.println(" success");

  if (modem.isNetworkConnected()) {
    DBG("Network connected");
  }

  printSimInfo();

  // TLS layer on top of the modem socket
  sslClient.setClient(&mqttTransport);
#if USE_CA_VERIFY
  sslClient.setCACert(ROOT_CA);
#else
  sslClient.setInsecure();
#endif
  sslClient.setBufferSizes(1024, 1024);   // rx, tx
  sslClient.setDebugLevel(1);

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setKeepAlive(60);
  mqtt.setSocketTimeout(30);   // the TLS handshake over 2G is slow
  mqtt.setBufferSize(512);

  networkReady = true;

  // Diagnostic: plain TCP reachability test over GPRS
  if (pingGoogleTcp()) {
    SerialMon.println("Internet test passed");
  } else {
    SerialMon.println("Internet test failed");
  }
}

void loop() {
  if (!networkReady) {
    delay(1000);
    return;
  }

  // Recover the data session if it dropped
  if (!modem.isGprsConnected()) {
    SerialMon.println("GPRS lost, reconnecting...");
    if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PASS)) {
      delay(5000);
      return;
    }
  }

  if (!mqtt.connected()) {
    connectMqtt();
  }

  if (mqtt.connected()) {
    mqtt.loop();

    if (millis() - lastPublish >= PUBLISH_INTERVAL_MS) {
      lastPublish = millis();
      if (!publishTelemetry()) {
        mqtt.disconnect();   // force a fresh TLS session on the next pass
      }
    }
  }

  delay(3000);
}

void printSimInfo() {
  SerialMon.println("\n===== SIM / NETWORK INFO =====");

  SimStatus simStatus = modem.getSimStatus();

  SerialMon.print("SIM status: ");
  switch (simStatus) {
    case SIM_READY:
      SerialMon.println("Ready");
      break;
    case SIM_LOCKED:
      SerialMon.println("Locked (PIN required)");
      break;
    case SIM_ANTITHEFT_LOCKED:
      SerialMon.println("Blocked / anti-theft locked");
      break;
    default:
      SerialMon.println("Not detected or error");
      break;
  }

  if (simStatus == SIM_READY) {
    SerialMon.print("ICCID: ");
    SerialMon.println(modem.getSimCCID());

    SerialMon.print("IMSI: ");
    SerialMon.println(modem.getIMSI());
  }

  SerialMon.print("IMEI (modem ID): ");
  SerialMon.println(modem.getIMEI());

  SerialMon.print("Operator: ");
  SerialMon.println(modem.getOperator());

  const int signal = modem.getSignalQuality();
  SerialMon.print("Signal quality: ");
  SerialMon.print(signal);
  SerialMon.println(signal == 99 ? " (unknown)" : " (0 poor, 31 strongest)");

  SerialMon.print("Network registered: ");
  SerialMon.println(modem.isNetworkConnected() ? "Yes" : "No");

  SerialMon.println("==============================");
}

bool pingGoogleTcp() {
  SerialMon.println("Connecting to GPRS...");

  if (!modem.isGprsConnected() &&
      !modem.gprsConnect(APN, GPRS_USER, GPRS_PASS)) {
    SerialMon.println("GPRS connection failed");
    return false;
  }

  SerialMon.println("Testing TCP connection to google.com:80...");

  if (testClient.connect("google.com", 80)) {
    SerialMon.println("Google is reachable");
    testClient.stop();
    return true;
  }

  SerialMon.println("Google is not reachable");
  testClient.stop();
  return false;
}

// Reads the modem's network clock (local time + zone offset), converts it to UTC,
// sets the ESP32 system clock (used for certificate validity) and, if given,
// writes an ISO 8601 UTC string like 2026-10-19T06:33:28Z into iso.
// Returns false when the modem clock isn't synced (modems default to 2004).
bool syncClock(char *iso, size_t len) {
  int yy, mo, dd, hh, mi, ss, tz;
  String s = modem.getGSMDateTime(DATE_FULL);   // e.g. "26/10/19,12:19:36+24" (tz in quarter-hours)

  if (sscanf(s.c_str(), "%d/%d/%d,%d:%d:%d%d", &yy, &mo, &dd, &hh, &mi, &ss, &tz) != 7) return false;
  if (yy < 25) return false;

  struct tm t = {};
  t.tm_year = 100 + yy;
  t.tm_mon = mo - 1;
  t.tm_mday = dd;
  t.tm_hour = hh;
  t.tm_min = mi;
  t.tm_sec = ss;

  time_t utc = mktime(&t) - (time_t)tz * 900;   // ESP32 default TZ is UTC

  struct timeval tv = { utc, 0 };
  settimeofday(&tv, nullptr);

  if (iso != nullptr) {
    struct tm g;
    gmtime_r(&utc, &g);
    strftime(iso, len, "%Y-%m-%dT%H:%M:%SZ", &g);
  }
  return true;
}

bool connectMqtt() {
  SerialMon.println("Attempting MQTT (TLS) connection...");

  if (!syncClock(nullptr, 0)) {
    SerialMon.println("[TIME] Modem clock not synced yet");
  }

  if (mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASSWORD)) {
    SerialMon.println("MQTT connected");
    return true;
  }

  SerialMon.print("MQTT connection failed, state=");
  SerialMon.println(mqtt.state());
  return false;
}

bool publishTelemetry() {
  JsonDocument doc;

  doc["message_id"] =
      String(DEVICE_ID) + "-" + String(millis()) + "-" + String(++messageCount);

  char ts[24];
  if (syncClock(ts, sizeof(ts))) {
    doc["timestamp"] = ts;   // ISO 8601 UTC, top level
  }

  JsonObject values = doc["values"].to<JsonObject>();

  // Demo values. Replace these with readings from your sensors.
  values["systemTemp"] = 25.0;
  values["Distance"] = 123;

  char payload[256];
  const size_t payloadLength =
      serializeJson(doc, payload, sizeof(payload));

  if (payloadLength == 0 || payloadLength >= sizeof(payload)) {
    SerialMon.println("Failed to create JSON payload");
    return false;
  }

  SerialMon.print("Publishing to topic: ");
  SerialMon.println(MQTT_TOPIC);
  SerialMon.print("Payload: ");
  SerialMon.println(payload);

  if (mqtt.publish(MQTT_TOPIC, payload)) {
    SerialMon.println("Publish successful");
    return true;
  }

  SerialMon.print("Publish failed, mqtt_state=");
  SerialMon.println(mqtt.state());
  return false;
}