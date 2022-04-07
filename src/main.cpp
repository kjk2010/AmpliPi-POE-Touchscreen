#include <Arduino.h>
#include <stdint.h>
#include "FS.h"
#include "SPIFFS.h"
#include <Free_Fonts.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include <SPI.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <utils.h>
#include <TJpg_Decoder.h>

#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT
#define ETH_PHY_POWER 12

#include <ETH.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <Update.h>

static bool eth_connected = false;

#define VERSION "1.0.0"

/* Debug options */
#define DEBUGAPIREQ false // Debug API requests to Serial

#define DEBUG_WEBSERVER false // are you running the AmpliPi debug server on port 5000?

/**************************************/
/* Configure screen colors and layout */
/**************************************/
// How quickly does the metadata refresh (in milliseconds)
#define REFRESH_INTERVAL 2000

// How long to show the selection screen before returning to the full-screen metadata screen (in milliseconds)
#define SELECTSCREEN_TIMEOUT 10000

// Colors
#define GREY 0x5AEB
#define BLUE 0x9DFF

// Source bar location
#define SRCBAR_X 0
#define SRCBAR_Y 0
#define SRCBAR_W TFT_WIDTH
#define SRCBAR_H 50

#define SRCBUTTON_X (TFT_WIDTH - 40) // Top right side of screen
#define SRCBUTTON_Y 0
#define SRCBUTTON_W 50 // Oversize the button for easy selection
#define SRCBUTTON_H 50 // Oversize the button for easy selection

// Settings and About Screen Buttons
#define LEFTBUTTON_X 0
#define LEFTBUTTON_Y (TFT_HEIGHT - 50)
#define LEFTBUTTON_W 130
#define LEFTBUTTON_H 50

#define CENTERBUTTON_X ((TFT_WIDTH / 2) - (44 / 2)) // Center
#define CENTERBUTTON_Y (TFT_HEIGHT - 45)
#define CENTERBUTTON_W 50
#define CENTERBUTTON_H 50

#define RIGHTBUTTON_W 130
#define RIGHTBUTTON_H 50
#define RIGHTBUTTON_X (TFT_WIDTH - RIGHTBUTTON_W)
#define RIGHTBUTTON_Y (TFT_HEIGHT - 50)

// Main area, normally where metadata is shown
#define MAINZONE_X 0
#define MAINZONE_Y 50
#define MAINZONE_W TFT_WIDTH
#define MAINZONE_H (TFT_HEIGHT - 50) // From below the source top bar to the bottom of the screen

// Album art location on selection screen
#define ALBUMART_X 60
#define ALBUMART_Y 50
#define ALBUMART_W 200
#define ALBUMART_H ALBUMART_W

// Album art location on metadata screen
#define ALBUMART_FULL_X 0
#define ALBUMART_FULL_Y 50
#define ALBUMART_FULL_W TFT_WIDTH
#define ALBUMART_FULL_H ALBUMART_W

// Source Control Buttons
#define PLAYPAUSEBUTTON_X (TFT_WIDTH - 36)
#define PLAYPAUSEBUTTON_Y 64
#define PLAYPAUSEBUTTON_W 36
#define PLAYPAUSEBUTTON_H 36

#define SKIPBUTTON_X (TFT_WIDTH - 36)
#define SKIPBUTTON_Y 118
#define SKIPBUTTON_W 36
#define SKIPBUTTON_H 36

#define LIKEBUTTON_X 0
#define LIKEBUTTON_Y 64
#define LIKEBUTTON_W 36
#define LIKEBUTTON_H 36

#define DISLIKEBUTTON_X 0
#define DISLIKEBUTTON_Y 118
#define DISLIKEBUTTON_W 36
#define DISLIKEBUTTON_H 36

// Metadata text location
#define METATEXT_X 0
#define METATEXT_Y (ALBUMART_Y + ALBUMART_H + 10) // Start Metadata text 10px below the album art
#define METATEXT_W TFT_WIDTH
#define METATEXT_H 90

// Warning zone
#define WARNZONE_X 0
#define WARNZONE_Y (TFT_HEIGHT - 87) // Just above volume bar(s)
#define WARNZONE_W TFT_WIDTH
#define WARNZONE_H 14

// Mute button location
#define MUTE_X 0
#define MUTE1_Y (TFT_HEIGHT - 73) // (upper)
#define MUTE2_Y (TFT_HEIGHT - 50) // (lower)
#define MUTE_W 50
#define MUTE_H 50

// Volume control bar position and size
#define VOLBAR_X 45
#define VOLBAR1_Y (TFT_HEIGHT - 60) // (upper)
#define VOLBAR2_Y (TFT_HEIGHT - 23) // (lower)
#define VOLBAR_W (TFT_WIDTH - 90) // 150 for 240w screen, 230 for 320ww screen
#define VOLBAR_H 6

// Volume control bar zone size
#define VOLBARZONE_X 50
#define VOLBARZONE1_Y (TFT_HEIGHT - 73) // (upper)
#define VOLBARZONE2_Y (TFT_HEIGHT - 50) // (lower)
#define VOLBARZONE_W (TFT_WIDTH - 50)
#define VOLBARZONE_H 50

#if TFT_WIDTH <= 240
    // Maximum length of the source name
    #define SRC_NAME_LEN 16

    // Max length for title and artist metadata
    #define TITLE_LEN 18
    #define ARTIST_LEN 18

    // Number of streams to show for stream selection
    #define MAX_STREAMS 5
#else
    // Maximum length of the source name
    #define SRC_NAME_LEN 25

    // Max length for title and artist metadata
    #define TITLE_LEN 24
    #define ARTIST_LEN 25

    // Number of streams to show for stream selection
    #define MAX_STREAMS 7
#endif

/******************************/
/* Configure system variables */
/******************************/
TFT_eSPI tft = TFT_eSPI(); // Invoke TFT display library

// This is the file name used to store the touch coordinate
// calibration data. Cahnge the name to start a new calibration.
#define CALIBRATION_FILE "/TouchCalData"

// Set REPEAT_CAL to true instead of false to run calibration
// again, otherwise it will only be done once.
// Repeat calibration if you change the screen rotation.
#define REPEAT_CAL false

#define AMPLIPIHOST_LEN     64
#define AMPLIPIZONE_LEN     6
char amplipiHost [AMPLIPIHOST_LEN] = "amplipi.local"; // Default settings
char amplipiZone1 [AMPLIPIZONE_LEN] = "0";
char amplipiZone2 [AMPLIPIZONE_LEN] = "-1";
char amplipiSource [AMPLIPIZONE_LEN] = "0";
char configFileName[] = "/config.json";
String hostname = "APCT"; // ETH MAC appended to this value
String controllerVersionURI = "/static/controller_version.txt";
String controllerBin = "/static/controller.bin";


/*************************************/
/* Configure globally used variables */
/*************************************/
String activeScreen = "select"; // Available screens: select, metadata, source, setting, about, off
String amplipiHostIP = "";
String sourceName = "";
String currentArtist = "";
String currentSong = "";
String currentStatus = "";
String currentAlbumArt = "";
String currentStreamID = "";
String currentStreamName = "";
String currentStreamType = "";
String sourceInput;
bool inWarning = false;
int newAmplipiSource = 0;
int newAmplipiZone1 = 0;
int newAmplipiZone2 = 0;
bool amplipiZone2Enabled = false;
int currentSourceOffset = 0;
int sourceIDs[9];
bool updateAlbumart = true;
bool updateSource = false;
bool updateMute1 = true;
bool updateMute2 = true;
bool muteZone1 = false;
bool muteZone2 = false;
float volPercent1 = 100;
float volPercent2 = 100;
bool updateVol1 = true;
bool updateVol2 = true;
bool metadata_refresh = true;
int screenRotation = 0; // Default value that can be changed in settings. 0 or 2

// Command statuses
bool cmdPlaying = true;
bool cmdLike = false;
bool cmdDislike = false;


/***********************/
/* Configure functions */
/***********************/
String findMDNS(String mDnsHost) {
    // If the input ends in '.local', we have to remove it for mDNS resolution
    int len = mDnsHost.length();
    if (mDnsHost.substring((len - 6)) == ".local") {
        mDnsHost.replace(".local", "");
    }

    Serial.println("Finding the mDNS details...");
    Serial.println(mDnsHost);

    //if (!MDNS.begin(chipid_char)) {
    if (!MDNS.begin(hostname.c_str())) {
        Serial.println("Error setting up MDNS responder.");
    } else {
        Serial.println("Finished initializing the MDNS client.");
        Serial.print("Hostname: ");
        Serial.println(hostname);
    }

    Serial.println("mDNS responder started");
    IPAddress serverIp = MDNS.queryHost(mDnsHost);
    while (serverIp.toString() == "0.0.0.0") {
        Serial.println("Trying again to resolve mDNS");
        tft.fillRect(0, 280, 240, 40, TFT_BLACK); // Clear area first
        tft.drawString("Looking for AmpliPi at: ", 120, 280, GFXFF); // Center Middle
        tft.drawString(mDnsHost, 120, 300, GFXFF); // Center Middle
        delay(250);
        serverIp = MDNS.queryHost(mDnsHost);
    }
    Serial.print("IP address of server: ");
    Serial.println(serverIp.toString());
    Serial.println("Finished finding the mDNS details...");
    return serverIp.toString();
}

bool saveFileFSConfigFile()
{
    Serial.println(F("Saving config"));

    DynamicJsonDocument json(1024);

    json["amplipiHost"]  = amplipiHost;
    json["amplipiZone1"] = amplipiZone1;
    json["amplipiZone2"] = amplipiZone2;
    json["amplipiSource"] = amplipiSource;
    json["screenRotation"] = tft.getRotation();

    File configFile = SPIFFS.open(configFileName, "w");

    if (!configFile)
    {
        Serial.println(F("Failed to open config file for writing"));

        return false;
    }

    serializeJsonPretty(json, Serial);
    // Write data to file and close it
    serializeJson(json, configFile);

    configFile.close();
    //end save

    return true;
}

