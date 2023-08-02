#include <Arduino.h>
#include <ESPmDNS.h>
#include <SimpleTimer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include "secrets.h"
#include "config.h"
#include "sntp.h"
#include "time.h"

int rssi = 0;
String localIp;
uint32_t chipId = 0;

float cpuTemp = 0;
float psuTemp = 0;
float nicTemp = 0;
float raidTemp = 0;
float expTemp = 0;

const int moboResetBtn = 19; // marked as D1 on the board D6 (12)
const int moboPowerBtn = 18; // marked as D2 on the board D7 (13)
const int moboPowerLed = 4;
const uint8_t cpuTempNtc = 34;
const uint8_t nicTempNtc = 35;
const uint8_t psuTempNtc = 32;
const uint8_t raidTempNtc = 33;
const uint8_t expTempNtc = 39;

String powerStatus = "OFF";
String oldPowerStatus = "OFF";

SimpleTimer timer;
WebServer server(PORT);

float readTemperature(byte pin) {
  int samples[NUMSAMPLES];
  uint8_t i;
  float average;

  // take N samples in a row, with a slight delay
  for (i = 0; i < NUMSAMPLES; i++) {
    samples[i] = analogRead(pin);
    delay(10);
  }

  // average all the samples out
  average = 0;
  for (i = 0; i < NUMSAMPLES; i++) {
    average += samples[i];
  }
  average /= NUMSAMPLES;

  // Serial.print("Average analog reading ");
  // Serial.println(average);

  // convert the value to resistance
  average = 4095 / average - 1;
  average = SERIESRESISTOR / average;
  // Serial.print("Thermistor resistance ");
  // Serial.println(average);

  float steinhart;
  steinhart = average / THERMISTORNOMINAL;          // (R/Ro)
  steinhart = log(steinhart);                       // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                        // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                      // Invert
  steinhart -= 273.15; // convert absolute temp to C

  return steinhart;
}

void powerSense() {
  if (pulseIn(moboPowerLed, HIGH, 300000) > 100) {
    powerStatus = "ON";
    if (powerStatus != oldPowerStatus) {
      Serial.print("Power led: ");
      Serial.println(powerStatus);
      oldPowerStatus = powerStatus;
    }
  } else if (digitalRead(moboPowerLed) == HIGH) {
    powerStatus = "ON";
    if (powerStatus != oldPowerStatus) {
      Serial.print("Power led: ");
      Serial.println(powerStatus);
      oldPowerStatus = powerStatus;
    }
  } else {
    powerStatus = "OFF";
    if (powerStatus != oldPowerStatus) {
      Serial.print("Power led: ");
      Serial.println(powerStatus);
      oldPowerStatus = powerStatus;
    }
  }
}

void rssiSense() { rssi = WiFi.RSSI(); }

void tempSense() {
  psuTemp = readTemperature(psuTempNtc);
  cpuTemp = readTemperature(cpuTempNtc);
  nicTemp = readTemperature(nicTempNtc);
  raidTemp = readTemperature(raidTempNtc);
  expTemp = readTemperature(expTempNtc);
}

void stats() {
  char serialBuffer[128];
  sprintf(
      serialBuffer,
      "Power: %s (%d), CPU: %.4f, PSU: %.4f, NIC: %.4f, Raid: %.4f, Exp: %.4f",
      powerStatus, digitalRead(moboPowerLed), cpuTemp, psuTemp, nicTemp,
      raidTemp, expTemp);
  Serial.println(serialBuffer);
}

// Actions
void resetPress() {
  digitalWrite(moboResetBtn, HIGH);
  delay(300);
  digitalWrite(moboResetBtn, LOW);
}

void powerPress() {
  Serial.println("Power press");
  digitalWrite(moboPowerBtn, HIGH);
  delay(1000);
  digitalWrite(moboPowerBtn, LOW);
}

void powerMomentaryPress() {
  Serial.println("Power momentary press");
  digitalWrite(moboPowerBtn, HIGH);
  delay(5000);
  digitalWrite(moboPowerBtn, LOW);
}

// Web Handlers
void rootHandler() { server.send(200, "text/plain"); }

void notFoundHandler() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void statusHandler() {
  String result;
  result += "{\n";
  result += "  \"power\": \"" + powerStatus + "\",\n";
  result += "  \"network\": {\n";
  result += "    \"ssid\": \"" + String(ssid) + "\",\n";
  result += "    \"ip\": \"" + String(localIp) + "\",\n";
  result += "    \"hostname\": \"" + String(HOSTNAME) + "\",\n";
  result += "    \"port\": " + String(PORT) + ",\n";
  result += "    \"rssi\": " + String(rssi) + ",\n";
  result += "    \"timezone\": \"" + String(TIMEZONE) + "\"\n";
  result += "   },\n";
  result += "  \"sensors\": {\n";
  result += "    \"cpu\": " + String(cpuTemp) + ",\n";
  result += "    \"psu\": " + String(psuTemp) + ",\n";
  result += "    \"nic\": " + String(nicTemp) + ",\n";
  result += "    \"raid\": " + String(raidTemp) + ",\n";
  result += "    \"exp\": " + String(expTemp) + "\n";
  result += "   }\n";
  result += "}";
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json; charset=utf-8", result);
}

