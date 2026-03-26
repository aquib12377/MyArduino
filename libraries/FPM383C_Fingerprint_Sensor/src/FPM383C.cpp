#include "FPM383C.h"

FPM383C::FPM383C(int rxPin, int txPin, int touchPin) {
  serial = new SoftwareSerial(rxPin, txPin);
  password = 0x00000000;
  this->touchPin = touchPin;
  lastError = FP_ERROR_SUCCESS;
  debugEnabled = false;
  
  if (touchPin >= 0) {
    pinMode(touchPin, INPUT);
  }
}

FPM383C::~FPM383C() {
  delete serial;
}

bool FPM383C::begin(uint32_t baudrate) {
  serial->begin(baudrate);
  delay(200); // Wait for module initialization
  
  // Check if module is responsive
  return heartbeat();
}

uint8_t FPM383C::calculateChecksum(uint8_t* data, uint16_t length) {
  int16_t sum = 0;
  for (uint16_t i = 0; i < length; i++) {
    sum += data[i];
  }
  return (uint8_t)((~sum) + 1);
}

uint8_t FPM383C::calculateFrameChecksum(uint16_t dataLength) {
  uint8_t frameHeader[] = {FP_FRAME_HEADER_0, FP_FRAME_HEADER_1, FP_FRAME_HEADER_2, 
                           FP_FRAME_HEADER_3, FP_FRAME_HEADER_4, FP_FRAME_HEADER_5, 
                           FP_FRAME_HEADER_6, FP_FRAME_HEADER_7};
  
  int16_t sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += frameHeader[i];
  }
  sum += (dataLength >> 8) & 0xFF;
  sum += dataLength & 0xFF;
  
  return (uint8_t)((~sum) + 1);
}

void FPM383C::sendFrame(uint8_t cmd1, uint8_t cmd2, uint8_t* data, uint16_t dataLen) {
  uint16_t totalLen = 7 + dataLen;
  
  // Send frame header
  serial->write(FP_FRAME_HEADER_0);
  serial->write(FP_FRAME_HEADER_1);
  serial->write(FP_FRAME_HEADER_2);
  serial->write(FP_FRAME_HEADER_3);
  serial->write(FP_FRAME_HEADER_4);
  serial->write(FP_FRAME_HEADER_5);
  serial->write(FP_FRAME_HEADER_6);
  serial->write(FP_FRAME_HEADER_7);
  
  // Send data length
  serial->write((totalLen >> 8) & 0xFF);
  serial->write(totalLen & 0xFF);
  
  // Send frame checksum
  serial->write(calculateFrameChecksum(totalLen));
  
  // Prepare application data
  uint8_t* appData = new uint8_t[totalLen];
  uint16_t idx = 0;
  
  // Password
  appData[idx++] = (password >> 24) & 0xFF;
  appData[idx++] = (password >> 16) & 0xFF;
  appData[idx++] = (password >> 8) & 0xFF;
  appData[idx++] = password & 0xFF;
  
  // Command
  appData[idx++] = cmd1;
  appData[idx++] = cmd2;
  
  // Data
  if (data && dataLen > 0) {
    memcpy(&appData[idx], data, dataLen);
    idx += dataLen;
  }
  
  // Calculate and add checksum
  uint8_t checksum = calculateChecksum(appData, idx);
  appData[idx++] = checksum;
  
  // Send application data
  for (int i = 0; i < idx; i++) {
    serial->write(appData[i]);
  }
  
  delete[] appData;
  
  if (debugEnabled) {
    debugPrint("Sent command: " + String(cmd1, HEX) + " " + String(cmd2, HEX));
  }
}

