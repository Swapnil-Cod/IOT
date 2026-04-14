#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <ArduinoOTA.h>
#include <time.h>
#include "esp_system.h"

// ——— AP Config ——————————————————————————————————————
static const char* AP_SSID = "HOMI";  // Open network

// ——— Hardware ————————————————————————————————————————
#define LED_PIN     2
#define SWITCH1_PIN 13
#define SWITCH2_PIN 12
#define SWITCH3_PIN 14
#define SWITCH4_PIN 27
#define DHT_PIN     15
#define DHT_TYPE    DHT11

// ——— MQTT Broker ————————————————————————————————————
static const char* MQTT_SERVER = "13.203.114.29";
static const int   MQTT_PORT   = 1883;
static const char* MQTT_USER   = "swap123";
static const char* MQTT_PASS   = "swap@123";

// ——— Globals —————————————————————————————————————————
WebServer      httpServer(80);
Preferences    prefs;
WiFiClient     netClient;
PubSubClient   mqtt(netClient);
DHT            dht(DHT_PIN, DHT_TYPE);

bool wifiOK = false;
bool mqttOK = false;

char ssidBuf[32] = {0};
char passBuf[64] = {0};
char macStr[13]; // 12-character MAC
char lwtTopic[64];

unsigned long lastBlink   = 0;
bool ledState = LOW;

// Blink intervals (ms)
const unsigned long BLINK_INTERVAL_WIFI_DOWN  = 500;  // while not connected to WiFi/internet
const unsigned long BLINK_INTERVAL_WIFI_UP    = 1000; // while WiFi connected but MQTT not connected

const unsigned long MQTT_TIMEOUT   = 30000;  // 30s timeout
const unsigned long PUBLISH_INTERVAL = 30000; // 30 sec
unsigned long lastPublish = 0;

// ——— Switch states & explicit pin map —————————————————————
bool switchState[4] = {false, false, false, false};
const uint8_t relayPins[4] = { SWITCH1_PIN, SWITCH2_PIN, SWITCH3_PIN, SWITCH4_PIN };

// ——— Helper: Get Efuse MAC for topics ———————————————————
void getMacString() {
  uint64_t chipid = ESP.getEfuseMac();
  sprintf(macStr, "%012llX", chipid);  // 12-character uppercase
  snprintf(lwtTopic, sizeof(lwtTopic), "homi/%s/device/status", macStr);
  Serial.printf("[SYS] Device MAC (Efuse) for topics: %s\n", macStr);
}

// ——— Save/load credentials ————————————————————————
void saveCredentials(const char* ssid, const char* pass) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
  Serial.println("[WiFi] Credentials saved.");
}

bool loadCredentials() {
  prefs.begin("wifi", true);
  String s = prefs.getString("ssid", "");
  String p = prefs.getString("pass", "");
  prefs.end();
  if (s.length() && p.length()) {
    s.toCharArray(ssidBuf, sizeof(ssidBuf));
    p.toCharArray(passBuf, sizeof(passBuf));
    Serial.printf("[WiFi] Loaded creds: %s\n", ssidBuf);
    return true;
  }
  return false;
}

// ——— Provisioning Handler ———————————————————————————————
void handleProvision() {
  if (httpServer.method() != HTTP_POST) {
    httpServer.send(405);
    return;
  }

  String body = httpServer.arg("plain");
  Serial.println("[HTTP] Provision request: " + body);

  StaticJsonDocument<200> doc;
  if (deserializeJson(doc, body)) {
    httpServer.send(400);
    return;
  }

  const char* s = doc["ssid"];
  const char* p = doc["password"];
  if (!s || !p) {
    httpServer.send(400);
    return;
  }

  strlcpy(ssidBuf, s, sizeof(ssidBuf));
  strlcpy(passBuf, p, sizeof(passBuf));
  saveCredentials(ssidBuf, passBuf);

  // Send immediate response with Efuse MAC
  getMacString();
  StaticJsonDocument<128> resp;
  resp["status"] = "ok";
  resp["mac"] = macStr;
  resp["devices"] = 4;

  String json;
  serializeJson(resp, json);
  httpServer.send(200, "application/json", json);

  // Switch to STA mode in background
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssidBuf, passBuf);
  Serial.println("[WiFi] Connecting to new network...");
}

// ——— Start AP mode ————————————————————————————————
void startProvisionAP() {
  wifiOK = mqttOK = false;
  Serial.println("\n=== Starting AP Mode ===");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  Serial.print("[AP] IP: ");
  Serial.println(WiFi.softAPIP());

  httpServer.on("/connect", HTTP_POST, handleProvision);
  httpServer.begin();
  Serial.println("[HTTP] Server ready on /connect");
}

