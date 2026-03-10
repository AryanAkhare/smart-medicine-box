#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>

// WiFi & MQTT settings
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.hivemq.com";

// Pin Definitions
#define DHTPIN 15
#define LDRPIN 34
#define LEDPIN 2
#define BUZZERPIN 4
#define BUTTONPIN 5

// OLED Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
RTC_DS1307 rtc;
DHT dht(DHTPIN, DHT22);

WiFiClient espClient;
PubSubClient client(espClient);

bool alarmActive = false;
unsigned long lastDisplayUpdate = 0;
unsigned long lastMqttPublish = 0;
unsigned long lastMqttReconnectAttempt = 0;

void setup_wifi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  unsigned long startAttempt = millis();

  // Try to connect for max ~15 seconds to avoid getting stuck on boot
  while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt) < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
  } else {
    Serial.println("\nWiFi connect failed, continuing in offline mode.");
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) message += (char)payload[i];
  
  if (message == "TIME_TO_TAKE_MEDS") {
    alarmActive = true;
    Serial.println("ALARM TRIGGERED!");
  }
}

void reconnect() {
  // Non-blocking-style reconnect: called from loop with timing checks
  if (client.connected()) return;

  Serial.print("Attempting MQTT...");
  if (client.connect("ESP32MedBoxClient")) {
    Serial.println("connected");
    client.subscribe("medbox/reminder");
  } else {
    Serial.print("failed, rc=");
    Serial.println(client.state());
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LDRPIN, INPUT);
  pinMode(LEDPIN, OUTPUT);
  pinMode(BUZZERPIN, OUTPUT);
  pinMode(BUTTONPIN, INPUT_PULLUP);

  // Initialize I2C for OLED & RTC
  Wire.begin(21, 22); // SDA on 21, SCL on 22 (Standard ESP32)

  dht.begin();
  
  // SSD1306 Fix: Use 0x3D or 0x3C depending on your Wokwi part
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("OLED failed - checking 0x3D"));
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
        Serial.println(F("OLED search failed. Check wiring."));
    }
  }
  
  if (!rtc.begin()) {
    Serial.println("RTC not found - system will continue without time.");
  }

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  display.println("System Online");
  display.display();
  delay(1000);
}

void loop() {
  // Handle MQTT only when WiFi is available
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      // Try MQTT reconnect every 5 seconds to avoid blocking
      if (millis() - lastMqttReconnectAttempt > 5000) {
        lastMqttReconnectAttempt = millis();
        reconnect();
      }
    } else {
      client.loop();
    }
  }

  // Button Handling to STOP Alarm
  if (digitalRead(BUTTONPIN) == LOW && alarmActive) {
    alarmActive = false;
    digitalWrite(LEDPIN, LOW);
    noTone(BUZZERPIN);
    client.publish("medbox/status", "MEDICINE_TAKEN"); 
    Serial.println("Ack: Medicine Taken");
    delay(500); 
  }

  // Alarm Visuals/Audio
  if (alarmActive) {
    digitalWrite(LEDPIN, (millis() / 500) % 2); // Blink LED
    tone(BUZZERPIN, 1000); // Constant tone during alarm
  } else {
    noTone(BUZZERPIN);
  }

  // Display Update (Every 2 seconds)
  if (millis() - lastDisplayUpdate > 2000) {
    lastDisplayUpdate = millis();
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    DateTime now = rtc.now();

    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    
    // Display Time
    display.printf("Time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
    
    // Display Sensors
    if (isnan(t)) display.println("Temp Error");
    else display.printf("Temp: %.1fC | Hum:%.0f%%\n", t, h);

    // Status Area
    display.setCursor(0, 45);
    if (alarmActive) {
      display.setTextSize(2);
      display.println("TAKE MEDS!");
    } else {
      display.setTextSize(1);
      display.println("Status: Monitoring...");
    }
    display.display();
  }

  // MQTT Data Publish (Every 10 seconds)
  if (millis() - lastMqttPublish > 10000) {
    lastMqttPublish = millis();
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    int ldr = analogRead(LDRPIN);

    String payload = "{\"t\":"+String(t)+",\"h\":"+String(h)+",\"l\":"+String(ldr)+"}";
    client.publish("medbox/sensors", payload.c_str());
  }
}