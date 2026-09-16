// AJ-SR04M (Mode 1/2) distance readout for ESP32
// Publishes distance over MQTT (Mosquitto) via local WiFi
// Works on 3.3V. If powering sensor at 5V, level-shift ECHO to 3.3V!

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "secrets.h"  // WIFI_SSID, WIFI_PASSWORD, MQTT_USER, MQTT_PASS — gitignored, see secrets.h.example

// -------- MQTT broker (Mosquitto, TLS) --------
const char* MQTT_BROKER   = "13.127.43.131";
const int   MQTT_PORT     = 8883;
const char* MQTT_TOPIC    = "sensor/ultrasonic/distance";
const char* MQTT_STATUS_TOPIC = "sensor/ultrasonic/status";  // Last Will topic: online/offline
const char* MQTT_CLIENT   = "esp32-ultrasonic";

// Broker's CA certificate (public, not secret) — must match the one used by the Android app
const char* MQTT_CA_CERT = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFeTCCA2GgAwIBAgIULvhFGZMRtxuHfwfFswOp6D4h+mYwDQYJKoZIhvcNAQEL
BQAwTDELMAkGA1UEBhMCSU4xFDASBgNVBAgMC01haGFyYXNodHJhMQwwCgYDVQQK
DANJb1QxGTAXBgNVBAMMEElvVC1NUVRULVJvb3QtQ0EwHhcNMjYwOTE2MDYwNzEx
WhcNMzYwOTEzMDYwNzExWjBMMQswCQYDVQQGEwJJTjEUMBIGA1UECAwLTWFoYXJh
c2h0cmExDDAKBgNVBAoMA0lvVDEZMBcGA1UEAwwQSW9ULU1RVFQtUm9vdC1DQTCC
AiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAO1f6RS67ShbT+IcpoLilZ85
q1ja4j3R0scdYmU6TCGQmmYqCNVVDbsGDBjCDBohX89YtsYE+ggzvc4ghOs+w7OU
jb0RL8Ep1gfb/Q0OTZohjt3iVJlvJUWx/5AvmXIgxuLi0gbDHS06b5c+3c1JQJXC
t7v35kipnG1hgiC/vuviY1LHW2V93V6LP54PCVS+oxHji2ai/Cx0i+F1o9P6ctAf
w4TA14AeonCP410J0v54ny9FBFi4eqs4fqn2CoKpy47b42d6LiPrGwmwnDX/zluY
QF2LqepvdNDrTlH+R5aHLpou27kn8Ej8VO3kYTWunpBG7Hjxd6s1uLli2npoz6yo
hdEfXl2tdU5Ct+IvyogjmYWOqfGG+NKYc941bgo7Dwb46BKrR/p+ei72w/B05Tom
uCYWSYWXqJgsmiBIcClkHVqfCDciG5QI+0FN53PJGgyWgd8qwVS0YXuKb5c1CDWV
7oFTNJYWz+1ZEKKUzlxkNFYhrU1IijJZKRsewCoeaxdOF9l0Sv/Be1IaQ0hxCyye
BDDdSd/1SwcBMSAn68CNOnS24GIEhSTM0SJ+xctmxDKAinIz74YDUzsKijUJ9gRZ
aSFqECzACYnElQBpgGop5AgB3Ft4AnzXnHoYmVwcZIN7wtmsJtcw4KdB19yZQFKn
ajVN3TY5KZBD39Q91p7RAgMBAAGjUzBRMB0GA1UdDgQWBBSQIS90OgKLlRql25N4
Wba9KsxEKjAfBgNVHSMEGDAWgBSQIS90OgKLlRql25N4Wba9KsxEKjAPBgNVHRMB
Af8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4ICAQCIbLSnOThJoldX7gKyfF/cFvFs
Lq4gIWaGOlP6O5UPYKtAqp5IauN+k7GVzHchQpw5Ah5c8El7teV9g6iiKL11yiP2
7c93NqW9e8d1zr0TCpPkhFbIEw+Bo8WJNRbn+m7BZdeGxiYaeXkaujEFUL30vLHZ
nTSi5IsfRkrzA04+UlH++5fBrxJ1WqMq6nT9xLJEt2gG3vYpchMB0sT29MsPvH9l
s8kfSnKtQ1AlT6oBGa68uw0M/b9/JiPSSkWec6cgvDI+P2Mfe2zZLHXRkIzIATco
WPHq0ap7D2hLzRYlV3+YU5/pwKUu/N0v5KAN3Sb2DOb2kr8iANdT5raZc2zR2yPK
43Qi1/aAk+y8ndvhrYCwj6HVI8PVRq3ljLB4alcuG3yMyTiLQzMFS9JyUIWCH5YF
pqi+/9TLLAzVedYlQHpy9ZpPKKjbkN6Bu1nxWcLhJNZF5M66yhQ+eG76aIXdi9oa
Ub0LvZ1El12mvy77BXyyY75Qmh5pOMvDmMMNUBcL4/QxMMy4ndFdduR+EFhfnOK4
YalAnjxRG0TJXiDws7ZsihHna7DDUeKoqOWhym7Ji+vPT5yJEVbvz1e3pP0pv1Is
7yjacad/2VTfrKvlBqhkFce2CglVEKw8HoOtTVVguP7MnHQaPdXfmEk0BDt2K9K8
oHR6SaxjP+ze2WiL+A==
-----END CERTIFICATE-----
)EOF";

// -------- pin selection --------
const int PIN_TRIG = 5;    // change to your wiring
const int PIN_ECHO = 18;   // change to your wiring

// measurement settings
const uint32_t PULSE_TIMEOUT_US = 30'000; // ~5 m max one-way (safe), adjust as needed
const int SAMPLES = 5;                    // median of N readings
const float MIN_VALID_CM = 20.0;          // datasheet "blind zone" (approx)
const float MAX_VALID_CM = 800.0;         // practical upper bound for AJ-SR04M

WiFiClientSecure espClient;
PubSubClient     mqttClient(espClient);

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

void syncTime() {
  // TLS cert validation fails if the device clock isn't roughly correct
  Serial.print("Syncing time via NTP");
  configTime(0, 0, "pool.ntp.org");
  while (time(nullptr) < 1700000000) {
    delay(200);
    Serial.print(".");
  }
  Serial.println(" done");
}

void connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT broker...");
    // Last Will: if this device disconnects without saying goodbye, the broker
    // publishes a retained "offline" on MQTT_STATUS_TOPIC for subscribers to see.
    if (mqttClient.connect(MQTT_CLIENT, MQTT_USER, MQTT_PASS, MQTT_STATUS_TOPIC, 1, true, "offline")) {
      Serial.println("connected");
      mqttClient.publish(MQTT_STATUS_TOPIC, "online", true);  // retained
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
  syncTime();
  espClient.setCACert(MQTT_CA_CERT);
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