bool FPM383C::receiveFrame(uint8_t* cmd1, uint8_t* cmd2, uint8_t* data, uint16_t maxDataLen, uint16_t* actualDataLen, uint32_t* errorCode) {
  uint32_t timeout = millis() + 5000; // 5 second timeout
  
  // Wait for frame header
  uint8_t headerIdx = 0;
  uint8_t expectedHeader[] = {FP_FRAME_HEADER_0, FP_FRAME_HEADER_1, FP_FRAME_HEADER_2, 
                             FP_FRAME_HEADER_3, FP_FRAME_HEADER_4, FP_FRAME_HEADER_5, 
                             FP_FRAME_HEADER_6, FP_FRAME_HEADER_7};
  
  while (millis() < timeout && headerIdx < 8) {
    if (serial->available()) {
      uint8_t byte = serial->read();
      if (byte == expectedHeader[headerIdx]) {
        headerIdx++;
      } else {
        headerIdx = 0;
        if (byte == expectedHeader[0]) headerIdx = 1;
      }
    }
  }
  
  if (headerIdx < 8) {
    lastError = FP_ERROR_TIMEOUT;
    return false;
  }
  
  // Read data length
  while (millis() < timeout && serial->available() < 2) {
    delay(1);
  }
  
  if (serial->available() < 2) {
    lastError = FP_ERROR_TIMEOUT;
    return false;
  }
  
  uint16_t dataLength = (serial->read() << 8) | serial->read();
  
  // Read frame checksum
  while (millis() < timeout && serial->available() < 1) {
    delay(1);
  }
  
  if (serial->available() < 1) {
    lastError = FP_ERROR_TIMEOUT;
    return false;
  }
  
  uint8_t frameChecksum = serial->read();
  
  // Verify frame checksum
  if (frameChecksum != calculateFrameChecksum(dataLength)) {
    lastError = FP_ERROR_INVALID_DATA;
    return false;
  }
  
  // Read application data
  while (millis() < timeout && serial->available() < dataLength) {
    delay(1);
  }
  
  if (serial->available() < dataLength) {
    lastError = FP_ERROR_TIMEOUT;
    return false;
  }
  
  uint8_t* appData = new uint8_t[dataLength];
  for (int i = 0; i < dataLength; i++) {
    appData[i] = serial->read();
  }
  
  // Verify application checksum
  uint8_t receivedChecksum = appData[dataLength - 1];
  uint8_t calculatedChecksum = calculateChecksum(appData, dataLength - 1);
  
  if (receivedChecksum != calculatedChecksum) {
    delete[] appData;
    lastError = FP_ERROR_INVALID_DATA;
    return false;
  }
  
  // Parse response
  uint32_t receivedPassword = (appData[0] << 24) | (appData[1] << 16) | (appData[2] << 8) | appData[3];
  *cmd1 = appData[4];
  *cmd2 = appData[5];
  *errorCode = (appData[6] << 24) | (appData[7] << 16) | (appData[8] << 8) | appData[9];
  
  // Copy data
  uint16_t responseDataLen = dataLength - 11; // 11 = 4 bytes password + 2 bytes cmd + 4 bytes error + 1 byte checksum
  if (responseDataLen > 0 && data && maxDataLen > 0) {
    *actualDataLen = min(responseDataLen, maxDataLen);
    memcpy(data, &appData[10], *actualDataLen);
  } else {
    *actualDataLen = 0;
  }
  
  delete[] appData;
  lastError = *errorCode;
  
  if (debugEnabled) {
    debugPrint("Received response: " + String(*cmd1, HEX) + " " + String(*cmd2, HEX) + 
               " Error: " + String(*errorCode, HEX));
  }
  
  return true;
}

bool FPM383C::sendCommand(uint8_t cmd1, uint8_t cmd2, uint8_t* data, uint16_t dataLen) {
  sendFrame(cmd1, cmd2, data, dataLen);
  return true;
}

bool FPM383C::receiveResponse(uint8_t cmd1, uint8_t cmd2, uint8_t* data, uint16_t maxDataLen, uint16_t* actualDataLen, uint32_t* errorCode) {
  uint8_t respCmd1, respCmd2;
  
  if (!receiveFrame(&respCmd1, &respCmd2, data, maxDataLen, actualDataLen, errorCode)) {
    return false;
  }
  
  // Verify command match
  if (respCmd1 != cmd1 || respCmd2 != cmd2) {
    lastError = FP_ERROR_INVALID_DATA;
    return false;
  }
  
  return true;
}

