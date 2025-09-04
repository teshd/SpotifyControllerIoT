#include "SpotConn.h"
#include "config.h"        
#include <base64.h>
#include <WiFi.h>          
#include <SPIFFS.h>        
#include <WiFiClientSecure.h>

#define HTTP_CODE_OK 200

SpotConn::SpotConn(Adafruit_SSD1306 &displayRef) : display(displayRef) {
  client = std::make_unique<WiFiClientSecure>();
  client->setInsecure();
}

bool SpotConn::getUserCode(String serverCode) {
  https.begin(*client, "https://accounts.spotify.com/api/token");
  String auth = "Basic " + base64::encode(String(CLIENT_ID) + ":" + String(CLIENT_SECRET));
  https.addHeader("Authorization", auth);
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String requestBody = "grant_type=authorization_code&code=" + serverCode + "&redirect_uri=" + String(REDIRECT_URI);
  int httpResponseCode = https.POST(requestBody);
  if (httpResponseCode == HTTP_CODE_OK) {
    String response = https.getString();
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return false;
    }
    accessToken = doc["access_token"].as<String>();
    refreshToken = doc["refresh_token"].as<String>();
    tokenExpireTime = doc["expires_in"].as<int>();
    tokenStartTime = millis();
    accessTokenSet = true;
    Serial.println(accessToken);
    Serial.println(refreshToken);
  } else {
    Serial.println(https.getString());
  }
  https.end();
  return accessTokenSet;
}

bool SpotConn::refreshAuth() {
  https.begin(*client, "https://accounts.spotify.com/api/token");
  String auth = "Basic " + base64::encode(String(CLIENT_ID) + ":" + String(CLIENT_SECRET));
  https.addHeader("Authorization", auth);
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String requestBody = "grant_type=refresh_token&refresh_token=" + String(refreshToken);
  int httpResponseCode = https.POST(requestBody);
  accessTokenSet = false;
  if (httpResponseCode == HTTP_CODE_OK) {
    String response = https.getString();
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return false;
    }
    accessToken = doc["access_token"].as<String>();
    tokenExpireTime = doc["expires_in"].as<int>();
    tokenStartTime = millis();
    accessTokenSet = true;
    Serial.println(accessToken);
    Serial.println(refreshToken);
  } else {
    Serial.println(https.getString());
  }
  https.end();
  return accessTokenSet;
}

bool SpotConn::getTrackInfo() {
  String url = "https://api.spotify.com/v1/me/player/currently-playing";
  https.useHTTP10(true);
  https.begin(*client, url);
  String auth = "Bearer " + String(accessToken);
  https.addHeader("Authorization", auth);
  int httpResponseCode = https.GET();

  if (httpResponseCode == 200) {
    String response = https.getString();
    https.end();

    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return false;
    }

    if (doc["item"].isNull()) {
      Serial.println("No item playing");
      return false;
    }

    JsonObject item = doc["item"];
    String trackName = item["name"].as<String>();
    String albumName = item["album"]["name"].as<String>();
    String artistName = "";
    if (!item["artists"].isNull() && item["artists"].size() > 0) {
      artistName = item["artists"][0]["name"].as<String>();
    } else {
      artistName = "Unknown Artist";
    }

    int durationMs = item["duration_ms"].as<int>();
    bool isPlay = doc["is_playing"].as<bool>();
    int progressMs = doc["progress_ms"].as<int>();

    String uri = item["uri"].as<String>();
    String songId = uri.substring(uri.lastIndexOf(":") + 1);

    currentSong.album = albumName;
    currentSong.artist = artistName;
    currentSong.song = trackName;
    currentSong.Id = songId;
    currentSong.durationMs = durationMs;
    currentSong.isLiked = findLikedStatus(songId);

    isPlaying = isPlay;
    currentSongPositionMs = progressMs;
    lastSongPositionMs = currentSongPositionMs;

    drawScreen(true);
    return true;
  } else {
    Serial.print("Error getting track info: ");
    Serial.println(httpResponseCode);
    String response = https.getString();
    Serial.println(response);
    https.end();
  }
  return false;
}

bool SpotConn::findLikedStatus(String songId) {
  String url = "https://api.spotify.com/v1/me/tracks/contains?ids=" + songId;
  https.begin(*client, url);
  String auth = "Bearer " + String(accessToken);
  https.addHeader("Authorization", auth);
  https.addHeader("Content-Type", "application/json");
  int httpResponseCode = https.GET();
  bool success = false;
  if (httpResponseCode == 200) {
    String response = https.getString();
    https.end();
    return (response == "[ true ]");
  } else {
    Serial.print("Error checking liked status: ");
    Serial.println(httpResponseCode);
    String response = https.getString();
    Serial.println(response);
    https.end();
  }
  return success;
}

bool SpotConn::toggleLiked(String songId) {
  String url = "https://api.spotify.com/v1/me/tracks/contains?ids=" + songId;
  https.begin(*client, url);
  String auth = "Bearer " + String(accessToken);
  https.addHeader("Authorization", auth);
  https.addHeader("Content-Type", "application/json");
  int httpResponseCode = https.GET();
  bool success = false;
  if (httpResponseCode == 200) {
    String response = https.getString();
    https.end();
    if (response == "[ true ]") {
      currentSong.isLiked = false;
      dislikeSong(songId);
    } else {
      currentSong.isLiked = true;
      likeSong(songId);
    }
    drawScreen(false, true);
    success = true;
  } else {
    Serial.print("Error toggling liked songs: ");
    Serial.println(httpResponseCode);
    String res = https.getString();
    Serial.println(res);
    https.end();
  }
  return success;
}

