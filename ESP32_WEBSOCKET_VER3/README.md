# ESP32 WebSocket I2C LED Controller

## Overview

This project allows the ESP32 to act as a WebSocket server that controls multiple LED strips connected via I2C. It listens for WebSocket messages and sends the corresponding I2C commands to control the initialization and manipulation of LEDs on remote devices. This can be useful for creating a centralized LED controller, especially for environments with multiple LED strips requiring remote management.

The system uses **WebSocket** for communication and **I2C** for sending control commands to the LED strips. The communication format is based on JSON, making it flexible and easy to extend.

## Features

* **WebSocket Server**: Allows real-time communication between clients (e.g., mobile apps or web interfaces) and the ESP32.
* **I2C Communication**: Sends initialization and LED control commands to external devices (I2C slaves).
* **LED Control**: Supports commands to control LED strips, including:

  * Initialization of LED strips (setting up strip index, pin, and LED count).
  * LED manipulation (setting LED colors, brightness, and command types like ON/OFF).

## Hardware Requirements

* **ESP32** development board
* **I2C-capable LED controller** connected to the ESP32 via I2C
* Multiple **LED strips** (compatible with the I2C LED controller)

## Software Requirements

* **Arduino IDE** with ESP32 support installed
* **Libraries**:

  * `WiFi.h` for Wi-Fi management
  * `WebSocketsServer.h` for WebSocket communication
  * `Wire.h` for I2C communication
  * `ArduinoJson.h` for parsing JSON data

## Wiring

* **ESP32** connected to an I2C LED controller (e.g., via GPIO pins for I2C communication).
* The **LED strips** are controlled through I2C commands sent by the ESP32.

## Code Explanation

### Libraries & Setup

The code begins by including the necessary libraries:

* `WiFi.h` for setting up the ESP32 as a Wi-Fi access point.
* `WebSocketsServer.h` to handle WebSocket communication.
* `Wire.h` for I2C communication.
* `ArduinoJson.h` to handle incoming JSON data.

### Global Variables

* **Wi-Fi Configuration**: The ESP32 is set up as an access point with the SSID `ESP32-Control-Hub` and password `password123`.
* **WebSocket Server**: The ESP32 starts a WebSocket server on port 81.
* **I2C Command Structures**: The structs `I2C_Initialize_Command` and `I2C_Led_Command` are used to define the commands sent over I2C for initialization and LED control.

### WebSocket Event Handler

* The function `webSocketEvent` handles incoming WebSocket messages.
* The WebSocket server listens for JSON messages containing `cmd_type` and `i2c_address`. Based on the `cmd_type`, it processes the message as either an initialization command (type `1`) or an LED command (type `2`).

  * **Initialization Command**: The ESP32 sends the `I2C_Initialize_Command` to configure the LED strips connected to the I2C device.
  * **LED Command**: The ESP32 sends a series of `I2C_Led_Command` messages to control specific LEDs, including their color, brightness, and command (ON/OFF).

### Setup Function

* Initializes the serial communication and the I2C interface.
* Sets up the ESP32 as a Wi-Fi access point.
* Starts the WebSocket server and assigns the event handler.

### Loop Function

* The `loop()` function continuously checks for incoming WebSocket messages, ensuring that the server remains responsive.

## Example JSON Commands

### Initialization Command

To initialize LED strips, send the following JSON message:

```json
{
  "cmd_type": 1,
  "i2c_address": 8,
  "strips": [
    {
      "strip_index": 0,
      "pin": 5,
      "led_count": 50
    },
    {
      "strip_index": 1,
      "pin": 6,
      "led_count": 100
    }
  ]
}
```

### LED Control Command

To send LED control commands, use the following JSON format:

```json
{
  "cmd_type": 2,
  "i2c_address": 8,
  "led_commands": [
    {
      "strip_index": 0,
      "start_led": 0,
      "led_count": 10,
      "command": 1,
      "r": 255,
      "g": 0,
      "b": 0,
      "brightness": 255
    },
    {
      "strip_index": 1,
      "start_led": 10,
      "led_count": 20,
      "command": 1,
      "r": 0,
      "g": 255,
      "b": 0,
      "brightness": 128
    }
  ]
}
```

* `strip_index`: The index of the LED strip.
* `start_led`: The starting LED index for the command.
* `led_count`: The number of LEDs to affect.
* `command`: The action to perform (e.g., 1 for ON, 0 for OFF).
* `r`, `g`, `b`: The color of the LEDs (RGB values).
* `brightness`: The brightness of the LEDs (0-255).

## How to Use

1. **Upload the Code**: Flash the ESP32 with the provided code using the Arduino IDE.
2. **Connect to the Access Point**: After the ESP32 boots, connect your device (PC, phone, etc.) to the Wi-Fi network `ESP32-Control-Hub`.
3. **Send WebSocket Commands**: Use a WebSocket client (e.g., a web app or mobile app) to send JSON commands to the ESP32's WebSocket server (default port: 81).
4. **Control LEDs**: The ESP32 will process the commands and send the corresponding I2C signals to the connected LED controller.

## Troubleshooting

* **WebSocket Not Connecting**: Ensure that your device is connected to the `ESP32-Control-Hub` network.
* **LED Not Responding**: Verify the I2C connections and ensure the I2C address matches between the ESP32 and the LED controller.

## License

This project is licensed under the MIT License.