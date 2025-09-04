#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_PN532.h>

#include "config.h"
#include "SpotConn.h"
#include "SPIFFSUtils.h"
#include <SPIFFS.h>

// Globals
WebServer server(80);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
SpotConn spotifyConnection(display);
Adafruit_PN532 nfc(PN532_SDA, PN532_SCL);

// Pages
const char mainPage[] PROGMEM = R"====(
<HTML>
    <HEAD>
        <TITLE>Spotify Auth</TITLE>
    </HEAD>
    <BODY>
        <CENTER>
            <B>Log into Spotify</B><br><br>
            <a href="https://accounts.spotify.com/authorize?response_type=code&client_id=%s&redirect_uri=%s&scope=user-modify-playback-state user-read-currently-playing user-read-playback-state user-library-modify user-library-read">Log in to spotify</a>
        </CENTER>
    </BODY>
</HTML>
)====";

const char errorPage[] PROGMEM = R"====(
<HTML>
    <HEAD>
        <TITLE>Error</TITLE>
    </HEAD>
    <BODY>
        <CENTER>
            <B>Authorization Error</B><br><br>
            <a href="https://accounts.spotify.com/authorize?response_type=code&client_id=%s&redirect_uri=%s&scope=user-modify-playback-state user-read-currently-playing user-read-playback-state user-library-modify user-library-read">Try Again</a>
        </CENTER>
    </BODY>
</HTML>
)====";

bool serverOn = true;
long refreshLoop;
bool buttonStates[] = {1, 1, 1, 1};
unsigned long debounceTimes[] = {0, 0, 0, 0};

void handleRoot() {
  Serial.println("handling root");
  char page[500];
  sprintf(page, mainPage, CLIENT_ID, REDIRECT_URI);
  server.send(200, "text/html", String(page) + "\r\n");
}

void handleCallbackPage() {
  if (!spotifyConnection.accessTokenSet) {
    if (server.arg("code") == "") {
      char page[500];
      sprintf(page, errorPage, CLIENT_ID, REDIRECT_URI);
      server.send(200, "text/html", String(page));
    } else {
      if (spotifyConnection.getUserCode(server.arg("code"))) {
        server.send(200, "text/html", "Spotify setup complete Auth refresh in :" + String(spotifyConnection.tokenExpireTime));
      } else {
        char page[500];
        sprintf(page, errorPage, CLIENT_ID, REDIRECT_URI);
        server.send(200, "text/html", String(page));
      }
    }
  } else {
    server.send(200, "text/html", "Spotify setup complete");
  }
}

void setup() {
  Serial.begin(115200);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialisation failed!");
    while (1) yield();
  }
  Serial.println("\r\nInitialisation done.");

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(0.8);
  display.setTextColor(SSD1306_WHITE);

  WiFi.begin(WIFI_SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/callback", handleCallbackPage);
  server.begin();
  Serial.println("HTTP server started");

  // Button pins
  for (int i = 0; i < 4; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  // Initialize PN532
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("Didn't find PN532 board");
    while (1);
  }
  nfc.SAMConfig();

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(WiFi.localIP());
  display.display();

  spotifyConnection.currVol = -1;  // force initial volume update
}

void loop() {
  if (spotifyConnection.accessTokenSet) {
    if (serverOn) {
      server.close();
      serverOn = false;
    }

    // Refresh token if expired
    if ((millis() - spotifyConnection.tokenStartTime) / 1000 > spotifyConnection.tokenExpireTime) {
      Serial.println("refreshing token");
      if (spotifyConnection.refreshAuth()) {
        Serial.println("refreshed token");
      }
    }

    // Update track info every 1s
    if ((millis() - refreshLoop) > 1000) {
      spotifyConnection.getTrackInfo();
      refreshLoop = millis();
    }

    // Check buttons
    for (int i = 0; i < 4; i++) {
      int reading = digitalRead(buttonPins[i]);
      if (reading != buttonStates[i]) {
        if (millis() - debounceTimes[i] > DEBOUNCE_DELAY) {
          buttonStates[i] = reading;
          if (reading == LOW) {
            switch (i) {
              case 0:
                spotifyConnection.togglePlay();
                break;
              case 1:
                spotifyConnection.toggleLiked(spotifyConnection.currentSong.Id);
                break;
              case 2:
                spotifyConnection.skipForward();
                break;
              case 3:
                spotifyConnection.skipBack();
                break;
              default:
                break;
            }
          }
          debounceTimes[i] = millis();
        }
      }
    }

    // Check volume changes
    int rawADC = analogRead(VOLUME_PIN);
    int volRequest = map(rawADC, 0, 4095, 0, 100);
    if (spotifyConnection.currVol < 0 || abs(volRequest - spotifyConnection.currVol) > 2) {
      spotifyConnection.adjustVolume(volRequest);
    }

    // Check for NFC Tag
    boolean success;
    uint8_t uid[7];
    uint8_t uidLength;
    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 50);
    if (success) {
      String tagUID = "";
      for (uint8_t i = 0; i < uidLength; i++) {
        if (uid[i] < 0x10) tagUID += "0";
        tagUID += String(uid[i], HEX);
      }
      tagUID.toUpperCase();
      Serial.print("Found an NFC Tag UID: ");
      Serial.println(tagUID);

      // Add your NFC-to-Album mappings here
      String albumURI = "";
      if (tagUID == "04A224B3C2XX") {
        albumURI = "spotify:album:7AB2cX2anGksDoxZ0DXkJb";
      } else if (tagUID == "04B12345ABCD") {
        albumURI = "spotify:album:1ATL5GLyefJaxhQzSPVrLX";
      }

      if (albumURI != "") {
        spotifyConnection.playAlbum(albumURI);
      }

      delay(1000);
    }

  } else {
    server.handleClient();
  }
}