bool FPM383C::heartbeat() {
  sendCommand(FP_CMD_MAINTENANCE_0, FP_CMD_HEARTBEAT, nullptr, 0);
  
  uint32_t errorCode;
  uint16_t dataLen;
  
  if (!receiveResponse(FP_CMD_MAINTENANCE_0, FP_CMD_HEARTBEAT, nullptr, 0, &dataLen, &errorCode)) {
    return false;
  }
  
  return errorCode == FP_ERROR_SUCCESS;
}

bool FPM383C::setPassword(uint32_t newPassword) {
  uint8_t data[4];
  data[0] = (newPassword >> 24) & 0xFF;
  data[1] = (newPassword >> 16) & 0xFF;
  data[2] = (newPassword >> 8) & 0xFF;
  data[3] = newPassword & 0xFF;
  
  sendCommand(FP_CMD_SYSTEM_0, FP_CMD_SET_PASSWORD, data, 4);
  
  uint32_t errorCode;
  uint16_t dataLen;
  
  if (!receiveResponse(FP_CMD_SYSTEM_0, FP_CMD_SET_PASSWORD, nullptr, 0, &dataLen, &errorCode)) {
    return false;
  }
  
  if (errorCode == FP_ERROR_SUCCESS) {
    password = newPassword;
    return true;
  }
  
  return false;
}

bool FPM383C::reset() {
  sendCommand(FP_CMD_SYSTEM_0, FP_CMD_RESET_MODULE, nullptr, 0);
  
  uint32_t errorCode;
  uint16_t dataLen;
  
  if (!receiveResponse(FP_CMD_SYSTEM_0, FP_CMD_RESET_MODULE, nullptr, 0, &dataLen, &errorCode)) {
    return false;
  }
  
  return errorCode == FP_ERROR_SUCCESS;
}

bool FPM383C::startEnrollment(uint8_t regIndex) {
  uint8_t data[1];
  data[0] = regIndex;
  
  sendCommand(FP_CMD_FINGERPRINT_0, FP_CMD_ENROLL, data, 1);
  
  uint32_t errorCode;
  uint16_t dataLen;
  
  if (!receiveResponse(FP_CMD_FINGERPRINT_0, FP_CMD_ENROLL, nullptr, 0, &dataLen, &errorCode)) {
    return false;
  }
  
  return errorCode == FP_ERROR_SUCCESS;
}

FingerprintEnrollResult FPM383C::queryEnrollmentResult() {
  FingerprintEnrollResult result = {0, 0, false};
  
  sendCommand(FP_CMD_FINGERPRINT_0, FP_CMD_QUERY_ENROLL, nullptr, 0);
  
  uint32_t errorCode;
  uint16_t dataLen;
  uint8_t data[3];
  
  if (!receiveResponse(FP_CMD_FINGERPRINT_0, FP_CMD_QUERY_ENROLL, data, 3, &dataLen, &errorCode)) {
    return result;
  }
  
  if (errorCode == FP_ERROR_SUCCESS && dataLen >= 3) {
    result.fingerprintId = (data[0] << 8) | data[1];
    result.progress = data[2];
    result.completed = (result.progress >= 100);
  }
  
  return result;
}

bool FPM383C::saveTemplate(uint16_t fingerprintId) {
  uint8_t data[2];
  data[0] = (fingerprintId >> 8) & 0xFF;
  data[1] = fingerprintId & 0xFF;
  
  sendCommand(FP_CMD_FINGERPRINT_0, FP_CMD_SAVE_TEMPLATE, data, 2);
  
  uint32_t errorCode;
  uint16_t dataLen;
  
  if (!receiveResponse(FP_CMD_FINGERPRINT_0, FP_CMD_SAVE_TEMPLATE, nullptr, 0, &dataLen, &errorCode)) {
    return false;
  }
  
  return errorCode == FP_ERROR_SUCCESS;
}

