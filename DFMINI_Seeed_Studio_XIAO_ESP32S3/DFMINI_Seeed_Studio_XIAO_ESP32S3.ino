#include <DFPlayerMini_Fast.h>

#if !defined(UBRR1H)
#include <SoftwareSerial.h>
SoftwareSerial mySerial(10, 11);  // RX, TX
#endif
int a = 0;
bool _init1 = false;
DFPlayerMini_Fast myMP3;

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600, 44, 43);
#if !defined(UBRR1H)
  a =1;
  mySerial.begin(9600);
  _init1 = myMP3.begin(mySerial, true);

#else
  a = 2;
  Serial1.begin(9600);
  _init1 = myMP3.begin(Serial1, true);
#endif

  delay(1000);
  Serial.print(_init1);
  Serial.print(a);
  Serial.println();
  Serial.println("Setting volume to max");
  myMP3.volume(30);

  Serial.println("Looping track 1");
  myMP3.play(1);
}

void loop() {
  //do nothing
}