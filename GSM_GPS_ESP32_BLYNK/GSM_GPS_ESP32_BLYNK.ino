/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL3JEqGaPle"
#define BLYNK_TEMPLATE_NAME "GPS TRACKER"
#define BLYNK_AUTH_TOKEN "2HcLagL0GpDs_QkZNYOZhjgaacXs9S8g"
// Select your modem:
#define TINY_GSM_MODEM_SIM800
//#define TINY_GSM_MODEM_SIM900
//#define TINY_GSM_MODEM_M590
//#define TINY_GSM_MODEM_A6
//#define TINY_GSM_MODEM_A7
//#define TINY_GSM_MODEM_BG96
//#define TINY_GSM_MODEM_XBEE

// Default heartbeat interval for GSM is 60
// If you want override this value, uncomment and set this option:
//#define BLYNK_HEARTBEAT 30

#include <TinyGPS++.h> //https://github.com/mikalhart/TinyGPSPlus
#include <TinyGsmClient.h> //https://github.com/vshymanskyy/TinyGSM
#include <BlynkSimpleTinyGSM.h> //https://github.com/blynkkk/blynk-library

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "U-DRO1-Dz_4Vn2qUee_BYk2HDeini8wS";

// Your GPRS credentials
// Leave empty, if missing user or pass
char apn[]  = "www";
char user[] = "";
char pass[] = "";


//sender phone number with country code.
//not gsm module phone number
//const String PHONE = "Enter_Your_Phone_Number";

//GSM Module Settings
//GSM Module RX pin to ESP32 2
//GSM Module TX pin to ESP32 4
#define rxPin 4
#define txPin 2
HardwareSerial sim800(1);
TinyGsm modem(sim800);

//GPS Module Settings
//GPS Module RX pin to ESP32 17
//GPS Module TX pin to ESP32 16
#define RXD2 16
#define TXD2 17
HardwareSerial neogps(2);
TinyGPSPlus gps;

WidgetMap myMap(V0);
BlynkTimer timer;
int pointIndex = 0;

void setup() {
   
  //Set Serial monitor baud rate
  Serial.begin(115200);
  Serial.println("esp32 serial initialize");
  delay(10);
  
  //Set GPS module baud rate
  neogps.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.println("neogps serial initialize");
  delay(10);

  //Set GSM module baud rate
  sim800.begin(115200, SERIAL_8N1, rxPin, txPin);
  Serial.println("SIM800L serial initialize");
  delay(3000); 
   
  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  Serial.println("Initializing modem...");
  modem.restart();

  // Unlock your SIM card with a PIN
  //modem.simUnlock("1234");

  Blynk.begin(BLYNK_AUTH_TOKEN, modem, apn, user, pass);
  //timer.setInterval(5000L, sendToBlynk);
}

void loop() {
  
  while(neogps.available())
  {
    if (gps.encode(neogps.read()))
    {
      sendToBlynk();
    }
  }

  Blynk.run();
  //timer.run();
  
} //main loop ends

void sendToBlynk()
{

  if (gps.location.isValid() )
  {
    //get latitude and longitude
    String latitude = String(gps.location.lat(),6);
    String longitude = String(gps.location.lng(),6);
    //get
    String speed = String(gps.speed.kmph());
    //get number of satellites
    String satellites = String(gps.satellites.value(),6);
    
    Serial.print("Latitude:  ");
    Serial.println(latitude);
    Serial.print("Longitude: ");
    Serial.println(longitude);
    Serial.print("Speed: ");
    Serial.println(speed); 
    Serial.print("Sattelite: ");
    Serial.println(satellites); 

    
    Blynk.virtualWrite(V1, latitude);
    Blynk.virtualWrite(V2, longitude);

Blynk.virtualWrite(V0,(gps.location.lat(),6),(gps.location.lng(),6));

    //myMap.location(pointIndex, , gps.location.lng(), "GPS_Location");
    
    Blynk.virtualWrite(V3, speed);
    Blynk.virtualWrite(V5, satellites);

  }
}