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

static bool eth_connected = false;

/* Debug options */
#define DEBUGAPIREQ false


/**************************************/
/* Configure screen colors and layout */
/**************************************/
// How quickly does the metadata refresh (in milliseconds)
#define REFRESH_INTERVAL 2000

// Colors
#define GREY 0x5AEB
#define BLUE 0x9DFF

// Source bar location
#define SRCBAR_X 0
#define SRCBAR_Y 0
#define SRCBAR_W TFT_WIDTH
#define SRCBAR_H 36

#define SRCBUTTON_X (TFT_WIDTH - 40) // Top right side of screen
#define SRCBUTTON_Y 0
#define SRCBUTTON_W 40 // Oversize the button for easy selection
#define SRCBUTTON_H 40 // Oversize the button for easy selection

// Settings Screen Buttons
#define LEFTBUTTON_X 0
#define LEFTBUTTON_Y (TFT_HEIGHT - 38)
#define LEFTBUTTON_W 100
#define LEFTBUTTON_H 38

#define RIGHTBUTTON_X 140
#define RIGHTBUTTON_Y (TFT_HEIGHT - 38)
#define RIGHTBUTTON_W 100
#define RIGHTBUTTON_H 38

#define SETTINGBUTTON_X 102
#define SETTINGBUTTON_Y (TFT_HEIGHT - 38)
#define SETTINGBUTTON_W 36
#define SETTINGBUTTON_H 36

// Main area, normally where metadata is shown
#define MAINZONE_X 0
#define MAINZONE_Y 36
#define MAINZONE_W TFT_WIDTH
#define MAINZONE_H (TFT_HEIGHT - 36) // From below the source top bar to the bottom of the screen

// Album art location
#define ALBUMART_X 60
#define ALBUMART_Y 36
#define ALBUMART_W (TFT_WIDTH - 120)
#define ALBUMART_H ALBUMART_W

// Metadata text location
#define METATEXT_X 0
#define METATEXT_Y (ALBUMART_Y + ALBUMART_H + 10) // Start Metadata text 10px below the album art
#define METATEXT_W TFT_WIDTH
#define METATEXT_H 90

// Warning zone
#define WARNZONE_X 0
#define WARNZONE_Y (TFT_HEIGHT - 85) // Just above volume bar(s)
#define WARNZONE_W TFT_WIDTH
#define WARNZONE_H 15

// Mute button location
#define MUTE_X 0
#define MUTE1_Y (TFT_HEIGHT - 73) // (upper)
#define MUTE2_Y (TFT_HEIGHT - 36) // (lower)
#define MUTE_W 36
#define MUTE_H 36

// Volume control bar position and size
#define VOLBAR_X 45
#define VOLBAR1_Y (TFT_HEIGHT - 60) // (upper)
#define VOLBAR2_Y (TFT_HEIGHT - 23) // (lower)
#define VOLBAR_W (TFT_WIDTH - 90) // 150 for 240w screen, 230 for 320ww screen
#define VOLBAR_H 6

// Volume control bar zone size
#define VOLBARZONE_X 36
#define VOLBARZONE1_Y (TFT_HEIGHT - 73) // (upper)
#define VOLBARZONE2_Y (TFT_HEIGHT - 36) // (lower)
#define VOLBARZONE_W (TFT_WIDTH - 36)
#define VOLBARZONE_H 36

#if TFT_WIDTH <= 240
    // Maximum length of the source name
    #define SRC_NAME_LEN 18

    // Max length for title and artist metadata
    #define TITLE_LEN 18
    #define ARTIST_LEN 18
#else
    // Maximum length of the source name
    #define SRC_NAME_LEN 25

    // Max length for title and artist metadata
    #define TITLE_LEN 25
    #define ARTIST_LEN 25
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


