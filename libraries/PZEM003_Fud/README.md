# PZEM003_Fud - Arduino Library for PZEM-003 with RS485 Auto



## Overview
**PZEM003_Fud** is a lightweight and easy-to-use **Arduino library** for interfacing with the **PZEM-003 power meter** using **RS485 Auto**. This library simplifies communication with the sensor by providing functions to read **voltage, current, power, and energy** directly from the module.

It supports various Arduino boards, including Arduino Uno, Mega, ESP8266 (NodeMCU), and ESP32, using SoftwareSerial for communication.

This library was inspired by the communication approach from [Project Semesin](https://www.project.semesin.com/2019/09/14/komunikasi-modbus-dengan-arduino-sebagai-master-read-input-register-tanpa-library/#comment-4556). However, instead of using a manual RS485 module, this library is designed for RS485 Auto, making the setup and usage much simpler. The code structure has been optimized, simplified, and packaged into a reusable Arduino library for easier integration into projects.

---

## Features
- Supports **PZEM-003** power meter
- Works with **RS485 Auto** module (simpler setup)
- Reads **voltage, current, power, and energy**
- Includes **energy reset function**
- Uses **SoftwareSerial** for Arduino boards with limited UART ports
- Open-source and lightweight

---

## Wiring Diagram
Below is the connection diagram for **PZEM-003 with Arduino using RS485 Auto**:



### **Connections:**

| **PZEM-003** | **RS485 Auto** | **Arduino (Uno/Mega, etc.)** | **ESP8266 / ESP32** |
|-------------|---------------|--------------------------|---------------------|
| A (RS485+) | A ---- RX     | D7 (SoftwareSerial RX)  | GPIO (e.g., 16 for RX) |
| B (RS485-) | B ---- TX     | D8 (SoftwareSerial TX)  | GPIO (e.g., 17 for TX) |
| VCC        | VCC (5V/3.3V) | 5V                       | 3.3V or 5V (check module support) |
| GND        | GND           | GND                      | GND |

---

## Installation

### 1. Install via Arduino Library Manager (Future Support)
- Buka Arduino IDE
- Pilih Sketch -> Include Library -> Manage Libraries
C- ari PZEM003_Fud dan klik Install

### 2. Manual Installation
1. **Download the repository**
   - Clone via Git:
     ```sh
     git clone https://github.com/makhfud/PZEM003_Fud.git
     ```
   - Or download the ZIP from GitHub

2. **Move the library folder**
   - Extract and place the `PZEM003_Fud` folder into:
     ```
     Documents/Arduino/libraries/
     ```

3. **Restart Arduino IDE**

---

## Usage

### Basic Example
```cpp
#include <PZEM003_Fud.h>

// Define RX and TX pins (SoftwareSerial)
PZEM003_Fud pzem(7, 8);  // RX, TX

void setup() {
    Serial.begin(115200);
    pzem.begin(9600);
}

void loop() {
    pzem.update();
    Serial.print("Voltage: ");
    Serial.print(pzem.getVoltage());
    Serial.println(" V");
    delay(1000);
}
```

---

## Functions

- `begin(baudRate)` → Initializes communication with the PZEM-003 module
- `update()` → Reads data from the module
- `resetEnergy()` → Resets the energy counter to zero
- `getVoltage()` → Returns the measured voltage (V)
- `getCurrent()` → Returns the measured current (A)
- `getPower()` → Returns the measured power (W)
- `getEnergy()` → Returns the accumulated energy (Wh)

---

## License
This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

---

## Author
📌 **M. Makhfud**  
📧 Email: m.makhfud.rt@gmail.com  
