#include <Arduino.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ==================================================
// PIN CONFIGURATION - ESP32-S3
// ==================================================

#define SOIL_PIN 1
#define DHT_PIN 15
#define LED_PIN 2

DHTesp dht;

float lastTemperature = NAN;
float lastHumidity = NAN;
unsigned long lastDHTRead = 0;
const unsigned long DHT_INTERVAL = 2500;

// ==================================================
// WI-FI + MQTT
// ==================================================

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

const char* MQTT_SERVER = "test.mosquitto.org";
const int MQTT_PORT = 1883;

const char* MQTT_TOPIC = "sih2026/farm/node1";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ==================================================
// MOISTURE HISTORY
// ==================================================

const int HISTORY_SIZE = 10;

float moistureHistory[HISTORY_SIZE];

int historyIndex = 0;
int historyCount = 0;

// ==================================================
// MQTT CONNECTION
// ==================================================

void connectMQTT() {

  while (!mqttClient.connected()) {

    Serial.print("Connecting to MQTT... ");

    String clientID = "SIH_Node1_";
    clientID += String(random(0xffff), HEX);

    if (mqttClient.connect(clientID.c_str())) {

      Serial.println("CONNECTED");
    }

    else {

      Serial.print("FAILED, state=");
      Serial.println(mqttClient.state());

      delay(2000);
    }
  }
}

// ==================================================
// SETUP
// ==================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  pinMode(LED_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);

  // DHT22 setup
  dht.setup(DHT_PIN, DHTesp::DHT22);

  // Allow DHT22 to stabilize before the first reading
  delay(2500);

  Serial.println("DHT22 initialized on GPIO 15");

  // ==================================================
  // ADC SETUP FOR ESP32-S3
  // ==================================================

  analogReadResolution(12);

  // ==================================================
  // STARTUP MESSAGE
  // ==================================================

  Serial.println();
  Serial.println("================================");
  Serial.println("     ESP32-S3 NODE 1");
  Serial.println("     SMART FARMING SYSTEM");
  Serial.println("================================");
  Serial.println();

  Serial.println("Soil Sensor : GPIO 1");
  Serial.println("DHT22       : GPIO 15");
  Serial.println("LED         : GPIO 2");
  Serial.println();

  // ==================================================
  // CONNECT TO WI-FI
  // ==================================================

  Serial.print("Connecting to Wi-Fi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi connected!");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // ==================================================
  // MQTT SETUP
  // ==================================================

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  connectMQTT();

  Serial.println();
  Serial.println("================================");
  Serial.println("   NODE 1 READY");
  Serial.println("   ENVIRONMENTAL INTELLIGENCE");
  Serial.println("================================");
  Serial.println();
}

// ==================================================
// LOOP
// ==================================================

