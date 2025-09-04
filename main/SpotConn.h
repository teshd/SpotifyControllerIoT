#ifndef SPOTCONN_H
#define SPOTCONN_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Adafruit_SSD1306.h>

struct songDetails {
  int durationMs;
  String album;
  String artist;
  String song;
  String Id;
  bool isLiked;
};

class SpotConn {
public:
  SpotConn(Adafruit_SSD1306 &display);
  bool getUserCode(String serverCode);
  bool refreshAuth();
  bool getTrackInfo();
  bool togglePlay();
  bool adjustVolume(int vol);
  bool skipForward();
  bool skipBack();
  bool toggleLiked(String songId);
  bool playAlbum(String albumURI);

  bool accessTokenSet = false;
  long tokenStartTime;
  int tokenExpireTime;
  songDetails currentSong;
  float currentSongPositionMs;
  float lastSongPositionMs;
  int currVol;

private:
  bool findLikedStatus(String songId);
  bool likeSong(String songId);
  bool dislikeSong(String songId);
  bool drawScreen(bool fullRefresh = false, bool likeRefresh = false);

  std::unique_ptr<WiFiClientSecure> client;
  HTTPClient https;
  bool isPlaying = false;
  String accessToken;
  String refreshToken;
  Adafruit_SSD1306 &display;
};

#endif
