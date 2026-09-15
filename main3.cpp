#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>

/
#define SOIL_PIN          1
#define DHT_PIN           15
#define LED_PIN           2
#define CAMERA_BUTTON_PIN 4


// ============================================================
// DHT SENSOR
// ============================================================

DHTesp dht;


// ============================================================
// WIFI
// ============================================================

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";


// ============================================================
// MQTT
// ============================================================

const char* MQTT_SERVER = "test.mosquitto.org";
const int MQTT_PORT = 1883;

const char* MQTT_TOPIC = "sih2026/farm/node3";

WiFiClient espClient;
PubSubClient mqttClient(espClient);


// ============================================================
// CAMERA EVENT LIST
// ============================================================

const char* cameraEvents[] =
{
    "NORMAL",
    "FLOOD",
    "PERSON",
    "ANIMAL",
    "VEHICLE",
    "FIRE",
    "OBJECT"
};

const int CAMERA_EVENT_COUNT = 7;

int currentCameraEvent = 0;


// ============================================================
// BUTTON CONTROL
// ============================================================

bool lastButtonState = HIGH;

unsigned long lastButtonTime = 0;

const unsigned long BUTTON_DEBOUNCE = 250;


// ============================================================
// SENSOR VARIABLES
// ============================================================

float temperature = 0.0;
float humidity = 0.0;

int soilMoisture = 0;


// ============================================================
// TIMERS
// ============================================================

unsigned long lastSensorRead = 0;
unsigned long lastMqttSend = 0;

const unsigned long SENSOR_INTERVAL = 3000;
const unsigned long MQTT_INTERVAL = 5000;


// ============================================================
// WIFI CONNECTION
// ============================================================

void connectWiFi()
{
    Serial.println();
    Serial.println("Connecting to WiFi...");

    WiFi.mode(WIFI_STA);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;

    while (WiFi.status() != WL_CONNECTED && attempts < 30)
    {
        delay(500);

        Serial.print(".");

        attempts++;
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("WiFi connected");

        Serial.print("IP address: ");

        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("WiFi connection failed");
    }
}


// ============================================================
// MQTT CONNECTION
// ============================================================

void connectMQTT()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    while (!mqttClient.connected())
    {
        Serial.print("Connecting to MQTT...");

        String clientID = "SIH_NODE3_";

        clientID += String((uint32_t)ESP.getEfuseMac(), HEX);

        if (mqttClient.connect(clientID.c_str()))
        {
            Serial.println("connected");
        }
        else
        {
            Serial.print("failed, state=");

            Serial.println(mqttClient.state());

            delay(2000);
        }
    }
}


// ============================================================
// READ ENVIRONMENTAL SENSORS
// ============================================================

void readSensors()
{
    TempAndHumidity data = dht.getTempAndHumidity();

    if (isnan(data.temperature) || isnan(data.humidity))
    {
        Serial.println("DHT22 read error!");

        return;
    }

    temperature = data.temperature;

    humidity = data.humidity;


    // Read soil sensor
    int rawSoil = analogRead(SOIL_PIN);


    // Convert ADC value to percentage
    soilMoisture = map(
        rawSoil,
        0,
        4095,
        0,
        100
    );


    soilMoisture = constrain(
        soilMoisture,
        0,
        100
    );
}


// ============================================================
// CAMERA EVENT SIMULATION
//
// In Wokwi:
// Button press = camera AI detects next event
//
// Real hardware:
// Camera -> AI -> event
// ============================================================

void checkCameraButton()
{
    bool buttonState = digitalRead(CAMERA_BUTTON_PIN);


    if (
        lastButtonState == HIGH &&
        buttonState == LOW &&
        millis() - lastButtonTime > BUTTON_DEBOUNCE
    )
    {
        lastButtonTime = millis();


        // Move to next camera event
        currentCameraEvent++;


        if (currentCameraEvent >= CAMERA_EVENT_COUNT)
        {
            currentCameraEvent = 0;
        }


        Serial.println();

        Serial.println("======================================");

        Serial.println("       CAMERA AI EVENT");

        Serial.println("======================================");


        Serial.print("Detected event: ");

        Serial.println(
            cameraEvents[currentCameraEvent]
        );


        Serial.println();

        Serial.println(
            "Simulation: Button represents camera AI output"
        );

        Serial.println(
            "Real hardware: OV2640/OV5640 -> Edge AI -> Event"
        );


        Serial.println("======================================");

        Serial.println();


        // Blink LED
        digitalWrite(LED_PIN, HIGH);

        delay(150);

        digitalWrite(LED_PIN, LOW);
    }


    lastButtonState = buttonState;
}