bool loadFileFSConfigFile()
{
    //read configuration from FS json
    Serial.println(F("Mounting FS..."));

    if (SPIFFS.begin())
    {
        Serial.println(F("Mounted file system"));
        if (SPIFFS.exists(configFileName))
        {
            //file exists, reading and loading
            Serial.println(F("Reading config file"));
            File configFile = SPIFFS.open(configFileName, "r");

            if (configFile)
            {
                Serial.print(F("Opened config file, size = "));
                size_t configFileSize = configFile.size();
                Serial.println(configFileSize);

                // Allocate a buffer to store contents of the file.
                std::unique_ptr<char[]> buf(new char[configFileSize + 1]);

                configFile.readBytes(buf.get(), configFileSize);

                Serial.print(F("\nJSON parseObject() result : "));

                DynamicJsonDocument json(1024);
                auto deserializeError = deserializeJson(json, buf.get(), configFileSize);

                if ( deserializeError )
                {
                    Serial.println(F("failed"));
                    return false;
                }
                else
                {
                    Serial.println(F("OK"));

                    if (json["amplipiHost"])
                        strncpy(amplipiHost,  json["amplipiHost"],  sizeof(amplipiHost));
                    
                    if (json["amplipiZone1"])
                        strncpy(amplipiZone1, json["amplipiZone1"], sizeof(amplipiZone1));
            
                    if (json["amplipiZone2"])
                        strncpy(amplipiZone2, json["amplipiZone2"], sizeof(amplipiZone2));
            
                    if (json["amplipiSource"])
                        strncpy(amplipiSource, json["amplipiSource"], sizeof(amplipiSource));
            
                    if (json["screenRotation"])
                        screenRotation = json["screenRotation"];
                }

                //serializeJson(json, Serial);
                serializeJsonPretty(json, Serial);

                configFile.close();
            }
        }
        else {
            // Config file doesn't exist. Create config file with default settings
            saveFileFSConfigFile();
        }
    }
    else
    {
        Serial.println(F("failed to mount FS"));
        return false;
    }
    return true;
}


void getAmpliPiIP()
{
    // Check to see if IP or DNS
    if (validateIPv4(amplipiHost)) {
        // Is an IP
        amplipiHostIP = String(amplipiHost);
        Serial.print("amplipiHostIP (IP): ");
        Serial.println(amplipiHostIP);
    }
    else {
        // Not an IP, look up via MDNS:
        amplipiHostIP = findMDNS(String(amplipiHost));
#if DEBUG_WEBSERVER
        amplipiHostIP += ":5000";
#endif
        //Serial.print("amplipiHostIP (mDNS): ");
        //Serial.println(amplipiHostIP);
    }
}


// Function to handle touchscreen calibration. Calibration only runs once, unless 
//  TouchCalData file is removed or REPEAT_CAL is set to true
void touch_calibrate()
{
    uint16_t calData[5];
    uint8_t calDataOK = 0;

    // check file system exists
    if (!SPIFFS.begin())
    {
        Serial.println("Formatting file system");
        SPIFFS.format();
        SPIFFS.begin();
    }

    // check if calibration file exists and size is correct
    if (SPIFFS.exists(CALIBRATION_FILE))
    {
        if (REPEAT_CAL)
        {
            // Delete if we want to re-calibrate
            SPIFFS.remove(CALIBRATION_FILE);
        }
        else
        {
            File f = SPIFFS.open(CALIBRATION_FILE, "r");
            if (f)
            {
                if (f.readBytes((char *)calData, 14) == 14)
                    calDataOK = 1;
                f.close();
            }
        }
    }

    if (calDataOK && !REPEAT_CAL)
    {
        // calibration data valid
        tft.setTouch(calData);
        Serial.println("Touch screen calibrated");
    }
    else
    {
        // data not valid so recalibrate
        Serial.println("Entering touch screen calibration...");
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(20, 20);
        tft.setFreeFont(FSS12);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);

        tft.println("Touch corners");
        tft.println("as indicated");

        //tft.setTextFont(1);
        tft.println();

        if (REPEAT_CAL)
        {
            //tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setFreeFont(FSS9);
            tft.println("Set REPEAT_CAL to false to stop this running again!");
        }

        tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

        //tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.println("Calibration complete!");

        // store data
        File f = SPIFFS.open(CALIBRATION_FILE, "w");
        if (f)
        {
            f.write((const unsigned char *)calData, 14);
            f.close();
        }
    }
}

// Used for wired ethernet, even though it's called WiFiEvent
void WiFiEvent(WiFiEvent_t event)
{
    String MAC;
    switch (event) {
        case SYSTEM_EVENT_ETH_START:
            Serial.println("ETH Started");

            MAC = ETH.macAddress();
            MAC.replace(":","");
            hostname += "-" + MAC;

            ETH.setHostname(hostname.c_str());
            break;
        case SYSTEM_EVENT_ETH_CONNECTED:
            Serial.println("ETH Connected");
            break;
        case SYSTEM_EVENT_ETH_GOT_IP:
            Serial.print("ETH MAC: ");
            Serial.print(ETH.macAddress());
            Serial.print(", IPv4: ");
            Serial.print(ETH.localIP());
            if (ETH.fullDuplex()) {
                Serial.print(", FULL_DUPLEX");
            }
            Serial.print(", ");
            Serial.print(ETH.linkSpeed());
            Serial.println("Mbps");
            eth_connected = true;
            break;
        case SYSTEM_EVENT_ETH_DISCONNECTED:
            Serial.println("ETH Disconnected");
            eth_connected = false;
            break;
        case SYSTEM_EVENT_ETH_STOP:
            Serial.println("ETH Stopped");
            eth_connected = false;
            break;
        default:
            break;
    }
}


// Clear the main area of the screen. Generally metadata is shown here, but also source select and settings
void clearMainArea()
{
    tft.fillRect(MAINZONE_X, MAINZONE_Y, MAINZONE_W, MAINZONE_H, TFT_BLACK); // Clear metadata area
}


// Show a warning near bottom of screen. Primarily used if we can't access AmpliPi API
void drawWarning(String message)
{
    if (!inWarning) {
        tft.fillRect(WARNZONE_X, WARNZONE_Y, WARNZONE_W, WARNZONE_H, TFT_BLACK); // Clear warning area
        Serial.print("Warning: ");
        Serial.println(message);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setFreeFont(FSS9);
        tft.setTextDatum(TL_DATUM);
        tft.drawString(message, (WARNZONE_X + 5), WARNZONE_Y);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setFreeFont(FSS12);
        inWarning = true;
    }
}

void clearWarning()
{
    Serial.println("Cleared warning.");
    tft.fillRect(WARNZONE_X, WARNZONE_Y, WARNZONE_W, WARNZONE_H, TFT_BLACK); // Clear warning area
    inWarning = false;
}


// Show bitmap image on screen
void drawBmp(const char *filename, int16_t x, int16_t y)
{

    if ((x >= tft.width()) || (y >= tft.height()))
        return;

    fs::File bmpFS;

    // Open requested file on SD card
    bmpFS = SPIFFS.open(filename, "r");

    if (!bmpFS)
    {
        Serial.print("File not found");
        return;
    }

    uint32_t seekOffset;
    uint16_t w, h, row;
    uint8_t r, g, b;

    uint32_t startTime = millis();

    if (read16(bmpFS) == 0x4D42)
    {
        read32(bmpFS);
        read32(bmpFS);
        seekOffset = read32(bmpFS);
        read32(bmpFS);
        w = read32(bmpFS);
        h = read32(bmpFS);

        if ((read16(bmpFS) == 1) && (read16(bmpFS) == 24) && (read32(bmpFS) == 0))
        {
            y += h - 1;

            bool oldSwapBytes = tft.getSwapBytes();
            tft.setSwapBytes(true);
            bmpFS.seek(seekOffset);

            uint16_t padding = (4 - ((w * 3) & 3)) & 3;
            uint8_t lineBuffer[w * 3 + padding];

            for (row = 0; row < h; row++)
            {

                bmpFS.read(lineBuffer, sizeof(lineBuffer));
                uint8_t *bptr = lineBuffer;
                uint16_t *tptr = (uint16_t *)lineBuffer;
                // Convert 24 to 16 bit colours
                for (uint16_t col = 0; col < w; col++)
                {
                    b = *bptr++;
                    g = *bptr++;
                    r = *bptr++;
                    *tptr++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                }

                // Push the pixel row to screen, pushImage will crop the line if needed
                // y is decremented as the BMP image is drawn bottom up
                tft.pushImage(x, y--, w, 1, (uint16_t *)lineBuffer);
            }
            tft.setSwapBytes(oldSwapBytes);
            Serial.print("Loaded in ");
            Serial.print(millis() - startTime);
            Serial.println(" ms");
        }
        else
            Serial.println("BMP format not recognized.");
    }
    bmpFS.close();
}


// This next function will be called during decoding of the jpeg file to
// render each block to the TFT
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
   // Stop further decoding as image is running off bottom of screen
  if ( y >= tft.height() ) return 0;

  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);

  // This might work instead if you adapt the sketch to use the Adafruit_GFX library
  // tft.drawRGBBitmap(x, y, bitmap, w, h);

  // Return 1 to decode next block
  return 1;
}


// API Request to Amplipi
String requestAPI(String request)
{
    HTTPClient http;

    String url = "http://" + amplipiHostIP + "/api/" + request;
    String payload;

#if DEBUGAPIREQ
    Serial.print("[HTTP] begin...\n");
    Serial.print("[HTTP] GET...\n");
#endif
    http.setConnectTimeout(5000);
    http.setTimeout(5000);
    http.begin(url); //HTTP

    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0)
    {
#if DEBUGAPIREQ
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
#endif

        // file found at server
        if (httpCode == HTTP_CODE_OK)
        {
            payload = http.getString();

#if DEBUGAPIREQ
            Serial.println(payload);
#endif

            // Clear the warning since we jsut received a successful API request
            if (inWarning)
            {
                clearWarning();
            }
        }
    }
    else
    {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        drawWarning("Unable to access AmpliPi");
    }

    http.end();

    return payload;
}


// Send PATCH to API
bool patchAPI(String request, String payload)
{
    HTTPClient http;

    bool result = false;

    String url = "http://" + amplipiHostIP + "/api/" + request;

#if DEBUGAPIREQ
    Serial.print("[HTTP] begin...\n");
    Serial.print("[HTTP] PATCH...\n");
#endif

    http.setConnectTimeout(5000);
    http.setTimeout(5000);
    http.begin(url); //HTTP
    http.addHeader("Accept", "application/json");
    http.addHeader("Content-Type", "application/json");

    // start connection and send HTTP header
    int httpCode = http.PATCH(payload);

    // httpCode will be negative on error
    if (httpCode > 0)
    {
#if DEBUGAPIREQ
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] PATCH... code: %d\n", httpCode);
#endif

        // file found at server
        if (httpCode == HTTP_CODE_OK)
        {
            result = true;

            // Clear the warning since we jsut received a successful API request
            if (inWarning)
            {
                clearWarning();
            }
        }
        String resultPayload = http.getString();

#if DEBUGAPIREQ
        Serial.println("[HTTP] PATCH result:");
        Serial.println(resultPayload);
#endif
    }
    else
    {
        Serial.printf("[HTTP] PATCH... failed, error: %s\n", http.errorToString(httpCode).c_str());
        drawWarning("Unable to access AmpliPi");
    }

    http.end();

    return result;
}


