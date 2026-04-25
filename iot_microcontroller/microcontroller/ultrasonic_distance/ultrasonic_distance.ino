// AJ-SR04M (Mode 1/2) distance readout for ESP32
// Publishes distance over MQTT (Mosquitto) via local WiFi
// Works on 3.3V. If powering sensor at 5V, level-shift ECHO to 3.3V!

#include <WiFi.h>
#include <PubSubClient.h>

// -------- WiFi credentials --------
const char* WIFI_SSID     = "Airtel_Devki_house";
const char* WIFI_PASSWORD = "Bhrt@0801";

// -------- MQTT broker (Mosquitto) --------
const char* MQTT_BROKER   = "192.168.1.17";   // this Windows machine running Mosquitto
const int   MQTT_PORT     = 1883;
const char* MQTT_TOPIC    = "sensor/ultrasonic/distance";
const char* MQTT_CLIENT   = "esp32-ultrasonic";

// -------- pin selection --------
const int PIN_TRIG = 5;    // change to your wiring
const int PIN_ECHO = 18;   // change to your wiring

// measurement settings
const uint32_t PULSE_TIMEOUT_US = 30'000; // ~5 m max one-way (safe), adjust as needed
const int SAMPLES = 5;                    // median of N readings
const float MIN_VALID_CM = 20.0;          // datasheet "blind zone" (approx)
const float MAX_VALID_CM = 800.0;         // practical upper bound for AJ-SR04M

WiFiClient   espClient;
PubSubClient mqttClient(espClient);

// speed of sound (temp-compensated): c = 331.3 + 0.606*T  [m/s]
float speedOfSound_cm_per_us(float tempC) {
  float c_ms = 331.3f + 0.606f * tempC;
  return (c_ms * 100.0f) / 1'000'000.0f;  // cm/us
}

// single raw reading in microseconds
uint32_t measureEchoUs() {
  // ensure clean trigger
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);

  // 10 us trigger pulse
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(12);
  digitalWrite(PIN_TRIG, LOW);

  // measure HIGH time on echo (timeout to avoid blocking)
  return pulseIn(PIN_ECHO, HIGH, PULSE_TIMEOUT_US);
}

// median filter helper
float medianDistanceCm(float tempC) {
  uint32_t us[SAMPLES];
  for (int i = 0; i < SAMPLES; i++) {
    us[i] = measureEchoUs();
    delay(50);
  }
  // sort small array
  for (int i = 0; i < SAMPLES - 1; i++) {
    for (int j = i + 1; j < SAMPLES; j++) {
      if (us[j] < us[i]) { auto t = us[i]; us[i] = us[j]; us[j] = t; }
    }
  }

  uint32_t echoUs = us[SAMPLES / 2]; // median
  if (echoUs == 0) return NAN;       // timeout

  float v = speedOfSound_cm_per_us(tempC);
  float cm = (echoUs * v) / 2.0f;    // divide by 2: out-and-back time

  if (cm < MIN_VALID_CM || cm > MAX_VALID_CM) return NAN;
  return cm;
}

void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());
  delay(1000);  // let WiFi stack fully stabilize before MQTT
}

void connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT broker...");
    if (mqttClient.connect(MQTT_CLIENT)) {
      Serial.println("connected");
    } else {
      Serial.print("failed (rc=");
      Serial.print(mqttClient.state());
      Serial.println("), retrying in 3s");
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);

  connectWiFi();
  mqttClient.setBufferSize(512);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setSocketTimeout(15);
  mqttClient.setKeepAlive(60);
}

void loop() {
  // reconnect WiFi / MQTT if dropped
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqttClient.connected()) connectMQTT();
  mqttClient.loop();

  // If you don't have a temperature sensor, set this to ambient (e.g., 25 C)
  float ambientC = 25.0f;
  float d = medianDistanceCm(ambientC);

  char payload[48];

  if (isnan(d)) {
    Serial.println("Out of range / timeout");
    snprintf(payload, sizeof(payload), "{\"error\":\"out_of_range\"}");
  } else {
    Serial.print("Distance: ");
    Serial.print(d, 1);
    Serial.println(" cm");
    // format as JSON: {"distance_cm":123.4}
    char numBuf[16];
    dtostrf(d, 1, 1, numBuf);
    snprintf(payload, sizeof(payload), "{\"distance_cm\":%s}", numBuf);
  }

  mqttClient.publish(MQTT_TOPIC, payload);
  Serial.print("Published to ");
  Serial.print(MQTT_TOPIC);
  Serial.print(": ");
  Serial.println(payload);

  delay(10000);
}
