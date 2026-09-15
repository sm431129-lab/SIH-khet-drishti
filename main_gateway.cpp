#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ============================================================
// SIH SMART FARMING
// CENTRAL EDGE GATEWAY
//
// SENSOR FUSION + FIELD INTELLIGENCE
// + DASHBOARD DATA PUBLISHER
// ============================================================


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

const char* NODE1_TOPIC = "sih2026/farm/node1";
const char* NODE2_TOPIC = "sih2026/farm/node2";
const char* NODE3_TOPIC = "sih2026/farm/node3";

// New dashboard topic
const char* DASHBOARD_TOPIC = "sih2026/farm/dashboard";
const char* CROP_HEALTH_TOPIC = "sih2026/farm/crop-health";


WiFiClient espClient;

PubSubClient mqttClient(espClient);


// ============================================================
// NODE DATA
// ============================================================

struct NodeData
{
    bool received;

    float moisture;
    float temperature;
    float humidity;

    String irrigation;
    String heat;
    String water;
    String dry;
    String flood;

    String cameraEvent;

    String rawPacket;
};


NodeData node1;
NodeData node2;
NodeData node3;


// ============================================================
// CROP HEALTH DATA FROM KHET DRISHTI
// ============================================================

struct CropHealthData
{
    bool received;
    String source;
    String crop;
    String cropName;
    String result;
    String category;
    float confidence;
    float affectedAreaPercent;
    String timestamp;
};

CropHealthData cropHealth;


// ============================================================
// TIMERS
// ============================================================

unsigned long lastEvaluation = 0;

const unsigned long EVALUATION_INTERVAL = 10000;


// ============================================================
// INITIALIZE NODE
// ============================================================

void initializeNode(NodeData &node)
{
    node.received = false;

    node.moisture = 0;
    node.temperature = 0;
    node.humidity = 0;

    node.irrigation = "NONE";
    node.heat = "NONE";
    node.water = "NONE";
    node.dry = "NONE";
    node.flood = "NONE";

    node.cameraEvent = "NONE";

    node.rawPacket = "";
}


// ============================================================
// WIFI CONNECTION
// ============================================================

void connectWiFi()
{
    Serial.println();
    Serial.println("Connecting Gateway to WiFi...");

    WiFi.mode(WIFI_STA);

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );

    int attempts = 0;

    while (
        WiFi.status() != WL_CONNECTED &&
        attempts < 30
    )
    {
        delay(500);

        Serial.print(".");

        attempts++;
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("Gateway WiFi connected");

        Serial.print("Gateway IP: ");

        Serial.println(
            WiFi.localIP()
        );
    }
    else
    {
        Serial.println(
            "Gateway WiFi connection failed"
        );
    }
}


// ============================================================
// GET VALUE FROM PACKET
// ============================================================

String getValue(
    const String &packet,
    const String &key
)
{
    String searchKey = key + "=";

    int start = packet.indexOf(
        searchKey
    );

    if (start == -1)
    {
        return "";
    }

    start += searchKey.length();

    int end = packet.indexOf(
        ',',
        start
    );

    if (end == -1)
    {
        end = packet.length();
    }

    return packet.substring(
        start,
        end
    );
}


// ============================================================
// PARSE NODE PACKET
// ============================================================

void parsePacket(
    const String &packet,
    NodeData &node
)
{
    node.received = true;

    node.rawPacket = packet;


    String value;


    value = getValue(packet, "M");

    if (value.length() > 0)
    {
        node.moisture = value.toFloat();
    }


    value = getValue(packet, "T");

    if (value.length() > 0)
    {
        node.temperature = value.toFloat();
    }


    value = getValue(packet, "H");

    if (value.length() > 0)
    {
        node.humidity = value.toFloat();
    }


    value = getValue(packet, "IRR");

    if (value.length() > 0)
    {
        node.irrigation = value;
    }


    value = getValue(packet, "HEAT");

    if (value.length() > 0)
    {
        node.heat = value;
    }


    value = getValue(packet, "WATER");

    if (value.length() > 0)
    {
        node.water = value;
    }


    value = getValue(packet, "DRY");

    if (value.length() > 0)
    {
        node.dry = value;
    }


    value = getValue(packet, "FLOOD");

    if (value.length() > 0)
    {
        node.flood = value;
    }


    value = getValue(packet, "CAM");

    if (value.length() > 0)
    {
        node.cameraEvent = value;
    }
}


