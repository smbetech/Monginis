#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>

// WiFi credentials
const char* ssid = "JawadWala"; 
const char* password = "Alohomora"; 

const char* firmwareUrl = "https://github.com/smbetech/Monginis/releases/latest/download/OTA_Test.ino.bin";

const char* versionUrl = "https://raw.githubusercontent.com/smbetech/Monginis/master/version.txt";

// Current firmware version
const char* currentFirmwareVersion = "1.0.1";

// Interval Logic
const unsigned long updateCheckInterval = 0.5 * 60 * 1000; // 2 minutes
unsigned long lastUpdateCheck = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nStarting ESP32 OTA Update");
  pinMode(2, OUTPUT);

  connectToWiFi();
  Serial.println("Device is ready.");
  Serial.println("Current Firmware Version: " + String(currentFirmwareVersion));
  
  // Perform an initial check on startup
  checkForFirmwareUpdate();
}

void loop() {
  // Task 1: Non-blocking LED Blink
  unsigned long currentMillis = millis();
  static unsigned long lastBlink = 0;
  if (currentMillis - lastBlink >= 1000) {
    lastBlink = currentMillis;
    digitalWrite(2, !digitalRead(2)); // Toggle LED
  }

  // Task 2: Check for updates based on the interval
  if (currentMillis - lastUpdateCheck >= updateCheckInterval) {
    lastUpdateCheck = currentMillis; // Reset the timer
    checkForFirmwareUpdate();
  }
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println("IP address: " + WiFi.localIP().toString());
}

void checkForFirmwareUpdate() {
  Serial.println("\n--- Periodic Update Check ---");
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Skipping check.");
    return;
  }

  String latestVersion = fetchLatestVersion();
  if (latestVersion == "") {
    Serial.println("Failed to fetch latest version");
    return;
  }

  if (latestVersion != currentFirmwareVersion) {
    Serial.println("New version found: " + latestVersion);
    downloadAndApplyFirmware();
  } else {
    Serial.println("Device is up to date (" + latestVersion + ").");
  }
}

// ... Keep your existing fetchLatestVersion, downloadAndApplyFirmware, 
// and startOTAUpdate functions here ...

String fetchLatestVersion() {
  HTTPClient http;
  http.begin(versionUrl);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String latestVersion = http.getString();
    latestVersion.trim();  // Remove any extra whitespace
    http.end();
    return latestVersion;
  } else {
    Serial.printf("Failed to fetch version. HTTP code: %d\n", httpCode);
    http.end();
    return "";
  }
}

void downloadAndApplyFirmware() {
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(firmwareUrl);

  int httpCode = http.GET();
  Serial.printf("HTTP GET code: %d\n", httpCode);

  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    Serial.printf("Firmware size: %d bytes\n", contentLength);

    if (contentLength > 0) {
      WiFiClient* stream = http.getStreamPtr();
      if (startOTAUpdate(stream, contentLength)) {
        Serial.println("OTA update successful, restarting...");
        delay(2000);
        ESP.restart();
      } else {
        Serial.println("OTA update failed");
      }
    } else {
      Serial.println("Invalid firmware size");
    }
  } else {
    Serial.printf("Failed to fetch firmware. HTTP code: %d\n", httpCode);
  }
  http.end();
}


bool startOTAUpdate(WiFiClient* client, int contentLength) {
  Serial.println("Initializing update...");
  if (!Update.begin(contentLength)) {
    Serial.printf("Update begin failed: %s\n", Update.errorString());
    return false;
  }

  Serial.println("Writing firmware...");
  size_t written = 0;
  int progress = 0;
  int lastProgress = 0;

  // Timeout variables
  const unsigned long timeoutDuration = 120*1000;  // 10 seconds timeout
  unsigned long lastDataTime = millis();

  while (written < contentLength) {
    if (client->available()) {
      uint8_t buffer[128];
      size_t len = client->read(buffer, sizeof(buffer));
      if (len > 0) {
        Update.write(buffer, len);
        written += len;

        // Calculate and print progress
        progress = (written * 100) / contentLength;
        if (progress != lastProgress) {
          Serial.printf("Writing Progress: %d%%\n", progress);
          lastProgress = progress;
        }
      }
    }
    // Check for timeout
    if (millis() - lastDataTime > timeoutDuration) {
      Serial.println("Timeout: No data received for too long. Aborting update...");
      Update.abort();
      return false;
    }

    yield();
  }
  Serial.println("\nWriting complete");

  if (written != contentLength) {
    Serial.printf("Error: Write incomplete. Expected %d but got %d bytes\n", contentLength, written);
    Update.abort();
    return false;
  }

  if (!Update.end()) {
    Serial.printf("Error: Update end failed: %s\n", Update.errorString());
    return false;
  }

  Serial.println("Update successfully completed");
  return true;
}