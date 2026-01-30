 /*
* ===============================================================================
* ESP32 ENHANCED VOICE CONTROL SYSTEM WITH EDGE IMPULSE
* ===============================================================================
* 
* Author: CircuitDigest/RithikKrisna[me_RK]
* Date: August 2025
* 
* DESCRIPTION:
* Advanced voice-controlled LED system using ESP32 microcontroller with Edge Impulse
* machine learning for wake word detection and command recognition. Features dual
* confidence thresholds for reliable command execution and enhanced user feedback.
* 
* FEATURES:
* --------
* • Wake Word Detection: "marvin" activates listening mode
* • Voice Commands: "on" and "off" to control LED
* • Dual Confidence System:
*   - 80% threshold for reliable command execution
*   - 50% threshold for recognition feedback
* • Visual Feedback System:
*   - Control LED (Pin 22): Main device being controlled
*   - Indicator LED (Pin 23): Status and pattern feedback
* • Listening Window: 10-second timeout after wake word
* • Real-time Audio Processing: Continuous inference with I2S microphone
* 
* LED PATTERNS:
* ------------
* • Wake Word ("marvin"): 2 quick pulses + steady listening indicator
* • Turn On Command: Triple flash + confirmation
* • Turn Off Command: Fade pattern + confirmation  
* • General Recognition: Single quick blink for detected words
* • Listening Mode: Steady indicator light
* 
* HARDWARE REQUIREMENTS:
* ---------------------
* • ESP32 Development Board
* • I2S Digital Microphone (INMP441 or similar)
* • 2x LEDs with appropriate resistors
* • Breadboard and connecting wires
* 
* PIN CONFIGURATION:
* -----------------
* • I2S Microphone:
*   - BCK (Bit Clock): Pin 26
*   - WS (Word Select): Pin 25  
*   - Data In: Pin 33
* • LEDs:
*   - Control LED: Pin 22
*   - Indicator LED: Pin 23 (optional, set to -1 to disable)
* 
* SOFTWARE DEPENDENCIES:
* ---------------------
* • Edge Impulse Arduino Library
* • ESP32 Arduino Core
* • FreeRTOS (included with ESP32 core)
* 
* USAGE:
* ------
* 1. Say "marvin" to activate listening mode (indicator LED turns on)
* 2. Within 10 seconds, say "on" or "off" to control the main LED
* 3. System provides visual feedback for all recognized words
* 
* ===============================================================================
*/
// ===============================================================================
// PREPROCESSOR DIRECTIVES AND MEMORY OPTIMIZATION
// ===============================================================================
// Memory optimization: Remove this macro to save 10K RAM if target is memory-limited
#define EIDSP_QUANTIZE_FILTERBANK 0
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
/*
** MEMORY ALLOCATION NOTE:
** If you encounter TFLite arena allocation issues due to dynamic memory fragmentation,
** try defining "-DEI_CLASSIFIER_ALLOCATION_STATIC" in boards.local.txt and copy to:
** <ARDUINO_CORE_INSTALL_PATH>/arduino/hardware/<mbed_core>/<core_version>/
** 
** Reference: https://support.arduino.cc/hc/en-us/articles/360012076960-Where-are-the…-
** If issues persist, insufficient memory for this model and application.
*/
// ===============================================================================
// SYSTEM INCLUDES
// ===============================================================================
#include <Voice_recognizer_inferencing.h>  // Edge Impulse generated library
#include "freertos/FreeRTOS.h"             // Real-time operating system
#include "freertos/task.h"                 // Task management
#include "driver/i2s.h"                    // I2S audio driver
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"

const char* SUPABASE_URL = "https://yefaixpcfkzifhvopxlv.supabase.co";
const char* SUPABASE_ANON_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InllZmFpeHBjZmt6aWZodm9weGx2Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NjMzNzMzNzYsImV4cCI6MjA3ODk0OTM3Nn0.JuMqfYO3HbLq4WeJ5LDfZlU0qeGxBP86iFHyIvrN-oY";
const char* BUCKET_NAME = "images";

