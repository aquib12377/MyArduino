#define IR_SENSOR 2
#define BUZZER 7

// How long eye must remain closed before alarm
const unsigned long SLEEP_TIME = 2000;  // 2 seconds

unsigned long eyeClosedStart = 0;
bool eyeClosed = false;

void setup()
{
  Serial.begin(9600);

  pinMode(IR_SENSOR, INPUT);
  pinMode(BUZZER, OUTPUT);

  // Buzzer OFF initially
  digitalWrite(BUZZER, LOW);

  Serial.println("Sleep Alert System Started...");
}

void loop()
{
  int irState = digitalRead(IR_SENSOR);

  // Active LOW IR sensor
  if (irState == LOW)
  {
    // Start timer when eye closure is first detected
    if (!eyeClosed)
    {
      eyeClosed = true;
      eyeClosedStart = millis();

      Serial.println("Eye Closed");
    }

    // Check how long the eye has remained closed
    if (millis() - eyeClosedStart >= SLEEP_TIME)
    {
      digitalWrite(BUZZER, HIGH);

      Serial.println("WARNING: SLEEP DETECTED!");
    }
  }
  else
  {
    // Eye opened again
    eyeClosed = false;
    eyeClosedStart = 0;

    digitalWrite(BUZZER, LOW);

    Serial.println("Eye Open");
  }

  delay(50);
}