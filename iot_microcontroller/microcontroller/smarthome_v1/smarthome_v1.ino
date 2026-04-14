#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#define LED_PIN 2  // Onboard LED pin for ESP32

AsyncWebServer server(80);
String ssid = "";
String password = "";
bool credentialsReceived = false;
bool isConnectedToWiFi = false;
unsigned long previousMillis = 0;
const long interval = 500;
bool ledState = LOW;

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Start Access Point
  WiFi.softAP("ESP32-Setup", "password123");
  Serial.println("Access Point started.");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Endpoint to receive SSID and password
  server.on("/connect", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    String body = "";
    for (size_t i = 0; i < len; i++) {
      body += (char)data[i];
    }
    Serial.println("Received data: " + body);

    // Parse JSON
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
      request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
      return;
    }

    ssid = doc["ssid"].as<String>();
    password = doc["password"].as<String>();
    Serial.println("Received SSID: " + ssid);
    Serial.println("Received Password: " + password);

    credentialsReceived = true;  // Flag that credentials are available
    request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Credentials received\"}");
  });

  server.begin();
  Serial.println("Web server started.");
}

void connectToWiFi() {
  if (credentialsReceived && ssid.length() > 0 && password.length() > 0) {
    Serial.println("Connecting to WiFi...");
    WiFi.begin(ssid.c_str(), password.c_str());
    int retryCount = 0;

    while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
      delay(500); // Allow LED blinking to continue
      Serial.print(".");
      retryCount++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      isConnectedToWiFi = true;
      digitalWrite(LED_PIN, HIGH);  // Turn LED ON when connected
      Serial.println("\nConnected to WiFi!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());

      // Turn off Access Point
      WiFi.softAPdisconnect(true);
      Serial.println("Access Point disabled.");
    } else {
      Serial.println("\nFailed to connect to WiFi. Retrying...");
      delay(5000);
    }
  }
}

void checkStatus() {
    // WiFiClient client = server.available();   // Listen for incoming clients
    
    // if (client) {                             // If a new client connects
    //     Serial.println("New Client Connected.");          // Print a message to the serial monitor

        // Check and print the number of stations connected if running in SoftAP mode
        int stationCount = WiFi.softAPgetStationNum();
        Serial.print("Connected Stations: ");
        Serial.println(stationCount);
        delay(5000);

        // Optionally, handle client communication here
        // Example: while (client.connected()) { client.read(), client.write() etc. }
    // }
}


void handleLEDBlink() {
  if (!isConnectedToWiFi) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      ledState = !ledState; // Toggle LED state
      digitalWrite(LED_PIN, ledState);
    }
  } else {
    digitalWrite(LED_PIN, HIGH);  // Steady ON when connected
  }
}

void loop() {
  checkStatus();
  if (WiFi.status() != WL_CONNECTED && credentialsReceived) {
    connectToWiFi();
  }

  handleLEDBlink();
}
