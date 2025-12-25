#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>

const char* ssid = "JawadWala"; 
const char* password = "Alohomora"; 
const char* firmwareUrl = "https://github.com/smbetech/Monginis/releases/latest/download/OTA_Test.ino.bin";
const char* versionUrl = "https://raw.githubusercontent.com/smbetech/Monginis/master/version.txt";

const char* currentFirmwareVersion = "1.0.2";
const unsigned long updateCheckInterval = 120000; // 2 minutes
unsigned long lastUpdateCheck = 0;

// Shared client to save memory
WiFiClientSecure client;

void setup() {
  Serial.begin(115200);
  pinMode(2, OUTPUT);
  connectToWiFi();
  client.setInsecure(); // Set once
  checkForFirmwareUpdate();
}

void loop() {
  if (millis() - lastUpdateCheck >= updateCheckInterval) {
    lastUpdateCheck = millis();
    checkForFirmwareUpdate();
  }
  
  // Minimal blink logic
  digitalWrite(2, (millis() / 500) % 2);
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println(F("\nConnected"));
}

void checkForFirmwareUpdate() {
  HTTPClient http;
  if (http.begin(client, versionUrl)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String latestVersion = http.getString();
      latestVersion.trim();
      
      if (latestVersion != currentFirmwareVersion) {
        Serial.println(F("New version found."));
        downloadAndApplyFirmware();
      }
    }
    http.end();
  }
}

void downloadAndApplyFirmware() {
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(client, firmwareUrl);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    if (contentLength > 0 && Update.begin(contentLength)) {
      // Use the internal stream directly to avoid extra buffers
      size_t written = Update.writeStream(client);
      
      if (written == contentLength && Update.end()) {
        if (Update.isFinished()) {
          Serial.println(F("Update Success. Rebooting..."));
          ESP.restart();
        }
      }
    }
  }
  http.end();
}