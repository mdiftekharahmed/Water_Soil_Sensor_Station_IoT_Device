#define GSM_PIN 25

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);
  Serial.println("Setting GSM_Enable");
  pinMode(GSM_PIN, OUTPUT);
  digitalWrite(GSM_PIN, HIGH);
  Serial.println("GSM Enabled Successfully");
}

void loop() {
  // put your main code here, to run repeatedly:

}