/*************************************/
/* Configure globally used variables */
/*************************************/
String activeScreen = "metadata"; // Available screens: metadata, source, setting
String amplipiHostIP = "";
String sourceName = "";
String currentArtist = "";
String currentSong = "";
String currentStatus = "";
String currentAlbumArt = "";
String currentSourceName = "";
bool inWarning = false;
int newAmplipiSource = 0;
int newAmplipiZone1 = 0;
int newAmplipiZone2 = 0;
bool amplipiZone2Enabled = false;
int totalStreams = 0;
int currentSourceOffset = 0;
int sourceIDs[6];
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

    //uint64_t chipid = ESP.getEfuseMac();
    //char chipid_char[16];
    //sprintf(chipid_char, "%16X", chipid);

    //if (!MDNS.begin(chipid_char)) {
    if (!MDNS.begin("esp32-poe-amplipi")) {
        Serial.println("Error setting up MDNS responder!");
    } else {
        Serial.println("Finished initializing the MDNS client...");
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
            //Serial.print("amplipiHostIP (mDNS): ");
            //Serial.println(amplipiHostIP);
        }
    }
    else
    {
        Serial.println(F("failed to mount FS"));
        return false;
    }
    return true;
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


void WiFiEvent(WiFiEvent_t event)
{
    switch (event) {
        case SYSTEM_EVENT_ETH_START:
            Serial.println("ETH Started");
            //set eth hostname here
            ETH.setHostname("esp32-ethernet");
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
    tft.fillRect(WARNZONE_X, WARNZONE_Y, WARNZONE_W, WARNZONE_H, TFT_BLACK); // Clear warning area
    Serial.print("Warning: ");
    Serial.println(message);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setFreeFont(FSS9);
    tft.drawString(message, (WARNZONE_X + 5), WARNZONE_Y);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setFreeFont(FSS12);
    inWarning = true;
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


// Download album art or logo from AmpliPi API. Requires code update to AmpliPi API
bool downloadAlbumart(String sourceID)
{
    HTTPClient http;
    bool outcome = true;
    String url = "http://" + amplipiHostIP + "/api/sources/" + sourceID + "/image/" + String(ALBUMART_W);
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
    // Only update album art on screen if we need
    if (!updateAlbumart)
    {
        return;
    };
    tft.fillRect(ALBUMART_X, ALBUMART_Y, ALBUMART_W, ALBUMART_H, TFT_BLACK); // Clear album art first
    Serial.println("Drawing album art.");

    TJpgDec.drawFsJpg(ALBUMART_X, ALBUMART_Y, "/albumart.jpg");
    updateAlbumart = false;
}


// Show the current source on screen, top left (by default)
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
    tft.drawString(sourceName, 2, 5, GFXFF); // Top Left
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
    String status_json = requestAPI(""); // Requesting /api/
    
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
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setFreeFont(FSS12);

    Serial.print("currentSourceOffice: ");
    Serial.println(currentSourceOffset);
    Serial.println("Streams:");
    int bi = 0; // Base iterator within 'print 6 items' loop (0-6)
    int i = 0; // Filtered iterator
    int max = currentSourceOffset + 6;
    for (JsonObject value : apiStatus["streams"].as<JsonArray>()) {
        JsonObject thisStream = value;
        streamName = thisStream["name"].as<char*>();
        Serial.print(thisStream["id"].as<char*>());
        Serial.print(" - ");
        Serial.print(streamName);
        
        if (streamName.length() >= 19) {
            streamName = streamName.substring(0,18) + "...";
        }

        // Only print 6 items on screen
        if (i >= currentSourceOffset && i < max) 
        {
            Serial.print(" - String X: ");
            Serial.print((MAINZONE_X + 10));
            Serial.print(" - String Y: ");
            Serial.println((MAINZONE_Y + (40 * bi) + 10));

            // Add to sourceIDs array to be used in the main loop when one of the button is selected
            sourceIDs[bi] = thisStream["id"].as<int>();

            // Display stream button
            tft.fillRoundRect(MAINZONE_X, (MAINZONE_Y + (40 * bi)), 240, 38, 6, TFT_NAVY); // Selection box
            tft.drawString(streamName, (MAINZONE_X + 10), (MAINZONE_Y + (40 * bi) + 10));
            ++bi;
        }
        ++i;
        ++totalStreams;
    }

    if (totalStreams > currentSourceOffset) { showNext = true; }
    if (currentSourceOffset >= 6) { showPrev = true; }

    // Previous and Next buttons
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);

    // Previous button
    if (showPrev)
    {
        tft.fillRoundRect(LEFTBUTTON_X, LEFTBUTTON_Y, LEFTBUTTON_W, LEFTBUTTON_H, 6, TFT_DARKGREY);
        tft.drawString("< Prev", (MAINZONE_X + 50), 292);
    }

    // Settings button
    drawBmp("/settings.bmp", SETTINGBUTTON_X, SETTINGBUTTON_Y);

    // Next button
    if (showNext)
    {
        tft.fillRoundRect(RIGHTBUTTON_X, RIGHTBUTTON_Y, RIGHTBUTTON_W, RIGHTBUTTON_H, 6, TFT_DARKGREY);
        tft.drawString("Next >", (MAINZONE_X + 140 + 50), 292);
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

    /* Box Y values
    1: 38 - 77
    2: 78 - 117
    3: 118 - 157
    4: 158 - 197
    5: 198 - 237
    6: 238 - 277
    */
    int selected = 0;
    if (y >= 38 && y < 78) { selected = 0; }
    else if (y >= 78 && y < 118) { selected = 1; }
    else if (y >= 118 && y < 158) { selected = 2; }
    else if (y >= 158 && y < 198) { selected = 3; }
    else if (y >= 198 && y < 238) { selected = 4; }
    else if (y >= 238 && y < 278) { selected = 5; }
    
    String inputID = "stream=" + String(sourceIDs[selected]);

    // Send 
    String payload = "{\"input\": \"" + inputID + "\"}";
    Serial.println(payload);
    bool result = patchAPI("sources/" + String(amplipiSource), payload);
    Serial.print("selectSource result: ");
    Serial.println(result);
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
    tft.drawString("Zone 1: " + String(amplipiZone1), 5, 50);

    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.fillRoundRect(160, 42, 36, 36, 6, TFT_DARKGREEN);
    tft.drawString("<", 170, 47);
    tft.fillRoundRect(200, 42, 36, 36, 6, TFT_DARKGREEN);
    tft.drawString(">", 212, 47);

    tft.fillRect(20, 80, 200, 1, GREY); // Seperator
    
    // Zone 2
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    String thisZone;
    if (atoi(amplipiZone2) < 0) { thisZone = "None"; }
    else { thisZone = String(amplipiZone2); }
    tft.drawString("Zone 2: " + thisZone, 5, 90);

    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.fillRoundRect(160, 82, 36, 36, 6, TFT_DARKGREEN);
    tft.drawString("<", 170, 87);
    tft.fillRoundRect(200, 82, 36, 36, 6, TFT_DARKGREEN);
    tft.drawString(">", 212, 87);
    
    tft.fillRect(20, 120, 200, 1, GREY); // Seperator

    // Source
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Source: " + String(amplipiSource), 5, 130);

    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.fillRoundRect(160, 122, 36, 36, 6, TFT_DARKGREEN);
    tft.drawString("<", 170, 127);
    tft.fillRoundRect(200, 122, 36, 36, 6, TFT_DARKGREEN);
    tft.drawString(">", 212, 127);

    tft.fillRect(20, 160, 200, 1, GREY); // Seperator

    // Reset WiFi & AmpliPi URL
    tft.setFreeFont(FSS9);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.fillRoundRect(0, 190, 240, 36, 6, TFT_DARKGREEN);
    tft.drawString("Reboot Controller", 10, 200);
    
    // Re-calibrate touchscreen
    tft.fillRoundRect(0, 230, 240, 36, 6, TFT_DARKGREEN);
    tft.drawString("Re-calibrate Touchscreen", 10, 240);

    // Save and Cancel buttons
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);

    tft.fillRoundRect(LEFTBUTTON_X, LEFTBUTTON_Y, LEFTBUTTON_W, LEFTBUTTON_H, 6, TFT_DARKGREY);
    tft.drawString("Save", (MAINZONE_X + 50), 292);

    tft.fillRoundRect(RIGHTBUTTON_X, RIGHTBUTTON_Y, RIGHTBUTTON_W, RIGHTBUTTON_H, 6, TFT_DARKGREY);
    tft.drawString("Cancel", (MAINZONE_X + 140 + 50), 292);

    // Reset to default
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setFreeFont(FSS12);
}


