#include <Arduino.h>
#include <WiFi.h>
#include <DHTesp.h>
#include <HTTPClient.h>
#include <PubSubClient.h>

// ==================================================
// WIFI
// ==================================================

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// ==================================================
// MQTT
// ==================================================

const char* MQTT_SERVER = "test.mosquitto.org";
const int MQTT_PORT = 1883;

const char* MQTT_TOPIC = "sih2026/farm/node2";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ==================================================
// ESP32-S3 PIN CONFIGURATION
// ==================================================

// ESP32-S3 ADC-capable pin
#define SOIL_PIN 1

// DHT22 data pin
#define DHT_PIN 15

// Status LED
#define LED_PIN 2

DHTesp dht;

// ==================================================
// MOISTURE HISTORY
// ==================================================

const int HISTORY_SIZE = 10;

float moistureHistory[HISTORY_SIZE];

int historyIndex = 0;
int historyCount = 0;

// ==================================================
// INTERNET TEST
// ==================================================

void testInternetConnection()
{
  HTTPClient http;

  http.begin("http://httpbin.org/get");

  int httpCode = http.GET();

  Serial.print("HTTP Test Code: ");
  Serial.println(httpCode);

  if (httpCode > 0)
  {
    Serial.println("Internet HTTP connection SUCCESS");
  }
  else
  {
    Serial.println("Internet HTTP connection FAILED");
  }

  http.end();
}

// ==================================================
// MQTT CONNECTION
// ==================================================

void connectMQTT()
{
  while (!mqttClient.connected())
  {
    Serial.print("Connecting to MQTT... ");

    String clientID = "SIH_Node2_";
    clientID += String(random(0xffff), HEX);

    if (mqttClient.connect(clientID.c_str()))
    {
      Serial.println("CONNECTED");
    }
    else
    {
      Serial.print("FAILED, state=");
      Serial.println(mqttClient.state());

      delay(2000);
    }
  }
}

// ==================================================
// SETUP
// ==================================================

void setup()
{
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // DHT22 setup for ESP32-S3
  dht.setup(DHT_PIN, DHTesp::DHT22);

  // Allow DHT22 to stabilize
  delay(2500);

  // ==================================================
  // WIFI
  // ==================================================

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED)
  {
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

  // ==================================================
  // INTERNET TEST
  // ==================================================

  testInternetConnection();

  // ==================================================
  // NODE 2 START MESSAGE
  // ==================================================

  Serial.println();
  Serial.println("================================");
  Serial.println("   SMART FARMING - NODE 2");
  Serial.println("   ENVIRONMENTAL INTELLIGENCE V2");
  Serial.println("   ESP32-S3");
  Serial.println("================================");
  Serial.println();
}

// ==================================================
// LOOP
// ==================================================