// ============================================================
// GET ZONE CONDITION
// ============================================================

String getZoneCondition(
    NodeData &node
)
{
    if (!node.received)
    {
        return "OFFLINE";
    }


    if (node.moisture < 30)
    {
        return "WATER_STRESS";
    }


    if (node.moisture >= 85)
    {
        return "EXCESS_MOISTURE";
    }


    if (node.temperature >= 40)
    {
        return "HEAT_STRESS";
    }


    return "NORMAL";
}


// ============================================================
// GET ZONE ACTION
// ============================================================

String getZoneAction(
    NodeData &node,
    bool cameraZone
)
{
    if (!node.received)
    {
        return "WAIT";
    }


    if (
        cameraZone &&
        node.cameraEvent != "NONE" &&
        node.cameraEvent != "NORMAL"
    )
    {
        if (node.cameraEvent == "FIRE")
        {
            return "IMMEDIATE_INSPECTION";
        }

        if (node.cameraEvent == "FLOOD")
        {
            return "CHECK_WATER_ACCUMULATION";
        }

        return "INSPECT_FIELD";
    }


    if (node.moisture < 30)
    {
        return "IRRIGATION";
    }


    if (node.moisture >= 85)
    {
        return "CHECK_DRAINAGE";
    }


    if (node.temperature >= 40)
    {
        return "MONITOR_HEAT";
    }


    return "NORMAL_MONITORING";
}


// ============================================================
// SIMPLE JSON VALUE HELPERS
// ============================================================
// Khet Drishti sends a compact JSON object. These helpers avoid
// requiring ArduinoJson for the prototype while keeping the
// Gateway lightweight.
// ============================================================

String getJsonString(const String &json, const String &key)
{
    String search = "\"" + key + "\":";
    int start = json.indexOf(search);

    if (start == -1)
        return "";

    start += search.length();

    while (start < (int)json.length() && json[start] == ' ')
        start++;

    if (start >= (int)json.length())
        return "";

    if (json[start] == '\"')
    {
        start++;
        int end = json.indexOf('\"', start);
        if (end == -1)
            return "";
        return json.substring(start, end);
    }

    int comma = json.indexOf(',', start);
    int brace = json.indexOf('}', start);
    int end = -1;

    if (comma == -1)
        end = brace;
    else if (brace == -1)
        end = comma;
    else
        end = min(comma, brace);

    if (end == -1)
        end = json.length();

    String value = json.substring(start, end);
    value.trim();
    return value;
}

float getJsonFloat(const String &json, const String &key, float fallback = 0)
{
    String value = getJsonString(json, key);
    if (value.length() == 0 || value == "null")
        return fallback;
    return value.toFloat();
}

void parseCropHealth(const String &json)
{
    cropHealth.received = true;
    cropHealth.source = getJsonString(json, "source");
    cropHealth.crop = getJsonString(json, "crop");
    cropHealth.cropName = getJsonString(json, "crop_name");
    cropHealth.result = getJsonString(json, "result");
    cropHealth.category = getJsonString(json, "category");
    cropHealth.confidence = getJsonFloat(json, "confidence", 0);
    cropHealth.affectedAreaPercent = getJsonFloat(json, "affected_area_percent", 0);
    cropHealth.timestamp = getJsonString(json, "timestamp");
}


// ============================================================
// MQTT CALLBACK
// ============================================================