// Send POST to API
bool postAPI(String request, String payload)
{
    HTTPClient http;

    bool result = false;

    String url = "http://" + amplipiHostIP + "/api/" + request;

#if DEBUGAPIREQ
    Serial.print("[HTTP] begin...\n");
    Serial.print("[HTTP] POST...\n");
#endif

    http.setConnectTimeout(5000);
    http.setTimeout(5000);
    http.begin(url); //HTTP
    http.addHeader("Accept", "application/json");
    http.addHeader("Content-Type", "application/json");

    // start connection and send HTTP header
    int httpCode = http.POST(payload);

    // httpCode will be negative on error
    if (httpCode > 0)
    {
#if DEBUGAPIREQ
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] POST... code: %d\n", httpCode);
#endif

        // file found at server
        if (httpCode == HTTP_CODE_OK)
        {
            result = true;

            // Clear the warning since we jsut received a successful API request
            if (inWarning)
            {
                clearWarning();
            }
        }
        String resultPayload = http.getString();

#if DEBUGAPIREQ
        Serial.println("[HTTP] POST result:");
        Serial.println(resultPayload);
#endif
    }
    else
    {
        Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
        drawWarning("Unable to access AmpliPi");
    }

    http.end();

    return result;
}


// Download album art or logo from AmpliPi API
bool downloadAlbumart(String sourceID)
{
    int aaW = 200;

    if (activeScreen == "select") {
        aaW = ALBUMART_W;
    }
    else if (activeScreen == "metadata"){
        aaW = ALBUMART_FULL_W;
    }

    HTTPClient http;
    bool outcome = true;
    String url = "http://" + amplipiHostIP + "/api/sources/" + sourceID + "/image/" + String(aaW);
    String filename = "/albumart.jpg";

    // configure server and url
    http.setConnectTimeout(20000);
    http.setTimeout(20000);
    http.begin(url);

    // start connection and send HTTP header
    int httpCode = http.GET();

#if DEBUGAPIREQ
    Serial.println(("[HTTP] GET DONE with code " + String(httpCode)));
#endif

    if (httpCode > 0)
    {
        Serial.println(F("-- >> OPENING FILE..."));

        SPIFFS.remove(filename);
        fs::File f = SPIFFS.open(filename, "w+");
        if (!f)
        {
            Serial.println(F("file open failed"));
            return false;
        }

#if DEBUGAPIREQ
        // HTTP header has been sent and Server response header has been handled
        Serial.printf("-[HTTP] GET... code: %d\n", httpCode);
        Serial.println("Free Heap: " + String(ESP.getFreeHeap()));
#endif

        // file found at server
        if (httpCode == HTTP_CODE_OK)
        {
            http.writeToStream(&f);

            Serial.println("[HTTP] connection closed or file end.");
        }
        f.close();
    }
    else
    {
        Serial.println("[HTTP] GET... failed, error: " + http.errorToString(httpCode));

        //drawWarning("Unable to access AmpliPi");
        outcome = false;
    }
    http.end();
    return outcome;
}


// Show downloaded album art of screen
void drawAlbumart()
{
    int aaX = 0;
    int aaY = 0;

    // Only update album art on screen if we need
    if (!updateAlbumart)
    {
        return;
    };

    if (activeScreen == "select") {
        aaX = ALBUMART_X;
        aaY = ALBUMART_Y;
    }
    else if (activeScreen == "metadata"){
        aaX = ALBUMART_FULL_X;
        aaY = ALBUMART_FULL_Y;
    }

    tft.fillRect(ALBUMART_FULL_X, ALBUMART_FULL_Y, ALBUMART_FULL_W, ALBUMART_FULL_H, TFT_BLACK); // Clear album art first
    Serial.println("Drawing album art.");

    TJpgDec.drawFsJpg(aaX, aaY, "/albumart.jpg");
    updateAlbumart = false;
}


// Show the current source on screen, top 36px of screen (by default)
void drawSource()
{
    // Only update source on screen if we need
    if (!updateSource)
    {
        return;
    };

    tft.setFreeFont(FSS9);
    tft.setTextDatum(TL_DATUM);
    tft.fillRect(SRCBAR_X, SRCBAR_Y, SRCBAR_W, SRCBAR_H, TFT_BLACK); // Clear source bar first
    tft.drawString(currentStreamName, 38, 10, GFXFF); // Top Left
    drawBmp("/power.bmp", 0, 0);
    drawBmp("/source.bmp", (SRCBAR_W - 36), SRCBAR_Y);

    updateSource = false;
}


void drawSourceSelection()
{
    bool showPrev = false;
    bool showNext = false;
    String streamName = "";

    // Stop metadata refresh
    metadata_refresh = false;

    Serial.println("Opening source selection screen.");

    // Download source options
    String status_json = requestAPI("streams"); // Requesting /api/streams
    
    // DynamicJsonDocument<N> allocates memory on the heap
    DynamicJsonDocument apiStatus(6144);

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(apiStatus, status_json);

    // Test if parsing succeeds.
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
    }

    // Clear screen
    clearMainArea();

    // Draw size selecton boxes
    tft.setTextDatum(TL_DATUM);
    //tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setFreeFont(FSS12);

    Serial.print("currentSourceOffset: ");
    Serial.println(currentSourceOffset);
    Serial.println("Streams:");
    int bi = 0; // Base iterator within 'print MAX_STREAMS items' loop (0-6)
    int i = 0; // Stream iterator
    int itemH = 54;
    int maxstreams = currentSourceOffset + MAX_STREAMS;

    uint16_t vlightgrey = tft.color565(240, 240, 240);
    tft.setTextColor(TFT_BLACK, vlightgrey);
    
    // Start with showing 'OFF' and 'Local - RCA' options at top of the list
    if (currentSourceOffset <= 0) {
        // Display 'OFF' button
        tft.fillRect(MAINZONE_X, (MAINZONE_Y + (itemH * bi)), TFT_WIDTH, 38, vlightgrey); // Selection box background
        tft.fillRect(MAINZONE_X, (MAINZONE_Y + (itemH * bi)), 12, 38, TFT_RED); // Left marker on selection box
        tft.drawRect(MAINZONE_X, (MAINZONE_Y + (itemH * bi)), TFT_WIDTH, 38, TFT_RED); // Selection box
        tft.drawString("OFF", (MAINZONE_X + 15), (MAINZONE_Y + (itemH * bi) + 10));
        sourceIDs[bi] = -1;
        ++bi;
        ++i;

        // Display 'Local - RCA' button
        //tft.setTextColor(TFT_WHITE, TFT_NAVY);
        tft.fillRect(MAINZONE_X, (MAINZONE_Y + (itemH * bi)), TFT_WIDTH, 38, vlightgrey); // Selection box background
        tft.fillRect(MAINZONE_X, (MAINZONE_Y + (itemH * bi)), 12, 38, TFT_NAVY); // Left marker on selection box
        tft.drawRect(MAINZONE_X, (MAINZONE_Y + (itemH * bi)), TFT_WIDTH, 38, TFT_NAVY); // Selection box
        tft.drawString("Local - RCA", (MAINZONE_X + 15), (MAINZONE_Y + (itemH * bi) + 10));
        sourceIDs[bi] = 0;
        ++bi;
        ++i;
    }
    else {
        currentSourceOffset = currentSourceOffset - 2; // Subtract the 'OFF' and 'Local - RCA' buttons
    }

    int currentSO = currentSourceOffset;
    bool zone2Enabled = amplipiZone2Enabled;

    Serial.print("Max streams: ");
    Serial.println(maxstreams);
    for (JsonObject value : apiStatus["streams"].as<JsonArray>()) {
        JsonObject thisStream = value;
        streamName = thisStream["name"].as<String>();
        Serial.print(thisStream["id"].as<String>());
        Serial.print(" - ");
        Serial.print(streamName);
        
        if (streamName.length() > SRC_NAME_LEN) {
            streamName = streamName.substring(0,SRC_NAME_LEN) + "...";
        }

        // Only print MAX_STREAM number of items on screen, but continue iterating through the rest of the list
        if (i >= currentSO && i < maxstreams)
        {
            Serial.print(" - String X: ");
            Serial.print((MAINZONE_X + 12));
            Serial.print(" - String Y: ");
            Serial.println((MAINZONE_Y + (itemH * bi) + 10));

            // Add to sourceIDs array to be used in the main loop when one of the button is selected
            sourceIDs[bi] = thisStream["id"].as<int>();

            // Display stream button
            tft.fillRect(MAINZONE_X, (MAINZONE_Y + (itemH * bi)), TFT_WIDTH, 38, vlightgrey); // Selection box background
            tft.fillRect(MAINZONE_X, (MAINZONE_Y + (itemH * bi)), 12, 38, TFT_NAVY); // Left marker on selection box
            tft.drawRect(MAINZONE_X, (MAINZONE_Y + (itemH * bi)), TFT_WIDTH, 38, TFT_NAVY); // Selection box
            tft.drawString(streamName, (MAINZONE_X + 15), (MAINZONE_Y + (itemH * bi) + 10));
            ++bi;
        }
        Serial.print("i: ");
        Serial.println(i);
        ++i;
    }
    currentSourceOffset = currentSO; // Fixes a bug where going through more than 6 items on the JSON array sets the currentSourceOffset to the sixth item's ID
    amplipiZone2Enabled = zone2Enabled; // Also affects this global setting

    if (i > (MAX_STREAMS + currentSourceOffset)) { showNext = true; }
    if (currentSourceOffset > 0) { showPrev = true; }

    // Previous and Next buttons
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);

    // Previous button
    if (showPrev)
    {
        tft.fillRoundRect(LEFTBUTTON_X, LEFTBUTTON_Y, LEFTBUTTON_W, LEFTBUTTON_H, 6, TFT_DARKGREY);
        tft.drawString("< Back", (LEFTBUTTON_X + 60), (LEFTBUTTON_Y + 15));
    }

    // Settings button
    drawBmp("/settings.bmp", CENTERBUTTON_X, CENTERBUTTON_Y);

    // Next button
    if (showNext)
    {
        tft.fillRoundRect(RIGHTBUTTON_X, RIGHTBUTTON_Y, RIGHTBUTTON_W, RIGHTBUTTON_H, 6, TFT_DARKGREY);
        tft.drawString("Next >", (RIGHTBUTTON_X + 60), (RIGHTBUTTON_Y + 15));
    }

    // Reset to default
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setFreeFont(FSS12);

}


