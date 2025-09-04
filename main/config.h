#ifndef CONFIG_H
#define CONFIG_H

// WiFi credentials
#define WIFI_SSID "***"
#define PASSWORD "****"

// Spotify API credentials
#define CLIENT_ID "****"
#define CLIENT_SECRET "****"
#define REDIRECT_URI "http://yourip/callback"  // my esp ip and path 

// OLED Display Setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// Buttons and volume pin
static const int buttonPins[] = {4, 2, 13, 14};
#define VOLUME_PIN 36

// Debounce
#define DEBOUNCE_DELAY 50

// PN532 pins
#define PN532_SDA 21
#define PN532_SCL 22

#endif