void mqttCallback(
    char* topic,
    byte* payload,
    unsigned int length
)
{
    String packet = "";


    for (
        unsigned int i = 0;
        i < length;
        i++
    )
    {
        packet += (char)payload[i];
    }


    Serial.println();

    Serial.println(
        "=========================================="
    );

    Serial.println(
        "          MQTT DATA RECEIVED"
    );

    Serial.println(
        "=========================================="
    );


    Serial.print("Topic : ");

    Serial.println(topic);


    Serial.print("Data  : ");

    Serial.println(packet);


    if (
        String(topic) == NODE1_TOPIC
    )
    {
        parsePacket(
            packet,
            node1
        );

        Serial.println(
            "Source: NODE 1"
        );
    }


    else if (
        String(topic) == NODE2_TOPIC
    )
    {
        parsePacket(
            packet,
            node2
        );

        Serial.println(
            "Source: NODE 2"
        );
    }


    else if (
        String(topic) == NODE3_TOPIC
    )
    {
        parsePacket(
            packet,
            node3
        );

        Serial.println(
            "Source: NODE 3"
        );
    }


    else if (
        String(topic) == CROP_HEALTH_TOPIC
    )
    {
        parseCropHealth(packet);

        Serial.println(
            "Source: KHET DRISHTI CROP HEALTH"
        );

        Serial.print("Crop        : ");
        Serial.println(cropHealth.cropName);

        Serial.print("Result      : ");
        Serial.println(cropHealth.result);

        Serial.print("Confidence  : ");
        Serial.print(cropHealth.confidence, 1);
        Serial.println(" %");

        Serial.print("Affected    : ");
        Serial.print(cropHealth.affectedAreaPercent, 1);
        Serial.println(" %");
    }


    Serial.println(
        "=========================================="
    );
}


// ============================================================
// MQTT CONNECTION
// ============================================================

void connectMQTT()
{
    if (
        WiFi.status() != WL_CONNECTED
    )
    {
        return;
    }


    while (!mqttClient.connected())
    {
        Serial.print(
            "Connecting Gateway to MQTT..."
        );


        String clientID =
            "SIH_GATEWAY_";


        clientID += String(
            (uint32_t)ESP.getEfuseMac(),
            HEX
        );


        if (
            mqttClient.connect(
                clientID.c_str()
            )
        )
        {
            Serial.println(
                "connected"
            );


            mqttClient.subscribe(
                NODE1_TOPIC
            );


            mqttClient.subscribe(
                NODE2_TOPIC
            );


            mqttClient.subscribe(
                NODE3_TOPIC
            );

            mqttClient.subscribe(
                CROP_HEALTH_TOPIC
            );


            Serial.println(
                "Subscribed to:"
            );


            Serial.println(
                NODE1_TOPIC
            );


            Serial.println(
                NODE2_TOPIC
            );


            Serial.println(
                NODE3_TOPIC
            );

            Serial.println(
                CROP_HEALTH_TOPIC
            );


            Serial.println(
                "Dashboard publishing enabled:"
            );


            Serial.println(
                DASHBOARD_TOPIC
            );
        }
        else
        {
            Serial.print(
                "failed, state="
            );


            Serial.println(
                mqttClient.state()
            );


            delay(2000);
        }
    }
}


// ============================================================
// PRINT ZONE
// ============================================================

void printZone(
    const char* zoneName,
    NodeData &node,
    bool showCamera
)
{
    Serial.println();

    Serial.println(
        "------------------------------------------"
    );


    Serial.println(
        zoneName
    );


    if (!node.received)
    {
        Serial.println(
            "Status      : WAITING FOR DATA"
        );

        return;
    }


    Serial.println(
        "Status      : ONLINE"
    );


    Serial.print(
        "Soil        : "
    );

    Serial.print(
        node.moisture,
        1
    );

    Serial.println(
        " %"
    );


    Serial.print(
        "Temperature : "
    );

    Serial.print(
        node.temperature,
        1
    );

    Serial.println(
        " C"
    );


    Serial.print(
        "Humidity    : "
    );

    Serial.print(
        node.humidity,
        1
    );

    Serial.println(
        " %"
    );


    Serial.print(
        "Condition   : "
    );

    Serial.println(
        getZoneCondition(node)
    );


    Serial.print(
        "Action      : "
    );

    Serial.println(
        getZoneAction(
            node,
            showCamera
        )
    );


    if (
        node.irrigation != "NONE"
    )
    {
        Serial.print(
            "Irrigation  : "
        );

        Serial.println(
            node.irrigation
        );
    }


    if (
        node.heat != "NONE"
    )
    {
        Serial.print(
            "Heat flag   : "
        );

        Serial.println(
            node.heat
        );
    }


    if (
        node.water != "NONE"
    )
    {
        Serial.print(
            "Water flag  : "
        );

        Serial.println(
            node.water
        );
    }


    if (
        node.dry != "NONE"
    )
    {
        Serial.print(
            "Dry flag    : "
        );

        Serial.println(
            node.dry
        );
    }


    if (
        node.flood != "NONE"
    )
    {
        Serial.print(
            "Flood flag  : "
        );

        Serial.println(
            node.flood
        );
    }


    if (showCamera)
    {
        Serial.print(
            "Camera      : "
        );

        Serial.println(
            node.cameraEvent
        );
    }
}


