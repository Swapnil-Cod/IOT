#include <WiFi.h>     // For Wi-Fi MAC
#include "esp_system.h"

#ifdef ARDUINO_ARCH_ESP32
  #include "esp_wifi.h"
  #include "esp_bt.h"
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== ESP32 System Information ===");

  // Chip info
  Serial.printf("Chip Model: %s\n", ESP.getChipModel());
  Serial.printf("Chip Revision: %d\n", ESP.getChipRevision());
  Serial.printf("CPU Cores: %d\n", ESP.getChipCores());

  // Clock speed
  Serial.printf("CPU Frequency: %d MHz\n", getCpuFrequencyMhz());

  // Flash
  Serial.printf("Flash Chip Size: %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.printf("Flash Speed: %d Hz\n", ESP.getFlashChipSpeed());

  // Heap / SRAM
  Serial.printf("Free Heap (SRAM): %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Minimum Free Heap Since Boot: %d bytes\n", ESP.getMinFreeHeap());

  // PSRAM
  if (psramFound()) {
    Serial.println("PSRAM: Available");
    Serial.printf("  Total PSRAM: %d bytes\n", ESP.getPsramSize());
    Serial.printf("  Free PSRAM: %d bytes\n", ESP.getFreePsram());
  } else {
    Serial.println("PSRAM: Not available");
  }

  // Wi-Fi MAC
  Serial.printf("Wi-Fi MAC Address: %s\n", WiFi.macAddress().c_str());

  // Bluetooth availability
  #ifdef ARDUINO_ARCH_ESP32
    if (btStart()) {
      Serial.println("Bluetooth: Supported and Enabled");
      btStop(); // Stop to save power
    } else {
      Serial.println("Bluetooth: Not supported or not available");
    }
  #endif

  // SDK
  Serial.printf("SDK Version: %s\n", ESP.getSdkVersion());

  Serial.println("=================================");
}

void loop() {
  // Nothing in loop
}