bool FPM383C::querySaveResult() {
  sendCommand(FP_CMD_FINGERPRINT_0, FP_CMD_QUERY_SAVE, nullptr, 0);
  
  uint32_t errorCode;
  uint16_t dataLen;
  
  if (!receiveResponse(FP_CMD_FINGERPRINT_0, FP_CMD_QUERY_SAVE, nullptr, 0, &dataLen, &errorCode)) {
    return false;
  }
  
  return errorCode == FP_ERROR_SUCCESS;
}

bool FPM383C::autoEnroll(uint16_t fingerprintId, uint8_t enrollCount, bool waitFingerLift) {
  uint8_t data[4];
  data[0] = waitFingerLift ? 1 : 0;
  data[1] = enrollCount;
  data[2] = (fingerprintId >> 8) & 0xFF;
  data[3] = fingerprintId & 0xFF;
  
  sendCommand(FP_CMD_FINGERPRINT_0, FP_CMD_AUTO_ENROLL, data, 4);
  
  // Auto enroll sends multiple responses
  uint32_t timeout = millis() + 30000; // 30 second timeout
  
  while (millis() < timeout) {
    uint32_t errorCode;
    uint16_t dataLen;
    uint8_t responseData[4];
    
    if (receiveResponse(FP_CMD_FINGERPRINT_0, FP_CMD_AUTO_ENROLL, responseData, 4, &dataLen, &errorCode)) {
      if (errorCode != FP_ERROR_SUCCESS) {
        return false;
      }
      
      if (dataLen >= 4) {
        uint8_t currentCount = responseData[0];
        uint8_t progress = responseData[3];
        
        // Check if enrollment is complete (currentCount == 0xFF means save complete)
        if (currentCount == 0xFF && progress == 100) {
          return true;
        }
      }
    }
    
    delay(100);
  }
  
  return false;
}

bool FPM383C::cancelOperation() {
  sendCommand(FP_CMD_FINGERPRINT_0, FP_CMD_CANCEL, nullptr, 0);
  
  uint32_t errorCode;
  uint16_t dataLen;
  
  if (!receiveResponse(FP_CMD_FINGERPRINT_0, FP_CMD_CANCEL, nullptr, 0, &dataLen, &errorCode)) {
    return false;
  }
  
  return errorCode == FP_ERROR_SUCCESS;
}

bool FPM383C::startMatch() {
  sendCommand(FP_CMD_FINGERPRINT_0, FP_CMD_MATCH, nullptr, 0);
  
  uint32_t errorCode;
  uint16_t dataLen;
  
  if (!receiveResponse(FP_CMD_FINGERPRINT_0, FP_CMD_MATCH, nullptr, 0, &dataLen, &errorCode)) {
    return false;
  }
  
  return errorCode == FP_ERROR_SUCCESS;
}

FingerprintMatchResult FPM383C::queryMatchResult() {
  FingerprintMatchResult result = {false, 0, 0};
  
  sendCommand(FP_CMD_FINGERPRINT_0, FP_CMD_QUERY_MATCH, nullptr, 0);
  
  uint32_t errorCode;
  uint16_t dataLen;
  uint8_t data[6];
  
  if (!receiveResponse(FP_CMD_FINGERPRINT_0, FP_CMD_QUERY_MATCH, data, 6, &dataLen, &errorCode)) {
    return result;
  }
  
  if (errorCode == FP_ERROR_SUCCESS && dataLen >= 6) {
    result.matched = (data[0] == 1);
    result.matchScore = (data[1] << 8) | data[2];
    result.fingerprintId = (data[4] << 8) | data[5];
  }
  
  return result;
}

