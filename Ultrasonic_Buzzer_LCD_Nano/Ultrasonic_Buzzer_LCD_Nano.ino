#include <HCSR04.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd (0x27,16,2);
HCSR04 hc(3, 4); //(trig pin , echo pin)

#define BUZZER 2

void setup() {
  Serial.begin(9600);
  Serial.println("[START] Project Started...");

  pinMode(BUZZER,OUTPUT);
  digitalWrite(BUZZER,LOW);

  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Motion");
  lcd.setCursor(0,1);
  lcd.print("Detection");
  delay(3000);
  lcd.clear();
}

void loop() {
  float dist = hc.dist();
  Serial.print("[RUN] Distance: ");
  Serial.println(dist,2);
  lcd.setCursor(0, 0);
  lcd.print("No Motion");
  if(dist < 50 && dist != 0 && dist > 0)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Motion Detected");
    beepBuzzer(3);
    lcd.clear();
  }
  else{
    beepBuzzer(0);
  }

}

void beepBuzzer(int n)
{
  if(n <= 0) {digitalWrite(BUZZER,LOW);return;}
  for(int i = 0; i < n; i++)
  {
    digitalWrite(BUZZER,HIGH);
    delay(200);
    digitalWrite(BUZZER,LOW);
    delay(200);
  }
}
