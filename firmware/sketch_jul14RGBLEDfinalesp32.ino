#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HardwareSerial.h>

// ---------------- WiFi & MQTT Credentials ----------------
const char* ssid = "xxxxx"; // put your own wifi ssid and password
const char* password = "xxxxxxx";

const char* mqtt_server = "xxxxxxxxxxx.s1.eu.hivemq.cloud"; //hivemq link should look like this
const int mqtt_port = 8883;
const char* mqtt_user = "xxxxxxx";//anything of your own choice
const char* mqtt_pass = "xxxxxxxx";//anything of ur own choice

const char* TOPIC_CMD = "color/hardware/cmd";
const char* TOPIC_LOG = "color/hardware/log";

WiFiClientSecure espClient;
PubSubClient client(espClient);

// ---------------- HC-05 (external BT module, separate UART) ----------------
#define HC05_RX_PIN 16   // ESP32 RX2  <- HC-05 TXD
#define HC05_TX_PIN 17   // ESP32 TX2  -> HC-05 RXD (via voltage divider if needed)
#define HC05_BAUD 9600   // check your module: common defaults are 9600 or 38400
HardwareSerial HC05(2);

// ---------------- TCS3200 Color Sensor Pin Definitions ----------------
#define S0 4
#define S1 5
#define S2 12
#define S3 13
#define OUT 14
#define LED 15

// ---------------- Indicator RGB LED (Pins corrected) ----------------
#define RGB_R 22   // Swapped from 21 to fix Red/Green mismatch
#define RGB_G 21   // Swapped from 22 to fix Red/Green mismatch
#define RGB_B 23   // Remains 23

const long THRESHOLD = 20000;
bool detectionMode = false;

struct ColorRef {
  long r;
  long g;
  long b;
};

ColorRef colors[4];
bool calibrated[4] = {false, false, false, false};

// ---------------- Forward Declarations ----------------
void getAverageRGB(int samples, long &rAvg, long &gAvg, long &bAvg);
void saveColor(int idx, long r, long g, long b);
void showColors();
void performDetection();
void turnOffAllLEDs();
void handleCommand(String cmd);
void readRGB(long &r, long &g, long &b);
int detectColor(long r, long g, long b);

void printAndPublish(String msg) {
  Serial.println(msg);

  // Send to website / MQTT log topic
  if (client.connected()) {
    client.publish(TOPIC_LOG, msg.c_str());
  }

  // Also echo back over Bluetooth so a phone app connected via HC-05 sees it
  HC05.println(msg);
}

void setupWiFi() {
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting MQTT...");

    if (client.connect("ESP32_Color", mqtt_user, mqtt_pass)) {
      Serial.println("Connected");
      client.subscribe(TOPIC_CMD);
    } else {
      Serial.print("Failed rc=");
      Serial.println(client.state());
      delay(3000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String cmd = "";

  for (unsigned int i = 0; i < length; i++) {
    cmd += (char)payload[i];
  }

  cmd.trim();
  handleCommand(cmd);
}

void setup() {
  Serial.begin(115200);

  // Start HC-05 UART
  HC05.begin(HC05_BAUD, SERIAL_8N1, HC05_RX_PIN, HC05_TX_PIN);

  setupWiFi();

  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  reconnect();

  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(OUT, INPUT);
  pinMode(LED, OUTPUT);

  // RGB Pins configuration
  pinMode(RGB_R, OUTPUT);
  pinMode(RGB_G, OUTPUT);
  pinMode(RGB_B, OUTPUT);

  digitalWrite(RGB_R, LOW);
  digitalWrite(RGB_G, LOW);
  digitalWrite(RGB_B, LOW);

  // Set sensor frequency scaling to 20%
  digitalWrite(S0, HIGH);
  digitalWrite(S1, LOW);

  // Turn on sensor onboard LEDs
  digitalWrite(LED, HIGH);

  Serial.println("Supported commands via Serial, MQTT, or HC-05 Bluetooth: c1, c2, c3, c4, show, D, stop");
}

void loop() {
  // Keep MQTT connection alive
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Command via local USB Serial Monitor
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) handleCommand(cmd);
  }

  // Command via HC-05 Bluetooth module (phone app, etc.)
  if (HC05.available()) {
    String cmd = HC05.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) handleCommand(cmd);
  }

  // Handle continuous tracking execution loop
  if (detectionMode) {
    performDetection();
    delay(200); // Fast tracking interval
  }
}

