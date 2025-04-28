#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

// Create server on port 80
AsyncWebServer server(80);

// AP credentials
const char* ap_ssid = "ESP32Graid";
const char* ap_password = "esp32graid"; // optional, can be open

// Replace with your network credentials
char* ssid = "";
char* password = "";

// HiveMQ Cloud credentials
const char* mqtt_server = "233cb4a43c2b4783bf56cb1bcb15581b.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "deneme";
const char* mqtt_password = "Deneme123!";

// MQTT topics
const char* mqtt_topic_sub = "esp32/command";
const char* mqtt_topic_pub_measurements = "esp32/measurements";
const char* mqtt_topic_pub_status = "esp32/status";


// Secure client
WiFiClientSecure secureClient;
PubSubClient client(secureClient);


void setup_wifi() {
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");

    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("connected");
      client.subscribe(mqtt_topic_sub);
      client.publish(mqtt_topic_pub_status, "ESP32 is online");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Message received [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  if (message == "getMeasurements") {
    Serial.println("Getting measurements...");

    // Simulate sensor values
    float temp = 23.5;
    float humidity = 60.0;

    // Create JSON-style message
    String response = "{ \"temp\": " + String(temp) + ", \"humidity\": " + String(humidity) + " }";
    //String response = handleMeasurementsRequest();

    client.publish(mqtt_topic_pub_measurements, response.c_str());
  }
}

String handleHexRequest(String hexStr) {
  Serial.println("Hex request has been recieved!");
  if (hexStr != "") {
    hexStr.trim(); // Remove any leading/trailing whitespace

    Serial.println("Received Hex String: " + hexStr);

    int len = hexStr.length();
    for (int i = 0; i < len;) {
      while (i < len && hexStr[i] == ' ') i++; // skip spaces

        if (i + 1 < len) {
          String byteStr = hexStr.substring(i, i + 2);
         uint8_t byteVal = (uint8_t) strtol(byteStr.c_str(), NULL, 16);
         Serial2.write(byteVal); // Send to RS485 via TTL
         i += 2;
       } else {
         break;
      }
    }

    Serial.println("Hex bytes sent over Serial2");

    // Wait a bit for response
    delay(100);  // Or use millis() for non-blocking

    // Read available response
    String responseHex = "";
    while (Serial2.available()) {
      uint8_t byteIn = Serial2.read();
      char hexPart[4]; // Enough for 2 hex chars + space + null
      sprintf(hexPart, "%02X ", byteIn);
      responseHex += hexPart;
    }

    Serial.println("Received from sensor:");
    Serial.println(responseHex);


    StaticJsonDocument<200> jsonDoc;

    jsonDoc["hexResponse"] = responseHex;

    String jsonResponse;
    serializeJson(jsonDoc, jsonResponse);
    return jsonResponse;
  } else {
    return "No hex string recieved";
    
  }
}


String handleMeasurementsRequest() {
   
  //String hexStr = request->getParam("hex", true)->value();
  //hexStr.trim(); // Remove any leading/trailing whitespace

  // Serial.println("Received Hex String: " + hexStr);

  Serial.println("Request Received");

  String requestStrings[] = {
    "01 03 00 12 00 02 64 0E", // temp and humidity
    "01 03 00 15 00 00 01 95", // EC
    "01 03 00 06 00 01 64 0B", // pH
    "01 03 00 1E 00 01 E4 0C", // N
    "01 03 00 1F 00 01 B5 CC", // P
    "01 03 00 20 00 01 85 C0", // K
  };

  double temp = 0.0;
  double humidity = 0.0;
  double ec = 0.0;
  double pH = 0.0;
  double N = 0.0;
  double P = 0.0;
  double K = 0.0;  

  for (int stringIndex = 0; stringIndex < 6; stringIndex++) {

    int len = requestStrings[stringIndex].length();
    for (int i = 0; i < len;) {
      while (i < len && requestStrings[stringIndex][i] == ' ') i++; // skip spaces

        if (i + 1 < len) {
          String byteStr = requestStrings[stringIndex].substring(i, i + 2);
          uint8_t byteVal = (uint8_t) strtol(byteStr.c_str(), NULL, 16);
          Serial2.write(byteVal); // Send to RS485 via TTL
          i += 2;
        } else {
          break;
      }
    }

    // Wait a bit for response
    delay(100);  // Or use millis() for non-blocking

    // Read available response
    String responseHex = "";
    while (Serial2.available()) {
      uint8_t byteIn = Serial2.read();
      char hexPart[4]; // Enough for 2 hex chars + space + null
      sprintf(hexPart, "%02X ", byteIn);
      responseHex += hexPart;
    }

    if (stringIndex == 0) // Temp ve Humidity (/10)
    {
      temp = ((double) hexPairToDecimalByIndex(responseHex, 3, 4)) / 10.0;
      humidity = ((double) hexPairToDecimalByIndex(responseHex, 5, 6)) / 10.0;
    }
    else if (stringIndex == 2) { // pH (/100)
      pH = ((double) hexPairToDecimalByIndex(responseHex, 3, 4)) / 100.0;
    }
    else { // EC, NPK (same)
      N = (double) hexPairToDecimalByIndex(responseHex, 3, 4);
      P = (double) hexPairToDecimalByIndex(responseHex, 3, 4);
      K = (double) hexPairToDecimalByIndex(responseHex, 3, 4);
    }

    Serial.println(N);
    Serial.println(P);
    Serial.println(K);
    Serial.println(temp);
    Serial.println(pH);

    StaticJsonDocument<200> jsonDoc;

    jsonDoc["nitrogen"] = N;
    jsonDoc["phosphorus"] = P;
    jsonDoc["potassium"] = K;
    jsonDoc["temperature"] = temp;
    jsonDoc["ec"] = ec;
    jsonDoc["ph"] = pH;
    jsonDoc["soilMoisture"] = humidity;

    // Convert JSON to string
    String jsonResponse;
    serializeJson(jsonDoc, jsonResponse);
    return jsonResponse;
  }
}

int hexPairToDecimalByIndex(String hexString, int highIndex, int lowIndex) {
  // Split the string into parts
  String parts[20];  // assuming max 20 hex bytes
  int partCount = 0;

  int start = 0;
  while (start < hexString.length()) {
    int end = hexString.indexOf(' ', start);
    if (end == -1) end = hexString.length();
    parts[partCount++] = hexString.substring(start, end);
    start = end + 1;
  }

  if (highIndex >= partCount || lowIndex >= partCount) {
    Serial.println("Index out of bounds!");
    return -1;
  }

  // Convert hex string to integers
  int highByte = strtol(parts[highIndex].c_str(), NULL, 16);
  int lowByte = strtol(parts[lowIndex].c_str(), NULL, 16);

  // Combine to form 16-bit value
  int result = (highByte << 8) | lowByte;
  return result;
}

void connectToWiFi(const char* ssid, const char* password) {
  Serial.println("Connecting to WiFi...");

  WiFi.softAPdisconnect(true); // stop AP mode
  delay(100);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int tryCount = 0;
  while (WiFi.status() != WL_CONNECTED && tryCount < 20) { // Try for ~10 seconds
    delay(500);
    Serial.print(".");
    tryCount++;
  }

  if(WiFi.status() == WL_CONNECTED){
    Serial.println("\nConnected to WiFi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Now you can connect to MQTT broker
    reconnect();
  }
  else {
    Serial.println("\nFailed to connect to WiFi");
    // Optional: Restart Access Point mode again
    // Start Access Point
    WiFi.softAP(ap_ssid, ap_password);
    Serial.println("Access Point Started");
  }
}

void setup() {
  Serial.begin(115200);

  // Start filesystem
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
    return;
  }
  Serial.println("LittleFS mounted successfully");

  // Start Access Point
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("Access Point Started");

  // Serve static files
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  server.on("/connect", HTTP_POST, [](AsyncWebServerRequest *request){
  String ssid, password;

  if (request->hasParam("ssid", true)) {
    ssid = request->getParam("ssid", true)->value();
  }
  if (request->hasParam("password", true)) {
    password = request->getParam("password", true)->value();
  }

  Serial.println("Received SSID: " + ssid);
  Serial.println("Received Password: " + password);

  // Now attempt to connect
  connectToWiFi(ssid.c_str(), password.c_str());

  request->send(200, "text/plain", "Trying to connect...");
  });

  // Start web server
  server.begin();

  secureClient.setInsecure(); // ⚠️ For test only; add cert for production
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
  }
}
