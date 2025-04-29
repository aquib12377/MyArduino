#include "esp_camera.h"
#include <WiFi.h>
#include <ESP_Mail_Client.h>
#include <WebServer.h>

// ---------- Camera Model Selection ----------
// For an AI Thinker board:
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// ---------- Wi-Fi Credentials ----------
const char* ssid     = "MyProject";
const char* password = "12345678";

// ---------- SMTP Email Settings ----------
// For example, using Gmail:
#define SMTP_HOST         "smtp.gmail.com"
#define SMTP_PORT         465
#define AUTHOR_EMAIL "aquibansari12377@gmail.com"
#define AUTHOR_PASSWORD "mmqe unbw dpqc xjmd"
#define RECIPIENT_EMAIL   "waghoorehan03@gmail.com"// The recipient email address

// ---------- Global Objects ----------
SMTPSession smtp;                    // Global SMTP session object
WebServer server(81);                // Web server running on port 81

// ---------- Function Prototypes ----------
void sendEmail(String ipAddress);
void startCameraServer();
void handleJPGStream();
  
void setup() {
  Serial.begin(115200);
  Serial.println();

  // ----- Connect to Wi-Fi -----
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
  
  // Retrieve the IP address as a string
  String ipAddress = WiFi.localIP().toString();
  Serial.print("IP Address: ");
  Serial.println(ipAddress);

  // ----- Send Email with the IP Address -----
  sendEmail(ipAddress);

  // ----- Camera Configuration -----
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0     = Y2_GPIO_NUM;
  config.pin_d1     = Y3_GPIO_NUM;
  config.pin_d2     = Y4_GPIO_NUM;
  config.pin_d3     = Y5_GPIO_NUM;
  config.pin_d4     = Y6_GPIO_NUM;
  config.pin_d5     = Y7_GPIO_NUM;
  config.pin_d6     = Y8_GPIO_NUM;
  config.pin_d7     = Y9_GPIO_NUM;
  config.pin_xclk   = XCLK_GPIO_NUM;
  config.pin_pclk   = PCLK_GPIO_NUM;
  config.pin_vsync  = VSYNC_GPIO_NUM;
  config.pin_href   = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn   = PWDN_GPIO_NUM;
  config.pin_reset  = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;  // JPEG format for streaming

  // Use PSRAM if available (for better frame buffering)
  if (psramFound()) {
    config.frame_size = FRAMESIZE_QVGA;  // Lower resolution for smoother streaming
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  // Initialize the camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // ----- Start the Camera Web Server -----
  startCameraServer();
  Serial.print("Camera Stream Ready! Use 'http://");
  Serial.print(ipAddress);
  Serial.println(":81' to connect");
}

void loop() {
  // Process incoming client requests
  server.handleClient();

  // Optional: check Wi-Fi connectivity and try to reconnect if needed
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi lost! Attempting to reconnect...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    delay(5000);
  }
}

// ---------- Function Definitions ----------

// Sends an email containing the live stream URL with the device's IP address
void sendEmail(String ipAddress) {
  smtp.debug(1);
  
  // Set up SMTP session configuration
  ESP_Mail_Session session;
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "";

  // Compose the email message
  SMTP_Message message;
  message.sender.name = "ESP32-CAM";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "ESP32-CAM Live Stream IP Address";
  message.addRecipient("User", RECIPIENT_EMAIL);

  String emailContent = "ESP32-CAM is online!\nAccess the live stream at:\n";
  emailContent += "http://" + ipAddress + "\n\n";
  emailContent += "Enjoy!";
  
  message.text.content = emailContent.c_str();
  message.text.charSet = "us-ascii";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  
  // Connect to the SMTP server and send the email
  if (!smtp.connect(&session)) {
    Serial.printf("Connection error, Status Code: %d, Error Code: %d, Reason: %s\n", 
                  smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return;
  }

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Failed to send email!");
  } else {
    Serial.println("Email sent successfully!");
  }
  
  smtp.sendingResult.clear();
}