FingerprintMatchResult FPM383C::matchSync() {
  FingerprintMatchResult result = {false, 0, 0};
  
  sendCommand(FP_CMD_FINGERPRINT_0, FP_CMD_MATCH_SYNC, nullptr, 0);
  
  uint32_t errorCode;
  uint16_t dataLen;
  uint8_t data[6];
  
  if (!receiveResponse(FP_CMD_FINGERPRINT_0, FP_CMD_MATCH_SYNC, data, 6, &dataLen, &errorCode)) {
    return result;
  }
  
  if (errorCode == FP_ERROR_SUCCESS && dataLen >= 6) {
    result.matched = (data[0] == 1);
    result.matchScore = (data[1] << 8) | data[2];
    result.fingerprintId = (data[4] << 8) | data[5];
  }
  
  return result;
}

bool FPM383C::deleteFingerprint(uint16_t fingerprintId) {
  uint8_t data[3];
  data[0] = 0x00; // Single fingerprint delete mode
  data[1] = (fingerprintId >> 8) & 0xFF;
  data[2] = fingerprintId & 0xFF;
  
  sendCommand(FP_CMD_FINGERPRINT_0, FP_CMD_DELETE, data, 3);
  
  uint32_t errorCode;
  uint16_t dataLen;
  
  if (!receiveResponse(FP_CMD_FINGERPRINT_0, FP_CMD_DELETE, nullptr, 0, &dataLen, &errorCode)) {
    return false;
  }
  
  return errorCode == FP_ERROR_SUCCESS;
}

bool FPM383C::deleteAllFingerprints() {
  uint8_t data[3];
  data[0] = 0x01; // Delete all mode
  data[1] = 0x00;
  data[2] = 0x01;
  
  sendCommand(FP_CMD_FINGERPRINT_0, FP_CMD_DELETE, data, 3);
  
  uint32_t errorCode;
  uint16_t dataLen;
  
  if (!receiveResponse(FP_CMD_FINGERPRINT_0, FP_CMD_DELETE, nullptr, 0, &dataLen, &errorCode)) {
    return false;
  }
  
  return errorCode == FP_ERROR_SUCCESS;
}

bool FPM383C::queryDeleteResult() {
  sendCommand(FP_CMD_FINGERPRINT_0, FP_CMD_QUERY_DELETE, nullptr, 0);
  
  uint32_t errorCode;
  uint16_t dataLen;
  
  if (!receiveResponse(FP_CMD_FINGERPRINT_0, FP_CMD_QUERY_DELETE, nullptr, 0, &dataLen, &errorCode)) {
    return false;
  }
  
  return errorCode == FP_ERROR_SUCCESS;
}

bool FPM383C::checkFingerprintExists(uint16_t fingerprintId) {
  uint8_t data[2];
  data[0] = (fingerprintId >> 8) & 0xFF;
  data[1] = fingerprintId & 0xFF;
  
  sendCommand(FP_CMD_FINGERPRINT_0, FP_CMD_CHECK_ID_EXIST, data, 2);
  
  uint32_t errorCode;
  uint16_t dataLen;
  uint8_t responseData[3];
  
  if (!receiveResponse(FP_CMD_FINGERPRINT_0, FP_CMD_CHECK_ID_EXIST, responseData, 3, &dataLen, &errorCode)) {
    return false;
  }
  
  if (errorCode == FP_ERROR_SUCCESS && dataLen >= 1) {
    return (responseData[0] == 1);
  }
  
  return false;
}

uint16_t FPM383C::getTemplateCount() {
  sendCommand(FP_CMD_SYSTEM_0, FP_CMD_GET_TEMPLATE_COUNT, nullptr, 0);
  
  uint32_t errorCode;
  uint16_t dataLen;
  uint8_t data[2];
  
  if (!receiveResponse(FP_CMD_SYSTEM_0, FP_CMD_GET_TEMPLATE_COUNT, data, 2, &dataLen, &errorCode)) {
    return 0;
  }
  
  if (errorCode == FP_ERROR_SUCCESS && dataLen >= 2) {
    return (data[0] << 8) | data[1];
  }
  
  return 0;
}

