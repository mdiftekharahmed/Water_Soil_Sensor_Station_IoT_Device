#define TRB_PIN 34
#define OTHER_RELAY_PIN 25
void setup() {
  Serial.begin(115200);
  pinMode(TRB_PIN, INPUT);

  pinMode(OTHER_RELAY_PIN, OUTPUT);
  digitalWrite(OTHER_RELAY_PIN, LOW);
}
void loop() {

  
  int sensorValue = analogRead(TRB_PIN);
  float voltage = sensorValue * (5.0 / 1024.0);
 
  Serial.println ("Sensor Output (V):");
  Serial.println (voltage);
  Serial.println();
  delay(1000);
}