// ================== YOUR WIFI ==================
const char* WIFI_SSID = "AAA";
const char* WIFI_PASS = "Acube123";
// ===============================================================================
// AUDIO PROCESSING DATA STRUCTURES
// ===============================================================================
/** 
* Audio inference buffer structure
* Manages circular buffer for real-time audio processing
*/
typedef struct {
  int16_t* buffer;     // Audio sample buffer
  uint8_t buf_ready;   // Buffer ready flag
  uint32_t buf_count;  // Current buffer position
  uint32_t n_samples;  // Total number of samples
} inference_t;
// ===============================================================================
// SYSTEM CONFIGURATION CONSTANTS
// ===============================================================================
// AI Model Confidence Thresholds
const float COMMAND_CONFIDENCE_THRESHOLD = 0.80;      // 80% for reliable command execution
const float RECOGNITION_CONFIDENCE_THRESHOLD = 0.50;  // 50% for recognition feedback
// Timing Configuration
const unsigned long LISTENING_DURATION_MS = 10000;  // 10-second listening window after wake word
// Hardware Pin Configuration
const int CONTROL_LED_PIN = 22;    // Main LED being controlled
const int INDICATOR_LED_PIN = 23;  // Status indicator LED (-1 to disable)
// Audio Processing Configuration
static const uint32_t sample_buffer_size = 2048;  // I2S sample buffer size
// ===============================================================================
// GLOBAL STATE VARIABLES
// ===============================================================================
// Wake Word System State
static bool wake_word_detected = false;        // Wake word detection flag
static unsigned long wake_word_timestamp = 0;  // Timestamp of last wake word
static bool listening_mode = false;            // Active listening state
static bool led_state = false;                 // Current LED state
// Audio Processing State
static inference_t inference;                          // Audio inference structure
static signed short sampleBuffer[sample_buffer_size];  // Raw audio sample buffer
static bool debug_nn = false;                          // Neural network debug output
static bool record_status = true;                      // Recording status flag
// ===============================================================================
// LED VISUAL FEEDBACK SYSTEM
// ===============================================================================

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;      // 20 MHz for smooth FPS
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;  // 640x480
    config.jpeg_quality = 10;           // better quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;  // 320x240
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }

  Serial.println("Camera initialized!");
  return true;
}

// ============ UPLOAD ONE FRAME TO SUPABASE ============
bool uploadFrameToSupabase(camera_fb_t* fb) {
  if (!fb) {
    Serial.println("Frame buffer is NULL");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();  // skip certificate verification (simple & works)

  HTTPClient http;

  // Unique file name
  String fileName = "esp32cam_" + String(millis()) + ".jpg";

  // Storage API endpoint:
  // POST https://<project>.supabase.co/storage/v1/object/<bucket>/<filename>
  String url = String(SUPABASE_URL) + "/storage/v1/object/" + BUCKET_NAME + "/" + fileName;

  Serial.print("Uploading to URL: ");
  Serial.println(url);

  if (!http.begin(client, url)) {
    Serial.println("HTTP begin failed");
    return false;
  }

  // Headers
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
  http.addHeader("Content-Type", "image/jpeg");
  http.addHeader("x-upsert", "true");  // overwrite if same name (optional)

  // Send raw JPEG buffer
  int httpCode = http.sendRequest("POST", fb->buf, fb->len);

  Serial.printf("HTTP response code: %d\n", httpCode);

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Response payload:");
    Serial.println(payload);
  }

  http.end();

  return (httpCode == 200 || httpCode == 201);
}