// ——— MQTT message handler ——————————————————————————
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String msg;
  for (unsigned i = 0; i < len; i++) msg += (char)payload[i];
  Serial.printf("[MQTT] IN [%s]: %s\n", topic, msg.c_str());

  // Factory reset
  String resetTopic = String("homi/") + macStr + "/reset";
  if (String(topic) == resetTopic && msg == "factory_reset") {
    Serial.println("[SYS] Factory reset triggered!");
    prefs.begin("wifi", false);
    prefs.clear();
    prefs.end();
    mqtt.publish(resetTopic.c_str(), "resetting", true);
    delay(100);
    ESP.restart();
    return;
  }

  // Switch handling — use relayPins[] to address correct pins
  for (int i = 0; i < 4; i++) {
    String cmdTopic = String("homi/") + macStr + "/s" + (i + 1) + "/cmd";
    String statusTopic = String("homi/") + macStr + "/s" + (i + 1) + "/status";

    if (String(topic) == cmdTopic) {
      if (msg == "on") switchState[i] = true;
      else if (msg == "off") switchState[i] = false;

      digitalWrite(relayPins[i], switchState[i] ? HIGH : LOW);
      mqtt.publish(statusTopic.c_str(), switchState[i] ? "on" : "off", true);

      Serial.printf("→ s%d %s (pin %d)\n", i + 1, switchState[i] ? "ON" : "OFF", relayPins[i]);
    }
  }
}

// ——— Ensure MQTT Connected ——————————————————————————
bool ensureMQTT() {
  unsigned long start = millis();
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  Serial.println("[MQTT] Connecting...");
  while (!mqtt.connected() && millis() - start < MQTT_TIMEOUT) {
    if (mqtt.connect(macStr, MQTT_USER, MQTT_PASS, lwtTopic, 1, true, "offline")) {
      mqttOK = true;
      Serial.println("[MQTT] Connected.");

      // Subscribe switch topics
      for (int i = 0; i < 4; i++) {
        String cmdTopic = String("homi/") + macStr + "/s" + (i + 1) + "/cmd";
        String statusTopic = String("homi/") + macStr + "/s" + (i + 1) + "/status";
        mqtt.subscribe(cmdTopic.c_str());
        mqtt.publish(statusTopic.c_str(), switchState[i] ? "on" : "off", true);
      }

      // Subscribe reset topic
      String resetTopic = String("homi/") + macStr + "/reset";
      mqtt.subscribe(resetTopic.c_str());

      // Publish LWT online
      mqtt.publish(lwtTopic, "online", true);
      return true;
    }
    delay(1000);
    Serial.print(".");
  }

  mqttOK = false;
  Serial.println("\n[MQTT] Failed to connect within timeout.");
  return false;
}

// ——— Publish temperature and humidity ———————————————————
void publishSensorData() {
  float T = dht.readTemperature();
  float H = dht.readHumidity();

  if (isnan(T) || isnan(H)) {
    Serial.println("[DHT] Read failed");
    return;
  }

  time_t now = time(nullptr);
  struct tm ti;
  localtime_r(&now, &ti);
  char ts[32];
  strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &ti);

  StaticJsonDocument<128> doc;
  doc["timestamp"] = ts;
  doc["temperature"] = T;
  doc["humidity"] = H;

  char buf[128];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  mqtt.publish(("homi/" + String(macStr) + "/sensor").c_str(), buf, n); // NOT retained

  Serial.printf("[SENSOR] Published: %s\n", buf);
}

// ——— Setup & Loop ————————————————————————————————
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Booting HOMI Device ===");

  pinMode(LED_PIN, OUTPUT);
  pinMode(SWITCH1_PIN, OUTPUT);
  pinMode(SWITCH2_PIN, OUTPUT);
  pinMode(SWITCH3_PIN, OUTPUT);
  pinMode(SWITCH4_PIN, OUTPUT);

  // Ensure relays are off initially
  for (int i = 0; i < 4; i++) {
    digitalWrite(relayPins[i], LOW);
  }

  // Start with LED off
  ledState = LOW;
  digitalWrite(LED_PIN, LOW);
  lastBlink = millis();

  dht.begin();
  getMacString(); // Get Efuse MAC for topics

  if (loadCredentials()) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssidBuf, passBuf);
    Serial.printf("[WiFi] Connecting to %s\n", ssidBuf);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      wifiOK = true;
      Serial.printf("\n[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());

      // Sync time
      configTime(19800, 0, "pool.ntp.org", "time.nist.gov");
      time_t now = time(nullptr);
      while (now < 1000000000UL) { delay(500); now = time(nullptr); }
      Serial.println("[TIME] Time synced");
    } else {
      startProvisionAP();
    }
  } else {
    startProvisionAP();
  }

  ArduinoOTA.begin();
}

void loop() {
  httpServer.handleClient();
  ArduinoOTA.handle();

  // Manage connections
  if (wifiOK && !mqttOK) {
    if (!ensureMQTT()) {
      Serial.println("[SYS] MQTT connection failed. Returning to AP mode...");
      startProvisionAP();
    }
  }

  if (mqttOK) {
    mqtt.loop();

    // Publish sensor data every 30 sec
    if (millis() - lastPublish > PUBLISH_INTERVAL) {
      lastPublish = millis();
      publishSensorData();
    }
  }

  // --- LED state machine ---
  // Desired behavior:
  // * if mqttOK -> LED solid ON
  // * else if wifiOK == false -> blink every 0.5s
  // * else if wifiOK == true && mqttOK == false -> blink every 1s

  unsigned long now = millis();

  if (mqttOK) {
    // Solid ON; ensure LED is HIGH and don't toggle
    if (!ledState) {
      ledState = true;
      digitalWrite(LED_PIN, HIGH);
    }
    // keep lastBlink updated so when we fall back to blinking timing is correct
    lastBlink = now;
  } else {
    unsigned long interval = wifiOK ? BLINK_INTERVAL_WIFI_UP : BLINK_INTERVAL_WIFI_DOWN;
    if (now - lastBlink >= interval) {
      lastBlink = now;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }
  }
}
