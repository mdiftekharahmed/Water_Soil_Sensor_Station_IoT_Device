#define TRB_PIN 14

void setup() {
  Serial.begin(9600);
  pinMode(TRB_PIN, INPUT);
 
}
void loop() {
  int sensorValue = analogRead(A0);
  float voltage = sensorValue * (5.0 / 1024.0);
 
  Serial.println ("Sensor Output (V):");
  Serial.println (voltage);
  Serial.println();
  delay(1000);
}