/**
* @brief Create wake word detection pattern
* Shows 2 quick pulses to indicate "marvin" was detected
*/
void show_wake_word_pattern() {
  if (INDICATOR_LED_PIN == -1) return;
  // Wake pattern - 2 quick pulses to indicate wake word detected
  for (int cycle = 0; cycle < 2; cycle++) {
    digitalWrite(INDICATOR_LED_PIN, HIGH);
    delay(150);
    digitalWrite(INDICATOR_LED_PIN, LOW);
    delay(100);
  }
}
/**
* @brief Activate listening mode indicator
* Shows steady light during command listening period
*/
void show_listening_mode() {
  if (INDICATOR_LED_PIN == -1) return;
  digitalWrite(INDICATOR_LED_PIN, HIGH);
}
/**
* @brief Show "turning on" confirmation pattern
* Triple flash followed by confirmation sequence
*/
void show_turning_on_pattern() {
  if (INDICATOR_LED_PIN == -1) return;
  // Quick triple flash then steady
  for (int i = 0; i < 1; i++) {
    digitalWrite(INDICATOR_LED_PIN, HIGH);
    delay(100);
    digitalWrite(INDICATOR_LED_PIN, LOW);
    delay(100);
  }
  // Brief pause then steady on for confirmation
  delay(200);
  digitalWrite(INDICATOR_LED_PIN, HIGH);
  delay(800);
  digitalWrite(INDICATOR_LED_PIN, LOW);
}
/**
* @brief Show "turning off" confirmation pattern
* Fade-like pattern to indicate device turning off
*/
void show_turning_off_pattern() {
  if (INDICATOR_LED_PIN == -1) return;
  // Start bright, then fade to off with pauses
  digitalWrite(INDICATOR_LED_PIN, HIGH);
  delay(300);
  digitalWrite(INDICATOR_LED_PIN, LOW);
  delay(150);
  digitalWrite(INDICATOR_LED_PIN, HIGH);
  delay(200);
  digitalWrite(INDICATOR_LED_PIN, LOW);
  delay(150);
  digitalWrite(INDICATOR_LED_PIN, HIGH);
  delay(100);
  digitalWrite(INDICATOR_LED_PIN, LOW);
  // Final confirmation that it's off
  delay(500);
}
/**
* @brief Show general word recognition pattern
* Single blink for words detected above 50% confidence (non-commands)
*/
void show_general_recognition_pattern() {
  if (INDICATOR_LED_PIN == -1) return;
  // Single quick blink for general recognition
  digitalWrite(INDICATOR_LED_PIN, HIGH);
  delay(150);
  digitalWrite(INDICATOR_LED_PIN, LOW);
}
/**
* @brief Deactivate listening mode
* Turns off indicator LED and resets listening state
*/
void stop_listening_mode() {
  if (INDICATOR_LED_PIN == -1) return;
  listening_mode = false;
  digitalWrite(INDICATOR_LED_PIN, LOW);
}
// ===============================================================================
// VOICE COMMAND PROCESSING ENGINE
// ===============================================================================
/**
* @brief Enhanced wake word detection and command processing system
* 
* Processes inference results with dual confidence thresholds:
* - High threshold (80%) for reliable command execution
* - Lower threshold (50%) for user feedback and recognition
* 
* @param result Edge Impulse inference result containing classification data
*/
void handle_wake_word_and_commands(ei_impulse_result_t& result) {
  // Find the classification label with highest confidence
  float max_confidence = 0.0;
  String detected_word = "";
  // Iterate through all possible classifications
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    if (result.classification[ix].value > max_confidence) {
      max_confidence = result.classification[ix].value;
      detected_word = String(result.classification[ix].label);
    }
  }
  float max_confidence_percentage = max_confidence * 100.0;
  // ENHANCED FEATURE: Recognition feedback for any detected word above 50%
  // Provides user feedback even for words below command threshold
  if (max_confidence >= RECOGNITION_CONFIDENCE_THRESHOLD && !detected_word.equals("noise")) {
    // Only show general recognition if not in command listening mode
    if (!listening_mode && !detected_word.equals("marvin") && !detected_word.equals("on") && !detected_word.equals("off")) {
      show_general_recognition_pattern();
    }
    ei_printf("Recognized: %s (%.1f%%)\n", detected_word.c_str(), max_confidence_percentage);
if (strcmp(detected_word.c_str(), "stop") == 0 &&
    max_confidence_percentage > 95){      Serial.println("Capturing photo...");
      camera_fb_t* fb = esp_camera_fb_get();
      if (!fb) {
        Serial.println("Camera capture failed");
        delay(5000);
      }

      Serial.printf("Captured image: %u bytes\n", fb->len);

      bool ok = uploadFrameToSupabase(fb);
      if (ok) {
        Serial.println("Upload SUCCESS");
      } else {
        Serial.println("Upload FAILED");
      }

      // Return frame buffer to driver
      esp_camera_fb_return(fb);

      // Capture every 60 seconds
      Serial.println("Waiting 60s before next capture...");
    }
  }
  // Check if listening window has expired (10-second timeout)
  if (wake_word_detected) {
    unsigned long current_time = millis();
    if (current_time - wake_word_timestamp > LISTENING_DURATION_MS) {
      wake_word_detected = false;
      listening_mode = false;
      stop_listening_mode();
      ei_printf("Listening window expired.\n");
      return;
    }
  }
  // COMMAND PROCESSING: Only execute commands above 80% confidence threshold
  if (max_confidence_percentage < (COMMAND_CONFIDENCE_THRESHOLD * 100)) {
    return;
  }
  // Ignore background noise classifications
  if (detected_word.equals("noise")) {
    return;
  }
  ei_printf("Detected: %s (%.2f%%)\n", detected_word.c_str(), max_confidence_percentage);
  // WAKE WORD PROCESSING: Activate listening mode
  if (detected_word.equals("marvin")) {
    wake_word_detected = true;
    wake_word_timestamp = millis();
    listening_mode = true;
    ei_printf("Wake word detected! Listening for commands for %d seconds...\n", LISTENING_DURATION_MS / 1000);
    // Visual feedback: Show wake pattern then enter listening mode
    show_wake_word_pattern();
    delay(200);  // Brief pause between patterns
    show_listening_mode();
    return;
  }
  // COMMAND PROCESSING: Execute LED control commands during active listening window
  if (wake_word_detected && listening_mode) {
    if (detected_word.equals("on")) {
      // Execute LED ON command sequence
      stop_listening_mode();                // End listening mode
      show_turning_on_pattern();            // Visual confirmation
      digitalWrite(CONTROL_LED_PIN, HIGH);  // Turn on main LED
      led_state = true;
      ei_printf("LED turned ON\n");
      wake_word_detected = false;  // Reset wake word state
    } else if (detected_word.equals("off")) {
      // Execute LED OFF command sequence
      stop_listening_mode();               // End listening mode
      show_turning_off_pattern();          // Visual confirmation
      digitalWrite(CONTROL_LED_PIN, LOW);  // Turn off main LED
      led_state = false;
      ei_printf("LED turned OFF\n");
      wake_word_detected = false;  // Reset wake word state
    }
  }
}
// ===============================================================================
// SYSTEM INITIALIZATION
// ===============================================================================
/**
* @brief Arduino setup function - System initialization
* Configures hardware, displays system information, and starts audio processing
*/
void setup() {
  // -------------------------------------------------------------------------
  // Serial Communication Setup
  // -------------------------------------------------------------------------
  Serial.begin(115200);
  while (!Serial)
    ;  // Wait for USB connection (comment out for standalone operation)
  Serial.println("Enhanced Voice Control Demo");
  if (!initCamera()) {
    Serial.println("Camera init failed, halting.");
    while (true) {
      delay(1000);
    }
  }

  // Connect WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  // -------------------------------------------------------------------------
  // Hardware Pin Initialization
  // -------------------------------------------------------------------------
  // Configure control LED (main device being controlled)
  pinMode(CONTROL_LED_PIN, OUTPUT);
  digitalWrite(CONTROL_LED_PIN, LOW);
  led_state = false;
  // Configure indicator LED if enabled (status feedback)
  if (INDICATOR_LED_PIN != -1) {
    pinMode(INDICATOR_LED_PIN, OUTPUT);
    digitalWrite(INDICATOR_LED_PIN, LOW);
  }
  // -------------------------------------------------------------------------
  // System Configuration Display
  // -------------------------------------------------------------------------
  ei_printf("Voice control ready. Say 'marvin' then 'on' or 'off'\n");
  ei_printf("Config: Command threshold=%.0f%%, Recognition threshold=%.0f%%\n",
            COMMAND_CONFIDENCE_THRESHOLD * 100, RECOGNITION_CONFIDENCE_THRESHOLD * 100);
  ei_printf("LEDs: Control=Pin%d, Indicator=Pin%d\n", CONTROL_LED_PIN, INDICATOR_LED_PIN);
  ei_printf("LED Patterns:\n");
  ei_printf("  - Wake word 'marvin': 2 quick pulses + listening mode (steady on)\n");
  ei_printf("  - Command 'on': Triple flash + confirmation\n");
  ei_printf("  - Command 'off': Fade pattern + confirmation\n");
  ei_printf("  - Other words: Single blink (when not listening)\n");
  // -------------------------------------------------------------------------
  // Edge Impulse Model Information Display
  // -------------------------------------------------------------------------
  ei_printf("Inferencing settings:\n");
  ei_printf("\tInterval: ");
  ei_printf_float((float)EI_CLASSIFIER_INTERVAL_MS);
  ei_printf(" ms.\n");
  ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
  ei_printf("\tSample length: %d ms.\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT / 16);
  ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) / sizeof(ei_classifier_inferencing_categories[0]));
  // -------------------------------------------------------------------------
  // Audio System Initialization
  // -------------------------------------------------------------------------
  ei_printf("\nStarting continuous inference in 2 seconds...\n");
  ei_sleep(2000);
  // Initialize microphone and audio buffer
  if (microphone_inference_start(EI_CLASSIFIER_RAW_SAMPLE_COUNT) == false) {
    ei_printf("ERR: Could not allocate audio buffer (size %d), this could be due to the window length of your model\r\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT);
    return;
  }
  ei_printf("Recording...\n");
}
// ===============================================================================
// MAIN PROCESSING LOOP
// ===============================================================================
/**
* @brief Arduino main loop - Continuous voice processing
* Handles real-time audio capture, ML inference, and command processing
*/
void loop() {
  // -------------------------------------------------------------------------
  // Audio Capture
  // -------------------------------------------------------------------------
  bool m = microphone_inference_record();
  if (!m) {
    ei_printf("ERR: Failed to record audio...\n");
    return;
  }
  // -------------------------------------------------------------------------
  // Machine Learning Inference
  // -------------------------------------------------------------------------
  // Prepare signal structure for Edge Impulse classifier
  signal_t signal;
  signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
  signal.get_data = microphone_audio_signal_get_data;
  ei_impulse_result_t result = { 0 };
  // Run ML classification on audio data
  EI_IMPULSE_ERROR r = run_classifier(&signal, &result, debug_nn);
  if (r != EI_IMPULSE_OK) {
    ei_printf("ERR: Failed to run classifier (%d)\n", r);
    return;
  }
  // -------------------------------------------------------------------------
  // Command Processing and LED Control
  // -------------------------------------------------------------------------
  handle_wake_word_and_commands(result);
  // -------------------------------------------------------------------------
  // Debug Information Output
  // -------------------------------------------------------------------------
  // Display inference timing and classification results
  ei_printf("Predictions ");
  ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
            result.timing.dsp, result.timing.classification, result.timing.anomaly);
  ei_printf(": \n");
  // Print all classification probabilities
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    ei_printf("    %s: ", result.classification[ix].label);
    ei_printf_float(result.classification[ix].value);
    ei_printf("\n");
  }
  // Print anomaly score if available