void selectSource(int y)
{
    // Send source selection
    Serial.print("Source Select - Y: ");
    Serial.println(y);

    /* Box Y values. Selection boxes are 38 pixels tall, with 4 pixels between boxes
    1: 36 - 74
    2: 78 - 116
    3: 120 - 158
    4: 162 - 200
    5: 204 - 242
    6: 246 - 284
    -- If screen length is greater than 320px:
    7: 288 - 326
    8: 330 - 368
    9: 372 - 410

    Add a couple pixels around the boxes:
    */
    int selected = 0;
    if (y >= 36 && y <= 76) { selected = 0; }
    else if (y >= 77 && y <= 118) { selected = 1; }
    else if (y >= 119 && y <= 160) { selected = 2; }
    else if (y >= 161 && y <= 202) { selected = 3; }
    else if (y >= 203 && y <= 244) { selected = 4; }
    else if (y >= 245 && y <= 286) { selected = 5; }
    else if (y >= 287 && y <= 328) { selected = 6; }
    else if (y >= 329 && y <= 370) { selected = 7; }
    else if (y >= 371 && y <= 412) { selected = 8; }

    String inputID;
    if (sourceIDs[selected] == -1) {
        inputID = "None";
    }
    else if (sourceIDs[selected] == 0) {
        inputID = "local";
    }
    else {
        inputID = "stream=" + String(sourceIDs[selected]);
    }

    // Send to API
    String payload = "{\"input\": \"" + inputID + "\"}";
    Serial.println(payload);
    bool result = patchAPI("sources/" + String(amplipiSource), payload);
    Serial.print("selectSource result: ");
    Serial.println(result);
}


void powerOffScreen() {
    // Stop metadata refresh
    metadata_refresh = false;

    // Clear screen
    tft.fillRect(0, 0, TFT_WIDTH, TFT_HEIGHT, TFT_BLACK);

    // Turn off backlight
    digitalWrite(TFT_BL, LOW);

}

void powerOnScreen() {
    // Turn on backlight
    digitalWrite(TFT_BL, HIGH);
}

void drawSettings()
{
    // Available settings:
    // - Select zones to manage
    // - Reset WiFi settings (delete 'wifi.json' config file and reboot, or enable web server to select AP)
    // - Restart
    // - Show version, Wifi AP, IP address

    // Stop metadata refresh
    metadata_refresh = false;

    // Clear screen
    clearMainArea();

    // Show settings:
    // Zone 1
    tft.setFreeFont(FSS12);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Zone 1: " + String(amplipiZone1), 5, 55);

    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.fillRoundRect((TFT_WIDTH - 80), 50, 36, 36, 6, TFT_DARKGREEN);
    tft.drawString("<", (TFT_WIDTH - 70), 55);
    tft.fillRoundRect((TFT_WIDTH - 40), 50, 36, 36, 6, TFT_DARKGREEN);
    tft.drawString(">", (TFT_WIDTH - 28), 55);

    tft.fillRect(20, 80, (TFT_WIDTH - 40), 1, GREY); // Seperator
    
    // Zone 2
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    String thisZone;
    if (atoi(amplipiZone2) < 0) { thisZone = "None"; }
    else { thisZone = String(amplipiZone2); }
    tft.drawString("Zone 2: " + thisZone, 5, 90);

    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.fillRoundRect((TFT_WIDTH - 80), 90, 36, 36, 6, TFT_DARKGREEN);
    tft.drawString("<", (TFT_WIDTH - 70), 95);
    tft.fillRoundRect((TFT_WIDTH - 40), 90, 36, 36, 6, TFT_DARKGREEN);
    tft.drawString(">", (TFT_WIDTH - 28), 95);
    
    tft.fillRect(20, 120, (TFT_WIDTH - 40), 1, GREY); // Seperator

    // Source
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Source: " + String(amplipiSource), 5, 130);

    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.fillRoundRect((TFT_WIDTH - 80), 130, 36, 36, 6, TFT_DARKGREEN);
    tft.drawString("<", (TFT_WIDTH - 70), 135);
    tft.fillRoundRect((TFT_WIDTH - 40), 130, 36, 36, 6, TFT_DARKGREEN);
    tft.drawString(">", (TFT_WIDTH - 28), 135);

    tft.fillRect(20, 170, (TFT_WIDTH - 40), 1, GREY); // Seperator

    // Restart Controller
    tft.setFreeFont(FSS9);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.fillRoundRect(0, 180, TFT_WIDTH, 36, 6, TFT_DARKGREEN);
    tft.drawString("Reboot Controller", 10, 188);

    // Re-calibrate touchscreen
    tft.fillRoundRect(0, 220, TFT_WIDTH, 36, 6, TFT_DARKGREEN);
    tft.drawString("Re-calibrate Touchscreen", 10, 228);

    // Rotate touchscreen up and down
    tft.fillRoundRect(0, 260, TFT_WIDTH, 36, 6, TFT_DARKGREEN);
    tft.drawString("Flip Screen & Re-calibrate", 10, 268);

    // Save, About, and Cancel buttons
    tft.setTextDatum(TC_DATUM);
    tft.setFreeFont(FSS12);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);

    tft.fillRoundRect(LEFTBUTTON_X, LEFTBUTTON_Y, LEFTBUTTON_W, LEFTBUTTON_H, 6, TFT_DARKGREY);
    tft.drawString("Save", (LEFTBUTTON_X + 60), (RIGHTBUTTON_Y + 15));

    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("?", (CENTERBUTTON_X + 22), (CENTERBUTTON_Y + 15)); // About

    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.fillRoundRect(RIGHTBUTTON_X, RIGHTBUTTON_Y, RIGHTBUTTON_W, RIGHTBUTTON_H, 6, TFT_DARKGREY);
    tft.drawString("Cancel", (RIGHTBUTTON_X + 60), (RIGHTBUTTON_Y + 15));

    // Reset to default
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setFreeFont(FSS12);
}


void drawAbout()
{
    activeScreen = "about";

    // Pull latest version number from a remote source (AmpliPi)
    HTTPClient http;

    String url = "http://" + amplipiHostIP + controllerVersionURI;
    String payload;
    String latestVersion = "";

#if DEBUGAPIREQ
    Serial.print("[HTTP] begin...\n");
    Serial.print("[HTTP] GET...\n");
#endif
    http.setConnectTimeout(5000);
    http.setTimeout(5000);
    http.begin(url); //HTTP

    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0)
    {
#if DEBUGAPIREQ
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
#endif
        // file found at server
        if (httpCode == HTTP_CODE_OK)
        {
            latestVersion = http.getString();
            latestVersion.trim();
#if DEBUGAPIREQ
            Serial.println(payload);
#endif
        }
    }
    else
    {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        //drawWarning("Unable to access AmpliPi");
        latestVersion = "Unavailable";
    }
    http.end();

    // Stop metadata refresh
    metadata_refresh = false;

    // Clear screen
    clearMainArea();

    // Show information:
    tft.setFreeFont(FSS9);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("IP: " + String(ETH.localIP()[0])+"."+String(ETH.localIP()[1])+"."+String(ETH.localIP()[2])+"."+String(ETH.localIP()[3]), 5, 50);
    tft.drawString("Name: " + String(hostname), 5, 73);

    String DX = " - ";
    if (ETH.fullDuplex()) {
        DX += "Full Duplex";
    }
    else {
        DX += "Half Duplex";
    }

    tft.drawString("ETH: " + String(ETH.linkSpeed()) + "Mbps" + DX, 5, 96);

    tft.fillRect(20, 120, (TFT_WIDTH - 40), 1, GREY); // Seperator
    
    tft.drawString("Controller Version:", 5, 125);
    tft.drawString(String(VERSION), 5, 145);
    tft.drawString("Latest Version:", 5, 170);
    tft.drawString(latestVersion, 5, 190);

    // AmpliPi logo
    tft.setCursor((TFT_WIDTH / 3) - 15, 265, 2); // center
    tft.setFreeFont(FSS18);
    tft.print("Ampli");
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("Pi");
    
    // Close / Update buttons
    tft.setFreeFont(FSS12);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);

    tft.fillRoundRect(LEFTBUTTON_X, LEFTBUTTON_Y, LEFTBUTTON_W, LEFTBUTTON_H, 6, TFT_DARKGREY);
    tft.drawString("Close", (LEFTBUTTON_X + 60), (LEFTBUTTON_Y + 15));

    if (latestVersion != "Unavailable" && String(VERSION) != latestVersion) {
        // Show Update button
        tft.fillRoundRect(RIGHTBUTTON_X, RIGHTBUTTON_Y, RIGHTBUTTON_W, RIGHTBUTTON_H, 6, TFT_DARKGREY);
        tft.drawString("Update", (RIGHTBUTTON_X + 60), (RIGHTBUTTON_Y + 15));
        tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    }
    // Reset to default
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setFreeFont(FSS12);
}


// Handle update of this device from OTA path
// From: https://github.com/espressif/arduino-esp32/blob/master/libraries/Update/examples/AWS_S3_OTA_Update/AWS_S3_OTA_Update.ino

// Variables to validate
// response from S3
long contentLength = 0;
bool isValidContentType = false;

// Utility to extract header value from headers
String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}

