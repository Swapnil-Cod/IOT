# Quick Start Guide

Welcome to the quick start guide for the ESP32 firmware and MQTT-driven device model project. This guide will help you get up and running with minimal setup.

## Prerequisites

Before you begin, ensure you have the following:

- An ESP32 development board
- A computer with the Arduino IDE or PlatformIO installed
- Basic knowledge of MQTT and IoT concepts

## Step 1: Install Required Software

1. **Install the Arduino IDE** or **PlatformIO**:
   - For Arduino IDE, download it from [the official Arduino website](https://www.arduino.cc/en/software).
   - For PlatformIO, follow the installation instructions on [the PlatformIO website](https://platformio.org/install).

2. **Install the ESP32 Board Package**:
   - In the Arduino IDE, go to `File` > `Preferences`, and add the following URL to the "Additional Board Manager URLs":
     ```
     https://dl.espressif.com/dl/package_esp32_index.json
     ```
   - Then, go to `Tools` > `Board` > `Boards Manager`, search for "ESP32", and install the package.

3. **Install Required Libraries**:
   - Install the necessary libraries for MQTT and Wi-Fi. You can find libraries like `PubSubClient` for MQTT and `WiFi` in the Library Manager.

## Step 2: Clone the Repository

Clone the project repository to your local machine using the following command:

```bash
git clone https://github.com/yourusername/your-repo.git
```

Navigate to the project directory:

```bash
cd your-repo
```

## Step 3: Configure Your Device

1. Open the example sketch in the `src` folder that corresponds to your device.
2. Update the Wi-Fi credentials and MQTT broker settings in the code.

## Step 4: Upload the Firmware

1. Connect your ESP32 board to your computer.
2. Select the correct board and port in the Arduino IDE or PlatformIO.
3. Upload the firmware to your ESP32 board.

## Step 5: Monitor the Output

Open the Serial Monitor in the Arduino IDE or PlatformIO to view the output from your device. You should see messages indicating the connection status and any published states.

## Step 6: Start Using Your Device

Once the device is connected to the MQTT broker, you can start sending commands and receiving state updates. Refer to the [API documentation](../reference/api.md) for details on available commands and topics.

Congratulations! You have successfully set up your ESP32 device with MQTT. For further information, check the other documentation sections.