void resetHandler() {
  String result;
  result += "{\n";
  if (powerStatus == "ON") {
    resetPress();
    result += "  \"status\": \"success\",\n";
    result += "  \"message\": \"System has been reset.\"\n";
  } else {
    result += "  \"status\": \"info\",\n";
    result += "  \"message\": \"System is off.\"\n";
  }
  result += "}";
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json; charset=utf-8", result);
}

void powerOnHandler() {
  String result;
  result += "{\n";
  if (powerStatus == "OFF") {
    powerPress();
    result += "  \"status\": \"success\",\n";
    result += "  \"message\": \"System has been powered up successfully.\"\n";
  } else {
    result += "  \"status\": \"info\",\n";
    result += "  \"message\": \"System is already on.\"\n";
  }
  result += "}";
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json; charset=utf-8", result);
}

void powerOffHandler() {
  String result;
  result += "{\n";
  if (powerStatus == "ON") {
    powerMomentaryPress();
    result += "  \"status\": \"success\",\n";
    result += "  \"message\": \"System has been powered down forcefully.\"\n";
  } else {
    result += "  \"status\": \"info\",\n";
    result += "  \"message\": \"System is already off.\"\n";
  }
  result += "}";
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json; charset=utf-8", result);
}

void powerShutdownHandler() {
  String result;
  result += "{\n";
  if (powerStatus == "ON") {
    powerPress();
    result += "  \"status\": \"success\",\n";
    result +=
        "  \"message\": \"Power off button pressed. ACPI shutdown started.\"\n";
  } else {
    result += "  \"status\": \"info\",\n";
    result += "  \"message\": \"System is off.\"\n";
  }

  result += "}";
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json; charset=utf-8", result);
}

void onWiFiDisconnect(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.printf("Disconnected from WiFi network. Reason: %d\n",
                info.wifi_sta_disconnected.reason);
  Serial.println("Reconnecting..");

  WiFi.begin(ssid, password);
}

void getChipId() {
  for (int i = 0; i < 17; i = i + 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }

  Serial.printf("ESP32 Chip model = %s Rev %d\n", ESP.getChipModel(),
                ESP.getChipRevision());
  Serial.printf("This chip has %d cores\n", ESP.getChipCores());
  Serial.print("Chip ID: ");
  Serial.println(chipId);
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("No time available (yet)");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void timeavailable(struct timeval *t) {
  Serial.println("Got time adjustment from NTP!");
  printLocalTime();
}

void initialize() {
  Serial.println("Setup ntp...");
  sntp_set_time_sync_notification_cb(timeavailable);
  sntp_servermode_dhcp(1); // (optional)
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
}

void setup() {
  delay(3000); // wait for serial monitor to start completely.
  Serial.begin(115200);

  pinMode(moboPowerLed, INPUT_PULLDOWN);
  pinMode(moboPowerBtn, OUTPUT);
  pinMode(moboResetBtn, OUTPUT);

  digitalWrite(moboPowerBtn, LOW);
  digitalWrite(moboResetBtn, LOW);

  // set notification call-back function
  initialize();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setHostname(HOSTNAME);

  Serial.printf("Connecting to %s \n", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  rssi = WiFi.RSSI();
  localIp = WiFi.localIP().toString();

  Serial.printf("Connected to SSID: %s, IP: %s, RSSI: %d dBm\n", ssid,
                localIp.c_str(), rssi);
  if (MDNS.begin(HOSTNAME)) {
    Serial.println("MDNS responder started");
  }

  server.onNotFound(notFoundHandler);

  server.on("/", rootHandler);
  server.on("/power/on", powerOnHandler);
  server.on("/power/off", powerOffHandler);
  server.on("/power/shutdown", powerShutdownHandler);
  server.on("/reset", resetHandler);
  server.on("/status", statusHandler);

  server.enableCORS(true);

  server.begin();
  Serial.println("HTTP server started.");

  timer.setInterval(100, powerSense);
  timer.setInterval(3000, rssiSense);
  timer.setInterval(1000, tempSense);
  timer.setInterval(1000, stats);
}

void loop() {
  server.handleClient();
  timer.run();
}