// ============================================================
// CREATE MQTT PACKET
// ============================================================

String createPacket()
{
    String packet = "";


    packet += "N3";


    packet += ",M=";

    packet += String(
        soilMoisture
    );


    packet += ",T=";

    packet += String(
        temperature,
        1
    );


    packet += ",H=";

    packet += String(
        humidity,
        1
    );


    packet += ",CAM=";

    packet += cameraEvents[
        currentCameraEvent
    ];


    return packet;
}


// ============================================================
// SEND MQTT DATA
// ============================================================

void sendMQTT()
{
    if (!mqttClient.connected())
    {
        connectMQTT();
    }


    if (!mqttClient.connected())
    {
        Serial.println(
            "MQTT unavailable - packet not sent"
        );

        return;
    }


    String packet = createPacket();


    bool success = mqttClient.publish(
        MQTT_TOPIC,
        packet.c_str()
    );


    if (success)
    {
        Serial.println();

        Serial.println(
            "MQTT packet sent:"
        );

        Serial.println(packet);
    }
    else
    {
        Serial.println(
            "MQTT publish failed"
        );
    }
}


// ============================================================
// PRINT NODE STATUS
// ============================================================

void printStatus()
{
    Serial.println();

    Serial.println(
        "--------------- NODE 3 STATUS ---------------"
    );


    Serial.print(
        "Soil Moisture : "
    );

    Serial.print(
        soilMoisture
    );

    Serial.println("%");


    Serial.print(
        "Temperature   : "
    );

    Serial.print(
        temperature,
        1
    );

    Serial.println(" C");


    Serial.print(
        "Humidity      : "
    );

    Serial.print(
        humidity,
        1
    );

    Serial.println(" %");


    Serial.print(
        "Camera Event  : "
    );

    Serial.println(
        cameraEvents[currentCameraEvent]
    );


    Serial.println(
        "---------------------------------------------"
    );
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    delay(1000);


    Serial.println();

    Serial.println(
        "=============================================="
    );

    Serial.println(
        "       SIH SMART FARMING - NODE 3"
    );

    Serial.println(
        " ENVIRONMENT + VISUAL SURVEILLANCE NODE"
    );

    Serial.println(
        "=============================================="
    );


    // --------------------------------------------------------
    // GPIO
    // --------------------------------------------------------

    pinMode(
        LED_PIN,
        OUTPUT
    );


    pinMode(
        CAMERA_BUTTON_PIN,
        INPUT_PULLUP
    );


    // --------------------------------------------------------
    // DHT22
    // --------------------------------------------------------

    dht.setup(
        DHT_PIN,
        DHTesp::DHT22
    );


    // --------------------------------------------------------
    // ADC
    // --------------------------------------------------------

    analogReadResolution(12);


    // --------------------------------------------------------
    // MQTT
    // --------------------------------------------------------

    mqttClient.setServer(
        MQTT_SERVER,
        MQTT_PORT
    );


    // --------------------------------------------------------
    // WIFI
    // --------------------------------------------------------

    connectWiFi();


    // --------------------------------------------------------
    // MQTT
    // --------------------------------------------------------

    connectMQTT();


    // --------------------------------------------------------
    // CAMERA SIMULATION INFORMATION
    // --------------------------------------------------------

    Serial.println();

    Serial.println(
        "Camera simulation:"
    );

    Serial.println(
        "Press CAMERA EVENT button"
    );

    Serial.println();

    Serial.println(
        "NORMAL -> FLOOD -> PERSON -> ANIMAL"
    );

    Serial.println(
        "-> VEHICLE -> FIRE -> OBJECT"
    );

    Serial.println();


    // Initial sensor read
    readSensors();

    printStatus();
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
    // --------------------------------------------------------
    // MQTT maintenance
    // --------------------------------------------------------

    if (!mqttClient.connected())
    {
        connectMQTT();
    }

    mqttClient.loop();


    // --------------------------------------------------------
    // Check camera simulation
    // --------------------------------------------------------

    checkCameraButton();


    // --------------------------------------------------------
    // Read sensors
    // --------------------------------------------------------

    if (
        millis() - lastSensorRead >= SENSOR_INTERVAL
    )
    {
        lastSensorRead = millis();

        readSensors();

        printStatus();
    }


    // --------------------------------------------------------
    // Send MQTT packet
    // --------------------------------------------------------

    if (
        millis() - lastMqttSend >= MQTT_INTERVAL
    )
    {
        lastMqttSend = millis();

        sendMQTT();
    }


    delay(10);
}