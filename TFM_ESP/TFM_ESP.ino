#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include "WiFi.h"
#include "arduino_conf.h"

// WiFi connection-related variables
WiFiClient wifiClient;
const char* ssid = SECRET_SSID;
const char* password = SECRET_PSW;

// Server-related variables
char server[]="raspberry-gateway.local";
int port = 1883;
String codeURL = String("http://") + server + ":8080/esp32code.bin";

// Device ID to identify the device (defined in setup)
String deviceID = "";

// Interval to publish sensed values (10 s)
const unsigned long publishInterval = 10000;
unsigned long lastPublish = 0;
String publishTopic = "telemetry/device";

MqttClient mqttClient(wifiClient);

void setup() {
  Serial.begin(9600);

  connectToWiFiNetwork();
  connectToMQTTBroker();
}

void loop() {
  mqttClient.poll();

  if (millis() - lastPublish >= publishInterval) {
    publishTelemetry();
    lastPublish = millis();
  }
}

void connectToWiFiNetwork() {
  // Connect to the WiFi network
  Serial.println("Starting WiFi network connection...");
  WiFi.begin(ssid, password);
  // If not connected yet, wait
  while (WiFi.status() != WL_CONNECTED) {
    delay (500);
    Serial.print("Connecting to WiFi...");
    Serial.println(WiFi.status());
  }
  Serial.println("Connected to WiFi");
  // Set deviceID based on mac address
  deviceID = WiFi.macAddress();
  deviceID.replace(":", ""); // Avoid displaying mac as XX:XX:XX:YY:YY:YY and display it as XXXXXXYYYYYY instead, just for readability
}

void connectToMQTTBroker() {
  Serial.println("Connecting to MQTT broker...");
  if (!mqttClient.connect(server, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    while (1);
  } else{
    Serial.println("Connected to MQTT broker");
  }

    // set the message receive callback
  mqttClient.onMessage(checkMQTTSubscribe);
  mqttClient.subscribe("status/device" + deviceID);
  mqttClient.subscribe("status/update");
  Serial.println("DeviceID: " + deviceID);
}

void checkMQTTSubscribe(int messageSize) {
  String topic = mqttClient.messageTopic();
  if (topic == "status/update") {
    t_httpUpdate_return ret = httpUpdate.update(wifiClient, codeURL);
    if (ret == HTTP_UPDATE_OK) {
      Serial.println("Succesfully updated code");
    } else {
      Serial.print("Error when updating code: ");
      Serial.println((int)ret);    }
  } else {
    String payload;
    while (mqttClient.available()) {
      payload += (char)mqttClient.read();
    }

    Serial.print("Received status change: ");
    Serial.println(payload);
  }
}

void publishTelemetry() {
    char payload[256];

    int8_t temperature = 30;
    uint8_t humidity = 90;
    uint8_t water_level = 0;
    uint8_t N = 0;
    uint8_t P = 0;
    uint8_t K = 0;

    snprintf(payload, sizeof(payload),
      "{"
      "\"device_id\":\"%s\","
      "\"temperature\":%d,"
      "\"humidity\":%d,"
      "\"water_level\":%d,"
      "\"N\":%d,"
      "\"P\":%d,"
      "\"K\":%d"
      "}",
      deviceID, temperature,
      humidity, water_level,
      N, P, K
    );

    mqttClient.beginMessage(publishTopic + deviceID);
    mqttClient.print(payload);
    mqttClient.endMessage();

    Serial.print("Sent the following message!: ");
    Serial.println(payload);
}