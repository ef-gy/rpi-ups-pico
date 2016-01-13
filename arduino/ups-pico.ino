int target = 12;

void setup() {                
  pinMode(target, OUTPUT);     
}

void loop() {
  digitalWrite(target, HIGH);
  delay(250);
  digitalWrite(target, LOW);
  delay(250);
}
