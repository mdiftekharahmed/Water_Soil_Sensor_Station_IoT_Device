#define TINY_GSM_MODEM_SIM800
#define SerialMon Serial
#define SerialAT Serial1
#define TINY_GSM_DEBUG SerialMon
#define GSM_PIN ""

 #define GSM_RELAY_PIN 18

#define ADMIN_NUMBER "+8801776418720" // put your phone number with country code
#define TINY_GSM_DEBUG SerialMon

#include <TinyGsmClient.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

// ESP32 and SIM800l pins
#define MODEM_TX 33
#define MODEM_RX 32


String phoneNumber = "";
String text = "";


const char APN[] = "gpinternet";
const char GPRS_USER[] = "";
const char GPRS_PASS[] = "";

TinyGsmClient testClient(modem);

void setup() {
  SerialMon.begin(115200);
  delay(1000);

  pinMode(GSM_RELAY_PIN, OUTPUT);
  digitalWrite(GSM_RELAY_PIN, LOW);

  SerialMon.println("Wait ...");
  SerialAT.begin(9600, SERIAL_8N1, MODEM_TX, MODEM_RX);
  delay(3000);
  SerialMon.println("Initializing modem ...");
  modem.restart();


  if (GSM_PIN && modem.getSimStatus() != 3) {
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

  // SerialAT.print("AT+CNMI=2,2,0,0,0\r");
  // delay(1000);
  // String smsMessage = "Hello From ESP32";
  // Serial.println(smsMessage);
  // modem.sendSMS(ADMIN_NUMBER, smsMessage);


  SerialMon.println(" success");


  printSimInfo();  // ADDED
}

void loop() {

// SerialMon.println(" success");

// printSimInfo();  // ADDED


bool googleReachable = pingGoogleTcp();

if (googleReachable) {
  SerialMon.println("Internet test passed");
} else {
  SerialMon.println("Internet test failed");
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

  int signal = modem.getSignalQuality();  // 0–31; 99 means unknown
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