void updateController()
{
    // Clear screen
    clearMainArea();

    tft.setFreeFont(FSS18);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Update starting.", 5, 55);
    tft.setFreeFont(FSS12);
    tft.drawString("This process can take a few", 5, 105);
    tft.drawString("minutes.", 5, 130);

    // Download and update this device
    Serial.println("Connecting to: " + String(amplipiHostIP));
    int port = 80;
#if DEBUG_WEBSERVER
        port = 5000;
#endif
    
    // For showing errors on screen.
    //tft.setFreeFont(FSS9);
    //tft.setCursor(0, 170, 1);

    // Connect to HTTP
    WiFiClient client;
    if (client.connect(amplipiHostIP.c_str(), port)) {
        // Connection Succeed.
        // Fecthing the bin
        Serial.println("Fetching Bin: " + String(controllerBin));

        // Get the contents of the bin file
        client.print(String("GET ") + controllerBin + " HTTP/1.1\r\n" +
                    "Host: " + amplipiHostIP + "\r\n" +
                    "Cache-Control: no-cache\r\n" +
                    "Connection: close\r\n\r\n");

        // Check what is being sent
        //    Serial.print(String("GET ") + controllerBin + " HTTP/1.1\r\n" +
        //                 "Host: " + amplipiHostIP + "\r\n" +
        //                 "Cache-Control: no-cache\r\n" +
        //                 "Connection: close\r\n\r\n");

        unsigned long timeout = millis();
        while (client.available() == 0) {
            if (millis() - timeout > 5000) {
                drawWarning("Client timeout");
                Serial.println("Client Timeout !");
                client.stop();
                return;
            }
        }
        // Once the response is available,
        // check stuff

        /*
        Response Structure
            HTTP/1.1 200 OK
            x-amz-id-2: NVKxnU1aIQMmpGKhSwpCBh8y2JPbak18QLIfE+OiUDOos+7UftZKjtCFqrwsGOZRN5Zee0jpTd0=
            x-amz-request-id: 2D56B47560B764EC
            Date: Wed, 14 Jun 2017 03:33:59 GMT
            Last-Modified: Fri, 02 Jun 2017 14:50:11 GMT
            ETag: "d2afebbaaebc38cd669ce36727152af9"
            Accept-Ranges: bytes
            Content-Type: application/octet-stream
            Content-Length: 357280
            Server: AmazonS3
                                    
            {{BIN FILE CONTENTS}}
        */
        while (client.available()) {
            // read line till /n
            String line = client.readStringUntil('\n');
            // remove space, to check if the line is end of headers
            line.trim();
//tft.println(String(line));
            // if the the line is empty,
            // this is end of headers
            // break the while and feed the
            // remaining `client` to the
            // Update.writeStream();
            if (!line.length()) {
                //headers ended
                break; // and get the OTA started
            }

            // Check if the HTTP Response is 200
            // else break and Exit Update
            if (line.startsWith("HTTP/1.1")) {
                if (line.indexOf("200") < 0) {
                    drawWarning("Update file unavailable");
                    Serial.println("Got a non 200 status code from server. Exiting OTA Update.");
                    break;
                }
            }

            // extract headers here
            // Start with content length.  Lower case header required because of AmpliPi's HTTP server
            if (line.startsWith("content-length: ")) {
                contentLength = atol((getHeaderValue(line, "content-length: ")).c_str());
                Serial.println("Got " + String(contentLength) + " bytes from server");
            }

            // Next, the content type. Lower case header required because of AmpliPi's HTTP server
            if (line.startsWith("content-type: ")) {
                String contentType = getHeaderValue(line, "Content-Type: ");
                Serial.println("Got " + contentType + " payload.");
                if (contentType == "application/octet-stream") {
                    isValidContentType = true;
                }
            }
        }
    } else {
        // Connect to HTTP failed
        // May be try?
        // Probably a choppy network?
        drawWarning("Unable to connect to AmpliPi");
        Serial.println("Connection to " + String(amplipiHostIP) + " failed. Please check your setup");
        // retry??
        // execOTA();
    }

    // Check what is the contentLength and if content type is `application/octet-stream`
    Serial.println("contentLength : " + String(contentLength) + ", isValidContentType : " + String(isValidContentType));

    // check contentLength and content type
    if (contentLength && isValidContentType) {
        // Check if there is enough to OTA Update
        bool canBegin = Update.begin(contentLength);

        // If yes, begin
        if (canBegin) {
            Serial.println("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
            // No activity would appear on the Serial monitor
            // So be patient. This may take 2 - 5mins to complete
            size_t written = Update.writeStream(client);

            if (written == contentLength) {
                Serial.println("Written : " + String(written) + " successfully");
            } else {
                Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?" );
                // retry??
                // execOTA();
            }

            if (Update.end()) {
                Serial.println("OTA done!");
                if (Update.isFinished()) {
                    Serial.println("Update successfully completed. Rebooting.");
                    ESP.restart();
                } else {
                    drawWarning("Error updating");
                    Serial.println("Update not finished? Something went wrong!");
                }
            } else {
                drawWarning("Error occurred while updating");
                Serial.println("Error Occurred. Error #: " + String(Update.getError()));
            }
        } else {
            // not enough space to begin OTA
            // Understand the partitions and
            // space availability
            drawWarning("Not enough space for update");
            Serial.println("Not enough space to begin OTA");
            client.flush();
        }
    } else {
        drawWarning("Update file was not valid");
        Serial.println("There was no content in the response");
        client.flush();
    }


    // Close button
    tft.setFreeFont(FSS12);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);

    tft.fillRoundRect(LEFTBUTTON_X, LEFTBUTTON_Y, LEFTBUTTON_W, LEFTBUTTON_H, 6, TFT_DARKGREY);
    tft.drawString("Close", (MAINZONE_X + 60), (LEFTBUTTON_Y + 15));
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
}


void drawCommandButtons()
{
    // Currently, only Pandora streams support these buttons
    if (currentStreamType == "pandora")
    {
        if (cmdLike) {
            drawBmp("/heart_on.bmp", LIKEBUTTON_X, LIKEBUTTON_Y);
        }
        else {
            drawBmp("/heart.bmp", LIKEBUTTON_X, LIKEBUTTON_Y);
        }
        if (cmdDislike) {
            drawBmp("/thumbdown_on.bmp", DISLIKEBUTTON_X, DISLIKEBUTTON_Y);
        }
        else {
            drawBmp("/thumbdown.bmp", DISLIKEBUTTON_X, DISLIKEBUTTON_Y);
        }

        if (cmdPlaying) {
            drawBmp("/pause.bmp", PLAYPAUSEBUTTON_X, PLAYPAUSEBUTTON_Y);
        }
        else {
            drawBmp("/play.bmp", PLAYPAUSEBUTTON_X, PLAYPAUSEBUTTON_Y);
        }

        drawBmp("/skip.bmp", SKIPBUTTON_X, SKIPBUTTON_Y);
    }
    else {
        // Clear buttons
        tft.fillRect(LIKEBUTTON_X, LIKEBUTTON_Y, 36, 36, TFT_BLACK);
        tft.fillRect(DISLIKEBUTTON_X, DISLIKEBUTTON_Y, 36, 36, TFT_BLACK);
        tft.fillRect(PLAYPAUSEBUTTON_X, PLAYPAUSEBUTTON_Y, 36, 36, TFT_BLACK);
        tft.fillRect(SKIPBUTTON_X, SKIPBUTTON_Y, 36, 36, TFT_BLACK);
    }
}

void sendCommand(String command) {
    if (command == "playpause") {
        if (cmdPlaying) { command = "pause"; cmdPlaying = false; }
        else { command = "play"; cmdPlaying = true; }
    }
    else if (command == "love") {
        if (cmdLike) { cmdLike = false; }
        else { cmdLike = true; }
    }
    else if (command == "ban") {
        if (cmdDislike) { cmdDislike = false; }
        else { cmdDislike = true; }
    }

    String payload = "{}";
    bool result = postAPI("streams/" + String(currentStreamID) + "/" + command, payload);
    Serial.print("sendCommand result: ");
    Serial.println(result);

    drawCommandButtons();
}


void sendVolUpdate(int zone)
{
    // Send volume update to API, but only one update per second so we don't spam it
    int volDb = 0;
    if (zone == 1) { volDb = (int)(volPercent1 * 0.79 - 79); } // Convert to proper AmpliPi number (-79 to 0)
    else if (zone == 2) { volDb = (int)(volPercent2 * 0.79 - 79); } // Convert to proper AmpliPi number (-79 to 0)

    String payload = "{\"vol\": " + String(volDb) + "}";
    Serial.println(payload);
    bool result = patchAPI("zones/" + String(amplipiZone1), payload);
    Serial.print("sendVolUpdate result: ");
    Serial.println(result);
}


void drawVolume(int x, int zone)
{
    if (!updateVol1 && !updateVol2)
    {
        return;
    }; // Only update volume on screen if we need to

    Serial.print("Drawing volume bar");

    // Bring to 100% if it's close to the screen edge
    if (x > (TFT_WIDTH - 55)) { x = TFT_WIDTH - 40; }

    if (amplipiZone2Enabled) {
        // Two Zone Mode
        if (zone == 1) {
            // Zone 1
            tft.fillRect(VOLBARZONE_X, VOLBARZONE1_Y, VOLBARZONE_W, VOLBARZONE_H, TFT_BLACK); // Clear area first

            // Volume control bar
            tft.fillRect(VOLBAR_X, VOLBAR1_Y, VOLBAR_W, VOLBAR_H, GREY);           // Grey bar
            if (muteZone1) {
                // Muted
                tft.fillCircle(x, (VOLBAR1_Y + 2), 8, GREY);                       // Circle marker
            }
            else {
                // Unmuted
                tft.fillRect(VOLBAR_X, VOLBAR1_Y, (x - VOLBAR_X), VOLBAR_H, BLUE); // Blue active bar
                tft.fillCircle(x, (VOLBAR1_Y + 2), 8, BLUE);                       // Circle marker
            }
            updateVol1 = false;
        }
        else if (zone == 2) {
            // Zone 2
            tft.fillRect(VOLBARZONE_X, VOLBARZONE2_Y, VOLBARZONE_W, VOLBARZONE_H, TFT_BLACK); // Clear area first

            // Volume control bar
            tft.fillRect(VOLBAR_X, VOLBAR2_Y, VOLBAR_W, VOLBAR_H, GREY);           // Grey bar
            if (muteZone2) {
                // Muted
                tft.fillCircle(x, (VOLBAR2_Y + 2), 8, GREY);                       // Circle marker
            }
            else {
                // Unmuted
                tft.fillRect(VOLBAR_X, VOLBAR2_Y, (x - VOLBAR_X), VOLBAR_H, BLUE); // Blue active bar
                tft.fillCircle(x, (VOLBAR2_Y + 2), 8, BLUE);                       // Circle marker
            }
            updateVol2 = false;
        }
    }
    else {
        // One Zone Mode
        tft.fillRect(VOLBARZONE_X, VOLBARZONE2_Y, VOLBARZONE_W, VOLBARZONE_H, TFT_BLACK);   // Clear area first

        // Volume control bar
        tft.fillRect(VOLBAR_X, VOLBAR2_Y, VOLBAR_W, VOLBAR_H, GREY);               // Grey bar
        if (muteZone1) {
            // Muted
            tft.fillCircle(x, (VOLBAR2_Y + 2), 8, GREY);
        }
        else {
            tft.fillRect(VOLBAR_X, VOLBAR2_Y, (x - VOLBAR_X), VOLBAR_H, BLUE);    // Blue active bar
            tft.fillCircle(x, (VOLBAR2_Y + 2), 8, BLUE);
        }
        updateVol1 = false;
        updateVol2 = false;
    }

}


void sendMuteUpdate(int zone)
{
    // Send volume update to API, but only one update per second so we don't spam it
    String payload;

    if (zone == 1)
    {
        if (muteZone1) {
            payload = "{\"mute\": true}";
        }
        else {
            payload = "{\"mute\": false}";
        }
        bool result = patchAPI("zones/" + String(amplipiZone1), payload);
        if (DEBUGAPIREQ)
        {
            Serial.print("sendMuteUpdate result: ");
            Serial.println(result);
        }
    }
    else if (zone == 2)
    {
        if (muteZone2) {
            payload = "{\"mute\": true}";
        }
        else {
            payload = "{\"mute\": false}";
        }
        bool result = patchAPI("zones/" + String(amplipiZone2), payload);
        if (DEBUGAPIREQ)
        {
            Serial.print("sendMuteUpdate result: ");
            Serial.println(result);
        }
    }
}