void loop() {

  // ==================================================
  // 1. READ SOIL MOISTURE
  // ==================================================

  int soilADC = analogRead(SOIL_PIN);

  float moisture =
      100.0 - ((soilADC / 4095.0) * 100.0);

  // Keep value between 0 and 100
  moisture = constrain(moisture, 0.0, 100.0);

  // ==================================================
  // 2. READ TEMPERATURE + HUMIDITY
  // ==================================================

  if (lastDHTRead == 0 || millis() - lastDHTRead >= DHT_INTERVAL)
  {
    TempAndHumidity data = dht.getTempAndHumidity();

    lastDHTRead = millis();

    if (!isnan(data.temperature) && !isnan(data.humidity))
    {
      lastTemperature = data.temperature;
      lastHumidity = data.humidity;

      Serial.println("DHT22 : READ OK");
    }
    else
    {
      Serial.print("DHT22 ERROR: ");
      Serial.println(dht.getStatusString());
    }
  }

  float temperature = lastTemperature;
  float humidity = lastHumidity;

  // ==================================================
  // 3. CHECK DHT22
  // ==================================================

  if (isnan(temperature) || isnan(humidity))
  {
    Serial.println("Waiting for first valid DHT22 reading...");

    digitalWrite(LED_PIN, HIGH);

    delay(2500);

    return;
  }

  // ==================================================
  // 4. STORE MOISTURE HISTORY
  // ==================================================

  moistureHistory[historyIndex] = moisture;

  historyIndex++;

  if (historyIndex >= HISTORY_SIZE) {

    historyIndex = 0;
  }

  if (historyCount < HISTORY_SIZE) {

    historyCount++;
  }

  // ==================================================
  // 5. DISPLAY SENSOR DATA
  // ==================================================

  Serial.println("--------------------------------");

  Serial.print("Soil ADC      : ");
  Serial.println(soilADC);

  Serial.print("Soil Moisture : ");
  Serial.print(moisture, 1);
  Serial.println("%");

  Serial.print("Temperature   : ");
  Serial.print(temperature, 1);
  Serial.println(" C");

  Serial.print("Humidity      : ");
  Serial.print(humidity, 1);
  Serial.println("%");

  // ==================================================
  // 6. INTELLIGENCE VARIABLES
  // ==================================================

  char irrigationStatus;
  char heatStatus;
  char waterStressStatus;
  char droughtStatus;
  char floodStatus;

  // ==================================================
  // 7. IRRIGATION INTELLIGENCE
  // ==================================================

  /*
     Very dry soil
     → HIGH irrigation requirement

     Moderately dry soil
     → MEDIUM

     Otherwise
     → LOW
  */

  if (moisture < 20) {

    irrigationStatus = 'H';
  }

  else if (moisture < 40) {

    irrigationStatus = 'M';
  }

  else {

    irrigationStatus = 'L';
  }

  // ==================================================
  // 8. WATER-STRESS INTELLIGENCE
  // ==================================================

  /*
     Current soil condition.

     This is different from drought.

     Drought = long-term drying trend.
     Water stress = current lack of available water.
  */

  if (moisture < 20) {

    waterStressStatus = 'H';
  }

  else if (moisture < 40) {

    waterStressStatus = 'M';
  }

  else {

    waterStressStatus = 'L';
  }

  // ==================================================
  // 9. HEAT-STRESS INTELLIGENCE
  // ==================================================

  /*
     Temperature alone is not enough.

     High temperature + low humidity
     creates stronger evaporative stress.
  */

  if (temperature >= 38 && humidity <= 40) {

    heatStatus = 'H';
  }

  else if (temperature >= 32) {

    heatStatus = 'M';
  }

  else {

    heatStatus = 'L';
  }

  // ==================================================
  // 10. WATERLOGGING / FLOOD RISK
  // ==================================================

  if (moisture >= 85 && humidity >= 80) {

    floodStatus = 'H';
  }

  else if (moisture >= 75 && humidity >= 75) {

    floodStatus = 'M';
  }

  else {

    floodStatus = 'L';
  }

  // ==================================================
  // 11. DROUGHT TREND INTELLIGENCE
  // ==================================================

  /*
     Drought is NOT simply "soil is dry".

     We look for a significant decrease
     in moisture over the stored history.
  */

  droughtStatus = 'L';

  if (historyCount >= HISTORY_SIZE) {

    // Calculate average moisture

    float averageMoisture = 0;

    for (int i = 0; i < HISTORY_SIZE; i++) {

      averageMoisture += moistureHistory[i];
    }

    averageMoisture =
        averageMoisture / HISTORY_SIZE;

    /*
       Current moisture significantly below
       recent average.
    */

    if (moisture < 40 &&
        averageMoisture - moisture >= 15) {

      droughtStatus = 'H';
    }

    else if (moisture < 50 &&
             averageMoisture - moisture >= 8) {

      droughtStatus = 'M';
    }
  }

  // ==================================================
  // 12. DISPLAY INTELLIGENCE
  // ==================================================

  Serial.println();
  Serial.println("===== NODE 1 ANALYSIS =====");

  Serial.print("Irrigation Risk : ");

  if (irrigationStatus == 'H')
    Serial.println("HIGH");

  else if (irrigationStatus == 'M')
    Serial.println("MEDIUM");

  else
    Serial.println("LOW");

  Serial.print("Water Stress    : ");

  if (waterStressStatus == 'H')
    Serial.println("HIGH");

  else if (waterStressStatus == 'M')
    Serial.println("MEDIUM");

  else
    Serial.println("LOW");

  Serial.print("Heat Stress     : ");

  if (heatStatus == 'H')
    Serial.println("HIGH");

  else if (heatStatus == 'M')
    Serial.println("MEDIUM");

  else
    Serial.println("LOW");

  Serial.print("Drought Risk    : ");

  if (droughtStatus == 'H')
    Serial.println("HIGH");

  else if (droughtStatus == 'M')
    Serial.println("MEDIUM");

  else
    Serial.println("LOW");

  Serial.print("Waterlogging    : ");

  if (floodStatus == 'H')
    Serial.println("HIGH");

  else if (floodStatus == 'M')
    Serial.println("MEDIUM");

  else
    Serial.println("LOW");

  // ==================================================
  // 13. FINAL DECISION
  // ==================================================

  Serial.println();
  Serial.println("===== FINAL NODE STATUS =====");

  // Waterlogging gets highest priority

  if (floodStatus == 'H') {

    Serial.println("STATUS : WATERLOGGING RISK");
    Serial.println("ACTION : STOP / AVOID IRRIGATION");

    digitalWrite(LED_PIN, HIGH);
  }

  // Severe heat + water stress

  else if (heatStatus == 'H' &&
           waterStressStatus == 'H') {

    Serial.println("STATUS : SEVERE WATER STRESS");
    Serial.println("ACTION : IRRIGATION REQUIRED");

    digitalWrite(LED_PIN, HIGH);
  }

  // High water stress

  else if (waterStressStatus == 'H') {

    Serial.println("STATUS : HIGH WATER STRESS");
    Serial.println("ACTION : IRRIGATION REQUIRED");

    digitalWrite(LED_PIN, HIGH);
  }

  // Medium irrigation requirement

  else if (irrigationStatus == 'M') {

    Serial.println("STATUS : MODERATE WATER NEED");
    Serial.println("ACTION : MONITOR / PLAN IRRIGATION");

    digitalWrite(LED_PIN, LOW);
  }

  // Heat without severe water stress

  else if (heatStatus == 'H') {

    Serial.println("STATUS : HEAT STRESS");
    Serial.println("ACTION : MONITOR CROP");

    digitalWrite(LED_PIN, LOW);
  }

  else {

    Serial.println("STATUS : NORMAL");
    Serial.println("ACTION : NO IMMEDIATE ACTION");

    digitalWrite(LED_PIN, LOW);
  }

  // ==================================================
  // 14. DATA PACKET
  // ==================================================

  String packet = "N1,M=";

  packet += String(moisture, 1);

  packet += ",T=";
  packet += String(temperature, 1);

  packet += ",H=";
  packet += String(humidity, 1);

  packet += ",IRR=";
  packet += String(irrigationStatus);

  packet += ",HEAT=";
  packet += String(heatStatus);

  packet += ",WATER=";
  packet += String(waterStressStatus);

  packet += ",DRY=";
  packet += String(droughtStatus);

  packet += ",FLOOD=";
  packet += String(floodStatus);

  Serial.println();
  Serial.println("===== DATA PACKET =====");
  Serial.println(packet);

  // ==================================================
  // 15. SEND PACKET THROUGH MQTT
  // ==================================================

  if (!mqttClient.connected()) {

    connectMQTT();
  }

  mqttClient.loop();

  if (mqttClient.publish(MQTT_TOPIC, packet.c_str())) {

    Serial.println("MQTT : PACKET SENT");
  }

  else {

    Serial.println("MQTT : SEND FAILED");
  }

  Serial.println("=======================");
  Serial.println();

  delay(1000);
}