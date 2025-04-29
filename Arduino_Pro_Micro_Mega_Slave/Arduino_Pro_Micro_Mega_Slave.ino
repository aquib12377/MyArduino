/* ------------  Pro Micro  –  I²C-to-USB Keyboard (25 keys) -------------
   • Listens on I²C address 0x08.
   • Expects button indices 0 … 24 from the Mega.
   • Sends the USB-HID keystroke ‘a’ for 0, ‘b’ for 1 … ‘y’ for 24.
   ----------------------------------------------------------------------- */

#include <Wire.h>
#include <Keyboard.h>

const uint8_t I2C_ADDR = 0x08;     // must match the master (Mega)

/* -----------------------------------------------------------------------
   ISR: runs when the Mega transmits data. We grab the first byte and
   stash it in a volatile variable so the main loop can act on it.
   ----------------------------------------------------------------------- */
volatile int8_t pendingIdx = -1;

void receiveEvent(int nBytes)
{
  if (nBytes < 1) return;
  uint8_t idx = Wire.read();       // read first byte
  if (idx < 25)                    // 0-24 only
    pendingIdx = idx;              // hand to main loop
}

void setup()
{
  Wire.begin(I2C_ADDR);            // become I²C slave
  Wire.onReceive(receiveEvent);

  Keyboard.begin();                // USB HID
  Serial.begin(115200);
  while (!Serial);                 // wait for native USB
  Serial.println(F("Pro Micro ready - mapping 0-24 to 'a'-'y'"));
}

void loop()
{
  static int8_t lastIdx = -1;

  if (pendingIdx != -1 && pendingIdx != lastIdx)
  {
    lastIdx = pendingIdx;          // remember we handled it
    char key = 'a' + pendingIdx;   // 0→a, 1→b, … 24→y

    Keyboard.write(key);           // imitate keypress
    Keyboard.releaseAll();         // safety

    Serial.print(F("Sent key: "));
    Serial.println(key);

    pendingIdx = -1;               // clear flag
  }
}