bool FPM383C::setSleepMode(uint8_t mode) {
  uint8_t data[1];
  data[0] = mode;
  
  sendCommand(FP_CMD_SYSTEM_0, FP_CMD_SET_SLEEP_MODE, data, 1);
  
  uint32_t errorCode;
  uint16_t dataLen;
  
  if (!receiveResponse(FP_CMD_SYSTEM_0, FP_CMD_SET_SLEEP_MODE, nullptr, 0, &dataLen, &errorCode)) {
    return false;
  }
  
  return errorCode == FP_ERROR_SUCCESS;
}

bool FPM383C::setEnrollCount(uint8_t count) {
  if (count < 1 || count > 6) return false;
  
  uint8_t data[1];
  data[0] = count;
  
  sendCommand(FP_CMD_SYSTEM_0, FP_CMD_SET_ENROLL_COUNT, data, 1);
  
  uint32_t errorCode;
  uint16_t dataLen;
  
  if (!receiveResponse(FP_CMD_SYSTEM_0, FP_CMD_SET_ENROLL_COUNT, nullptr, 0, &dataLen, &errorCode)) {
    return false;
  }
  
  return errorCode == FP_ERROR_SUCCESS;
}

bool FPM383C::setLED(uint8_t mode, uint8_t color, uint8_t param1, uint8_t param2, uint8_t param3) {
  uint8_t data[5];
  data[0] = mode;
  data[1] = color;
  data[2] = param1;
  data[3] = param2;
  data[4] = param3;
  
  sendCommand(FP_CMD_SYSTEM_0, FP_CMD_SET_LED, data, 5);
  
  uint32_t errorCode;
  uint16_t dataLen;
  
  if (!receiveResponse(FP_CMD_SYSTEM_0, FP_CMD_SET_LED, nullptr, 0, &dataLen, &errorCode)) {
    return false;
  }
  
  return errorCode == FP_ERROR_SUCCESS;
}

String FPM383C::getModuleId() {
  sendCommand(FP_CMD_MAINTENANCE_0, FP_CMD_GET_MODULE_ID, nullptr, 0);
  
  uint32_t errorCode;
  uint16_t dataLen;
  uint8_t data[16];
  
  if (!receiveResponse(FP_CMD_MAINTENANCE_0, FP_CMD_GET_MODULE_ID, data, 16, &dataLen, &errorCode)) {
    return "";
  }
  
  if (errorCode == FP_ERROR_SUCCESS && dataLen >= 16) {
    String moduleId = "";
    for (int i = 0; i < 16; i++) {
      if (data[i] != 0) {
        moduleId += (char)data[i];
      }
    }
    return moduleId;
  }
  
  return "";
}

bool FPM383C::setBaudrate(uint32_t baudrate) {
  uint8_t data[4];
  data[0] = (baudrate >> 24) & 0xFF;
  data[1] = (baudrate >> 16) & 0xFF;
  data[2] = (baudrate >> 8) & 0xFF;
  data[3] = baudrate & 0xFF;
  
  sendCommand(FP_CMD_MAINTENANCE_0, FP_CMD_SET_BAUDRATE, data, 4);
  
  uint32_t errorCode;
  uint16_t dataLen;
  
  if (!receiveResponse(FP_CMD_MAINTENANCE_0, FP_CMD_SET_BAUDRATE, nullptr, 0, &dataLen, &errorCode)) {
    return false;
  }
  
  if (errorCode == FP_ERROR_SUCCESS) {
    serial->end();
    delay(100);
    serial->begin(baudrate);
    delay(100);
    return true;
  }
  
  return false;
}

