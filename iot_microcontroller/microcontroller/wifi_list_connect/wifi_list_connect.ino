#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#define LED_PIN 2  // Onboard LED pin for ESP32

AsyncWebServer server(80);
String ssid = "";       // Selected Wi-Fi SSID
String password = "";   // Wi-Fi Password
bool credentialsReceived = false;

void scanWiFiNetworks(AsyncWebServerRequest *request) {
  int n = WiFi.scanNetworks();  // Perform Wi-Fi network scan
  Serial.println("Scanning Wi-Fi networks...");
  String html = "<html><body>"
                "<h1>Select Wi-Fi Network</h1>"
                "<ul>";

  // List available networks as clickable links
  for (int i = 0; i < n; ++i) {
    String networkSSID = WiFi.SSID(i);
    html += "<li><a href='/select?ssid=" + networkSSID + "'>" + networkSSID + "</a></li>";
  }

  html += "</ul></body></html>";
  request->send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);

  // Set LED pin as output
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Start ESP32 in Access Point mode
  WiFi.softAP("ESP32-Setup", "password123");
  Serial.println("Access Point started.");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Serve a webpage to scan and list Wi-Fi networks
  server.on("/", HTTP_GET, scanWiFiNetworks);

  // Handle selection of Wi-Fi network
  server.on("/select", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("ssid")) {
      ssid = request->getParam("ssid")->value();
      Serial.println("Selected SSID: " + ssid);

      // Serve a page to enter password
      String html = "<html><body>"
                    "<h1>Enter Password for " + ssid + "</h1>"
                    "<form action='/submit' method='post'>"
                    "Password: <input type='password' name='password'><br>"
                    "<input type='submit' value='Connect'>"
                    "</form></body></html>";
      request->send(200, "text/html", html);
    } else {
      request->send(400, "text/html", "Error: Missing SSID parameter.");
    }
  });

  // Handle Wi-Fi credentials submission
  server.on("/submit", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("password", true)) {
      password = request->getParam("password", true)->value();
      Serial.println("Received Password: " + password);

      credentialsReceived = true;  // Flag that credentials are available

      request->send(200, "text/html", "Credentials received! Connecting to Wi-Fi...");
    } else {
      request->send(400, "text/html", "Error: Missing parameters.");
    }
  });

  server.begin();
  Serial.println("Web server started.");
}

void connectToWiFi() {
  if (credentialsReceived && ssid.length() > 0 && password.length() > 0) {
    Serial.println("Connecting to WiFi...");

    WiFi.begin(ssid.c_str(), password.c_str());
    int retryCount = 0;

    while (WiFi.status() != WL_CONNECTED && retryCount < 20) {  // Retry up to 20 times
      digitalWrite(LED_PIN, HIGH);
      delay(500);
      digitalWrite(LED_PIN, LOW);
      delay(500);
      Serial.print(".");
      retryCount++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      digitalWrite(LED_PIN, HIGH);  // Turn LED ON when connected
      Serial.println("\nConnected to WiFi!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());

      // Turn off Access Point
      WiFi.softAPdisconnect(true);
      Serial.println("Access Point disabled.");
      
    } else {
      Serial.println("\nFailed to connect to WiFi. Retrying...");
      delay(5000);  // Wait 5 seconds before retrying
    }
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED && credentialsReceived) {
    connectToWiFi();
  }
}