// ============================================================
// FIELD RISK CALCULATION
// ============================================================

String calculateFieldRisk()
{
    bool waterStress = false;

    bool excessWater = false;

    bool heatStress = false;

    bool cameraAlert = false;
    bool cropHealthAlert = false;


    if (cropHealth.received &&
        cropHealth.confidence >= 50 &&
        cropHealth.category != "healthy" &&
        cropHealth.category != "HEALTHY" &&
        cropHealth.result.length() > 0)
    {
        cropHealthAlert = true;
    }


    if (node1.received)
    {
        if (node1.moisture < 30)
            waterStress = true;

        if (node1.moisture >= 85)
            excessWater = true;

        if (node1.temperature >= 40)
            heatStress = true;
    }


    if (node2.received)
    {
        if (node2.moisture < 30)
            waterStress = true;

        if (node2.moisture >= 85)
            excessWater = true;

        if (node2.temperature >= 40)
            heatStress = true;
    }


    if (node3.received)
    {
        if (node3.moisture < 30)
            waterStress = true;

        if (node3.moisture >= 85)
            excessWater = true;

        if (node3.temperature >= 40)
            heatStress = true;


        if (
            node3.cameraEvent != "NONE" &&
            node3.cameraEvent != "NORMAL"
        )
        {
            cameraAlert = true;
        }
    }


    int score = 0;


    if (waterStress)
        score++;


    if (excessWater)
        score++;


    if (heatStress)
        score++;


    if (cameraAlert)
        score += 2;

    if (cropHealthAlert)
        score++;


    if (score == 0)
        return "LOW";


    if (score <= 2)
        return "MEDIUM";


    return "HIGH";
}


// ============================================================
// PUBLISH DASHBOARD DATA
//
// Example:
//
// {
//   "field_risk":"HIGH",
//   "n1_m":21.1,
//   "n1_t":28.0,
//   "n1_h":65.0,
//   "n1_status":"WATER_STRESS",
//   "n1_action":"IRRIGATION",
//   ...
// }
//
// ============================================================

void publishDashboardData()
{
    if (!mqttClient.connected())
    {
        return;
    }


    String risk =
        calculateFieldRisk();


    String json = "{";


    // --------------------------------------------------------
    // Overall field
    // --------------------------------------------------------

    json += "\"field_risk\":\"";
    json += risk;
    json += "\",";


    // --------------------------------------------------------
    // Node 1
    // --------------------------------------------------------

    json += "\"n1_m\":";
    json += String(node1.moisture, 1);
    json += ",";


    json += "\"n1_t\":";
    json += String(node1.temperature, 1);
    json += ",";


    json += "\"n1_h\":";
    json += String(node1.humidity, 1);
    json += ",";


    json += "\"n1_status\":\"";
    json += getZoneCondition(node1);
    json += "\",";


    json += "\"n1_action\":\"";
    json += getZoneAction(node1, false);
    json += "\",";


    // --------------------------------------------------------
    // Node 2
    // --------------------------------------------------------

    json += "\"n2_m\":";
    json += String(node2.moisture, 1);
    json += ",";


    json += "\"n2_t\":";
    json += String(node2.temperature, 1);
    json += ",";


    json += "\"n2_h\":";
    json += String(node2.humidity, 1);
    json += ",";


    json += "\"n2_status\":\"";
    json += getZoneCondition(node2);
    json += "\",";


    json += "\"n2_action\":\"";
    json += getZoneAction(node2, false);
    json += "\",";


    // --------------------------------------------------------
    // Node 3
    // --------------------------------------------------------

    json += "\"n3_m\":";
    json += String(node3.moisture, 1);
    json += ",";


    json += "\"n3_t\":";
    json += String(node3.temperature, 1);
    json += ",";


    json += "\"n3_h\":";
    json += String(node3.humidity, 1);
    json += ",";


    json += "\"n3_status\":\"";
    json += getZoneCondition(node3);
    json += "\",";


    json += "\"n3_action\":\"";
    json += getZoneAction(node3, true);
    json += "\",";


    json += "\"camera\":\"";
    json += node3.cameraEvent;
    json += "\",";


    // --------------------------------------------------------
    // Khet Drishti crop-health result
    // --------------------------------------------------------

    json += "\"crop_health_received\":";
    json += cropHealth.received ? "true" : "false";
    json += ",";

    json += "\"crop_health_crop\":\"";
    json += cropHealth.crop;
    json += "\",";

    json += "\"crop_health_crop_name\":\"";
    json += cropHealth.cropName;
    json += "\",";

    json += "\"crop_health_result\":\"";
    json += cropHealth.result;
    json += "\",";

    json += "\"crop_health_category\":\"";
    json += cropHealth.category;
    json += "\",";

    json += "\"crop_health_confidence\":";
    json += String(cropHealth.confidence, 1);
    json += ",";

    json += "\"crop_health_affected_area\":";
    json += String(cropHealth.affectedAreaPercent, 1);


    // End JSON
    json += "}";


    // --------------------------------------------------------
    // Publish
    // --------------------------------------------------------

    bool success =
        mqttClient.publish(
            DASHBOARD_TOPIC,
            json.c_str()
        );


    if (success)
    {
        Serial.println();

        Serial.println(
            "******** DASHBOARD DATA ********"
        );

        Serial.println(
            json
        );

        Serial.println(
            "*********************************"
        );
    }
    else
    {
        Serial.println(
            "Dashboard MQTT publish failed"
        );
    }
}


