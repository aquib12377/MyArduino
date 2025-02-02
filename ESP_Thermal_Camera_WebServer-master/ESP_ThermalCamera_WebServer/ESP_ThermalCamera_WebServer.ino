/********************************************************
    Includes
*********************************************************/
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include "SPIFFS.h"

// MLX90640 API includes
#include "MLX90640_I2C_Driver.h"
#include "MLX90640_API.h"

/********************************************************
    Azure Blob Info
*********************************************************/
String baseBlobURL   = "https://thermaldata.blob.core.windows.net";
String containerName = "thermal-data";
// Example SAS token (NOTE: This is just an example)
String sasToken = "?sv=2022-11-02&ss=bfqt&srt=co&sp=rwdlacupiytfx&se=2025-02-27T21:21:44Z&st=2025-01-26T13:21:44Z&spr=https,http&sig=UnJ0p7lMGPZFh4bJb5PAgNQbuimWfDRVzlqfW2ZDjTM%3D";

/********************************************************
    WiFi Credentials
*********************************************************/
const char* WIFI_SSID     = "MyProject";
const char* WIFI_PASSWORD = "12345678";

/********************************************************
    MLX90640 Defines & Variables
*********************************************************/
const byte MLX90640_address = 0x33; // Default 7-bit address of the MLX90640
#define TA_SHIFT 8  // Default shift for MLX90640 in open air

float mlx90640To[768];
paramsMLX90640 mlx90640;

float MaxTemp = 0;
float MinTemp = 0;
float CenterTemp = 0;

/********************************************************
    Color Map
*********************************************************/
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

const uint16_t camColors[] = {
  0x480F, 0x400F, 0x400F, 0x400F, 0x4010, 0x3810, 0x3810, 
  /* ... truncated for brevity ... */
  0xF800
};

/********************************************************
    Utility Functions
*********************************************************/

// Check if MLX90640 is connected on I2C
boolean isConnected()
{
  Wire.beginTransmission((uint8_t)MLX90640_address);
  return (Wire.endTransmission() == 0); // true if ACK
}

/**
 * Generate /thermal.bmp in SPIFFS from the float[] data
 * with the known min/max. 
 */
void ThermalImageToSPIFFS(float* mlx90640To, float minT, float maxT)
{
  const int WIDTH  = 32;
  const int HEIGHT = 24;

  // BMP Padding info
  int extrabytes = (4 - ((WIDTH * 3) % 4)) % 4;
  int paddedsize = ((WIDTH * 3) + extrabytes) * HEIGHT;
  uint32_t headers[13];

  // Prepare BMP headers
  headers[0]  = paddedsize + 54;  // bfSize (whole file size)
  headers[1]  = 0;                // bfReserved
  headers[2]  = 54;               // bfOffBits
  headers[3]  = 40;               // biSize
  headers[4]  = WIDTH;            // biWidth
  headers[5]  = HEIGHT;           // biHeight
  headers[7]  = 0;                // biCompression
  headers[8]  = paddedsize;       // biSizeImage
  headers[9]  = 0;                // biXPelsPerMeter
  headers[10] = 0;                // biYPelsPerMeter
  headers[11] = 0;                // biClrUsed
  headers[12] = 0;                // biClrImportant

  // Open file for writing in SPIFFS (always overwriting the same name here)
  File file = SPIFFS.open("/thermal.bmp", "wb");
  if(!file){
    Serial.println("Error opening /thermal.bmp for writing!");
    return;
  }

  // Write BMP header
  file.print("BM");
  for (int i = 0; i <= 5; i++) {
    file.write((uint8_t)(headers[i] & 0xFF));
    file.write((uint8_t)((headers[i] >> 8) & 0xFF));
    file.write((uint8_t)((headers[i] >> 16) & 0xFF));
    file.write((uint8_t)((headers[i] >> 24) & 0xFF));
  }
  // biPlanes (2 bytes) + biBitCount (2 bytes):
  file.write((uint8_t)1);
  file.write((uint8_t)0);
  file.write((uint8_t)24);
  file.write((uint8_t)0);

  for (int i = 7; i <= 12; i++) {
    file.write((uint8_t)(headers[i] & 0xFF));
    file.write((uint8_t)((headers[i] >> 8) & 0xFF));
    file.write((uint8_t)((headers[i] >> 16) & 0xFF));
    file.write((uint8_t)((headers[i] >> 24) & 0xFF));
  }

  // Write pixel data (bottom to top)
  for (int row = HEIGHT - 1; row >= 0; row--) {
    for (int col = 0; col < WIDTH; col++) {
      int pixelIndex = col + (row * WIDTH);
      // map the temperature to 0..255
      uint8_t colorIndex = map(mlx90640To[pixelIndex],
                               minT - 5.0,  // margin
                               maxT + 5.0,
                               0, 255);
      colorIndex = constrain(colorIndex, 0, 255);
      uint16_t color = camColors[colorIndex];

      // Convert 16-bit color to 24-bit (B, G, R)
      int red   = ((((color >> 11) & 0x1F) * 527) + 23) >> 6;
      int green = ((((color >> 5)  & 0x3F) * 259) + 33) >> 6;
      int blue  = ((((color)       & 0x1F) * 527) + 23) >> 6;

      file.write((uint8_t)blue);
      file.write((uint8_t)green);
      file.write((uint8_t)red);
    }
    // padding
    for (int n = 0; n < extrabytes; n++){
      file.write((uint8_t)0);
    }
  }

  file.close();
}