// ---------------- Central command handler ----------------
void handleCommand(String cmd) {
  long r, g, b;

  printAndPublish("[Command Received] -> " + cmd);

  if (cmd == "c1") {
    getAverageRGB(5, r, g, b);
    saveColor(0, r, g, b);
  }
  else if (cmd == "c2") {
    getAverageRGB(5, r, g, b);
    saveColor(1, r, g, b);
  }
  else if (cmd == "c3") {
    getAverageRGB(5, r, g, b);
    saveColor(2, r, g, b);
  }
  else if (cmd == "c4") {
    getAverageRGB(5, r, g, b);
    saveColor(3, r, g, b);
  }
  else if (cmd == "show") {
    showColors();
  }
  else if (cmd == "D") {
    detectionMode = true;
    printAndPublish("Continuous Detection Started");
  }
  else if (cmd == "stop") {
    detectionMode = false;
    turnOffAllLEDs();
    printAndPublish("Detection Stopped");
  }
  else {
    printAndPublish("Unknown command: " + cmd);
  }
}

// ---------------- TCS3200 Low-Level Driver ----------------
void readRGB(long &r, long &g, long &b) {
  // RED
  digitalWrite(S2, LOW);   digitalWrite(S3, LOW);
  r = pulseIn(OUT, LOW);

  // GREEN
  digitalWrite(S2, HIGH);  digitalWrite(S3, HIGH);
  g = pulseIn(OUT, LOW);

  // BLUE
  digitalWrite(S2, LOW);   digitalWrite(S3, HIGH);
  b = pulseIn(OUT, LOW);
}

void getAverageRGB(int samples, long &rAvg, long &gAvg, long &bAvg) {
  long sumR = 0, sumG = 0, sumB = 0;

  for (int i = 0; i < samples; i++) {
    long r, g, b;
    readRGB(r, g, b);
    sumR += r;
    sumG += g;
    sumB += b;
    delay(50);
  }

  rAvg = sumR / samples;
  gAvg = sumG / samples;
  bAvg = sumB / samples;
}

void saveColor(int idx, long r, long g, long b) {
  colors[idx].r = r;
  colors[idx].g = g;
  colors[idx].b = b;
  calibrated[idx] = true;

  printAndPublish("Color " + String(idx + 1) + " Saved successfully.");
  printAndPublish("Stored Profile -> R: " + String(r) + " G: " + String(g) + " B: " + String(b));
}

int detectColor(long r, long g, long b) {
  long bestDistance = 999999999;
  int bestColor = -1;

  for (int i = 0; i < 4; i++) {
    if (!calibrated[i]) continue;

    long dr = r - colors[i].r;
    long dg = g - colors[i].g;
    long db = b - colors[i].b;
    long distance = dr * dr + dg * dg + db * db;

    if (distance < bestDistance) {
      bestDistance = distance;
      bestColor = i;
    }
  }

  printAndPublish("Best Calculated Distance: " + String(bestDistance));

  if (bestDistance > THRESHOLD) return -1;
  return bestColor;
}

void turnOffAllLEDs() {
  digitalWrite(RGB_R, LOW);
  digitalWrite(RGB_G, LOW);
  digitalWrite(RGB_B, LOW);
}

void showColors() {
  for (int i = 0; i < 4; i++) {
    String msg = "Color " + String(i + 1) + ": ";
    if (!calibrated[i]) {
      printAndPublish(msg + "Not Calibrated");
      continue;
    }
    msg += "R=" + String(colors[i].r) + " G=" + String(colors[i].g) + " B=" + String(colors[i].b);
    printAndPublish(msg);
  }
}

void performDetection() {
  long r, g, b;
  getAverageRGB(3, r, g, b);

  printAndPublish("Current Sample -> R: " + String(r) + " G: " + String(g) + " B: " + String(b));

  int detected = detectColor(r, g, b);
  turnOffAllLEDs();

  if (detected == -1) {
    printAndPublish("Result: Unknown Color");
  } else {
    printAndPublish("Result -> Detected: Color " + String(detected + 1));
    switch (detected) {
      case 0: 
        // RGB -> Red (col1)
        digitalWrite(RGB_R, HIGH);
        digitalWrite(RGB_G, LOW);
        digitalWrite(RGB_B, LOW);
        break;
      case 1: 
        // RGB -> Yellow (col2)
        digitalWrite(RGB_R, HIGH);
        digitalWrite(RGB_G, HIGH);
        digitalWrite(RGB_B, LOW);
        break;
      case 2: 
        // RGB -> Blue (col3)
        digitalWrite(RGB_R, LOW);
        digitalWrite(RGB_G, LOW);
        digitalWrite(RGB_B, HIGH);
        break;
      case 3: 
        // RGB -> White (col4)
        digitalWrite(RGB_R, HIGH);
        digitalWrite(RGB_G, HIGH);
        digitalWrite(RGB_B, HIGH);
        break;
    }
  }
}