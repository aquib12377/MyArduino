#ifndef FPM383C_H
#define FPM383C_H

#include <Arduino.h>
#include <SoftwareSerial.h>

// Frame header
#define FP_FRAME_HEADER_0 0xF1
#define FP_FRAME_HEADER_1 0x1F
#define FP_FRAME_HEADER_2 0xE2
#define FP_FRAME_HEADER_3 0x2E
#define FP_FRAME_HEADER_4 0xB6
#define FP_FRAME_HEADER_5 0x6B
#define FP_FRAME_HEADER_6 0xA8
#define FP_FRAME_HEADER_7 0x8A

// Command categories
#define FP_CMD_FINGERPRINT_0 0x01
#define FP_CMD_SYSTEM_0 0x02
#define FP_CMD_MAINTENANCE_0 0x03

// Fingerprint commands
#define FP_CMD_ENROLL 0x11
#define FP_CMD_QUERY_ENROLL 0x12
#define FP_CMD_SAVE_TEMPLATE 0x13
#define FP_CMD_QUERY_SAVE 0x14
#define FP_CMD_CANCEL 0x15
#define FP_CMD_UPDATE_FEATURE 0x16
#define FP_CMD_QUERY_UPDATE 0x17
#define FP_CMD_AUTO_ENROLL 0x18
#define FP_CMD_MATCH 0x21
#define FP_CMD_QUERY_MATCH 0x22
#define FP_CMD_MATCH_SYNC 0x23
#define FP_CMD_DELETE 0x31
#define FP_CMD_QUERY_DELETE 0x32
#define FP_CMD_CHECK_ID_EXIST 0x33
#define FP_CMD_GET_STORAGE_INFO 0x34
#define FP_CMD_CHECK_FINGER_STATUS 0x35
#define FP_CMD_DELETE_SYNC 0x36
#define FP_CMD_CONFIRM_ENROLL 0x41
#define FP_CMD_QUERY_CONFIRM 0x42

// System commands
#define FP_CMD_SET_PASSWORD 0x01
#define FP_CMD_RESET_MODULE 0x02
#define FP_CMD_GET_TEMPLATE_COUNT 0x03
#define FP_CMD_GET_GAIN 0x09
#define FP_CMD_GET_THRESHOLD 0x0B
#define FP_CMD_SET_SLEEP_MODE 0x0C
#define FP_CMD_SET_ENROLL_COUNT 0x0D
#define FP_CMD_SET_LED 0x0F
#define FP_CMD_GET_POLICY 0xFB
#define FP_CMD_SET_POLICY 0xFC

// Maintenance commands
#define FP_CMD_GET_MODULE_ID 0x01
#define FP_CMD_HEARTBEAT 0x03
#define FP_CMD_SET_BAUDRATE 0x04
#define FP_CMD_SET_COMM_PASSWORD 0x05

// Error codes
#define FP_ERROR_SUCCESS 0x00000000
#define FP_ERROR_UNKNOWN_CMD 0x00000001
#define FP_ERROR_INVALID_LENGTH 0x00000002
#define FP_ERROR_INVALID_DATA 0x00000003
#define FP_ERROR_SYSTEM_BUSY 0x00000004
#define FP_ERROR_NO_REQUEST 0x00000005
#define FP_ERROR_SOFTWARE_ERROR 0x00000006
#define FP_ERROR_HARDWARE_ERROR 0x00000007
#define FP_ERROR_TIMEOUT 0x00000008
#define FP_ERROR_EXTRACTION_ERROR 0x00000009
#define FP_ERROR_TEMPLATE_EMPTY 0x0000000A
#define FP_ERROR_STORAGE_FULL 0x0000000B
#define FP_ERROR_WRITE_FAILED 0x0000000C
#define FP_ERROR_READ_FAILED 0x0000000D
#define FP_ERROR_POOR_IMAGE 0x0000000E
#define FP_ERROR_DUPLICATE 0x0000000F
#define FP_ERROR_SMALL_AREA 0x00000010

