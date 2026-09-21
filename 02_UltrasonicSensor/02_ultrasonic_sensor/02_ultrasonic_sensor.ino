// #define TRIG_PIN 21
// #define ECHO_PIN 19
// #define USONIC_DIV 58.0
// #define MEASURE_SAMPLE_DELAY 5
// #define MEASURE_SAMPLES 20
// #define MEASURE_DELAY 25

// // Function Definition
//   long singleMeasurement();


// void setup() {
//   // put your setup code here, to run once:
//   Serial.begin(9600);
//   pinMode(TRIG_PIN, OUTPUT);
//   pinMode(ECHO_PIN, INPUT);
//   digitalWrite(TRIG_PIN, LOW);
//   delayMicroseconds(500);
// }

// void loop() {
//   delay(MEASURE_DELAY);
//   long distance= measureHight();
//   Serial.print("Distance: ");
//   Serial.print(distance+22);
//   Serial.println("mm");
//   // put your main code here, to run repeatedly:

// }


// long measureHight(){
//   long measureSum = 0;
//   for(int i=0;i<MEASURE_SAMPLES;i++){
//     delay(MEASURE_SAMPLE_DELAY);
//     measureSum += singleMeasurement();
//   }
//   return measureSum / MEASURE_SAMPLES;
// }

// long singleMeasurement(){
//   long duration = 0;
//   digitalWrite(TRIG_PIN, HIGH);
//   delayMicroseconds(11);
//   digitalWrite(TRIG_PIN, LOW);
//   duration = pulseIn(ECHO_PIN, HIGH);

//   return (long)(((float)duration / USONIC_DIV)*10.0);
// }




const int trigPin = 26;
const int echoPin = 34;
const float speedOfSound = 0.0343;

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  digitalWrite(trigPin, LOW);
  Serial.printf("Starting measurements...\n\n");
}

void loop() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(5);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(20);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);

  if (duration == 0) {
    Serial.printf("Out of range or no echo received\n");
  } else {
    float distance = (duration * speedOfSound) / 2.0;

    if (distance < 20.0 || distance > 600.0) {
      Serial.printf("Unreliable reading: %.1f cm (valid range: 20-600 cm)\n", distance);
    } else {
      Serial.printf("Distance: %.1f cm\n", distance);
    }
  }
  delay(200);
}