bool FPM383C::isFingerPresent() {
  if (touchPin >= 0) {
    return digitalRead(touchPin) == HIGH;
  }
  
  sendCommand(FP_CMD_FINGERPRINT_0, FP_CMD_CHECK_FINGER_STATUS, nullptr, 0);
  
  uint32_t errorCode;
  uint16_t dataLen;
  uint8_t data[1];
  
  if (!receiveResponse(FP_CMD_FINGERPRINT_0, FP_CMD_CHECK_FINGER_STATUS, data, 1, &dataLen, &errorCode)) {
    return false;
  }
  
  if (errorCode == FP_ERROR_SUCCESS && dataLen >= 1) {
    return (data[0] == 1);
  }
  
  return false;
}

bool FPM383C::waitForFinger(uint32_t timeout) {
  uint32_t startTime = millis();
  
  while (millis() - startTime < timeout) {
    if (isFingerPresent()) {
      return true;
    }
    delay(50);
  }
  
  return false;
}

bool FPM383C::waitForFingerRemoval(uint32_t timeout) {
  uint32_t startTime = millis();
  
  while (millis() - startTime < timeout) {
    if (!isFingerPresent()) {
      return true;
    }
    delay(50);
  }
  
  return false;
}

bool FPM383C::updateFeature(uint16_t fingerprintId) {
  uint8_t data[2];
  data[0] = (fingerprintId >> 8) & 0xFF;
  data[1] = fingerprintId & 0xFF;
  
  sendCommand(FP_CMD_FINGERPRINT_0, FP_CMD_UPDATE_FEATURE, data, 2);
  
  uint32_t errorCode;
  uint16_t dataLen;
  
  if (!receiveResponse(FP_CMD_FINGERPRINT_0, FP_CMD_UPDATE_FEATURE, nullptr, 0, &dataLen, &errorCode)) {
    return false;
  }
  
  return errorCode == FP_ERROR_SUCCESS;
}

bool FPM383C::queryUpdateResult() {
  sendCommand(FP_CMD_FINGERPRINT_0, FP_CMD_QUERY_UPDATE, nullptr, 0);
  
  uint32_t errorCode;
  uint16_t dataLen;
  
  if (!receiveResponse(FP_CMD_FINGERPRINT_0, FP_CMD_QUERY_UPDATE, nullptr, 0, &dataLen, &errorCode)) {
    return false;
  }
  
  return errorCode == FP_ERROR_SUCCESS;
}

uint32_t FPM383C::getLastError() {
  return lastError;
}

String FPM383C::getErrorString(uint32_t errorCode) {
  switch (errorCode) {
    case FP_ERROR_SUCCESS: return "Success";
    case FP_ERROR_UNKNOWN_CMD: return "Unknown command";
    case FP_ERROR_INVALID_LENGTH: return "Invalid data length";
    case FP_ERROR_INVALID_DATA: return "Invalid data";
    case FP_ERROR_SYSTEM_BUSY: return "System busy";
    case FP_ERROR_NO_REQUEST: return "No request sent";
    case FP_ERROR_SOFTWARE_ERROR: return "Software error";
    case FP_ERROR_HARDWARE_ERROR: return "Hardware error";
    case FP_ERROR_TIMEOUT: return "Timeout";
    case FP_ERROR_EXTRACTION_ERROR: return "Feature extraction error";
    case FP_ERROR_TEMPLATE_EMPTY: return "Template library empty";
    case FP_ERROR_STORAGE_FULL: return "Storage full";
    case FP_ERROR_WRITE_FAILED: return "Write failed";
    case FP_ERROR_READ_FAILED: return "Read failed";
    case FP_ERROR_POOR_IMAGE: return "Poor image quality";
    case FP_ERROR_DUPLICATE: return "Duplicate fingerprint";
    case FP_ERROR_SMALL_AREA: return "Finger area too small";
    default: return "Unknown error: 0x" + String(errorCode, HEX);
  }
}

void FPM383C::enableDebug(bool enable) {
  debugEnabled = enable;
}

void FPM383C::debugPrint(String message) {
  if (debugEnabled) {
    Serial.println("[FPM383C] " + message);
  }
}