// ============================================================
// FIELD INTELLIGENCE
// ============================================================

void evaluateField()
{
    bool waterStress = false;

    bool excessWater = false;

    bool heatStress = false;

    bool cameraAlert = false;
    bool cropHealthAlert = false;

    if (cropHealth.received &&
        cropHealth.confidence >= 50 &&
        cropHealth.category != "healthy" &&
        cropHealth.category != "HEALTHY" &&
        cropHealth.result.length() > 0)
    {
        cropHealthAlert = true;
    }


    if (
        node1.received &&
        node1.moisture < 30
    )
    {
        waterStress = true;
    }


    if (
        node2.received &&
        node2.moisture < 30
    )
    {
        waterStress = true;
    }


    if (
        node3.received &&
        node3.moisture < 30
    )
    {
        waterStress = true;
    }


    if (
        node1.received &&
        node1.moisture >= 85
    )
    {
        excessWater = true;
    }


    if (
        node2.received &&
        node2.moisture >= 85
    )
    {
        excessWater = true;
    }


    if (
        node3.received &&
        node3.moisture >= 85
    )
    {
        excessWater = true;
    }


    if (
        node1.received &&
        node1.temperature >= 40
    )
    {
        heatStress = true;
    }


    if (
        node2.received &&
        node2.temperature >= 40
    )
    {
        heatStress = true;
    }


    if (
        node3.received &&
        node3.temperature >= 40
    )
    {
        heatStress = true;
    }


    if (
        node3.received &&
        node3.cameraEvent != "NONE" &&
        node3.cameraEvent != "NORMAL"
    )
    {
        cameraAlert = true;
    }


    // ========================================================
    // DISPLAY
    // ========================================================

    Serial.println();

    Serial.println(
        "=============================================="
    );

    Serial.println(
        "             FIELD INTELLIGENCE"
    );

    Serial.println(
        "=============================================="
    );


    if (waterStress)
    {
        Serial.println(
            "WATER STRESS       : DETECTED"
        );
    }


    if (excessWater)
    {
        Serial.println(
            "EXCESS MOISTURE    : DETECTED"
        );
    }


    if (heatStress)
    {
        Serial.println(
            "HEAT STRESS        : DETECTED"
        );
    }


    if (cameraAlert)
    {
        Serial.print(
            "VISUAL EVENT       : "
        );

        Serial.println(
            node3.cameraEvent
        );
    }

    if (cropHealthAlert)
    {
        Serial.print("CROP HEALTH       : ");
        Serial.println(cropHealth.result);

        Serial.print("CROP CONFIDENCE   : ");
        Serial.print(cropHealth.confidence, 1);
        Serial.println(" %");

        Serial.print("AFFECTED AREA     : ");
        Serial.print(cropHealth.affectedAreaPercent, 1);
        Serial.println(" %");
    }


    if (
        !waterStress &&
        !excessWater &&
        !heatStress &&
        !cameraAlert
    )
    {
        Serial.println(
            "FIELD CONDITIONS   : NORMAL"
        );
    }


    Serial.println();


    Serial.print(
        "OVERALL FIELD RISK : "
    );

    Serial.println(
        calculateFieldRisk()
    );


    Serial.println();

    Serial.println(
        "RECOMMENDATIONS"
    );


    if (waterStress)
    {
        Serial.println(
            "-> Check dry zones and irrigate where required"
        );
    }


    if (excessWater)
    {
        Serial.println(
            "-> Inspect drainage and avoid unnecessary irrigation"
        );
    }


    if (heatStress)
    {
        Serial.println(
            "-> Monitor crops for heat stress"
        );
    }


    if (cameraAlert)
    {
        if (
            node3.cameraEvent == "FIRE"
        )
        {
            Serial.println(
                "-> IMMEDIATE VISUAL INSPECTION REQUIRED"
            );
        }
        else if (
            node3.cameraEvent == "FLOOD"
        )
        {
            Serial.println(
                "-> Inspect water accumulation in visible area"
            );
        }
        else
        {
            Serial.println(
                "-> Inspect camera-visible field area"
            );
        }
    }

    if (cropHealthAlert)
    {
        Serial.print("-> Review crop-health alert for ");
        Serial.print(cropHealth.cropName);
        Serial.print(": ");
        Serial.print(cropHealth.result);
        Serial.println("; inspect affected plants and consider appropriate treatment.");
    }


    if (
        !waterStress &&
        !excessWater &&
        !heatStress &&
        !cameraAlert &&
        !cropHealthAlert
    )
    {
        Serial.println(
            "-> Continue normal monitoring"
        );
    }


    Serial.println(
        "=============================================="
    );
}


