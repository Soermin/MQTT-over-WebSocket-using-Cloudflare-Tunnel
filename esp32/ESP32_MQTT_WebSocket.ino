#include <WiFi.h>
#include <DHT.h>
#include "mqtt_client.h"

// ============================================================
// ESP32 MQTT over WebSocket via Cloudflare Tunnel
// Tested architecture:
// ESP32 -> ws://public-hostname:80 -> Cloudflare Tunnel
//       -> http://localhost:8000 -> Mosquitto WebSocket listener
// ============================================================

// ---------------- WIFI ----------------
const char* WIFI_SSID     = "NAMA_WIFI";
const char* WIFI_PASSWORD = "PASSWORD_WIFI";

// ---------------- MQTT ----------------
// Example: ws://mqtt.example.com:80
// For production, prefer wss://mqtt.example.com:443 and configure CA validation.
const char* MQTT_URI      = "ws://mqtt.example.com:80";
const char* MQTT_USERNAME = "esp32-node";
const char* MQTT_PASSWORD = "PASSWORD_MQTT";

// ---------------- NODE ----------------
const char* NODE_ID = "node-01";

// ---------------- DHT22 ----------------
#define DHTPIN  15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ---------------- TIMING ----------------
const unsigned long SEND_INTERVAL_MS = 10000UL;
const unsigned long WIFI_TIMEOUT_MS  = 20000UL;

esp_mqtt_client_handle_t mqttClient = nullptr;
bool mqttConnected = false;
unsigned long lastSend = 0;

char topicData[64];
char topicCommand[64];

void connectWiFi() {
  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print('.');
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWiFi connection timed out. ESP32 will retry automatically.");
  }
}

void publishSensorData() {
  if (!mqttConnected) {
    Serial.println("Skip publish: MQTT is not connected");
    return;
  }

  const float temperature = dht.readTemperature();
  const float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("DHT22 read failed. Message not published.");
    return;
  }

  char payload[192];
  snprintf(payload, sizeof(payload),
           "{\"node\":\"%s\",\"temperature\":%.1f,\"humidity\":%.1f,\"uptime_s\":%lu}",
           NODE_ID, temperature, humidity, millis() / 1000UL);

  const int messageId = esp_mqtt_client_publish(
      mqttClient,
      topicData,
      payload,
      0,  // length 0 = calculate with strlen()
      0,  // QoS 0
      0   // retain false
  );

  if (messageId >= 0) {
    Serial.printf("Published: %s\n", payload);
  } else {
    Serial.println("Publish failed");
  }
}

static void mqttEventHandler(
    void* handlerArgs,
    esp_event_base_t base,
    int32_t eventId,
    void* eventData
) {
  esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(eventData);

  switch (eventId) {
    case MQTT_EVENT_CONNECTED:
      mqttConnected = true;
      Serial.println("MQTT connected");
      esp_mqtt_client_subscribe(mqttClient, topicCommand, 0);
      Serial.printf("Subscribed: %s\n", topicCommand);
      break;

    case MQTT_EVENT_DISCONNECTED:
      mqttConnected = false;
      Serial.println("MQTT disconnected. ESP-MQTT will reconnect automatically.");
      break;

    case MQTT_EVENT_DATA:
      Serial.printf("Command received on %.*s: %.*s\n",
                    event->topic_len, event->topic,
                    event->data_len, event->data);
      break;

    case MQTT_EVENT_ERROR:
      mqttConnected = false;
      Serial.println("MQTT transport error");
      if (event->error_handle &&
          event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        Serial.printf("esp_tls error: %d | socket errno: %d\n",
                      event->error_handle->esp_tls_last_esp_err,
                      event->error_handle->esp_transport_sock_errno);
      }
      break;

    default:
      break;
  }
}

void startMqtt() {
  esp_mqtt_client_config_t mqttConfig = {};

  mqttConfig.broker.address.uri = MQTT_URI;
  mqttConfig.credentials.username = MQTT_USERNAME;
  mqttConfig.credentials.authentication.password = MQTT_PASSWORD;
  mqttConfig.session.keepalive = 30;
  mqttConfig.network.timeout_ms = 10000;
  mqttConfig.network.reconnect_timeout_ms = 5000;

  mqttClient = esp_mqtt_client_init(&mqttConfig);
  esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_ANY, mqttEventHandler, nullptr);
  esp_mqtt_client_start(mqttClient);

  Serial.printf("MQTT client started: %s\n", MQTT_URI);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  snprintf(topicData, sizeof(topicData), "sensors/%s/data", NODE_ID);
  snprintf(topicCommand, sizeof(topicCommand), "sensors/%s/command", NODE_ID);

  dht.begin();
  connectWiFi();
  startMqtt();

  Serial.printf("Publish topic : %s\n", topicData);
  Serial.printf("Command topic : %s\n", topicCommand);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    mqttConnected = false;
    connectWiFi();
  }

  const unsigned long now = millis();
  if (now - lastSend >= SEND_INTERVAL_MS) {
    lastSend = now;
    publishSensorData();
  }

  delay(10);
}