#if EI_CLASSIFIER_HAS_ANOMALY == 1
  ei_printf("    anomaly score: ");
  ei_printf_float(result.anomaly);
  ei_printf("\n");
#endif
}
// ===============================================================================
// AUDIO PROCESSING SUBSYSTEM
// ===============================================================================
/**
* @brief Audio data callback function
* Called when new audio samples are available from I2S
* 
* @param n_bytes Number of bytes received
*/
static void audio_inference_callback(uint32_t n_bytes) {
  // Copy samples from I2S buffer to inference buffer
  for (int i = 0; i < n_bytes >> 1; i++) {
    inference.buffer[inference.buf_count++] = sampleBuffer[i];
    // Check if buffer is full
    if (inference.buf_count >= inference.n_samples) {
      inference.buf_count = 0;  // Reset buffer position
      inference.buf_ready = 1;  // Signal buffer ready for processing
    }
  }
}
/**
* @brief FreeRTOS task for continuous audio sample capture
* Runs on separate core for real-time audio processing
* 
* @param arg I2S bytes to read per iteration
*/
static void capture_samples(void* arg) {
  const int32_t i2s_bytes_to_read = (uint32_t)arg;
  size_t bytes_read = i2s_bytes_to_read;
  // Continuous audio capture loop
  while (record_status) {
    // Read audio data from I2S microphone
    i2s_read((i2s_port_t)1, (void*)sampleBuffer, i2s_bytes_to_read, &bytes_read, 100);
    // Error handling for I2S read operations
    if (bytes_read <= 0) {
      ei_printf("Error in I2S read : %d", bytes_read);
    } else {
      // Handle partial reads
      if (bytes_read < i2s_bytes_to_read) {
        ei_printf("Partial I2S read");
      }
      // Audio signal amplification (scale by 8x for better sensitivity)
      for (int x = 0; x < i2s_bytes_to_read / 2; x++) {
        sampleBuffer[x] = (int16_t)(sampleBuffer[x]) * 8;
      }
      // Forward samples to inference callback if still recording
      if (record_status) {
        audio_inference_callback(i2s_bytes_to_read);
      } else {
        break;
      }
    }
  }
  // Clean up FreeRTOS task when done
  vTaskDelete(NULL);
}
// ===============================================================================
// MICROPHONE CONTROL FUNCTIONS
// ===============================================================================
/**
* @brief Initialize microphone inference system
* Allocates buffers and starts audio capture task
* 
* @param n_samples Number of audio samples in buffer
* @return true if successful, false if memory allocation failed
*/
static bool microphone_inference_start(uint32_t n_samples) {
  // Allocate memory for audio sample buffer
  inference.buffer = (int16_t*)malloc(n_samples * sizeof(int16_t));
  if (inference.buffer == NULL) {
    return false;
  }
  // Initialize inference structure
  inference.buf_count = 0;          // Reset buffer position
  inference.n_samples = n_samples;  // Set buffer size
  inference.buf_ready = 0;          // Buffer not ready initially
  // Initialize I2S audio interface
  if (i2s_init(EI_CLASSIFIER_FREQUENCY)) {
    ei_printf("Failed to start I2S!");
  }
  ei_sleep(100);         // Allow I2S to stabilize
  record_status = true;  // Enable recording
  // Create FreeRTOS task for audio capture (high priority task)
  xTaskCreate(capture_samples, "CaptureSamples", 1024 * 32, (void*)sample_buffer_size, 10, NULL);
  return true;
}
/**
* @brief Wait for microphone inference buffer to be ready
* Blocks until audio buffer contains new samples for processing
* 
* @return true when buffer is ready
*/
static bool microphone_inference_record(void) {
  bool ret = true;
  // Wait for buffer to be filled by capture task
  while (inference.buf_ready == 0) {
    delay(10);
  }
  inference.buf_ready = 0;  // Reset ready flag
  return ret;
}
/**
* @brief Convert audio samples to float format for ML processing
* Edge Impulse callback function for accessing audio data
* 
* @param offset Starting position in buffer
* @param length Number of samples to convert
* @param out_ptr Output buffer for float samples
* @return 0 on success
*/
static int microphone_audio_signal_get_data(size_t offset, size_t length, float* out_ptr) {
  numpy::int16_to_float(&inference.buffer[offset], out_ptr, length);
  return 0;
}
/**
* @brief Clean up microphone inference resources
* Stops I2S and frees allocated memory
*/
static void microphone_inference_end(void) {
  i2s_deinit();
  ei_free(inference.buffer);
}
// ===============================================================================
// I2S AUDIO INTERFACE CONFIGURATION
// ===============================================================================
/**
* @brief Initialize I2S audio interface for microphone input
* Configures ESP32 I2S peripheral for digital microphone communication
* 
* @param sampling_rate Audio sampling frequency (typically 8kHz or 16kHz)
* @return 0 on success, error code on failure
*/
static int i2s_init(uint32_t sampling_rate) {
  // I2S Configuration Structure
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),  // Master mode with RX
    .sample_rate = sampling_rate,                                       // Set by Edge Impulse model
    .bits_per_sample = (i2s_bits_per_sample_t)16,                       // 16-bit audio samples
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,                       // Mono audio (right channel)
    .communication_format = I2S_COMM_FORMAT_I2S,                        // Standard I2S protocol
    .intr_alloc_flags = 0,                                              // Default interrupt allocation
    .dma_buf_count = 8,                                                 // Number of DMA buffers
    .dma_buf_len = 512,                                                 // DMA buffer length
    .use_apll = false,                                                  // Use PLL for clock generation
    .tx_desc_auto_clear = false,                                        // Don't auto-clear TX descriptors
    .fixed_mclk = -1,                                                   // Auto-calculate master clock
  };
  // I2S Pin Configuration for INMP441 Digital Microphone
  i2s_pin_config_t pin_config = {
    .bck_io_num = 14,    // Bit Clock (SCLK)
    .ws_io_num = 15,     // Word Select (LRCLK)
    .data_out_num = -1,  // Data Output (not used for input-only)
    .data_in_num = 13,   // Data Input (SD pin from microphone)
  };
  esp_err_t ret = 0;
  // Install I2S driver
  ret = i2s_driver_install((i2s_port_t)1, &i2s_config, 0, NULL);
  if (ret != ESP_OK) {
    ei_printf("Error in i2s_driver_install");
  }
  // Configure I2S pins
  ret = i2s_set_pin((i2s_port_t)1, &pin_config);
  if (ret != ESP_OK) {
    ei_printf("Error in i2s_set_pin");
  }
  // Clear DMA buffers
  ret = i2s_zero_dma_buffer((i2s_port_t)1);
  if (ret != ESP_OK) {
    ei_printf("Error in initializing dma buffer with 0");
  }
  return int(ret);
}
/**
* @brief Deinitialize I2S audio interface
* Stops and uninstalls I2S driver
* 
* @return 0 on success
*/
static int i2s_deinit(void) {
  i2s_driver_uninstall((i2s_port_t)1);  // Stop and destroy I2S driver
  return 0;
}
// ===============================================================================
// COMPILE-TIME VALIDATION
// ===============================================================================
// Ensure correct sensor type is configured in Edge Impulse model
#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_MICROPHONE
#error "Invalid model for current sensor."
#endif
// ===============================================================================
// END OF FILE
// ===============================================================================