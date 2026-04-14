#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <ArduinoOTA.h>
#include <time.h>

// ——— Provisioning AP credentials —————————————————————————————————————
static const char* AP_SSID = "MY HOME";
static const char* AP_PASS = "12345678";

// ——— Hardware & timing ————————————————————————————————————————————
#define LED_PIN         2
#define DHT_PIN         15
#define DHT_TYPE        DHT11

const unsigned long BLINK_INTERVAL   = 500;    // ms
const unsigned long PUBLISH_INTERVAL = 5000;   // ms

// ——— MQTT broker details ————————————————————————————————————————
static const char* MQTT_SERVER      = "13.234.149.187";
static const int   MQTT_PORT        = 1883;
static const char* MQTT_USER        = "swap123";
static const char* MQTT_PASS        = "swap@123";
static const char* MQTT_CLIENT_ID   = "Swap_Device0009";
static const char* TOPIC_PUB        = "Swap_SensorData";
static const char* TOPIC_SUB        = "Swap_CommandRequest";
static const char* LWT_TOPIC        = "Swap_Device0009/status";  // ✅ LWT topic

// ——— Globals —————————————————————————————————————————————————————
WebServer      httpServer(80);
Preferences    prefs;
DHT            dht(DHT_PIN, DHT_TYPE);
WiFiClient     netClient;
PubSubClient   mqtt(netClient);

bool           wifiOK  = false;
bool           mqttOK  = false;

char           ssidBuf[32]  = {0};
char           passBuf[64]  = {0};

unsigned long  lastBlink    = 0;
bool           ledState     = LOW;

unsigned long  lastPublish  = 0;
unsigned long  mqttBackoff  = 1000;

// ——— Helpers: store/load creds ———————————————————————————————————————
void saveCredentials(const char* ssid, const char* pass) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
  Serial.println("Credentials saved to flash");
}

bool loadCredentials() {
  prefs.begin("wifi", true);
  String s = prefs.getString("ssid", "");
  String p = prefs.getString("pass", "");
  prefs.end();
  if (s.length() && p.length()) {
    s.toCharArray(ssidBuf, sizeof(ssidBuf));
    p.toCharArray(passBuf, sizeof(passBuf));
    Serial.printf("Loaded creds: SSID=\"%s\"\n", ssidBuf);
    return true;
  }
  return false;
}

// ——— HTTP provisioning handler —————————————————————————————————————
void handleProvision() {
  if (httpServer.method() != HTTP_POST) {
    httpServer.send(405);
    return;
  }
  String body = httpServer.arg("plain");
  Serial.print("Provision request body: ");
  Serial.println(body);

  StaticJsonDocument<200> doc;
  if (deserializeJson(doc, body)) {
    httpServer.send(400);
    Serial.println("  → JSON parse error");
    return;
  }
  const char* s = doc["ssid"];
  const char* p = doc["password"];
  if (!s || !p) {
    httpServer.send(400);
    Serial.println("  → Missing ssid or password");
    return;
  }
  strlcpy(ssidBuf, s, sizeof(ssidBuf));
  strlcpy(passBuf, p, sizeof(passBuf));
  saveCredentials(ssidBuf, passBuf);

  httpServer.send(200, "application/json", "{\"status\":\"ok\"}");
  Serial.println("  → Acknowledged, tearing down AP");

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssidBuf, passBuf);
}

// ——— Start the AP & HTTP server for provisioning ————————————————————
void startProvisionAP() {
  wifiOK = mqttOK = false;
  digitalWrite(LED_PIN, LOW);
  lastBlink = millis();

  Serial.println("Starting provisioning AP...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("  AP IP: ");
  Serial.println(WiFi.softAPIP());

  httpServer.on("/connect", HTTP_POST, handleProvision);
  httpServer.begin();
  Serial.println("HTTP server started, waiting for POST /connect");
}

// ——— MQTT message callback (on/off/factory_reset) —————————————————————
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String msg;
  for (unsigned i = 0; i < len; i++) msg += (char)payload[i];
  Serial.printf("MQTT in [%s]: %s\n", topic, msg.c_str());

  if (String(topic) == TOPIC_SUB) {
    if (msg == "on") {
      digitalWrite(LED_PIN, HIGH);
      Serial.println(" → LED ON");
    }
    else if (msg == "off") {
      digitalWrite(LED_PIN, LOW);
      Serial.println(" → LED OFF");
    }
    else if (msg == "factory_reset") {
      Serial.println("⚠️ Factory reset command received!");
      prefs.begin("wifi", false);
      prefs.clear();  
      prefs.end();
      mqtt.publish(TOPIC_SUB, "resetting");
      delay(100);
      ESP.restart();
    }
  }
}

