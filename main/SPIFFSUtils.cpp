#include <SPIFFS.h>
#include <FS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <memory>
#include "SPIFFSUtils.h"

void listSPIFFS(void) {
  Serial.println(F("\r\nListing SPIFFS files:"));

  File root = SPIFFS.open("/");
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  static const char line[] PROGMEM = "=================================================";
  Serial.println(FPSTR(line));
  Serial.println(F("  File name                              Size"));
  Serial.println(FPSTR(line));

  File file = root.openNextFile();
  while (file) {
    String fileName = file.name();
    Serial.print(fileName);
    int spaces = 33 - fileName.length();
    if (spaces < 1) spaces = 1;
    while (spaces--) Serial.print(" ");
    String fileSize = String(file.size());
    spaces = 10 - fileSize.length();
    if (spaces < 1) spaces = 1;
    while (spaces--) Serial.print(" ");
    Serial.println(fileSize + " bytes");
    file = root.openNextFile();
  }

  Serial.println(FPSTR(line));
  Serial.println();
  delay(1000);
}

bool getFile(String url, String filename) {
  // If it exists then no need to fetch it
  if (SPIFFS.exists(filename)) {
    Serial.println("Found " + filename);
    return false;
  }

  Serial.println("Downloading " + filename + " from " + url);

  if (WiFi.status() == WL_CONNECTED) {
    // Just create a local WiFiClientSecure object
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      File f = SPIFFS.open(filename, "w+");
      if (!f) {
        Serial.println("file open failed");
        return false;
      }
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      if (httpCode == HTTP_CODE_OK) {
        int total = http.getSize();
        int len = total;
        uint8_t buff[128] = { 0 };
        WiFiClient *stream = http.getStreamPtr();
        while (http.connected() && (len > 0 || len == -1)) {
          size_t size = stream->available();
          if (size) {
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
            f.write(buff, c);
            if (len > 0) {
              len -= c;
            }
          }
          yield();
        }
        Serial.println();
        Serial.print("[HTTP] connection closed or file end.\n");
      }
      f.close();
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
  return true;
}