void drawMuteBtn(int zone)
{
    if (amplipiZone2Enabled && !updateMute1 && !updateMute2)
    {
        // Only update mute button on screen if needed (Dual zone)
        return;
    };

    if (!amplipiZone2Enabled && !updateMute1)
    {
        // Only update mute button on screen if needed (single zone)
        return;
    };

    Serial.print("Drawing mute button for zone ");
    Serial.println(zone);
    Serial.print("amplipiZone2Enabled setting: ");
    Serial.println(amplipiZone2Enabled);

    if (amplipiZone2Enabled) {
        // Two Zone Mode
        if (zone == 1) {
            tft.fillRect(MUTE_X, MUTE1_Y, MUTE_W, MUTE_H, TFT_BLACK); // Upper section
        }
        else if (zone == 2) {
            tft.fillRect(MUTE_X, MUTE2_Y, MUTE_W, MUTE_H, TFT_BLACK); // Lower section
        }
    }
    else {
        // One Zone Mode, lower section
        tft.fillRect(MUTE_X, MUTE2_Y, MUTE_W, MUTE_H, TFT_BLACK);
    }

    // Mute/unmute button
    if (amplipiZone2Enabled) {
        // Two Zone Mode
        if (zone == 1) {
            // Upper section
            if (muteZone1) { drawBmp("/volume_off.bmp", MUTE_X, MUTE1_Y); }
            else { drawBmp("/volume_up.bmp", MUTE_X, MUTE1_Y); }
            updateMute1 = false;
        }
        else if (zone == 2) {
            // Lower section
            if (muteZone2) { drawBmp("/volume_off.bmp", MUTE_X, MUTE2_Y); }
            else { drawBmp("/volume_up.bmp", MUTE_X, MUTE2_Y); }
            updateMute2 = false;
        }
    }
    else {
        // One Zone Mode, lower section
        if (muteZone1) { drawBmp("/volume_off.bmp", MUTE_X, MUTE2_Y); }
        else { drawBmp("/volume_up.bmp", MUTE_X, MUTE2_Y); }
        updateMute1 = false;
        Serial.println("Disabling updateMute1.");
    }

}


// Split a string by a separator
String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = {0, -1};
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++)
    {
        if (data.charAt(i) == separator || i == maxIndex)
        {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i + 1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}


void getZone()
{
    // Multiply the new volume percent by the screen width minus 80 and add 45 pixels (offset for the mute button) to get the x coord
    float volBarWidth = (TFT_WIDTH - 80) / 100; // 1.6 for 240px screen, 2.4 for 320px screen.

    if (amplipiZone2Enabled) {
        // Two Zone Mode

        // Zone 1
        String json = requestAPI("zones/" + String(amplipiZone1));
        DynamicJsonDocument ampSourceStatus(1000); // DynamicJsonDocument<N> allocates memory on the heap
        DeserializationError error = deserializeJson(ampSourceStatus, json); // Deserialize the JSON document

        // Test if parsing succeeds.
        if (error)
        {
            Serial.print(F("getZone() (2 zones, zone 1) deserializeJson() failed: "));
            Serial.println(error.f_str());
        }

        // Update mute if data from API has changed
        bool currentMute1 = ampSourceStatus["mute"];
        if (currentMute1 != muteZone1) {
            muteZone1 = currentMute1;
            updateMute1 = true;
            updateVol1 = true;
        }
        drawMuteBtn(1);

        // Update volume bar if data from API has changed
        int currentVol = ampSourceStatus["vol"];
        float newVolPercent1;
        if (currentVol < 0) {
            newVolPercent1 = currentVol / 0.79 + 100; // Convert from AmpliPi number (-79 to 0) to percent
        }
        else {
            newVolPercent1 = 100;
        }

        if (volPercent1 != newVolPercent1) {
            volPercent1 = newVolPercent1;
            updateVol1 = true;
        }
        Serial.println("Calling drawVolume (1 of 2)");
        drawVolume(int((volPercent1 * volBarWidth) + 45), 1);

        // Zone 2
        String json2 = requestAPI("zones/" + String(amplipiZone2));
        DynamicJsonDocument ampSourceStatus2(1000); // DynamicJsonDocument<N> allocates memory on the heap
        DeserializationError error2 = deserializeJson(ampSourceStatus2, json2); // Deserialize the JSON document

        // Test if parsing succeeds.
        if (error2)
        {
            Serial.print(F("getZone() (2 zones, zone 2) deserializeJson() failed: "));
            Serial.println(error2.f_str());
        }

        // Update mute if data from API has changed
        bool currentMute2 = ampSourceStatus2["mute"];
        if (currentMute2 != muteZone2) {
            muteZone2 = currentMute2;
            updateMute2 = true;
            updateVol2 = true;
        }
        drawMuteBtn(2);

        // Update volume bar if data from API has changed
        int currentVol2 = ampSourceStatus2["vol"];
        float newVolPercent2;
        if (currentVol2 < 0) {
            newVolPercent2 = currentVol2 / 0.79 + 100; // Convert from AmpliPi number (-79 to 0) to percent
        }
        else {
            newVolPercent2 = 100;
        }
        
        if (volPercent2 != newVolPercent2) {
            volPercent2 = newVolPercent2;
            updateVol2 = true;
        }
        Serial.println("Calling drawVolume (2 of 2)");
        drawVolume(int((volPercent2 * volBarWidth) + 45), 2); // Multiply by 1.5 (150px) and add 40 pixels to give it the x coord
    }
    else {
        // One Zone Mode
        String json = requestAPI("zones/" + String(amplipiZone1));
        DynamicJsonDocument ampSourceStatus(1000); // DynamicJsonDocument<N> allocates memory on the heap
        DeserializationError error = deserializeJson(ampSourceStatus, json); // Deserialize the JSON document

        // Test if parsing succeeds.
        if (error)
        {
            Serial.print(F("getZone() (1 zone) deserializeJson() failed: "));
            Serial.println(error.f_str());
        }

        // Update mute if data from API has changed
        bool currentMute = ampSourceStatus["mute"];
        if (currentMute != muteZone1) {
            muteZone1 = currentMute;
            updateMute1 = true;
            updateVol1 = true;
            Serial.print("Updating mute and volume. currentMute:");
            Serial.println(currentMute);
            Serial.print("muteZone1:");
            Serial.println(muteZone1);
        }
        drawMuteBtn(1);

        // Update volume bar if data from API has changed
        int currentVol = ampSourceStatus["vol"];
        float newVolPercent1;
        if (currentVol < 0) {
            newVolPercent1 = currentVol / 0.79 + 100; // Convert from AmpliPi number (-79 to 0) to percent
        }
        else {
            newVolPercent1 = 100;
        }

        if (volPercent1 != newVolPercent1) {
            volPercent1 = newVolPercent1;
            updateVol1 = true;
            Serial.print("Updating volume. volPercent1:");
            Serial.println(volPercent1);
            Serial.print("newVolPercent1:");
            Serial.println(newVolPercent1);
        }
        Serial.println("Calling drawVolume (1 of 1)");
        drawVolume(int((volPercent1 * volBarWidth) + 45), 1); // Multiply by 1.5 (150px) and add 40 pixels to give it the x coord
    }

}


// Re-draw metadata, for example after source select is canceled
void drawMetadata()
{
    String displaySong;
    String displayArtist;

    if (currentSong.length() >= (TITLE_LEN + 1)) {
        displaySong = currentSong.substring(0,TITLE_LEN) + "...";
    }
    else {
        displaySong = currentSong;
    }

    if (currentArtist.length() >= (ARTIST_LEN + 1)) {
        displayArtist = currentArtist.substring(0,ARTIST_LEN) + "...";
    }
    else {
        displayArtist = currentArtist;
    }

    Serial.println("Refreshing metadata on screen");

    tft.setTextDatum(TC_DATUM);
    tft.setFreeFont(FSSB12); // Bold font
    tft.fillRect(METATEXT_X, METATEXT_Y, METATEXT_W, METATEXT_H, TFT_BLACK); // Clear metadata area first
    tft.drawString(displaySong, (TFT_WIDTH / 2), (METATEXT_Y + 5), GFXFF);   // Center Middle
    tft.setFreeFont(FSS12);
    tft.fillRect(20, (METATEXT_Y + 32), (METATEXT_W - 40), 1, GREY);         // Seperator between song and artist
    tft.drawString(displayArtist, (TFT_WIDTH / 2), (METATEXT_Y + 40), GFXFF);

    // Draw control buttons for streams that support it
    cmdLike = false;
    cmdDislike = false;
    drawCommandButtons();
}

void getSource(String sourceID)
{
    String streamArtist = "";
    String streamAlbum = "";
    String streamSong = "";
    String streamStatus = "";
    String albumArt = "";
    String streamID = "";
    String streamName = "";

    String json = requestAPI("sources/" + String(sourceID));

    // DynamicJsonDocument<N> allocates memory on the heap
    DynamicJsonDocument ampSourceStatus(2000);

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(ampSourceStatus, json);

    // Test if parsing succeeds.
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());

        //String errormsg = "Error parsing results from Amplipi API";
    }

    sourceInput = ampSourceStatus["input"].as<String>();
    Serial.print("sourceInput: ");
    Serial.println(sourceInput);

    if (sourceInput == "local")
    {
        streamID = "0";
    }
    else if (sourceInput == "None")
    {
        streamID = "-1";
    }
    else
    {
        streamID = getValue(sourceInput, '=', 1);
    }

    Serial.print("streamID: ");
    Serial.println(streamID);

    // Update stream name if it has changed
    if (currentStreamID != streamID && streamID != "0" && streamID != "-1")
    {
        currentStreamID = streamID;

        String streamJson = requestAPI("streams/" + String(streamID));

        // DynamicJsonDocument<N> allocates memory on the heap
        DynamicJsonDocument ampStreamStatus(2000);

        // Deserialize the JSON document
        DeserializationError streamError = deserializeJson(ampStreamStatus, streamJson);

        // Test if parsing succeeds.
        if (streamError)
        {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(streamError.f_str());

            //String errormsg = "Error parsing results from Amplipi API";
        }

        currentStreamName = ampStreamStatus["name"].as<String>();
        if (currentStreamName.length() >= (SRC_NAME_LEN + 1)) {
            currentStreamName = currentStreamName.substring(0,SRC_NAME_LEN) + "...";
        }

        currentStreamType = ampStreamStatus["type"].as<String>();

        updateSource = true;
        drawSource();
    }
    else if (currentStreamID != streamID && streamID == "0") {
        // Local - RCA Stream
        currentStreamID = streamID;
        currentStreamName = "Local - RCA";
        currentStreamType = "local";

        updateSource = true;
        drawSource();
    }
    else if (currentStreamID != streamID && streamID == "-1") {
        // No Stream, Disabled
        currentStreamID = streamID;
        currentStreamName = "Source Off";
        currentStreamType = "none";

        updateSource = true;
        drawSource();
    }

    // Get stream metadata
    streamArtist = ampSourceStatus["info"]["artist"].as<String>();
    if (streamArtist == "null")
    {
        streamArtist = "";
    }
    streamAlbum = ampSourceStatus["info"]["album"].as<String>();
    if (streamAlbum == "null")
    {
        streamAlbum = "";
    }
    streamSong = ampSourceStatus["info"]["track"].as<String>();
    if (streamSong == "null")
    {
        streamSong = "";
    }
    streamStatus = ampSourceStatus["status"].as<String>();
    if (streamStatus == "null")
    {
        streamStatus = "";
    }
    albumArt = ampSourceStatus["info"]["img_url"].as<String>();
    if (albumArt == "null")
    {
        albumArt = "";
    }

    // Only refresh screen if we have new data
    if (currentArtist != streamArtist || currentSong != streamSong || currentStatus != streamStatus)
    {
        Serial.println("Printing artist and song on screen.");
        currentArtist = streamArtist;
        currentSong = streamSong;
        currentStatus = streamStatus;
        drawMetadata();
    }

    // Download and refresh album art if it has changed
    if (albumArt != currentAlbumArt)
    {
        Serial.println("Album art changed from " + currentAlbumArt + " to " + albumArt);
        currentAlbumArt = albumArt;
        updateAlbumart = true;
        downloadAlbumart(sourceID);
        drawAlbumart();
    }

}