// ——— Wi-Fi event handlers ————————————————————————————————————————
void onWiFiGotIP(WiFiEvent_t, WiFiEventInfo_t) {
  wifiOK = true;
  Serial.print("Wi-Fi connected, IP = ");
  Serial.println(WiFi.localIP());

  // sync time via SNTP
  Serial.print("Syncing time via SNTP");
  configTime(19800, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 1000000000UL) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println();
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  Serial.print("Time synced: ");
  Serial.println(buf);

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqttOK = false;
  mqttBackoff = 1000;
}

void onWiFiDisconnected(WiFiEvent_t, WiFiEventInfo_t) {
  wifiOK = mqttOK = false;
  Serial.println("Wi-Fi disconnected, restarting provisioning AP");
  startProvisionAP();
}

// ——— MQTT reconnect with LWT & keepAlive ——————————————————————
void ensureMQTT() {
  if (mqtt.connected()) return;

  Serial.print("Connecting to MQTT broker…");

  if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS,
                   LWT_TOPIC, 1, true, "offline")) {
    mqtt.publish(LWT_TOPIC, "online", true);  // ✅ retained online status
    mqtt.subscribe(TOPIC_SUB);
    mqttOK = true;
    digitalWrite(LED_PIN, HIGH);
    Serial.println("OK, subscribed and LED solid");
  } else {
    mqttOK = false;
    Serial.printf("failed (rc=%d), retry in %lums\n",
                  mqtt.state(), mqttBackoff);
    delay(mqttBackoff);
    mqttBackoff = min(mqttBackoff * 2, 30000UL);
  }

  // ✅ Set keepAlive to 5s for fast offline detection
  mqtt.setKeepAlive(5);
}

// ——— Publish DHT11 readings + timestamp ————————————————————————————
void publishSensorData() {
  float T = dht.readTemperature();
  float H = dht.readHumidity();
  if (isnan(T) || isnan(H)) {
    Serial.println("DHT read failed");
    return;
  }

  time_t now = time(nullptr);
  struct tm ti;
  localtime_r(&now, &ti);
  char ts[32];
  strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &ti);

  StaticJsonDocument<128> doc;
  doc["timestamp"]   = ts;
  doc["temperature"] = T;
  doc["humidity"]    = H;
  char buf[128];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  mqtt.publish(TOPIC_PUB, buf, n);

  Serial.print("Published @ ");
  Serial.print(ts);
  Serial.print(" → ");
  Serial.println(buf);
}

// ——— Setup & Loop —————————————————————————————————————————————
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_PIN, OUTPUT);
  dht.begin();

  WiFi.onEvent(onWiFiGotIP,        ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(onWiFiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  if (loadCredentials()) {
    Serial.println("Joining saved Wi-Fi…");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssidBuf, passBuf);
  } else {
    startProvisionAP();
  }

  ArduinoOTA.begin();
  Serial.println("Setup complete");
}

void loop() {
  httpServer.handleClient();
  ArduinoOTA.handle();

  // blink LED until Wi-Fi & MQTT are up
  if (!(wifiOK && mqttOK)) {
    unsigned long now = millis();
    if (now - lastBlink >= BLINK_INTERVAL) {
      lastBlink = now;
      ledState  = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
  }

  if (wifiOK) {
    ensureMQTT();
    if (mqttOK) {
      mqtt.loop();
      if (millis() - lastPublish >= PUBLISH_INTERVAL) {
        lastPublish = millis();
        publishSensorData();
      }
    }
  }
}
