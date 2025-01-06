#include <TM1637Display.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// Wi-Fi credentials
const char* ssid = "I Hub Robotics R&D 2.4G";
const char* password = "ihr@2025";
int manualBatteryPercentage = 1;
// Server on port 5000
WebServer server(5000);
// TM1637 Pins
#define CLK  22
#define DIO  23
#define BUZZER_PIN 4

// FastLED Configuration
#define LED_PIN     19
#define NUM_LEDS    8
#define BRIGHTNESS  128
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB

// Voltage Measurement Configuration
const float esp32VCC = 3.125;
unsigned long ValueR1 = 15000;
unsigned long ValueR2 = 1000;
const int analogPin = 34;
const int inputResolution = 4095;
const float average_of = 500;
float calibrationFactor = 16.78;
const float cutoffVoltage = 36.0;
const float maxVoltage = 40.30;
const float minVoltage = 36.0;

float voltage;
CRGB leds[NUM_LEDS];
bool blinkWhiteActive = false;
bool redLightActive = false;

TM1637Display display(CLK, DIO);
// Function to handle GET request
void handleGetBattery() {
  StaticJsonDocument<200> jsonResponse;
  jsonResponse["status"] = "success";
  jsonResponse["battery_percentage"] = manualBatteryPercentage;

  String response;
  serializeJson(jsonResponse, response);
  server.send(200, "application/json", response);
}


void handlePostBattery() {
  if (server.hasArg("plain")) {
    StaticJsonDocument<200> jsonRequest;
    StaticJsonDocument<200> jsonResponse;

    // Parse the incoming JSON payload
    DeserializationError error = deserializeJson(jsonRequest, server.arg("plain"));
    if (!error) {
      if (jsonRequest.containsKey("battery_percentage")) {
        manualBatteryPercentage = jsonRequest["battery_percentage"];
        jsonResponse["status"] = "success";
        jsonResponse["message"] = String("Battery percentage updated to ") + manualBatteryPercentage + "%";

        String response;
        serializeJson(jsonResponse, response);
        server.send(200, "application/json", response);
      } else {
        jsonResponse["status"] = "error";
        jsonResponse["message"] = "Invalid input. Provide 'battery_percentage'.";

        String response;
        serializeJson(jsonResponse, response);
        server.send(400, "application/json", response);
      }
    } else {
      server.send(400, "application/json", "{\"status\": \"error\", \"message\": \"Invalid JSON format.\"}");
    }
  } else {
    server.send(400, "application/json", "{\"status\": \"error\", \"message\": \"Missing payload.\"}");
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to Wi-Fi...");
  }
  Serial.println("Connected to Wi-Fi!");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

 Serial.print("Gateway (router) IP: ");

 Serial.println(WiFi.gatewayIP());
 Serial.print("Subnet Mask: " );

 Serial.println(WiFi.subnetMask());

 Serial.print("Primary DNS: ");

 Serial.println(WiFi.dnsIP(0));

 Serial.print("Secondary DNS: ");

 Serial.println(WiFi.dnsIP(1));


  // Define routes
  server.on("/battery", HTTP_GET, handleGetBattery);
  server.on("/battery", HTTP_POST, handlePostBattery);

  // Start the server
  server.begin();
  Serial.println("HTTP server started.");


  Serial.println("ESP32: Voltage Monitoring and LED Control");
  delay(500);

  // Initialize TM1637 Display
  display.clear();
  display.setBrightness(7);

  // Initialize Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize FastLED
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  // Rotate LEDs for 1 second
  unsigned long startTime = millis();
  while (millis() - startTime < 5000) {
    rotateLEDs();
    delay(100);
  }
}

void loop() {
  // Get the average voltage and apply the calibration factor
  float voltageReading = getVoltageAverage() * calibrationFactor;

  // Calculate battery percentage
  int batteryPercentage = calculateBatteryPercentage(voltageReading);

  // Print voltage and percentage to Serial Monitor
  Serial.print("Vin: ");
  Serial.print(voltageReading);
  Serial.print(" V, Battery: ");
  Serial.print(batteryPercentage);
  Serial.println("%");
  manualBatteryPercentage = batteryPercentage;
  server.handleClient();

  // Add a small delay to control the increment speed
  delay(100);
  // Display battery percentage
  displayPercentage(batteryPercentage);

  // Check for low voltage and activate buzzer and red light
  if (voltageReading < cutoffVoltage) {
    redLightActive = true;     // Activate red light mode
    blinkWhiteActive = false;  // Deactivate blinking mode
  } else {
    blinkWhiteActive = true;   // Activate blinkWhite mode
    redLightActive = false;    // Deactivate red light mode
  }

  // LED Control via Serial Commands
  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == 'A') {
      blinkWhiteActive = true; // Activate blinkWhite mode
      redLightActive = false;  // Deactivate red light mode
    } else if (command == 'B') {
      blinkWhiteActive = false; // Deactivate blinkWhite mode
      redLightActive = false;   // Deactivate red light mode
    } else if (command == 'C') {
      redLightActive = true;    // Activate red light mode
      blinkWhiteActive = false; // Deactivate blinkWhite mode
    }
  }

  // LED Pattern Logic
  if (blinkWhiteActive) {
    blinkWhite();
  } else if (redLightActive) {
    showRedLight();
  } else {
    rotateLEDs();
  }
}

// Functions for Voltage Measurement
void readVoltage() {
  int analogValue = analogRead(analogPin);
  float voltage_sensed = analogValue * (esp32VCC / (float)inputResolution);
  voltage = voltage_sensed * (1 + ((float)ValueR2 / (float)ValueR1));
}

float getVoltageAverage() {
  float voltage_temp_average = 0;
  for (int i = 0; i < average_of; i++) {
    readVoltage();
    voltage_temp_average += voltage;
  }
  return voltage_temp_average / average_of;
}

int calculateBatteryPercentage(float voltageReading) {
  if (voltageReading >= maxVoltage) return 100;
  if (voltageReading <= minVoltage) return 0;
  return (int)((voltageReading - minVoltage) / (maxVoltage - minVoltage) * 100);
}

// TM1637 Display Function
void displayPercentage(int batteryPercentage) {
  display.showNumberDec(batteryPercentage, false, 3, 0);
}

// LED Control Functions
void blinkWhite() {
  fill_solid(leds, NUM_LEDS, CRGB::White);
  FastLED.show();
  delay(4000);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  delay(500);
}

void showRedLight() {
  fill_solid(leds, NUM_LEDS, CRGB::Red);
  FastLED.show();
}

void rotateLEDs() {
  static int position = 0;
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  leds[position] = CRGB::Red;
  leds[(position + 1) % NUM_LEDS] = CRGB::Red;
  leds[(position + 1) % NUM_LEDS].fadeLightBy(77);
  FastLED.show();
  position = (position + 1) % NUM_LEDS;
}