void setup() {
    Serial.begin(115200);
    Serial.println("AmpliPi System Startup");

    // Load configuration file
    loadFileFSConfigFile();

    newAmplipiZone1 = atoi(amplipiZone1);
    newAmplipiZone2 = atoi(amplipiZone2);
    newAmplipiSource = atoi(amplipiSource);

    // If amplipiZone2 is 0 or great, Zone 2 should be enabled
    if (atoi(amplipiZone2) >= 0) { amplipiZone2Enabled = true; }
    else { amplipiZone2Enabled = false; }


    // Initialize screen
    tft.init();
    Serial.println("Screen initialized");

    // Set the rotation before we calibrate
    tft.setRotation(screenRotation);

    // Wrap test at right and bottom of screen
    tft.setTextWrap(true, true);

    tft.setFreeFont(FSS18);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.setSwapBytes(true); // We need to swap the colour bytes (endianess), otherwise JPG images won't look correct

    // The jpeg image can be scaled by a factor of 1, 2, 4, or 8
    TJpgDec.setJpgScale(1);

    // The decoder must be given the exact name of the rendering function above
    TJpgDec.setCallback(tft_output);

    // Call screen calibration
    //  This also handles formatting the filesystem if it hasn't been formatted yet
    touch_calibrate();

    // clear screen
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.setCursor((TFT_WIDTH / 3) - 15, 40, 2); // center
    tft.setFreeFont(FSS18);

    tft.print("Ampli");
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("Pi");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setFreeFont(FSS12);
    tft.setCursor((TFT_WIDTH / 3) - 5, 100, 2); // center
    tft.println("Welcome");
    tft.println("");

    WiFi.onEvent(WiFiEvent);
    ETH.begin();

    Serial.print("Connecting to network");
    tft.setFreeFont(FSS9);
    tft.drawString("Connecting to network", (TFT_WIDTH / 2), (TFT_HEIGHT - 20), GFXFF); // Center Middle

    //while (WiFi.status() != WL_CONNECTED)
    while (!eth_connected)
    {
        Serial.print(".");
        delay(500);
    }
    Serial.println("");
    Serial.print("Connected to network. Local IP: ");
    Serial.println(ETH.localIP());
    
    tft.fillRect(0, (TFT_HEIGHT - 40), TFT_WIDTH, 40, TFT_BLACK); // Clear area first
    tft.drawString("Connected. IP address: ", (TFT_WIDTH / 2), (TFT_HEIGHT - 40), GFXFF); // Center Middle
    tft.drawString(String(ETH.localIP()[0])+"."+String(ETH.localIP()[1])+"."+String(ETH.localIP()[2])+"."+String(ETH.localIP()[3]), (TFT_WIDTH / 2), (TFT_HEIGHT - 20), GFXFF); // Center Middle
    delay(500);

    getAmpliPiIP();

    // Clear screen
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 20, 2);
}

/**
 * This is where the touch events and automatic metadata refresh happen.
 * We first check to see what screen we're currently on, then we look for the touch events (buttons, etc) that are available for that screen.
 */
