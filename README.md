
# SpotifyControllerIoT
A smart IoT device using an ESP32 microcontroller to control Spotify playback and nfc based album selection

Login once via a local web server, then enjoy play/pause, skip, like/unlike, volume control, and NFC triggered album playback all from your device.

## Features

- Spotify auth via local ESP32 web server
- OLED display for showing track info and status
- playback controls with physical buttons:
	- play/pause
	- like/unlike track
	- skip forward
	- skip backward
- Analog volume control using potentiometer
- NFC integration - tap a card or tag to start a specific album
- Automatic token refresh for continuous playback

## Hardware requirements
- ESP32
- OLED display (I2C)
- NFC module (I2C)
- 4x push buttons
- Potentiometer
- Breadboard and wires

### Screenshots
project in action:


### Wiring Diagram

## Getting started
1) Clone Repository
2) Edit `config.h` with your credentials `client ID & Secret from Spotify Dev Dashboard`
3) Upload code: open project in Arduino IDE, select ESP32 board and COM port finally upload
4) Authenticate: Either open serial montior and note ESP32's IP or should be displayed on OLED
Visit http://<esp32_ip>/ in browser, login to spotify and grant permissions.

## Usage
- Use buttons to control playback
- Adjust volume with potentiometer
- Tap NFC cards once configed to start sepcific ablums (edit `main.cpp` to map tag UIDS to spotify URIs)
