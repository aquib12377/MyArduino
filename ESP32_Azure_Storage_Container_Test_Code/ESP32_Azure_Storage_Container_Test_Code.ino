#include <WiFi.h>
#include <HTTPClient.h>

// Wi-Fi credentials
const char* WIFI_SSID     = "MyProject";
const char* WIFI_PASSWORD = "12345678";

// Example Azure Blob info
String baseBlobURL = "https://thermaldata.blob.core.windows.net";
String containerName = "thermal-data";
// Example SAS token
String sasToken = "?sv=2022-11-02&ss=bfqt&srt=co&sp=rwdlacupiytfx&se=2025-02-27T21:21:44Z&st=2025-01-26T13:21:44Z&spr=https,http&sig=UnJ0p7lMGPZFh4bJb5PAgNQbuimWfDRVzlqfW2ZDjTM%3D"; 

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected!");
}

void loop() {
  // Example: upload a small text or binary file
  String blobName = "test_upload.txt";
  
  // Full URL: https://<account>.blob.core.windows.net/my-container/test_upload.txt?sv=...&sig=...
  String url = baseBlobURL + "/" + containerName + "/" + blobName + sasToken;

  // Some data to upload
  String data = "Hello from ESP32 to Azure Blob!";
  
  HTTPClient http;
  http.begin(url);

  // For Azure BlockBlob, you must set "x-ms-blob-type" header:
  http.addHeader("x-ms-blob-type", "BlockBlob");
  // The content type, e.g. plain text
  http.addHeader("Content-Type", "text/plain");

  // Make a PUT request
  int statusCode = http.PUT((uint8_t*)data.c_str(), data.length());
  if (statusCode > 0) {
    Serial.print("HTTP status: ");
    Serial.println(statusCode);
    if (statusCode == 201 || statusCode == 200) {
      Serial.println("Upload success!");
    }
  } else {
    Serial.print("Request failed: ");
    Serial.println(http.errorToString(statusCode));
  }
  http.end();

  // Wait some time
  delay(30000);
}