void loop() {
    uint16_t x, y;

    // See if there's any touch data for us
    if (tft.getTouch(&x, &y))
    {
        // Draw a block spot to show where touch was calculated to be
        //tft.fillCircle(x, y, 2, TFT_BLUE);
        Serial.print("X: ");
        Serial.print(x);
        Serial.print(" - Y: ");
        Serial.println(y);

        // Touch screen control

        // Turn screen back on if off
        if (activeScreen == "off")
        {
            Serial.println("Turning screen back on");

            // Reload main selection screen
            activeScreen = "select";
            metadata_refresh = true;
            updateSource = true;
            updateMute1 = true;
            updateMute2 = true;
            updateAlbumart = true;
            updateVol1 = true;
            updateVol2 = true;
            currentSourceOffset = 0;

            drawSource();
            if (amplipiZone2Enabled) {
                drawMuteBtn(1);
                drawMuteBtn(2);
            }
            else {
                drawMuteBtn(1);
            }
            //drawSource();
            drawMetadata();
            drawAlbumart();

            powerOnScreen();

            delay(200); // debounce
        }
        else if (activeScreen == "select")
        {
            // Main Selection screen
            Serial.println("Current screen: select");

            // Source Select (location: top right)
            if ((x >= SRCBUTTON_X) && (x <= (SRCBUTTON_X + SRCBUTTON_W)) && (y >= SRCBUTTON_Y) && (y <= (SRCBUTTON_Y + SRCBUTTON_H)))
            {
                activeScreen = "source";
                drawSourceSelection();
                Serial.print("Source select button hit.");
                delay(200); // Debounce
            }
            
            // Power screen off (location: top left)
            if ((x >= SRCBAR_X) && (x <= (SRCBAR_X + 40)) && (y >= SRCBAR_Y) && (y <= (SRCBAR_Y + 40)))
            {
                activeScreen = "off";
                powerOffScreen();
                Serial.print("Power off screen button hit.");
                delay(200); // Debounce
            }

            if ((x >= ALBUMART_X) && (x <= (ALBUMART_X + ALBUMART_W)) && (y >= ALBUMART_Y) && (y <= (ALBUMART_Y + ALBUMART_H)))
            {
                activeScreen = "metadata";
                Serial.print("Switching to full screen metadata mode");

                clearMainArea();
                drawMetadata();
                drawAlbumart();
                delay(200); // Debounce
            }

            // Control buttons for supported streams
            // Currently, only Pandora streams support these buttons
            if (currentStreamType == "pandora")
            {
                // Play/pause
                if ((x >= PLAYPAUSEBUTTON_X) && (x <= (PLAYPAUSEBUTTON_X + PLAYPAUSEBUTTON_W)) && (y >= PLAYPAUSEBUTTON_Y) && (y <= (PLAYPAUSEBUTTON_Y + PLAYPAUSEBUTTON_H))) {
                    sendCommand("playpause");
                }

                // Skip
                if ((x >= SKIPBUTTON_X) && (x <= (SKIPBUTTON_X + SKIPBUTTON_W)) && (y >= SKIPBUTTON_Y) && (y <= (SKIPBUTTON_Y + SKIPBUTTON_H))) {
                    sendCommand("next");
                }

                // Like
                if ((x >= LIKEBUTTON_X) && (x <= (LIKEBUTTON_X + LIKEBUTTON_W)) && (y >= LIKEBUTTON_Y) && (y <= (LIKEBUTTON_Y + LIKEBUTTON_H))) {
                    sendCommand("love");
                }

                // Disike
                if ((x >= DISLIKEBUTTON_X) && (x <= (DISLIKEBUTTON_X + DISLIKEBUTTON_W)) && (y >= DISLIKEBUTTON_Y) && (y <= (DISLIKEBUTTON_Y + DISLIKEBUTTON_H))) {
                    sendCommand("ban");
                }
                delay(200); // Debounce
            }

            // Mute button
            if ((x > MUTE_X) && (x < (MUTE_X + MUTE_W)))
            {
                if (amplipiZone2Enabled && (y > MUTE1_Y) && (y <= (MUTE1_Y + MUTE_H)))
                {
                    // Two Zone Mode, upper section
                    if (muteZone1) { muteZone1 = false; }
                    else { muteZone1 = true; }
                    updateMute1 = true;
                    updateVol1 = true;
                    drawMuteBtn(1);
                    sendMuteUpdate(1);
                    Serial.print("Mute button hit.");
                }
                else if ((y > MUTE2_Y) && (y <= (MUTE2_Y + MUTE_H)))
                {
                    if (amplipiZone2Enabled) {
                        // Two Zone Mode, lower section
                        if (muteZone2) { muteZone2 = false; }
                        else { muteZone2 = true; }
                        updateMute2 = true;
                        updateVol2 = true;
                        drawMuteBtn(2);
                        sendMuteUpdate(2);
                    }
                    else {
                        // One Zone Mode
                        if (muteZone1) { muteZone1 = false; }
                        else { muteZone1 = true; }
                        updateMute1 = true;
                        updateVol1 = true;
                        drawMuteBtn(1);
                        sendMuteUpdate(1);
                    }
                    Serial.print("Mute button hit.");
                }
                delay(200); // Debounce
            }

            // Volume control
            if ((x > VOLBARZONE_X) && (x < (VOLBARZONE_X + VOLBARZONE_W)))
            {
                // Multiply the new volume percent by the screen width minus 80 and add 45 pixels (offset for the mute button) to get the x coord
                float volBarWidth = (TFT_WIDTH - 80) / 100; // 1.6 for 240px screen, 2.4 for 320px screen.
                if (amplipiZone2Enabled && (y > VOLBARZONE1_Y) && (y <= (VOLBARZONE1_Y + VOLBARZONE_H)))
                {
                    // Two Zone Mode, upper section
                    updateVol1 = true;
                    volPercent1 = (x - 40) / volBarWidth;
                    drawVolume(x, 1);
                    sendVolUpdate(1);
                    Serial.print("Volume control hit.");
                }
                else if ((y > VOLBARZONE2_Y) && (y <= (VOLBARZONE2_Y + VOLBARZONE_H)))
                {
                    if (amplipiZone2Enabled) {
                        // Two Zone Mode, lower section
                        updateVol2 = true;
                        volPercent2 = (x - 40) / volBarWidth;
                        drawVolume(x, 2);
                        sendVolUpdate(2);
                    }
                    else {
                        // One Zone Mode
                        updateVol1 = true;
                        volPercent1 = (x - 40) / volBarWidth;
                        drawVolume(x, 1);
                        sendVolUpdate(1);
                    }
                    Serial.print("Volume control hit.");
                }
                delay(100); // Debounce
            }
        }
        else if (activeScreen == "metadata")
        {
            // Main Selection screen
            Serial.println("Current screen: metadata");

            // Reload main selection screen
            activeScreen = "select";
            metadata_refresh = true;
            updateSource = true;
            updateMute1 = true;
            updateMute2 = true;
            updateAlbumart = true;
            updateVol1 = true;
            updateVol2 = true;
            currentSourceOffset = 0;

            drawSource();
            if (amplipiZone2Enabled) {
                drawMuteBtn(1);
                drawMuteBtn(2);
            }
            else {
                drawMuteBtn(1);
            }
            //drawSource();
            drawMetadata();
            drawAlbumart();

        }
        else if (activeScreen == "source")
        {
            // Source Selection screen
            Serial.println("Current screen: source");

            // Power screen off (location: top left)
            if ((x >= SRCBAR_X) && (x <= (SRCBAR_X + 36)) && (y >= SRCBAR_Y) && (y <= (SRCBAR_Y + 36)))
            {
                activeScreen = "off";
                powerOffScreen();
                Serial.print("Power off screen button hit.");
            }
            // Source Select button (cancel source select)
            else if ((x > SRCBUTTON_X) && (x < (SRCBUTTON_X + SRCBUTTON_W)))
            {
                if ((y > SRCBUTTON_Y) && (y <= (SRCBUTTON_Y + SRCBUTTON_H)))
                {
                    // Reload main metadata screen
                    activeScreen = "metadata";
                    metadata_refresh = true;
                    updateMute1 = true;
                    updateMute2 = true;
                    updateAlbumart = true;
                    updateVol1 = true;
                    updateVol2 = true;
                    currentSourceOffset = 0;

                    clearMainArea();
                    if (amplipiZone2Enabled) {
                        drawMuteBtn(1);
                        drawMuteBtn(2);
                    }
                    else {
                        drawMuteBtn(1);
                    }
                    drawMetadata();
                    drawAlbumart();
                }
            }
            // Select source (anything between source bar and Prev/Next buttons)
            else if ((y > 36) && (y <= RIGHTBUTTON_Y))
            {
                selectSource(y);

                // Reload main metadata screen
                activeScreen = "metadata";
                metadata_refresh = true;
                updateMute1 = true;
                updateMute2 = true;
                updateAlbumart = true;
                updateVol1 = true;
                updateVol2 = true;
                currentSourceOffset = 0;

                clearMainArea();
                if (amplipiZone2Enabled) {
                    drawMuteBtn(1);
                    drawMuteBtn(2);
                }
                else {
                    drawMuteBtn(1);
                }
                drawMetadata();
                drawAlbumart();
            }
            // Previous list of sources
            else if ((x > LEFTBUTTON_X) && (x < (LEFTBUTTON_X + LEFTBUTTON_W)))
            {
                if ((y > LEFTBUTTON_Y) && (y <= (LEFTBUTTON_Y + LEFTBUTTON_H)))
                {
                    // Show previous set of streams
                    currentSourceOffset = currentSourceOffset - MAX_STREAMS;
                    if (currentSourceOffset < 0) { currentSourceOffset = 0; }
                    drawSourceSelection();
                }
            }
            // Next list of sources
            else if ((x > RIGHTBUTTON_X) && (x < (RIGHTBUTTON_X + RIGHTBUTTON_W)))
            {
                if ((y > RIGHTBUTTON_Y) && (y <= (RIGHTBUTTON_Y + RIGHTBUTTON_H)))
                {
                    // Show next set of streams
                    currentSourceOffset = currentSourceOffset + MAX_STREAMS;
                    drawSourceSelection();
                }
            }
            // Settings screen
            else if ((x > CENTERBUTTON_X) && (x < (CENTERBUTTON_X + CENTERBUTTON_W)))
            {
                if ((y > CENTERBUTTON_Y) && (y <= (CENTERBUTTON_Y + CENTERBUTTON_H)))
                {
                    // Show settings screen
                    activeScreen = "setting";
                    drawSettings();
                }
            }

            delay(200); // Debounce
        }
        else if (activeScreen == "about")
        {
            // Close button
            if ((x > LEFTBUTTON_X) && (x < (LEFTBUTTON_X + LEFTBUTTON_W)) && (y > LEFTBUTTON_Y) && (y <= (LEFTBUTTON_Y + LEFTBUTTON_H)))
            {
                // Reload main metadata screen
                activeScreen = "metadata";
                metadata_refresh = true;
                updateMute1 = true;
                updateMute2 = true;
                updateAlbumart = true;
                updateVol1 = true;
                updateVol2 = true;
                currentSourceOffset = 0;

                clearMainArea();
                if (amplipiZone2Enabled) {
                    drawMuteBtn(1);
                    drawMuteBtn(2);
                }
                else {
                    drawMuteBtn(1);
                }
                drawMetadata();
                drawAlbumart();
            }
            
            // Update Controller button
            if ((x > RIGHTBUTTON_X) && (x < (RIGHTBUTTON_X + RIGHTBUTTON_W)) && (y > RIGHTBUTTON_Y) && (y <= (RIGHTBUTTON_Y + RIGHTBUTTON_H)))
            {
                updateController();
            }
            delay(200); // Debounce
        }
        else if (activeScreen == "setting")
        {
            // Zone 1 Change
            if ((y > 50) && (y <= 90))
            {
                if ((x > (TFT_WIDTH - 80)) && (x < (TFT_WIDTH - 40))) { --newAmplipiZone1; } // Decrement zone number
                else if ((x > (TFT_WIDTH - 40)) && (x < TFT_WIDTH)) { ++newAmplipiZone1; } // Increment zone number

                if (newAmplipiZone1 < 0) { newAmplipiZone1 = 0; }
                else if (newAmplipiZone1 > 5) { newAmplipiZone1 = 5; }

                tft.fillRect(0, 41, 160, 38, TFT_BLACK);
                tft.drawString("Zone 1: " + String(newAmplipiZone1), 5, 55);
            }
            // Zone 2 Change
            else if ((y > 90) && (y <= 130))
            {
                if ((x > (TFT_WIDTH - 80)) && (x < (TFT_WIDTH - 40))) { --newAmplipiZone2; } // Decrement zone number
                else if ((x > (TFT_WIDTH - 40)) && (x < TFT_WIDTH)) { ++newAmplipiZone2; } // Increment zone number

                if (newAmplipiZone2 < -1) { newAmplipiZone2 = -1; }
                else if (newAmplipiZone2 > 5) { newAmplipiZone2 = 5; }
                
                String thisZone;
                if (newAmplipiZone2 < 0) { thisZone = "None"; }
                else { thisZone = String(newAmplipiZone2); }

                tft.fillRect(0, 81, 160, 38, TFT_BLACK);
                tft.drawString("Zone 2: " + thisZone, 5, 95);
            }
            // Source Change
            else if ((y > 130) && (y <= 170))
            {
                if ((x > (TFT_WIDTH - 80)) && (x < (TFT_WIDTH - 40))) { --newAmplipiSource; } // Decrement source number
                else if ((x > (TFT_WIDTH - 40)) && (x < TFT_WIDTH)) { ++newAmplipiSource; } // Increment source number

                if (newAmplipiSource < 0) { newAmplipiSource = 0; }
                else if (newAmplipiSource > 3) { newAmplipiSource = 3; }

                tft.fillRect(0, 121, 160, 38, TFT_BLACK);
                tft.drawString("Source: " + String(newAmplipiSource), 5, 135);
            }

            // Restart
            if ((y >= 180) && (y < 220))
            {
                ESP.restart();
            }

            // Re-calibrate Touchscreen
            if ((y >= 220) && (y < 260))
            {
                // Delete TouchCalData file and reboot
                if (SPIFFS.exists(CALIBRATION_FILE))
                {
                    // Delete if we want to re-calibrate
                    SPIFFS.remove(CALIBRATION_FILE);
                }
                ESP.restart();
            }

            // Rotate Touchscreen
            if ((y >= 260) && (y < 292))
            {
                uint8_t currentRotation = tft.getRotation();
                Serial.print("Current rotation: ");
                Serial.println(currentRotation);
                if (currentRotation == 0)
                {
                    tft.setRotation(2);

                }
                else {
                    tft.setRotation(0);
                }

                saveFileFSConfigFile();

                // Delete TouchCalData file and reboot
                if (SPIFFS.exists(CALIBRATION_FILE))
                {
                    // Delete if we want to re-calibrate
                    SPIFFS.remove(CALIBRATION_FILE);
                }
                ESP.restart();
            }

            // Save Changes
            if ((x > LEFTBUTTON_X) && (x < (LEFTBUTTON_X + LEFTBUTTON_W)))
            {
                if ((y > LEFTBUTTON_Y) && (y <= (LEFTBUTTON_Y + LEFTBUTTON_H)))
                {
                    // Save Settings
                    sprintf(amplipiZone1, "%d", newAmplipiZone1);
                    sprintf(amplipiZone2, "%d", newAmplipiZone2);
                    sprintf(amplipiSource, "%d", newAmplipiSource);
                    saveFileFSConfigFile();
                    
                    // If the new amplipiZone2 setting is 0 or great, Zone 2 should be enabled
                    if (newAmplipiZone2 >= 0) { amplipiZone2Enabled = true; }
                    else { amplipiZone2Enabled = false; }

                    // Reload main metadata screen
                    activeScreen = "metadata";
                    metadata_refresh = true;
                    updateMute1 = true;
                    updateMute2 = true;
                    updateAlbumart = true;
                    updateVol1 = true;
                    updateVol2 = true;
                    currentSourceOffset = 0;

                    clearMainArea();
                    if (amplipiZone2Enabled) {
                        drawMuteBtn(1);
                        drawMuteBtn(2);
                    }
                    else {
                        drawMuteBtn(1);
                    }
                    drawMetadata();
                    drawAlbumart();
                }
            }

            // About Screen
            if ((x > CENTERBUTTON_X) && (x < (CENTERBUTTON_X + CENTERBUTTON_W)) && (y > RIGHTBUTTON_Y) && (y <= (RIGHTBUTTON_Y + RIGHTBUTTON_H)))
            {
                drawAbout();
            }
            
            // Cancel Changes
            if ((x > RIGHTBUTTON_X) && (x < (RIGHTBUTTON_X + RIGHTBUTTON_W)))
            {
                if ((y > RIGHTBUTTON_Y) && (y <= (RIGHTBUTTON_Y + RIGHTBUTTON_H)))
                {
                    // Reload main metadata screen
                    activeScreen = "metadata";
                    metadata_refresh = true;
                    updateMute1 = true;
                    updateMute2 = true;
                    updateAlbumart = true;
                    updateVol1 = true;
                    updateVol2 = true;
                    currentSourceOffset = 0;

                    clearMainArea();
                    if (amplipiZone2Enabled) {
                        drawMuteBtn(1);
                        drawMuteBtn(2);
                    }
                    else {
                        drawMuteBtn(1);
                    }
                    drawMetadata();
                    drawAlbumart();
                }
            }
            delay(200); // Debounce
        }
    }

    // Metadata refresh loop
    static unsigned long lastRefreshTime = 0;
    if (millis() - lastRefreshTime >= REFRESH_INTERVAL)
    {
        if (metadata_refresh) {
            Serial.println("Refreshing metadata");
            getZone();
            getSource(String(amplipiSource));
        }
        lastRefreshTime += REFRESH_INTERVAL;
    }
}