bool SpotConn::drawScreen(bool fullRefresh, bool likeRefresh) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Display artist
  display.setCursor(0, 0);
  display.print("Artist: ");
  display.print(currentSong.artist);

  // Display song
  display.setCursor(0, 10);
  display.print("Song: ");
  display.print(currentSong.song);

  
  display.setCursor(0, 30); // Skipping a line bc of display limitations

  // Liked status
  display.print("Liked: ");
  display.print(currentSong.isLiked ? "Yes" : "No");

  // Play/Pause Status
  display.setCursor(0, 40);
  display.print("Status: ");
  display.print(isPlaying ? "Playing" : "Paused");

  // Progress Bar
  float progress = 0.0;
  if (currentSong.durationMs > 0) {
    progress = (float)currentSongPositionMs / (float)currentSong.durationMs;
  }
  int barWidth = 20;
  int filled = (int)(barWidth * progress);
  display.setCursor(0, 50); // Progress bar further down
  display.print("[");
  for (int i = 0; i < barWidth; i++) {
    if (i < filled) display.print("#");
    else display.print("-");
  }
  display.print("]");

  display.display();
  return true;
}

bool SpotConn::togglePlay() {
  String url = "https://api.spotify.com/v1/me/player/" + String(isPlaying ? "pause" : "play");
  isPlaying = !isPlaying;
  https.begin(*client, url);
  String auth = "Bearer " + String(accessToken);
  https.addHeader("Authorization", auth);
  https.addHeader("Content-Type", "application/json");
  https.addHeader("Content-Length", "0");
  int httpResponseCode = https.PUT("");
  bool success = false;
  if (httpResponseCode == 204) {
    success = true;
  } else {
    Serial.print("Error pausing/playing: ");
    Serial.println(httpResponseCode);
    String response = https.getString();
    Serial.println(response);
  }
  https.end();
  getTrackInfo();
  return success;
}

bool SpotConn::adjustVolume(int vol) {
  String url = "https://api.spotify.com/v1/me/player/volume?volume_percent=" + String(vol);
  https.begin(*client, url);
  String auth = "Bearer " + String(accessToken);
  https.addHeader("Authorization", auth);
  https.addHeader("Content-Type", "application/json");
  https.addHeader("Content-Length", "0");
  int httpResponseCode = https.PUT("");
  bool success = false;
  if (httpResponseCode == 204) {
    currVol = vol;
    success = true;
  } else {
    Serial.print("Error setting volume: ");
    Serial.println(httpResponseCode);
    String response = https.getString();
    Serial.println(response);
  }
  https.end();
  return success;
}

bool SpotConn::skipForward() {
  String url = "https://api.spotify.com/v1/me/player/next";
  https.begin(*client, url);
  String auth = "Bearer " + String(accessToken);
  https.addHeader("Authorization", auth);
  int httpResponseCode = https.POST("");
  bool success = false;
  if (httpResponseCode == 204) {
    success = true;
  } else {
    Serial.print("Error skipping forward: ");
    Serial.println(httpResponseCode);
    String response = https.getString();
    Serial.println(response);
  }
  https.end();
  getTrackInfo();
  return success;
}

bool SpotConn::skipBack() {
  String url = "https://api.spotify.com/v1/me/player/previous";
  https.begin(*client, url);
  String auth = "Bearer " + String(accessToken);
  https.addHeader("Authorization", auth);
  int httpResponseCode = https.POST("");
  bool success = false;
  if (httpResponseCode == 204) {
    success = true;
  } else {
    Serial.print("Error skipping backward: ");
    Serial.println(httpResponseCode);
    String response = https.getString();
    Serial.println(response);
  }
  https.end();
  getTrackInfo();
  return success;
}

bool SpotConn::likeSong(String songId) {
  String url = "https://api.spotify.com/v1/me/tracks?ids=" + songId;
  https.begin(*client, url);
  String auth = "Bearer " + String(accessToken);
  https.addHeader("Authorization", auth);
  https.addHeader("Content-Type", "application/json");
  char requestBody[] = "{\"ids\":[\"string\"]}";
  int httpResponseCode = https.PUT(requestBody);
  bool success = false;
  if (httpResponseCode == 200) {
    Serial.println("added track to liked songs");
    success = true;
  } else {
    Serial.print("Error adding to liked songs: ");
    Serial.println(httpResponseCode);
    String response = https.getString();
    Serial.println(response);
  }
  https.end();
  return success;
}

bool SpotConn::dislikeSong(String songId) {
  String url = "https://api.spotify.com/v1/me/tracks?ids=" + songId;
  https.begin(*client, url);
  String auth = "Bearer " + String(accessToken);
  https.addHeader("Authorization", auth);
  int httpResponseCode = https.sendRequest("DELETE");
  bool success = false;
  if (httpResponseCode == 200) {
    Serial.println("removed liked songs");
    success = true;
  } else {
    Serial.print("Error removing from liked songs: ");
    Serial.println(httpResponseCode);
    String response = https.getString();
    Serial.println(response);
  }
  https.end();
  return success;
}

bool SpotConn::playAlbum(String albumURI) {
  String url = "https://api.spotify.com/v1/me/player/play";
  https.begin(*client, url);
  String auth = "Bearer " + String(accessToken);
  https.addHeader("Authorization", auth);
  https.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(200);
  doc["context_uri"] = albumURI;
  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = https.PUT(requestBody);
  bool success = false;
  if (httpResponseCode == 204) {
    Serial.println("Album started playing");
    success = true;
  } else {
    Serial.print("Error starting album: ");
    Serial.println(httpResponseCode);
    String response = https.getString();
    Serial.println(response);
  }
  https.end();
  delay(1000);
  getTrackInfo(); // update screen info
  return success;
}
