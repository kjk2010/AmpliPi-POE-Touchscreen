# AmpliPi-POE-Touchscreen-Keypad
POE-powered touchscreen controller for AmpliPi
A touchscreen keypad designed to control AmpliPi, using the Olimex ESP32-PoE board.

For the WiFi version, go to this repo: https://github.com/kjk2010/AmpliPi-Touchscreen-Keypad

Currently a work in progress.

Built using VS Code with Platform.IO.

#### Currently supported hardware
- Olimex ESP32-PoE board
- ILI9341 compatible 320x240 TFT touchscreen

#### Basic setup instructions
1. Wire the TFT touchscreen to the ESP32-PoE using the UEXT connector (additional details coming soon)

###### Pin selection for the Olimex ESP32-PoE board's UEXT connector
- TFT_MISO 16
- TFT_MOSI 13
- TFT_SCLK 14
- TFT_CS   15
- TFT_DC    2
- TFT_RST   5
- TOUCH_CS  4

![alt text](https://github.com/kjk2010/AmpliPi-Touchscreen-Keypad/blob/main/docs/ESP32-to-TFT-pin-assignment.jpg?raw=true)

2. Clone this Github project
3. Upload Filesystem Image to ESP32-PoE (this uploads the files in the data folder to the ESP32's file system)
4. Upload to ESP32-PoE

Note: Some screens can't be reliably powered via the board's 3.3v pins and instead should be powered from 5v or an external power source. On the ESP32-PoE, this requires soldering a pin to the 5v pin on EXT1.

Also, WiFi functionality on the ESP32-PoE is not available in this repo. If WiFi is needed, use the WiFi repo: https://github.com/kjk2010/AmpliPi-Touchscreen-Keypad 

#### To do items
- [ ] Add support for stream commands: Play/Pause, Next, Stop, Like
- [ ] Add local inputs to source selection screen
- [ ] Show configured names for local inputs
- [ ] Add AmpliPi preset functionality
- [ ] Put metadata into sprites and scroll long titles
- [ ] Split display functions from update data functions. Display functions should be drawing everything from memory, and update functions should be updating the data and triggering a draw if data has changed.
- [ ] Show album art for all available stream sources (will require patching AmpliPi)
- [x] Switch to JPEG images instead of BMP in order to speed up downloading and display of album art
- [x] Add support full multiple screen sizes
- [x] Add POE and Ethernet capabilities, utilizing the Olimex ESP32-POE board
- [x] Add source selection screen
- [x] Add local source to source selection screen
- [x] Add settings screen: Change Zone, Change Source, Reset WiFi & AmpliPi Host, Reset Touchscreen
- [x] Move zone selection to settings screen and save to config file
- [x] Show album art for local inputs
- [x] Support controlling one or two zones
- [x] Add mDNS resolution support so touchscreen can find amplipi.local

#### Future features
- Screen time out options (PIR, touch, screensaver showing full screen only metadata?)
- Switch to using SSE or WebSockets, whichever AmpliPi server offers, instead of spamming API
- Simple web server to allow changing of AmpliPi name/IP, display settings, color scheme
- Add OTA upgrades
- Integrate Home Assistant control options on a second screen
- Add support for expansion units (more than 6 output zones)
- Control stream sources like Spotify playlists or Pandora stations
