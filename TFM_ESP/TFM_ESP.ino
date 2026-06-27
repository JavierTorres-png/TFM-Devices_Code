#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ESP32Servo.h>
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
// Interval to publish sensed values (10 s)
const unsigned long publishInterval = 10000;
unsigned long lastPublish = 0;
String publishTopic = "telemetry/device";
MqttClient mqttClient(wifiClient);

// Sensors related variables
#define WATER_SENSOR_PIN 34
#define WATER_PUMP_PIN 33
#define FAN_PIN 18

// Si7021-related variables
Adafruit_Si7021 si7021 = Adafruit_Si7021();
float temperatureSum = 0;
float humiditySum = 0;
bool siAvailable = false;

// Water level sensor is not completely accurate, so value is scalated. We use double instead of int to avoid having problems when doing divisions
const double waterLevelRawMin = 1400;
const double waterLevelRawMax = 2300;
float waterSum = 0;

// Servo-related variables
Servo servo;
uint8_t FAN_OFF = 0;
uint8_t FAN_ON = 90;

const unsigned long readingInterval = 2000;
unsigned long lastSensorReading = 0;
float samples = 0;


void setup() {
  Serial.begin(115200);

  connectToWiFiNetwork();
  connectToMQTTBroker();
  sensorsInit();
  actuatorsInit();
}

void loop() {
  mqttClient.poll();

  // Read sensors
  if (millis() - lastSensorReading >= readingInterval) {
    // Read humidity and temperature
    float humidity, temperature;
    readSi7021(&humidity, &temperature);
    float waterLevel;
    readWaterLevel(&waterLevel);
    calculateSum(&humidity, &temperature, &waterLevel);
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

    // Parse JSON
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print("JSON parse failed: ");
      Serial.println(error.c_str());
      return;
    }

    const char* water_pump_status = doc["water_pump_status"];

    if (water_pump_status != nullptr) {
      Serial.print("Pump state: ");
      Serial.println(water_pump_status);

      if (strcmp(water_pump_status, "ON") == 0) {
        digitalWrite(WATER_PUMP_PIN, HIGH);
      } else if (strcmp(water_pump_status, "OFF") == 0) {
        digitalWrite(WATER_PUMP_PIN, LOW);
      } else {
        Serial.println("Unknown water_pump_status value");
      }
    } else {
      Serial.println("Key water_pump_status not found");
    }

    const char* fan_status = doc["fan_status"];

    if (fan_status != nullptr) {
      Serial.print("Fan state: ");
      Serial.println(fan_status);

      if (strcmp(fan_status, "ON") == 0) {
        servo.write(FAN_ON);
      } else if (strcmp(fan_status, "OFF") == 0) {
        servo.write(FAN_OFF);
      } else {
        Serial.println("Unknown fan_status value");
      }
    } else {
      Serial.println("Key fan_status not found");
    }
    
  }
}

void sensorsInit() {
  // SI702
  if (!si7021.begin()) {
    Serial.println("Did not find Si7021 sensor!");
  } else {
    Serial.println("Found Si7021 sensor!");
    siAvailable = true;
  }
  // Water level sensor
  pinMode(WATER_SENSOR_PIN, INPUT);
}

void actuatorsInit() {
  // Water pump actuator, start without pumping water
  pinMode(WATER_PUMP_PIN, OUTPUT);
  digitalWrite(WATER_PUMP_PIN, LOW);

  // Fan actuator, 
  servo.attach(FAN_PIN);
  servo.write(FAN_OFF);
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

void readWaterLevel(float *waterLevel) {
  int sensorValue = analogRead(WATER_SENSOR_PIN);

  // Convert the sensor value to a percentage (0 to 100%)
  if (sensorValue <= waterLevelRawMin) { // Min the sensor detects
    *waterLevel = 0;
  } else if (sensorValue >= waterLevelRawMax) { // Max detected by sensor
    *waterLevel = 100;
  } else {
    *waterLevel = ((sensorValue - waterLevelRawMin) / (waterLevelRawMax - waterLevelRawMin)) * 100;
  }

  Serial.print("Water level (raw): ");
  Serial.print(sensorValue);
  Serial.print("\tWater level (percentage): ");
  Serial.print(*waterLevel, 2);
  Serial.println("%");
}

void calculateSum(float *humidity, float *temperature, float *waterLevel) {
  temperatureSum += *temperature;
  humiditySum += *humidity;
  waterSum += *waterLevel;
  samples++;
}

void publishTelemetry() {
    char payload[256];

    float humidityAverage = humiditySum/samples;
    float temperatureAverage = temperatureSum/samples;
    uint8_t waterLevelAverage = waterSum/samples;
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
      humidityAverage, waterLevelAverage,
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
  waterSum = 0;
  samples = 0;
}