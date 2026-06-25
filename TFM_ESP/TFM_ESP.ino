#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include "WiFi.h"
#include "Adafruit_Si7021.h"
#include "arduino_conf.h"

// WiFi connection-related variables
WiFiClient wifiClient;
const char* ssid = SECRET_SSID;
const char* password = SECRET_PSW;

// Server-related variables
char server[]="raspberry-gateway.local";
int port = 1883;
String codeURL = String("http://") + server + ":8080/esp32_code.bin";

// Device ID to identify the device (defined in setup)
String deviceID = "";

// MQTT-related variables
// Interval to publish sensed values (20 s)
const unsigned long publishInterval = 20000;
unsigned long lastPublish = 0;
String publishTopic = "telemetry/device";
MqttClient mqttClient(wifiClient);

// Sensors related variables
const unsigned long readingInterval = 5000;
unsigned long lastSensorReading = 0;
Adafruit_Si7021 si7021 = Adafruit_Si7021();
bool siAvailable = false;
float temperatureSum = 0;
float humiditySum = 0;
float samples = 0;


void setup() {
  Serial.begin(115200);

  connectToWiFiNetwork();
  connectToMQTTBroker();
  checkSensors();
}

void loop() {
  mqttClient.poll();

  // Read sensors
  if (millis() - lastSensorReading >= readingInterval) {
    // Read humidity and temperature
    float humidity, temperature;
    readSi7021(&humidity, &temperature);
    calculateSum(&humidity, &temperature);
    lastSensorReading = millis();
  }

  // MQTT publish
  if (millis() - lastPublish >= publishInterval && samples != 0) {
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
    Serial.println("Received code update");
    t_httpUpdate_return ret = httpUpdate.update(wifiClient, codeURL);
    switch(ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf(
          "Update failed. Error (%d): %s\n",
          httpUpdate.getLastError(),
          httpUpdate.getLastErrorString().c_str()
        );
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("No updates available");
        break;

      case HTTP_UPDATE_OK:
        Serial.println("Update successful");
        break;
    }
  } else {
    String payload;
    while (mqttClient.available()) {
      payload += (char)mqttClient.read();
    }

    Serial.print("Received status change: ");
    Serial.println(payload);
  }
}

void checkSensors() {
  // SI702
  if (!si7021.begin()) {
    Serial.println("Did not find Si7021 sensor!");
  } else {
    Serial.println("Found Si7021 sensor!");
    siAvailable = true;
  }
}

void readSi7021(float *humidity, float *temperature) {
  if (siAvailable) {
    *humidity = si7021.readHumidity();
    *temperature = si7021.readTemperature();
  } else {
    *humidity = 50.0;
    *temperature = 30.0;
  }
    Serial.print("Humidity:    ");
    Serial.print(*humidity, 2);
    Serial.print("\tTemperature: ");
    Serial.println(*temperature, 2);
}

void calculateSum(float *humidity, float *temperature) {
  temperatureSum += *temperature;
  humiditySum += *humidity;
  samples++;
}

void publishTelemetry() {
    char payload[256];

    float humidityAverage = humiditySum/samples;
    float temperatureAverage = temperatureSum/samples;
    uint8_t water_level = 0;
    uint8_t N = 0;
    uint8_t P = 0;
    uint8_t K = 0;

    snprintf(payload, sizeof(payload),
      "{"
      "\"device_id\":\"%s\","
      "\"temperature\":%.2f,"
      "\"humidity\":%.2f,"
      "\"water_level\":%d,"
      "\"N\":%d,"
      "\"P\":%d,"
      "\"K\":%d"
      "}",
      deviceID, temperatureAverage,
      humidityAverage, water_level,
      N, P, K
    );

    mqttClient.beginMessage(publishTopic + deviceID);
    mqttClient.print(payload);
    mqttClient.endMessage();

    Serial.print("Sent the following message: ");
    Serial.println(payload);

    resetData();
}

void resetData() {
  humiditySum = 0;
  temperatureSum = 0;
  samples = 0;
}