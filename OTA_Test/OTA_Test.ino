#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>

// WiFi credentials
const char* ssid = "JawadWala"; 
const char* password = "Alohomora"; 

const char* firmwareUrl = "https://github.com/smbetech/Monginis/releases/latest/download/OTA_Test.ino.bin";
const char* versionUrl = "https://raw.githubusercontent.com/smbetech/Monginis/master/version.txt";

// Current firmware version
const char* currentFirmwareVersion = "1.0.2";
const unsigned long updateCheckInterval = 0.5 * 60 * 1000;  // 2 minutes
unsigned long lastUpdateCheck = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- ESP32 OTA System Started ---");
  pinMode(2, OUTPUT);

  connectToWiFi();
  
  // Initial check on startup
  checkForFirmwareUpdate();
}

void loop() {
  // 1. Non-blocking LED Blink (Visual status)
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink >= 500) {
    lastBlink = millis();
    digitalWrite(2, !digitalRead(2)); 
  }

  // 2. Interval-based update check
  if (millis() - lastUpdateCheck >= updateCheckInterval) {
    lastUpdateCheck = millis();
    checkForFirmwareUpdate();
  }
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
}

void checkForFirmwareUpdate() {
  Serial.println("\nChecking for firmware update...");
  
  WiFiClientSecure client;
  client.setInsecure(); // Required to handle GitHub's HTTPS without SSL certs

  HTTPClient http;
  http.begin(client, versionUrl);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String latestVersion = http.getString();
    latestVersion.trim();
    
    Serial.printf("Current: %s | Latest: %s\n", currentFirmwareVersion, latestVersion.c_str());

    if (latestVersion != currentFirmwareVersion) {
      Serial.println("New version detected. Starting update...");
      downloadAndApplyFirmware();
    } else {
      Serial.println("System is up to date.");
    }
  } else {
    Serial.printf("Version check failed. HTTP error: %d\n", httpCode);
  }
  http.end();
}

void downloadAndApplyFirmware() {
  WiFiClientSecure client;
  client.setInsecure(); 

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000); // 15s timeout for the request
  http.begin(client, firmwareUrl);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    if (contentLength > 0) {
      if (startOTAUpdate(client, contentLength)) {
        Serial.println("Update success! Rebooting...");
        delay(2000);
        ESP.restart();
      }
    } else {
      Serial.println("Error: Content length is 0");
    }
  } else {
    Serial.printf("Firmware download failed. HTTP error: %d\n", httpCode);
  }
  http.end();
}

bool startOTAUpdate(WiFiClient& client, int contentLength) {
  if (!Update.begin(contentLength)) {
    Serial.printf("Not enough space for OTA: %s\n", Update.errorString());
    return false;
  }

  size_t written = 0;
  uint8_t buffer[1024]; // Larger buffer for faster writes
  unsigned long lastDataTime = millis();
  const unsigned long stallTimeout = 20000; // 20s stall timeout

  while (Update.isRunning() && written < contentLength) {
    size_t available = client.available();
    
    if (available > 0) {
      size_t len = client.read(buffer, sizeof(buffer));
      if (len > 0) {
        Update.write(buffer, len);
        written += len;
        lastDataTime = millis(); // RESET timeout timer on every successful read

        // Progress percentage
        static int lastProgress = -1;
        int progress = (written * 100) / contentLength;
        if (progress != lastProgress) {
          Serial.printf("Progress: %d%%\n", progress);
          lastProgress = progress;
        }
      }
    } else if (millis() - lastDataTime > stallTimeout) {
      Serial.println("Error: Stream stalled. Aborting.");
      Update.abort();
      return false;
    }
    yield();
  }

  if (Update.end()) {
    if (Update.isFinished()) return true;
  } else {
    Serial.printf("Update error: %s\n", Update.errorString());
  }
  return false;
}