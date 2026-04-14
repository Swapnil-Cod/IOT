#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// Wi-Fi credentials
const char* ssid = "Airtel__NV";
const char* password = "Air03515";  

// MQTT Broker Details
String device_id = "Device0001";
const char *mqtt_server = "13.234.149.187"; // Replace with your Mosquitto broker IP or hostname 
const int mqtt_port = 1883;
const char *mqtt_user = "swap123";
const char *mqtt_password = "swap@123";  
const char *mqtt_clientId = "Swap_Device0001";
const char *topic_publish = "Swap_SensorData";
const char *topic_subscribe = "Swap_CommandRequest"; 

// Onboard LED pin (for ESP32)
const int LED_PIN = 2;

// DHT Sensor settings
#define DHTPIN 15 // GPIO pin where the DHT11 sensor is connected
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// Timer to control publishing interval
unsigned long lastPublishTime = 0;
const unsigned long publishInterval = 5000; // 5 seconds

// Function to connect to Wi-Fi
void setupWiFi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);

  // Non-blocking Wi-Fi connection loop
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
    delay(1000);
    Serial.print(".");
    retryCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected.");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to Wi-Fi. Retrying in the loop...");
  }
}

// Callback function to handle received MQTT messages
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");

  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  // Toggle LED based on the message
  if (String(topic) == topic_subscribe) {
    if (message == "on") {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED turned ON");
    } else if (message == "off") {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED turned OFF");
    }
  }
}

// Function to connect to MQTT broker
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_clientId, mqtt_user, mqtt_password)) {
      Serial.println("Connected to MQTT broker.");
      // Subscribe to the topic
      client.subscribe(topic_subscribe);
      Serial.print("Subscribed to topic: ");
      Serial.println(topic_subscribe);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}


// Read temperature and humidity data from the DHT11 sensor
void readSensorData(float &temperature, float &humidity) {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    temperature = 0.0;
    humidity = 0.0;
  }
}

// Publish sensor data to MQTT broker
void publishSensorData(float temperature, float humidity) {
  String payload = "{\"temperature\": " + String(temperature, 1) + ", \"humidity\": " + String(humidity, 1) + "}";
  client.publish(topic_publish, payload.c_str());
  Serial.print("Published: ");
  Serial.println(payload);
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Configure LED pin as output
  pinMode(LED_PIN, OUTPUT);

  // Initialize DHT sensor
  dht.begin();

  // Connect to Wi-Fi
  setupWiFi();

  // Configure MQTT client
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  // Check and reconnect Wi-Fi if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    setupWiFi();
  }

  // Check and reconnect MQTT if disconnected
  if (!client.connected()) {
    reconnectMQTT();
  }

  // Process MQTT messages
  client.loop();

  // Publish sensor data every 5 seconds
  if (millis() - lastPublishTime >= publishInterval) {
    lastPublishTime = millis();

    float temperature, humidity;
    readSensorData(temperature, humidity);
    publishSensorData(temperature, humidity);
  }
}
