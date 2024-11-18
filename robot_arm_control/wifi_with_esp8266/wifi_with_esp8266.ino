#include <Braccio.h>
#include <Servo.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>

// ESP8266 TX/RX pins connected to Arduino Due
#define ESP_RX 10  // Connect to ESP8266 TX
#define ESP_TX 11  // Connect to ESP8266 RX

// WiFi settings
const char* WIFI_SSID = "YourWiFiName";
const char* WIFI_PASS = "YourWiFiPassword";
const int PORT = 80;

SoftwareSerial esp8266(ESP_RX, ESP_TX);

// Servo objects
Servo base;
Servo shoulder;
Servo elbow;
Servo wrist_rot;
Servo wrist_ver;
Servo gripper;

// Current position storage
struct Position {
  int base;       // M1: 0-180
  int shoulder;   // M2: 15-165
  int elbow;      // M3: 0-180
  int wrist_ver;  // M4: 0-180
  int wrist_rot;  // M5: 0-180
  int gripper;    // M6: 10-73
} currentPos;

// Buffer for incoming data
char buffer[1024];
int bufferIndex = 0;

void setup() {
  Serial.begin(115200);  // Debug serial
  esp8266.begin(115200); // ESP8266 serial
  
  // Initialize Braccio
  Braccio.begin();
  
  // Set initial position
  currentPos = {90, 45, 180, 180, 90, 10};
  moveToPosition(currentPos);
  
  // Setup ESP8266
  setupWiFi();
}

void setupWiFi() {
  // Reset ESP8266
  sendCommand("AT+RST", 5000);
  
  // Configure as station mode
  sendCommand("AT+CWMODE=1", 2000);
  
  // Connect to WiFi
  String cwjap = "AT+CWJAP=\"";
  cwjap += WIFI_SSID;
  cwjap += "\",\"";
  cwjap += WIFI_PASS;
  cwjap += "\"";
  sendCommand(cwjap.c_str(), 5000);
  
  // Configure for multiple connections
  sendCommand("AT+CIPMUX=1", 2000);
  
  // Start server
  String cipserver = "AT+CIPSERVER=1,";
  cipserver += String(PORT);
  sendCommand(cipserver.c_str(), 2000);
  
  // Get IP address
  sendCommand("AT+CIFSR", 2000);
}

void loop() {
  if (esp8266.available()) {
    handleESP8266Data();
  }
}

void handleESP8266Data() {
  String data = esp8266.readStringUntil('\n');
  
  if (data.indexOf("+IPD") != -1) {
    // Extract connection ID
    int connectionId = data.charAt(data.indexOf("+IPD,") + 5) - '0';
    
    // Extract HTTP request
    String request = esp8266.readStringUntil('\n');
    
    if (request.indexOf("GET /position") != -1) {
      sendCurrentPosition(connectionId);
    }
    else if (request.indexOf("POST /move") != -1) {
      processMove(connectionId);
    }
    else {
      sendHomePage(connectionId);
    }
  }
}

void sendCommand(const char* cmd, int timeout) {
  Serial.print("Sending: ");
  Serial.println(cmd);
  
  esp8266.println(cmd);
  delay(timeout);
  
  while (esp8266.available()) {
    Serial.write(esp8266.read());
  }
}

void sendCurrentPosition(int connectionId) {
  // Create JSON response
  StaticJsonDocument<200> doc;
  doc["base"] = currentPos.base;
  doc["shoulder"] = currentPos.shoulder;
  doc["elbow"] = currentPos.elbow;
  doc["wrist_ver"] = currentPos.wrist_ver;
  doc["wrist_rot"] = currentPos.wrist_rot;
  doc["gripper"] = currentPos.gripper;
  
  String response = "";
  serializeJson(doc, response);
  
  // Send HTTP response
  String header = "HTTP/1.1 200 OK\r\n";
  header += "Content-Type: application/json\r\n";
  header += "Connection: close\r\n";
  header += "Content-Length: " + String(response.length()) + "\r\n\r\n";
  
  sendResponse(connectionId, header + response);
}

