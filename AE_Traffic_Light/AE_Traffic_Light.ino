// Pins: R=8, Y=9, G=10 (active HIGH)
const int R=13, Y=12, G=11;

void setup(){ pinMode(R,OUTPUT); pinMode(Y,OUTPUT); pinMode(G,OUTPUT); }

void loop(){
  digitalWrite(G,HIGH); delay(3000);
  digitalWrite(G,LOW);  digitalWrite(Y,HIGH); delay(1000);
  digitalWrite(Y,LOW);  digitalWrite(R,HIGH); delay(3000);
  digitalWrite(R,LOW);  delay(500);
}
