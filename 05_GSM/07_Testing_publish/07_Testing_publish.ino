#define TINY_GSM_MODEM_SIM800
#define SerialMon Serial
#define SerialAT Serial1
#define TINY_GSM_DEBUG SerialMon
#define GSM_PIN ""

#define GSM_RELAY_PIN 18
#define ADMIN_NUMBER "+8801776418720"

#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <string.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

// ESP32 and SIM800L pins
#define MODEM_TX 33
#define MODEM_RX 32

String phoneNumber = "";
String text = "";

const char APN[] = "gpinternet";
const char GPRS_USER[] = "";
const char GPRS_PASS[] = "";

// MQTT settings: plain MQTT
const char MQTT_HOST[] = "telemetry.durnibar.tech";
const uint16_t MQTT_PORT = 1883;
const char MQTT_USER[] = "TEST_03";
const char MQTT_PASSWORD[] = "PUT_YOUR_ROTATED_PASSWORD_HERE";
const char DEVICE_ID[] = "TEST_03";
const char MQTT_TOPIC[] = "devices/TEST_03/telemetry";

TinyGsmClient testClient(modem);
TinyGsmClient mqttTransport(modem);
PubSubClient mqtt(mqttTransport);

bool networkReady = false;
unsigned long lastPublish = 0;
unsigned long messageCount = 0;

void printSimInfo();
bool pingGoogleTcp();
bool connectMqtt();
bool publishTelemetry();

void setup() {
  SerialMon.begin(115200);
  delay(1000);

  pinMode(GSM_RELAY_PIN, OUTPUT);
  digitalWrite(GSM_RELAY_PIN, LOW);

  SerialMon.println("Wait ...");

  // Keeping your working TX-then-RX argument order as requested.
  SerialAT.begin(9600, SERIAL_8N1, MODEM_TX, MODEM_RX);

  delay(3000);
  SerialMon.println("Initializing modem ...");
  modem.restart();

  if (strlen(GSM_PIN) > 0 && modem.getSimStatus() == SIM_LOCKED) {
    modem.simUnlock(GSM_PIN);
  }

  SerialMon.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }

  SerialMon.println(" success");

  if (modem.isNetworkConnected()) {
    DBG("Network connected");
  }

  // SMS example remains available if needed.
  // SerialAT.print("AT+CNMI=2,2,0,0,0\r");
  // delay(1000);
  // String smsMessage = "Hello From ESP32";
  // Serial.println(smsMessage);
  // modem.sendSMS(ADMIN_NUMBER, smsMessage);

  SerialMon.println(" success");
  printSimInfo();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setKeepAlive(60);
  mqtt.setSocketTimeout(30);
  mqtt.setBufferSize(512);

  networkReady = true;
}

void loop() {
  if (!networkReady) {
    delay(1000);
    return;
  }

  // Keep the Google TCP test in the loop for diagnostic logging.
  const bool googleReachable = pingGoogleTcp();

  if (googleReachable) {
    SerialMon.println("Internet test passed");
  } else {
    SerialMon.println("Internet test failed");
  }

  if (modem.isGprsConnected()) {
    if (!mqtt.connected()) {
      connectMqtt();
    }

    if (mqtt.connected()) {
      mqtt.loop();

      if (millis() - lastPublish >= 10000UL) {
        lastPublish = millis();
        publishTelemetry();
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
  SerialMon.println(signal == 99
                        ? " (unknown)"
                        : " (0 poor, 31 strongest)");

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

bool connectMqtt() {
  SerialMon.println("Attempting MQTT connection...");

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
      "msg-" + String(millis()) + "-" + String(++messageCount);

  JsonObject values = doc["values"].to<JsonObject>();

  // Demo values. Replace these with readings from your sensors.
  values["temperature"] = 24.5;
  values["humidity"] = 60.0;
  values["systemTemp"] = 25.0;
  values["NPK"] = 123;

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

  SerialMon.println("Publish failed");
  return false;
}