// LED colors
#define FP_LED_OFF 0x00
#define FP_LED_GREEN 0x01
#define FP_LED_RED 0x02
#define FP_LED_RED_GREEN 0x03
#define FP_LED_BLUE 0x04
#define FP_LED_RED_BLUE 0x05
#define FP_LED_GREEN_BLUE 0x06
#define FP_LED_ALL_COLORS 0x07

// LED control modes
#define FP_LED_MODE_OFF 0x00
#define FP_LED_MODE_ON 0x01
#define FP_LED_MODE_AUTO 0x02
#define FP_LED_MODE_PWM 0x03
#define FP_LED_MODE_BLINK 0x04

struct FingerprintMatchResult {
  bool matched;
  uint16_t fingerprintId;
  uint16_t matchScore;
};

struct FingerprintEnrollResult {
  uint16_t fingerprintId;
  uint8_t progress;
  bool completed;
};

struct FingerprintStorageInfo {
  uint16_t totalCount;
  uint8_t storageMap[64];
};

class FPM383C {
private:
  SoftwareSerial* serial;
  uint32_t password;
  int touchPin;
  
  // Communication functions
  uint8_t calculateChecksum(uint8_t* data, uint16_t length);
  uint8_t calculateFrameChecksum(uint16_t dataLength);
  bool sendCommand(uint8_t cmd1, uint8_t cmd2, uint8_t* data, uint16_t dataLen);
  bool receiveResponse(uint8_t cmd1, uint8_t cmd2, uint8_t* data, uint16_t maxDataLen, uint16_t* actualDataLen, uint32_t* errorCode);
  void sendFrame(uint8_t cmd1, uint8_t cmd2, uint8_t* data, uint16_t dataLen);
  bool receiveFrame(uint8_t* cmd1, uint8_t* cmd2, uint8_t* data, uint16_t maxDataLen, uint16_t* actualDataLen, uint32_t* errorCode);
  
public:
  FPM383C(int rxPin, int txPin, int touchPin = -1);
  ~FPM383C();
  
  // Initialization
  bool begin(uint32_t baudrate = 57600);
  bool setPassword(uint32_t newPassword);
  bool heartbeat();
  bool reset();
  
  // Fingerprint enrollment
  bool startEnrollment(uint8_t regIndex);
  FingerprintEnrollResult queryEnrollmentResult();
  bool saveTemplate(uint16_t fingerprintId);
  bool querySaveResult();
  bool autoEnroll(uint16_t fingerprintId, uint8_t enrollCount = 6, bool waitFingerLift = false);
  bool cancelOperation();
  
  // Fingerprint matching
  bool startMatch();
  FingerprintMatchResult queryMatchResult();
  FingerprintMatchResult matchSync();
  
  // Fingerprint management
  bool deleteFingerprint(uint16_t fingerprintId);
  bool deleteAllFingerprints();
  bool deleteMultipleFingerprints(uint16_t* ids, uint8_t count);
  bool deleteRangeFingerprints(uint16_t startId, uint16_t endId);
  bool queryDeleteResult();
  bool checkFingerprintExists(uint16_t fingerprintId);
  FingerprintStorageInfo getStorageInfo();
  uint16_t getTemplateCount();
  
  // System functions
  bool setSleepMode(uint8_t mode = 0);
  bool setEnrollCount(uint8_t count);
  bool setLED(uint8_t mode, uint8_t color, uint8_t param1 = 0, uint8_t param2 = 0, uint8_t param3 = 0);
  bool setBaudrate(uint32_t baudrate);
  String getModuleId();
  bool updateFeature(uint16_t fingerprintId);
  bool queryUpdateResult();
  
  // Touch detection
  bool isFingerPresent();
  bool waitForFinger(uint32_t timeout = 10000);
  bool waitForFingerRemoval(uint32_t timeout = 5000);
  
  // Utility functions
  uint32_t getLastError();
  String getErrorString(uint32_t errorCode);
  void enableDebug(bool enable);
  
private:
  uint32_t lastError;
  bool debugEnabled;
  void debugPrint(String message);
};

#endif