/********************************************************
    Capture & Upload Function
*********************************************************/

/**
 * Capture one frame from MLX90640, save to SPIFFS as
 * '/thermal.bmp', then upload to Azure with a unique
 * blob name (using 'photoIndex').
 */
void captureAndUploadImage(unsigned long photoIndex)
{
  // 1) Read both subpages for better data
  uint16_t mlx90640Frame[834];
  for (uint8_t x = 0; x < 2; x++){
    int status = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
    if (status < 0){
      Serial.print("MLX90640_GetFrameData error: ");
      Serial.println(status);
      return;
    }
    float vdd = MLX90640_GetVdd(mlx90640Frame, &mlx90640); // Not used here, but available
    float Ta  = MLX90640_GetTa(mlx90640Frame, &mlx90640);
    float tr = Ta - TA_SHIFT; // reflected temperature
    float emissivity = 0.95;
    MLX90640_CalculateTo(mlx90640Frame, &mlx90640, emissivity, tr, mlx90640To);
  }

  // 2) Find min, max, and center temperature for debugging/logging
  CenterTemp = (mlx90640To[367] + mlx90640To[368]
              + mlx90640To[399] + mlx90640To[400]) / 4.0;
  MaxTemp = mlx90640To[0];
  MinTemp = mlx90640To[0];
  for (int i = 1; i < 768; i++){
    if (mlx90640To[i] > MaxTemp) MaxTemp = mlx90640To[i];
    if (mlx90640To[i] < MinTemp) MinTemp = mlx90640To[i];
  }
  Serial.printf("[Photo %lu] Center=%.2f  Min=%.2f  Max=%.2f\n", photoIndex, CenterTemp, MinTemp, MaxTemp);

  // 3) Save BMP to SPIFFS
  ThermalImageToSPIFFS(mlx90640To, MinTemp, MaxTemp);

  // 4) Upload to Azure Blob Storage with a unique name
  // Example: "thermal_0.bmp", "thermal_1.bmp", etc.
  String blobName = "thermal_" + String(photoIndex) + ".bmp";
  String url = baseBlobURL + "/" + containerName + "/" + blobName + sasToken;

  // Read the BMP from SPIFFS
  File file = SPIFFS.open("/thermal.bmp", "rb");
  if(!file){
    Serial.println("Failed to open /thermal.bmp for reading!");
    return;
  }

  size_t fileSize = file.size();
  uint8_t* fileBuf = (uint8_t*)malloc(fileSize);
  if(!fileBuf){
    Serial.println("Memory alloc failed for file buffer.");
    file.close();
    return;
  }
  file.read(fileBuf, fileSize);
  file.close();

  HTTPClient http;
  http.begin(url);
  http.addHeader("x-ms-blob-type", "BlockBlob");  // Azure requirement
  http.addHeader("Content-Type", "image/bmp");    // We'll label it BMP

  Serial.printf("Uploading %s to Azure...\n", blobName.c_str());
  int statusCode = http.PUT(fileBuf, fileSize);
  free(fileBuf);
  http.end();

  if (statusCode > 0){
    Serial.print("Azure PUT status: ");
    Serial.println(statusCode);
    if (statusCode == 201 || statusCode == 200){
      Serial.println("Upload success!");
    } else {
      Serial.println("Upload failed with code " + String(statusCode));
    }
  } else {
    Serial.print("HTTP PUT failed: ");
    Serial.println(http.errorToString(statusCode));
  }
}

/********************************************************
    Setup
*********************************************************/
void setup()
{
  Serial.begin(115200);

  // 1) Initialize I2C for MLX90640
  Wire.begin();
  Wire.setClock(400000);

  // 2) Initialize SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("SPIFFS Mount Failed");
    while(1);
  }

  // 3) Connect to WiFi in Station Mode
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ");
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // 4) Check MLX90640 presence
  if (!isConnected()){
    Serial.println("MLX90640 not detected. Stopping.");
    while(1);
  }
  Serial.println("MLX90640 found!");

  // 5) Extract parameters from MLX90640
  uint16_t eeMLX90640[832];
  int status = MLX90640_DumpEE(MLX90640_address, eeMLX90640);
  if(status != 0)
    Serial.println("Failed to load system parameters (MLX90640)");

  status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
  if(status != 0)
    Serial.println("Parameter extraction failed");

  // Example: set refresh rate to 4 Hz
  // MLX90640_SetRefreshRate(MLX90640_address, 0x03);

  // Example: Set Chess Mode (or Interleaved)
  MLX90640_SetChessMode(MLX90640_address);
}

/********************************************************
    Main Loop
*********************************************************/

void loop()
{
  static unsigned long photoIndex = 0;

  // -- 1st Session: 4 photos, each 10s apart
  for(int i = 0; i < 4; i++){
    captureAndUploadImage(photoIndex++);
    delay(10 * 1000); // 10 seconds
  }
  // Wait 1 minute
  delay(60 * 1000);

  // -- 2nd Session: 4 photos, each 10s apart
  for(int i = 0; i < 4; i++){
    captureAndUploadImage(photoIndex++);
    delay(10 * 1000); // 10 seconds
  }
  // Wait 1 minute
  delay(60 * 1000);
}
