# AmpliPi POE Touchscreen Controller
A POE-powered touchscreen controller designed to control [AmpliPi](https://amplipi.com), using the Olimex ESP32-PoE board.

Currently a work in progress, nearing initial release.

Built using VS Code with Platform.IO.

### Preview of device and UI
##### Primary screen with currently playing metadata:
![Internet radio source with metadata](https://github.com/kjk2010/AmpliPi-POE-Touchscreen/blob/main/docs/InternetRadioWithMetadata.jpg?raw=true)

- Top left button turns off screen. Touching the screen while off will turn it back on.
- Top right button shows source selection screen.
- Bottom area is volume level and mute control.

##### FM radio source with multiple zones
![FM radio source with multiple zones](https://github.com/kjk2010/AmpliPi-POE-Touchscreen/blob/main/docs/FMRadioWithMultiZones.jpg?raw=true)

Ability to control up to two zone volumes from the main screen.

##### Local (RCA) source
![Local (RCA) source](https://github.com/kjk2010/AmpliPi-POE-Touchscreen/blob/main/docs/LocalInput.jpg?raw=true)

The primary screen when the local source is selected.

##### Source selection screen
![Source selection screen](https://github.com/kjk2010/AmpliPi-POE-Touchscreen/blob/main/docs/SourceSelection.jpg?raw=true)

Clicking the button on the top right lets you select any available source.

##### Settings screen
![Settings screen](https://github.com/kjk2010/AmpliPi-POE-Touchscreen/blob/main/docs/Settings.jpg?raw=true)

A few settings to adjust which zones and source ID to control.


#### Currently supported hardware
- Olimex ESP32-PoE board
- ILI9341 or ILI9488 compatible TFT touchscreen with resolutions of 240x320 or 320x480

#### Basic setup instructions
1. Wire the TFT touchscreen to the ESP32-PoE using the UEXT connector (PCB details coming soon). Edit platform.ini as needed.

###### Pin selection for the Olimex ESP32-PoE board's UEXT connector
- TFT_CS=15
- TFT_RST=5
- TFT_DC=2
- TFT_MOSI=13
- TFT_SCLK=14
- TFT_MISO=16
- TFT_BL=16
- DTOUCH_CS=4

2. Clone this Github project
3. Upload Filesystem Image to ESP32-PoE (this uploads the files in the data folder to the ESP32's file system)
4. Compile and upload to ESP32-PoE
5. Power on controller. It will look for an AmpliPi to http://amplipi.local

A 3D printable faceplate design will be available after the first controller release.

Note: WiFi functionality on the ESP32-PoE is not available in this repo. If WiFi is needed, use the WiFi repo: https://github.com/kjk2010/AmpliPi-Touchscreen-Keypad 

#### To do items for first release
- [ ] Publish compiled binary
- [ ] Increase image sizes on screen to improve UX

#### Finished items
- [x] Add controller update support from local AmpliPi source (will require a minor AmpliPi web server modification)
- [x] Fix touch locations for buttons on settings page on larger screens
- [x] Add rotation setting to settings screen
- [x] Fix bug where top source bar is blank on startup if source is "None"
- [x] Update GUI design, including source selection buttons
- [x] Fix bug on stream selection screen showing 'next' button incorrectly when more than 8 streams exist
- [x] Add local inputs on stream selection screen
- [x] Add support for stream commands: Play/Pause, Next, Stop, Like
- [x] Show album art for all available stream sources
- [x] Backlight control (button to turn screen on and off)
- [x] Switch to JPEG images instead of BMP in order to speed up downloading and display of album art
- [x] Add support full multiple screen sizes
- [x] Add POE and Ethernet capabilities, utilizing the Olimex ESP32-POE board
- [x] Add stream selection screen
- [x] Add local source to stream selection screen
- [x] Add settings screen: Change Zone, Change Source, Reset WiFi & AmpliPi Host, Reset Touchscreen
- [x] Move zone selection to settings screen and save to config file
- [x] Show album art for local inputs
- [x] Support controlling one or two zones
- [x] Add mDNS resolution support so touchscreen can find amplipi.local

#### Future features
- Infrared receiver support to control volume level and mute on configured channel
- Quick access buttons for controllable features, such as turning lights on and off
- Touching album art should go to a full screen mode with only album art and metadata. When clicking anywhere on the screen in this mode, switch back to regular metadata mode.
- Put metadata into sprites to scroll long values
- Add AmpliPi preset functionality
- Screen time out options (PIR, touch, screensaver showing full screen only metadata?)
- Switch to using SSE or WebSockets, whichever AmpliPi server offers, instead of spamming API
- Simple web server to allow changing of AmpliPi name/IP, display settings, color scheme
- Add support for expansion units (more than 6 output zones)
- Allow firmware updates to come from AmpliPi server
- Integrate Home Assistant control options on a second screen