void loop()
{
  // ==================================================
  // 1. READ SOIL MOISTURE
  // ==================================================

  int soilADC = analogRead(SOIL_PIN);

  float moisture =
      100.0 - ((soilADC / 4095.0) * 100.0);

  // Keep moisture within valid range
  moisture = constrain(moisture, 0.0, 100.0);

  // ==================================================
  // 2. READ TEMPERATURE + HUMIDITY
  // ==================================================

  TempAndHumidity data = dht.getTempAndHumidity();

  float temperature = data.temperature;
  float humidity = data.humidity;

  // ==================================================
  // 3. CHECK DHT22
  // ==================================================

  if (isnan(temperature) || isnan(humidity))
  {
    Serial.println("DHT22 ERROR");

    digitalWrite(LED_PIN, HIGH);

    delay(2000);

    return;
  }

  // ==================================================
  // 4. STORE MOISTURE HISTORY
  // ==================================================

  moistureHistory[historyIndex] = moisture;

  historyIndex++;

  if (historyIndex >= HISTORY_SIZE)
  {
    historyIndex = 0;
  }

  if (historyCount < HISTORY_SIZE)
  {
    historyCount++;
  }

  // ==================================================
  // 5. DISPLAY SENSOR DATA
  // ==================================================

  Serial.println("--------------------------------");

  Serial.print("Node          : 2");
  Serial.println();

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

  if (moisture < 20)
  {
    irrigationStatus = 'H';
  }
  else if (moisture < 40)
  {
    irrigationStatus = 'M';
  }
  else
  {
    irrigationStatus = 'L';
  }

  // ==================================================
  // 8. WATER-STRESS INTELLIGENCE
  // ==================================================

  if (moisture < 20)
  {
    waterStressStatus = 'H';
  }
  else if (moisture < 40)
  {
    waterStressStatus = 'M';
  }
  else
  {
    waterStressStatus = 'L';
  }

  // ==================================================
  // 9. HEAT-STRESS INTELLIGENCE
  // ==================================================

  if (temperature >= 38 && humidity <= 40)
  {
    heatStatus = 'H';
  }
  else if (temperature >= 32)
  {
    heatStatus = 'M';
  }
  else
  {
    heatStatus = 'L';
  }

  // ==================================================
  // 10. WATERLOGGING / FLOOD RISK
  // ==================================================

  if (moisture >= 85 && humidity >= 80)
  {
    floodStatus = 'H';
  }
  else if (moisture >= 75 && humidity >= 75)
  {
    floodStatus = 'M';
  }
  else
  {
    floodStatus = 'L';
  }

  // ==================================================
  // 11. DROUGHT TREND INTELLIGENCE
  // ==================================================

  droughtStatus = 'L';

  if (historyCount >= HISTORY_SIZE)
  {
    float averageMoisture = 0;

    for (int i = 0; i < HISTORY_SIZE; i++)
    {
      averageMoisture += moistureHistory[i];
    }

    averageMoisture =
        averageMoisture / HISTORY_SIZE;

    if (moisture < 40 &&
        averageMoisture - moisture >= 15)
    {
      droughtStatus = 'H';
    }
    else if (moisture < 50 &&
             averageMoisture - moisture >= 8)
    {
      droughtStatus = 'M';
    }
  }

  // ==================================================
  // 12. DISPLAY INTELLIGENCE
  // ==================================================

  Serial.println();
  Serial.println("===== NODE 2 ANALYSIS =====");

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

  if (floodStatus == 'H')
  {
    Serial.println("STATUS : WATERLOGGING RISK");
    Serial.println("ACTION : STOP / AVOID IRRIGATION");

    digitalWrite(LED_PIN, HIGH);
  }

  else if (heatStatus == 'H' &&
           waterStressStatus == 'H')
  {
    Serial.println("STATUS : SEVERE WATER STRESS");
    Serial.println("ACTION : IRRIGATION REQUIRED");

    digitalWrite(LED_PIN, HIGH);
  }

  else if (waterStressStatus == 'H')
  {
    Serial.println("STATUS : HIGH WATER STRESS");
    Serial.println("ACTION : IRRIGATION REQUIRED");

    digitalWrite(LED_PIN, HIGH);
  }

  else if (irrigationStatus == 'M')
  {
    Serial.println("STATUS : MODERATE WATER NEED");
    Serial.println("ACTION : MONITOR / PLAN IRRIGATION");

    digitalWrite(LED_PIN, LOW);
  }

  else if (heatStatus == 'H')
  {
    Serial.println("STATUS : HEAT STRESS");
    Serial.println("ACTION : MONITOR CROP");

    digitalWrite(LED_PIN, LOW);
  }

  else
  {
    Serial.println("STATUS : NORMAL");
    Serial.println("ACTION : NO IMMEDIATE ACTION");

    digitalWrite(LED_PIN, LOW);
  }

  // ==================================================
  // DATA PACKET
  // ==================================================

  String packet = "N2,M=";
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

  // ==================================================
  // DISPLAY DATA PACKET
  // ==================================================

  Serial.println();
  Serial.println("===== DATA PACKET =====");
  Serial.println(packet);

  // ==================================================
  // SEND THROUGH MQTT
  // ==================================================

  if (!mqttClient.connected())
  {
    connectMQTT();
  }

  mqttClient.loop();

  if (mqttClient.publish(MQTT_TOPIC, packet.c_str()))
  {
    Serial.println("MQTT : PACKET SENT");
  }
  else
  {
    Serial.println("MQTT : SEND FAILED");
  }

  Serial.println("=======================");
  Serial.println();

  delay(2000);
}