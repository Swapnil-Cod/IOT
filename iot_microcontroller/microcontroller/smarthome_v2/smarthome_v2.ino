#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <DHT.h>

// --- AP credentials ---
const char* AP_SSID     = "ESP32_Config";
const char* AP_PASSWORD = "12345678";

// --- LED & blink timing ---
const int LED_PIN             = 2;
const unsigned long BLINK_MS  = 500;

// --- DHT11 sensor ---
#define DHTPIN   15
#define DHTTYPE  DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- MQTT Broker Details ---
const char* mqtt_server     = "13.234.149.187";
const int   mqtt_port       = 1883;
const char* mqtt_user       = "swap123";
const char* mqtt_password   = "swap@123";
const char* mqtt_clientId   = "Swap_Device0001";
const char* topic_publish   = "Swap_SensorData";
const char* topic_subscribe = "Swap_CommandRequest";

// —————————————————————————————————————

WebServer    server(80);
WiFiClient   netClient;
PubSubClient mqtt(netClient);

String       targetSSID, targetPW;
bool         wifiOK  = false;
bool         mqttOK  = false;

unsigned long lastBlink    = 0;
bool          ledState     = LOW;

unsigned long lastPublish  = 0;
const unsigned long publishInterval = 5000;

// Forward declarations
void startAP();
void connectToWiFi();
void setupMQTT();
void reconnectMQTT();
void mqttCallback(char*, byte*, unsigned int);
void publishSensorData();

// ─── HTTP handler ─────────────────────────────────────────────────────────────
void handleConnect() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"status\":\"method_not_allowed\"}");
    return;
  }
  String body = server.arg("plain");
  if (body.isEmpty()) {
    server.send(400, "application/json", "{\"status\":\"empty_body\"}");
    return;
  }
  StaticJsonDocument<200> doc;
  auto err = deserializeJson(doc, body);
  if (err) {
    server.send(400, "application/json", "{\"status\":\"bad_json\"}");
    return;
  }
  targetSSID = doc["ssid"] | "";
  targetPW   = doc["password"] | "";
  if (targetSSID.isEmpty() || targetPW.isEmpty()) {
    server.send(400, "application/json", "{\"status\":\"missing_ssid_or_password\"}");
    return;
  }

  // Echo for debug
  Serial.printf("Received SSID:     %s\n", targetSSID.c_str());
  Serial.printf("Received Password: %s\n", targetPW.c_str());

  server.send(200, "application/json", "{\"status\":\"connecting\"}");
  delay(100);  // allow response to flush
  connectToWiFi();
}

// ─── Start AP & HTTP server ───────────────────────────────────────────────────
void startAP() {
  wifiOK  = false;
  mqttOK  = false;
  digitalWrite(LED_PIN, LOW);
  lastBlink = millis();

  Serial.println("Starting AP…");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("  AP IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/connect", HTTP_POST, handleConnect);
  server.begin();
}

// ─── Tear down AP → join STA → then setup MQTT ────────────────────────────────
void connectToWiFi() {
  Serial.println("Tearing down AP…");
  WiFi.softAPdisconnect(true);

  WiFi.mode(WIFI_STA);
  WiFi.begin(targetSSID.c_str(), targetPW.c_str());
  Serial.printf("Joining Wi-Fi \"%s\" …", targetSSID.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifiOK = true;
    Serial.println("✅ Wi-Fi connected");
    Serial.print("   IP: ");
    Serial.println(WiFi.localIP());
    setupMQTT();
  } else {
    Serial.println("❌ Wi-Fi failed. Restarting AP…");
    startAP();
  }
}

// ─── MQTT setup & reconnect ───────────────────────────────────────────────────
void setupMQTT() {
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(mqttCallback);
  reconnectMQTT();
}

void reconnectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Connecting to MQTT…");
    if (mqtt.connect(mqtt_clientId, mqtt_user, mqtt_password)) {
      Serial.println("✅ MQTT up");
      mqtt.subscribe(topic_subscribe);
      Serial.print("  Subscribed to ");
      Serial.println(topic_subscribe);
      mqttOK = true;
      // Stop blinking, light steady
      digitalWrite(LED_PIN, HIGH);
    } else {
      Serial.print("❌ rc=");
      Serial.print(mqtt.state());
      Serial.println(" retry in 5s");
      delay(5000);
    }
  }
}

// ─── Handle incoming MQTT “on”/“off” ───────────────────────────────────────────
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String msg;
  for (unsigned i = 0; i < len; i++) msg += (char)payload[i];
  Serial.printf("MQTT in [%s]: %s\n", topic, msg.c_str());

  if (String(topic) == topic_subscribe) {
    if (msg == "on") {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED → ON");
    }
    else if (msg == "off") {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED → OFF");
    }
  }
}

// ─── Publish DHT11 every 5 s ──────────────────────────────────────────────────
void publishSensorData() {
  float T = dht.readTemperature();
  float H = dht.readHumidity();
  if (isnan(T) || isnan(H)) {
    Serial.println("DHT read failed");
    return;
  }
  String pl = "{\"temperature\": " + String(T,1)
            + ", \"humidity\": " + String(H,1) + "}";
  mqtt.publish(topic_publish, pl.c_str());
  Serial.print("Published: ");
  Serial.println(pl);
}

// ─── Setup & Loop ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  dht.begin();
  startAP();
}

void loop() {
  server.handleClient();

  unsigned long now = millis();

  // blink until BOTH Wi-Fi & MQTT are up
  if (!(wifiOK && mqttOK)) {
    if (now - lastBlink >= BLINK_MS) {
      lastBlink = now;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
  }

  // once Wi-Fi’s up, keep MQTT alive and publish
  if (wifiOK) {
    if (!mqttOK) {
      reconnectMQTT();
    } else {
      mqtt.loop();
      if (now - lastPublish >= publishInterval) {
        lastPublish = now;
        publishSensorData();
      }
    }
  }
}