void sendVolUpdate(int zone)
{
    // Send volume update to API, but only one update per second so we don't spam it
    int volDb = 0;
    if (zone == 1) { volDb = (int)(volPercent1 * 0.79 - 79); } // Convert to proper AmpliPi number (-79 to 0)
    else if (zone == 2) { volDb = (int)(volPercent2 * 0.79 - 79); } // Convert to proper AmpliPi number (-79 to 0)

    String payload = "{\"vol\": " + String(volDb) + "}";
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

    if (x > 185)
    {
        x = 200;
    } // Bring to 100% if it's close

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
        Serial.print("sendMuteUpdate result: ");
        Serial.println(result);
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
        Serial.print("sendMuteUpdate result: ");
        Serial.println(result);
    }
}

void drawMuteBtn(int zone)
{
    if (!updateMute1 && !updateMute2)
    {
        return;
    }; // Only update mute button on screen if we need to

    Serial.print("Drawing mute button for zone ");
    Serial.println(zone);

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
    if (amplipiZone2Enabled) {
        // Two Zone Mode

        // Zone 1
        String json = requestAPI("zones/" + String(amplipiZone1));
        DynamicJsonDocument ampSourceStatus(1000); // DynamicJsonDocument<N> allocates memory on the heap
        DeserializationError error = deserializeJson(ampSourceStatus, json); // Deserialize the JSON document

        // Test if parsing succeeds.
        if (error)
        {
            Serial.print(F("deserializeJson() failed: "));
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
        int newVolPercent1;
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
        drawVolume(int((volPercent1 * 1.6) + 45), 1); // Multiply by 1.5 (150px) and add 40 pixels to give it the x coord

        // Zone 2
        String json2 = requestAPI("zones/" + String(amplipiZone2));
        DynamicJsonDocument ampSourceStatus2(1000); // DynamicJsonDocument<N> allocates memory on the heap
        DeserializationError error2 = deserializeJson(ampSourceStatus2, json2); // Deserialize the JSON document

        // Test if parsing succeeds.
        if (error2)
        {
            Serial.print(F("deserializeJson() failed: "));
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
        int newVolPercent2;
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

        drawVolume(int((volPercent2 * 1.5) + 45), 2); // Multiply by 1.5 (150px) and add 40 pixels to give it the x coord
    }
    else {
        // One Zone Mode
        String json = requestAPI("zones/" + String(amplipiZone1));
        DynamicJsonDocument ampSourceStatus(1000); // DynamicJsonDocument<N> allocates memory on the heap
        DeserializationError error = deserializeJson(ampSourceStatus, json); // Deserialize the JSON document

        // Test if parsing succeeds.
        if (error)
        {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
        }

        // Update mute if data from API has changed
        bool currentMute = ampSourceStatus["mute"];
        if (currentMute != muteZone1) {
            muteZone1 = currentMute;
            updateMute1 = true;
            updateVol1 = true;
        }
        drawMuteBtn(1);

        // Update volume bar if data from API has changed
        int currentVol = ampSourceStatus["vol"];
        int newVolPercent1;
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
        drawVolume(int((volPercent1 * 1.5) + 45), 1); // Multiply by 1.5 (150px) and add 40 pixels to give it the x coord
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
    tft.setFreeFont(FSS12);
    tft.fillRect(METATEXT_X, METATEXT_Y, METATEXT_W, METATEXT_H, TFT_BLACK); // Clear metadata area first
    tft.drawString(displaySong, (TFT_WIDTH / 2), (METATEXT_Y + 5), GFXFF);   // Center Middle
    tft.fillRect(20, (METATEXT_Y + 32), (METATEXT_W - 40), 1, GREY);         // Seperator between song and artist
    tft.drawString(displayArtist, (TFT_WIDTH / 2), (METATEXT_Y + 40), GFXFF);
}

void getSource(String sourceID)
{
    String streamArtist = "";
    String streamAlbum = "";
    String streamSong = "";
    String streamStatus = "";
    String albumArt = "";
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

        String errormsg = "Error parsing results from Amplipi API";
    }

    currentSourceName = ampSourceStatus["name"].as<String>();
    if (currentSourceName.length() >= (SRC_NAME_LEN + 1)) {
        currentSourceName = currentSourceName.substring(0,SRC_NAME_LEN) + "...";
    }

    String sourceInput = ampSourceStatus["input"];
    Serial.print("sourceInput: ");
    Serial.println(sourceInput);
    String streamID = "";

    if (sourceInput == "local")
    {
        streamID = "0";
    }
    else
    {
        streamID = getValue(sourceInput, '=', 1);
    }

    Serial.print("streamID: ");
    Serial.println(streamID);

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
    streamName = ampSourceStatus["name"].as<String>();
    if (streamName == "null")
    {
        streamName = "";
    }
    albumArt = ampSourceStatus["info"]["img_url"].as<String>();
    if (albumArt == "null")
    {
        albumArt = "";
    }

    // Update source name if it has changed
    if (streamName != sourceName)
    {
        sourceName = streamName;
        updateSource = true;
        drawSource();
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

    // Initialize screen
    tft.init();
    Serial.println("Screen initialized");

    // Set the rotation before we calibrate
    tft.setRotation(0);

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
    tft.setCursor((TFT_WIDTH / 3), 40, 2);
    tft.setFreeFont(FSS18);

    tft.print("Ampli");
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("Pi");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setFreeFont(FSS12);
    tft.setCursor((TFT_WIDTH / 3), 100, 2);
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

    loadFileFSConfigFile();

    newAmplipiZone1 = atoi(amplipiZone1);
    newAmplipiZone2 = atoi(amplipiZone2);
    newAmplipiSource = atoi(amplipiSource);

    // If amplipiZone2 is 0 or great, Zone 2 should be enabled
    if (atoi(amplipiZone2) >= 0) { amplipiZone2Enabled = true; }
    else { amplipiZone2Enabled = false; }

    // Clear screen
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 20, 2);
}

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

        // Main Metadata screen
        if (activeScreen == "metadata")
        {
            Serial.println("Current screen: metadata");

            // Source Select
            if ((x > SRCBUTTON_X) && (x < (SRCBUTTON_X + SRCBUTTON_W)))
            {
                if ((y > SRCBUTTON_Y) && (y <= (SRCBUTTON_Y + SRCBUTTON_H)))
                {
                    activeScreen = "source";
                    drawSourceSelection();
                }
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
                }
                Serial.print("Mute button hit.");
            }

            // Volume control
            if ((x > VOLBARZONE_X) && (x < (VOLBARZONE_X + VOLBARZONE_W)))
            {
                if (amplipiZone2Enabled && (y > VOLBARZONE1_Y) && (y <= (VOLBARZONE1_Y + VOLBARZONE_H)))
                {
                    // Two Zone Mode, upper section
                    updateVol1 = true;
                    volPercent1 = (x - 35) / 1.5;
                    drawVolume(x, 1);
                    sendVolUpdate(1);
                }
                else if ((y > VOLBARZONE2_Y) && (y <= (VOLBARZONE2_Y + VOLBARZONE_H)))
                {
                    if (amplipiZone2Enabled) {
                        // Two Zone Mode, lower section
                        updateVol2 = true;
                        volPercent2 = (x - 35) / 1.5;
                        drawVolume(x, 2);
                        sendVolUpdate(2);
                    }
                    else {
                        // One Zone Mode
                        updateVol1 = true;
                        volPercent1 = (x - 35) / 1.5;
                        drawVolume(x, 1);
                        sendVolUpdate(1);
                    }
                }
                Serial.print("Volume control hit.");
            }
            
            delay(200); // Debounce
        }
        else if (activeScreen == "source")
        {
            // Source Selection screen
            Serial.println("Current screen: source");
            
            // Source Select button (cancel source select)
            if ((x > SRCBUTTON_X) && (x < (SRCBUTTON_X + SRCBUTTON_W)))
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
            if ((y > 36) && (y <= RIGHTBUTTON_Y))
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
            if ((x > LEFTBUTTON_X) && (x < (LEFTBUTTON_X + LEFTBUTTON_W)))
            {
                if ((y > LEFTBUTTON_Y) && (y <= (LEFTBUTTON_Y + LEFTBUTTON_H)))
                {
                    // Show previous set of streams
                    currentSourceOffset = currentSourceOffset - 6;
                    if (currentSourceOffset < 0) { currentSourceOffset = 0; }
                    drawSourceSelection();
                }
            }
            
            // Next list of sources
            if ((x > RIGHTBUTTON_X) && (x < (RIGHTBUTTON_X + RIGHTBUTTON_W)))
            {
                if ((y > RIGHTBUTTON_Y) && (y <= (RIGHTBUTTON_Y + RIGHTBUTTON_H)))
                {
                    // Show next set of streams
                    currentSourceOffset = currentSourceOffset + 6;
                    drawSourceSelection();
                }
            }

            // Settings screen
            if ((x > SETTINGBUTTON_X) && (x < (SETTINGBUTTON_X + SETTINGBUTTON_W)))
            {
                if ((y > SETTINGBUTTON_Y) && (y <= (SETTINGBUTTON_Y + SETTINGBUTTON_H)))
                {
                    // Show settings screen
                    activeScreen = "setting";
                    drawSettings();
                }
            }

            delay(200); // Debounce
        }
        else if (activeScreen == "setting")
        {
            // Zone 1 Change
            if ((y > 40) && (y <= 80))
            {
                if ((x > 160) && (x < 200)) { --newAmplipiZone1; } // Decrement zone number
                else if ((x > 200) && (x < 240)) { ++newAmplipiZone1; } // Increment zone number

                if (newAmplipiZone1 < 0) { newAmplipiZone1 = 0; }
                else if (newAmplipiZone1 > 3) { newAmplipiZone1 = 3; }

                tft.fillRect(0, 41, 160, 38, TFT_BLACK);
                tft.drawString("Zone 1: " + String(newAmplipiZone1), 5, 50);
            }
            // Zone 2 Change
            else if ((y > 80) && (y <= 120))
            {
                if ((x > 160) && (x < 200)) { --newAmplipiZone2; } // Decrement zone number
                else if ((x > 200) && (x < 240)) { ++newAmplipiZone2; } // Increment zone number

                if (newAmplipiZone2 < -1) { newAmplipiZone2 = -1; }
                else if (newAmplipiZone2 > 3) { newAmplipiZone2 = 3; }
                
                String thisZone;
                if (newAmplipiZone2 < 0) { thisZone = "None"; }
                else { thisZone = String(newAmplipiZone2); }

                tft.fillRect(0, 81, 160, 38, TFT_BLACK);
                tft.drawString("Zone 2: " + thisZone, 5, 90);
            }
            // Source Change
            else if ((y > 120) && (y <= 160))
            {
                if ((x > 160) && (x < 200)) { --newAmplipiSource; } // Decrement source number
                else if ((x > 200) && (x < 240)) { ++newAmplipiSource; } // Increment source number

                if (newAmplipiSource < 0) { newAmplipiSource = 0; }
                else if (newAmplipiSource > 3) { newAmplipiSource = 3; }

                tft.fillRect(0, 121, 160, 38, TFT_BLACK);
                tft.drawString("Source: " + String(newAmplipiSource), 5, 130);
            }

            // Restart
            if ((y >= 190) && (y < 230))
            {
                ESP.restart();
            }

            // Re-calibrate Touchscreen
            if ((y >= 230) && (y <= 270))
            {
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
                    
                    // If amplipiZone2 is 0 or great, Zone 2 should be enabled
                    if (newAmplipiSource >= 0) { amplipiZone2Enabled = true; }
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