// ============================================================
// FIELD REPORT
// ============================================================

void generateFieldReport()
{
    Serial.println();

    Serial.println();

    Serial.println(
        "================================================"
    );

    Serial.println(
        "             SMART FARM FIELD STATUS"
    );

    Serial.println(
        "================================================"
    );


    printZone(
        "ZONE 1  [NODE 1]",
        node1,
        false
    );


    printZone(
        "ZONE 2  [NODE 2]",
        node2,
        false
    );


    printZone(
        "ZONE 3  [NODE 3 + CAMERA]",
        node3,
        true
    );


    Serial.println();

    Serial.println(
        "================================================"
    );


    evaluateField();


    // Publish dashboard packet
    publishDashboardData();


    Serial.println();
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
        "       SIH SMART FARMING GATEWAY"
    );

    Serial.println(
        "       SENSOR FUSION + EDGE INTELLIGENCE"
    );

    Serial.println(
        "=============================================="
    );


    initializeNode(node1);

    initializeNode(node2);

    initializeNode(node3);

    cropHealth.received = false;
    cropHealth.source = "";
    cropHealth.crop = "";
    cropHealth.cropName = "";
    cropHealth.result = "";
    cropHealth.category = "";
    cropHealth.confidence = 0;
    cropHealth.affectedAreaPercent = 0;
    cropHealth.timestamp = "";


    mqttClient.setServer(
        MQTT_SERVER,
        MQTT_PORT
    );

    mqttClient.setBufferSize(1536);


    mqttClient.setCallback(
        mqttCallback
    );


    connectWiFi();

    connectMQTT();


    Serial.println();

    Serial.println(
        "Gateway ready."
    );


    Serial.println(
        "Waiting for Node 1, Node 2 and Node 3..."
    );


    Serial.println();
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    if (!mqttClient.connected())
    {
        connectMQTT();
    }


    mqttClient.loop();


    if (
        millis() - lastEvaluation >=
        EVALUATION_INTERVAL
    )
    {
        lastEvaluation = millis();

        generateFieldReport();
    }


    delay(10);
}