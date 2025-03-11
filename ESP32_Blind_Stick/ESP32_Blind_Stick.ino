#include <HardwareSerial.h>
#include <BluetoothSerial.h>

// Pin Definitions
#define TRIG_PIN 13
#define ECHO_PIN 12
#define BUZZER_PIN 33
#define RAIN_SENSOR_PIN 34

// GPS Module on Serial2
HardwareSerial gpsSerial(2);
BluetoothSerial SerialBT;

// Hardcoded Location Link (if GPS fails)
const char* HARD_CODED_LINK = "https://www.google.com/maps?q=28.7041,77.1025"; // Example location

void setup() {
    Serial.begin(115200);
    gpsSerial.begin(9600, SERIAL_8N1, 16, 17);  // GPS on Serial2 (RX=16, TX=17)
    SerialBT.begin("Blind Stick ESP32");        // Start Bluetooth

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(RAIN_SENSOR_PIN, INPUT_PULLUP);

    Serial.println("Setup complete. System ready.");
}

void loop() {
    Serial.println("Checking sensors...");
    checkObstacle();
    checkRain();
    sendLocation();
    delay(2000);  // Adjust as needed
}

// Function to check obstacles using Ultrasonic Sensor
void checkObstacle() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH);
    int distance = duration * 0.034 / 2;  // Convert to cm

    Serial.print("Obstacle Distance: ");
    Serial.print(distance);
    Serial.println(" cm");

    if (distance > 0 && distance <= 50) {  // If obstacle within 50cm
        Serial.println("Obstacle detected! Beeping...");
        beepBuzzer(200, 3);  // Short beep pattern
    }
}

// Function to check Rain Sensor
void checkRain() {
    int rainDetected = digitalRead(RAIN_SENSOR_PIN);
    Serial.print("Rain Sensor State: ");
    Serial.println(rainDetected);

    if (rainDetected == HIGH) {  // If rain detected
        Serial.println("Rain detected! Beeping...");
        beepBuzzer(500, 2);  // Long beep pattern
    }
}

// Function to beep the buzzer differently for obstacles & rain
void beepBuzzer(int duration, int count) {
    for (int i = 0; i < count; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(duration);
        digitalWrite(BUZZER_PIN, LOW);
        delay(duration);
    }
}

// Function to send GPS data via Bluetooth
void sendLocation() {
    String gpsData = readGPS();
    if (gpsData == "") {
        Serial.println("GPS signal not available. Sending hardcoded location.");
        SerialBT.println("Location: " + String(HARD_CODED_LINK));  // Send hardcoded link
    } else {
        Serial.println("Sending GPS location: " + gpsData);
        SerialBT.println("Location: " + gpsData);  // Send real-time GPS link
    }
}

// Function to read GPS data
String readGPS() {
    String gpsBuffer = "";
    while (gpsSerial.available()) {
        char c = gpsSerial.read();
        gpsBuffer += c;
    }

    Serial.print("Raw GPS Data: ");
    Serial.println(gpsBuffer);

    if (gpsBuffer.indexOf("$GPGGA") != -1) {  // Check for valid GPS data
        float latitude = extractCoordinate(gpsBuffer, "lat");
        float longitude = extractCoordinate(gpsBuffer, "lon");

        if (latitude != 0.0 && longitude != 0.0) {
            String gpsLink = "https://www.google.com/maps?q=" + String(latitude, 6) + "," + String(longitude, 6);
            Serial.println("Extracted GPS Location: " + gpsLink);
            return gpsLink;
        }
    }
    return "";  // Return empty if no valid GPS data
}

// Function to extract coordinates from GPS NMEA sentence
float extractCoordinate(String nmea, String type) {
    int startIndex = nmea.indexOf("$GPGGA");
    if (startIndex == -1) return 0.0;

    int commas[15];  // Stores positions of commas
    int count = 0;
    for (int i = startIndex; i < nmea.length(); i++) {
        if (nmea[i] == ',') {
            commas[count++] = i;
            if (count >= 15) break;
        }
    }

    if (type == "lat") {
        String latRaw = nmea.substring(commas[1] + 1, commas[2]);
        float lat = latRaw.toFloat();
        return convertToDecimal(lat, nmea.substring(commas[2] + 1, commas[3]));
    }

    if (type == "lon") {
        String lonRaw = nmea.substring(commas[3] + 1, commas[4]);
        float lon = lonRaw.toFloat();
        return convertToDecimal(lon, nmea.substring(commas[4] + 1, commas[5]));
    }

    return 0.0;
}

// Function to convert GPS coordinates to decimal format
float convertToDecimal(float coordinate, String direction) {
    int degrees = (int)(coordinate / 100);
    float minutes = coordinate - (degrees * 100);
    float decimal = degrees + (minutes / 60.0);

    if (direction == "S" || direction == "W") decimal *= -1;
    return decimal;
}
