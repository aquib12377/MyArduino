# FPM383C Fingerprint Sensor Library

Arduino library for HLK-FPM383C capacitive fingerprint sensor with complete functionality support.

## Features

- **Complete Protocol Implementation**: Full support for FPM383C communication protocol
- **Fingerprint Management**: Enrollment, matching, deletion, and template management
- **LED Control**: RGB LED control with various modes (solid, blinking, breathing)
- **Touch Detection**: Hardware and software finger presence detection
- **Error Handling**: Comprehensive error codes with human-readable messages
- **Self-Learning**: Automatic feature updating after successful matches
- **Power Management**: Sleep mode support for battery-powered applications

## Hardware Requirements

- **Power Supply**: 3.3V only (using 5V will damage the sensor)
- **Current Draw**: Up to 45mA during operation, <25µA in sleep mode
- **Connections**: UART communication at 57600 baud (configurable)

## Wiring

| FPM383C Pin | Arduino Pin | Description |
|-------------|-------------|-------------|
| V_TOUCH (1) | 3.3V | Touch sensor power |
| TOUCHOUT (2) | Digital Pin 3 | Interrupt output |
| VCC (3) | 3.3V | Main power supply |
| TX (4) | Digital Pin 2 | UART transmit |
| RX (5) | Digital Pin 4 | UART receive |
| GND (6) | GND | Ground |

**Important**: Add a 10kΩ pull-up resistor on the RX line (Arduino side).

## Quick Start

```
#include <FPM383C.h>

// Initialize sensor (RX pin 2, TX pin 4, Touch interrupt pin 3)
FPM383C fingerprint(2, 4, 3);

void setup() {
Serial.begin(115200);

if (fingerprint.begin()) {
Serial.println("Sensor initialized successfully!");
fingerprint.setLED(FP_LED_MODE_ON, FP_LED_GREEN);
} else {
Serial.println("Failed to initialize sensor!");
}
}

void loop() {
// Your code here
}
```

## Examples

See the `examples` folder for complete usage examples:

- **BasicUsage**: Simple enrollment and matching
- **Enrollment**: Detailed fingerprint enrollment process
- **Matching**: Fingerprint verification and matching
- **AdvancedFeatures**: LED control, sleep mode, and advanced features

## API Reference

### Initialization
- `bool begin(uint32_t baudrate = 57600)`
- `bool setPassword(uint32_t newPassword)`
- `bool heartbeat()`
- `bool reset()`

### Fingerprint Operations
- `bool startEnrollment(uint8_t regIndex)`
- `FingerprintEnrollResult queryEnrollmentResult()`
- `bool saveTemplate(uint16_t fingerprintId)`
- `bool autoEnroll(uint16_t fingerprintId, uint8_t enrollCount = 6, bool waitFingerLift = false)`
- `FingerprintMatchResult matchSync()`
- `bool deleteFingerprint(uint16_t fingerprintId)`

### System Functions
- `bool setLED(uint8_t mode, uint8_t color, uint8_t param1 = 0, uint8_t param2 = 0, uint8_t param3 = 0)`
- `bool setSleepMode(uint8_t mode = 0)`
- `uint16_t getTemplateCount()`
- `String getModuleId()`

## License

This library is released under the MIT License.

## Contributing

Contributions are welcome! Please read the contributing guidelines before submitting pull requests.