void sendHomePage(int connectionId) {
  String html = "<!DOCTYPE HTML><html><head>";
  html += "<title>Braccio WiFi Control</title>";
  html += "<style>";
  html += "body { font-family: Arial; margin: 20px; }";
  html += ".slider { width: 200px; }";
  html += "</style></head><body>";
  html += "<h1>Braccio Robot Arm Control</h1>";
  
  // Add sliders for each joint
  const char* joints[] = {"Base", "Shoulder", "Elbow", "Wrist Ver", "Wrist Rot", "Gripper"};
  const int ranges[][2] = {{0,180}, {15,165}, {0,180}, {0,180}, {0,180}, {10,73}};
  
  for (int i = 0; i < 6; i++) {
    html += "<div><label>" + String(joints[i]) + ": </label>";
    html += "<input type='range' class='slider' min='" + String(ranges[i][0]);
    html += "' max='" + String(ranges[i][1]);
    html += "' value='" + String(getCurrentJointValue(i));
    html += "' onchange='updatePosition(this.value, " + String(i) + ")'/></div>";
  }
  
  // Add JavaScript
  html += "<script>";
  html += "function updatePosition(value, joint) {";
  html += "  const joints = ['base', 'shoulder', 'elbow', 'wrist_ver', 'wrist_rot', 'gripper'];";
  html += "  const data = {};";
  html += "  data[joints[joint]] = parseInt(value);";
  html += "  fetch('/move', {";
  html += "    method: 'POST',";
  html += "    headers: {'Content-Type': 'application/json'},";
  html += "    body: JSON.stringify(data)";
  html += "  });";
  html += "}";
  html += "</script></body></html>";
  
  // Send HTTP response
  String header = "HTTP/1.1 200 OK\r\n";
  header += "Content-Type: text/html\r\n";
  header += "Connection: close\r\n";
  header += "Content-Length: " + String(html.length()) + "\r\n\r\n";
  
  sendResponse(connectionId, header + html);
}

void sendResponse(int connectionId, String response) {
  String cmd = "AT+CIPSEND=";
  cmd += String(connectionId);
  cmd += ",";
  cmd += String(response.length());
  sendCommand(cmd.c_str(), 1000);
  
  sendCommand(response.c_str(), 1000);
  
  String closeCommand = "AT+CIPCLOSE=";
  closeCommand += String(connectionId);
  sendCommand(closeCommand.c_str(), 1000);
}

void processMove(int connectionId) {
  // Read JSON payload
  String payload = "";
  while (esp8266.available()) {
    char c = esp8266.read();
    if (c == '{') payload += c;
    else if (!payload.isEmpty()) payload += c;
    if (c == '}') break;
  }
  
  // Parse JSON
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload);
  
  if (!error) {
    Position newPos = currentPos;
    if (doc.containsKey("base")) newPos.base = doc["base"];
    if (doc.containsKey("shoulder")) newPos.shoulder = doc["shoulder"];
    if (doc.containsKey("elbow")) newPos.elbow = doc["elbow"];
    if (doc.containsKey("wrist_ver")) newPos.wrist_ver = doc["wrist_ver"];
    if (doc.containsKey("wrist_rot")) newPos.wrist_rot = doc["wrist_rot"];
    if (doc.containsKey("gripper")) newPos.gripper = doc["gripper"];
    
    moveToPosition(newPos);
    
    String response = "{\"status\": \"success\"}";
    String header = "HTTP/1.1 200 OK\r\n";
    header += "Content-Type: application/json\r\n";
    header += "Connection: close\r\n";
    header += "Content-Length: " + String(response.length()) + "\r\n\r\n";
    
    sendResponse(connectionId, header + response);
  }
}

int getCurrentJointValue(int joint) {
  switch(joint) {
    case 0: return currentPos.base;
    case 1: return currentPos.shoulder;
    case 2: return currentPos.elbow;
    case 3: return currentPos.wrist_ver;
    case 4: return currentPos.wrist_rot;
    case 5: return currentPos.gripper;
    default: return 0;
  }
}

void moveToPosition(Position pos) {
  // Validate and constrain values
  pos.base = constrain(pos.base, 0, 180);
  pos.shoulder = constrain(pos.shoulder, 15, 165);
  pos.elbow = constrain(pos.elbow, 0, 180);
  pos.wrist_ver = constrain(pos.wrist_ver, 0, 180);
  pos.wrist_rot = constrain(pos.wrist_rot, 0, 180);
  pos.gripper = constrain(pos.gripper, 10, 73);
  
  // Move servos
  Braccio.ServoMovement(20, pos.base, pos.shoulder, pos.elbow, 
                        pos.wrist_ver, pos.wrist_rot, pos.gripper);
  
  // Update current position
  currentPos = pos;
}