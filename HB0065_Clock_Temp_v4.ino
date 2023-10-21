/* *** Use this one!!! ***
   HackerBox 0065 Clock Demo for 64x32 LED Array
   Adapted from SmartMatrix example file

   <<< Use board ESP32 Arduino > DOIT ESP32 DEVKIT V1 >>>
   **** Don't upgrade the ESP32 board manager file - use version 1.0.6 ****

   v4 - Added code to check for Wifi disconnect and restart, if necessary.
   - Added code for NTP time check
   v3 - Modified logic for rounding temps to integers
   v2 - Modified July 20, 2021 by Ted Nunn
   - Removed LCD code (see below) and merged with SmartMatrix display
   - Adapted for ESP32 vs. ESP8266
   - Changed to use zip code in US vs. city code
   - Modified data displayed to show local temp. and "Feels Like" temp.
   - http://api.openweathermap.org/data/2.5/weather?zip=78217,us&units=imperial&appid=20a51562fb744c129d3b52285a23191c
   v1 - Modified July 15, 2021 by Ted Nunn
   - Per "Dragongears" input, include Wire.h and set Wire.begin(14, 13) instead of customizing RTClib.h
   - Per "TimGTech" input, added code to update the time in void setup()
   - Modified data displayed to suit personal preferences; added background color
   
**** Credits for added OpenWeatherMap and Wifi setups **************

   Original Author: Emmanuel Odunlade, 2017

   Modified May 31st, 2019 by Ethan Tabler
   - Updated code for use with a 16x2 or 20x4 LCD screen with I2C module
   - Removed support for LED outputs based on future conditions
   - Main code supports current conditions, but this can be changed to a forecast according to api and Json parameters
   - Most comments from Micheal (Thank you btw!) have been edited to fit the current code
   
   Modified March 12, 2019 by Michael Shu
   - Updated code to use modern API and parsing (USE ARDUINOJSON 5.13.5)
   - Adjusted for NodeMCU 1.0
   
   Original project idea and code by Emmanuel Odunlade.
   Complete Project Details https://randomnerdtutorials.com/esp8266-weather-forecaster/
   
*/
#include "RTClib.h"
#include <MatrixHardware_ESP32_V0.h>    
#include <SmartMatrix.h>
#include "Wire.h" // added instead of RTClib hack
#include "time.h"

// Below are for Wifi and Weather data
// *** Requires ArduinoJson version 5.13.5 (not the latest)
#include <ArduinoJson.h>
#include <WiFi.h> // for ESP32 vs. ESP8266
#include <WiFiClient.h>
#include "arduino_secrets.h"

#define COLOR_DEPTH 24                  // Choose the color depth used for storing pixels in the layers: 24 or 48 (24 is good for most sketches - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24)
const uint16_t kMatrixWidth = 64;       // Set to the width of your display, must be a multiple of 8
const uint16_t kMatrixHeight = 32;      // Set to the height of your display
const uint8_t kRefreshDepth = 36;       // Tradeoff of color quality vs refresh rate, max brightness, and RAM usage.  36 is typically good, drop down to 24 if you need to.  On Teensy, multiples of 3, up to 48: 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48.  On ESP32: 24, 36, 48
const uint8_t kDmaBufferRows = 4;       // known working: 2-4, use 2 to save RAM, more to keep from dropping frames and automatically lowering refresh rate.  (This isn't used on ESP32, leave as default)
const uint8_t kPanelType = SM_PANELTYPE_HUB75_32ROW_MOD16SCAN;   // Choose the configuration that matches your panels.  See more details in MatrixCommonHub75.h and the docs: https://github.com/pixelmatix/SmartMatrix/wiki
const uint32_t kMatrixOptions = (SM_HUB75_OPTIONS_NONE);        // see docs for options: https://github.com/pixelmatix/SmartMatrix/wiki
const uint8_t kIndexedLayerOptions = (SM_INDEXED_OPTIONS_NONE);
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_INDEXED_LAYER(indexedLayer1, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kIndexedLayerOptions);
SMARTMATRIX_ALLOCATE_INDEXED_LAYER(indexedLayer2, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kIndexedLayerOptions);
SMARTMATRIX_ALLOCATE_INDEXED_LAYER(indexedLayer3, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kIndexedLayerOptions);

RTC_DS1307 rtc;
const int defaultBrightness = (15*255)/100;     // dim: 15% brightness
char daysOfTheWeek[7][4] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
char monthsOfTheYr[12][4] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

// Add background color
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);
const rgb24 defaultBackgroundColor = {0x40, 0, 0};

//-------------- NETWORK & API SETUP --------------\\
// Define SSID, password, and OWM API details in arduino_secrets.h
char ssid[] = SECRET_SSID;        
char pass[] = SECRET_PASS;
char apiKey[] = OWM_API;   // Obtain free from OpenWeatherMap.org

WiFiClient client;

// Open Weather Map API server name
const char server[] = "api.openweathermap.org";

//Outside of US, change to use City/Country
String zipCode = "78217,us";

//----------------- JSON SETUP --------------------\\
int jsonend = 0;
boolean startJson = false;
int status = WL_IDLE_STATUS;

#define JSON_BUFF_DIMENSION 2500

unsigned long lastConnectionTime = 60 * 1000;  // Last time you connected to the server, in milliseconds
const unsigned long postInterval = 1 * 60 * 1000;  // This is set for a connection interval of 1 minute, this can easily be adjusted (X minutes * 60 seconds * 1000 milliseconds)
//-------------------------------------------------\\

//--------------------NTP SETUP-------------------\\
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 18000;   //Replace with your GMT offset (seconds)
const int   daylightOffset_sec = 0;  //Replace with your daylight offset (seconds)



void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println("DS1307RTC Test");

  // Per "Dragongears", add instead of RTCLib hack (also add #include "Wire.h")
  Wire.begin(14, 13);
  
  // Per TimGTech, add for clock set
  rtc.begin();
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  //
  
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();   
  }
  
  if (! rtc.isrunning()) { 
    Serial.println("RTC is NOT running, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // rtc.adjust(DateTime(2021, 7, 18, 11, 34, 0));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
  
  // setup matrix
  matrix.addLayer(&backgroundLayer);
  matrix.addLayer(&indexedLayer1); 
  matrix.addLayer(&indexedLayer2);
  matrix.addLayer(&indexedLayer3);
  matrix.setBrightness(defaultBrightness);
  matrix.begin();

//--------------- JSON & WiFi SETUP ---------------\\
  text.reserve(JSON_BUFF_DIMENSION);

  WiFi.begin(ssid,pass);
  Serial.println("connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi Connected");
  // Print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // Print your WiFi module's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP address: ");
  Serial.println(ip);
  Serial.println("..................................");  
//-------------------------------------------------\\  
}
}
void loop() {
  char txtBuffer[12];
  DateTime now = rtc.now();

  // clear screen before writing new text
  indexedLayer1.fillScreen(0);
  indexedLayer2.fillScreen(0);
  indexedLayer3.fillScreen(0);
  
  // added background layer
  backgroundLayer.fillScreen(defaultBackgroundColor);
  backgroundLayer.swapBuffers();
  //

// Layer1 = HH:MM:SS or XM; %02d = 2 digit number, %s = string
  // adjust for 12 hour clock display
  if (now.hour()>12){
    sprintf(txtBuffer, "%02d:%02d:%02d", now.hour()-12, now.minute(), now.second());
  //  sprintf(txtBuffer, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  }
    else sprintf(txtBuffer, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  
  indexedLayer1.setFont(font8x13);
  indexedLayer1.setIndexedColor(1,{0x00, 0x00, 0xff});
  // NOTE: drawString(x position, y position, color?, text)
  indexedLayer1.drawString(0, 0, 1, txtBuffer);
  indexedLayer1.swapBuffers();

// Layer2 = Day-of-week + DD MMM
  sprintf(txtBuffer, "%s  %s% 02d", daysOfTheWeek[now.dayOfTheWeek()], monthsOfTheYr[(now.month()-1)], now.day());
  indexedLayer2.setFont(font5x7);
  indexedLayer2.setIndexedColor(1,{0x00, 0xff, 0x00});
  indexedLayer2.drawString(7, 15, 1, txtBuffer);
  indexedLayer2.swapBuffers();

//---------- Fetch Temp data -----------------//
  // Check if X minutes have passed then connect again and pull
  if (millis() - lastConnectionTime > postInterval) {
    // Note the time that the connection was made:
    lastConnectionTime = millis();

    // Close any existing connection before send a new request to allow client make connection to server
    client.stop();
    int jsonend = 0;
    String text;

    // If there's a successful connection:
    if (client.connect(server, 80)) {
      Serial.println("connecting to OpenWeatherMap......");
      // Send the HTTP request:
      client.println("GET /data/2.5/forecast?zip=" + zipCode + "&APPID=" + apiKey + "&mode=json&units=imperial&cnt=2 HTTP/1.1");
      client.println("Host: api.openweathermap.org");
      client.println("User-Agent: ArduinoWiFi/1.1");
      client.println("Connection: close");
      client.println();

      unsigned long timeout = millis();
      while (client.available() == 0) {
        if (millis() - timeout > 5000) {
          Serial.println(">>> Client Timeout !");
          client.stop();
          return;
        }
      }

      char c = 0;
      while (client.available()) {
        c = client.read();
        // Since Json contains equal number of open and close curly brackets, this means we can determine when a json is completely received
        // by counting the open and close occurences,
        //Serial.print(c);
        if (c == '{') {
          startJson = true;         // set startJson true to indicate json message has started
          jsonend++;
        }
        else if (c == '}') {
          jsonend--;
        }
        if (startJson == true) {
          text += c;
          delay(0); // Fixes some issues some other users experienced (Tested and working! ~E)
        }
        // if jsonend = 0 then we have have received equal number of curly braces
        if (jsonend == 0 && startJson == true) {
          // if parsing is broken, try putting the output from serial into https://arduinojson.org/v5/assistant/
          const size_t capacity = 2 * JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(2) + 2 * JSON_OBJECT_SIZE(0) + 4 * JSON_OBJECT_SIZE(1) + 3 * JSON_OBJECT_SIZE(2) + 3 * JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + 4 * JSON_OBJECT_SIZE(8) + 740;
          DynamicJsonBuffer jsonBuffer(capacity);

          // sample json below that can be used for debugging
          // const char* json = "{\"cod\":\"200\",\"message\":0.0051,\"cnt\":2,\"list\":[{\"dt\":1552446000,\"main\":{\"temp\":-5.41,\"temp_min\":-5.41,\"temp_max\":-4.18,\"pressure\":1024.8,\"sea_level\":1024.8,\"grnd_level\":1012.59,\"humidity\":76,\"temp_kf\":-1.24},\"weather\":[{\"id\":800,\"main\":\"Clear\",\"description\":\"clear sky\",\"icon\":\"01n\"}],\"clouds\":{\"all\":0},\"wind\":{\"speed\":2.8,\"deg\":223.501},\"snow\":{},\"sys\":{\"pod\":\"n\"},\"dt_txt\":\"2019-03-13 03:00:00\"},{\"dt\":1552456800,\"main\":{\"temp\":-5.63,\"temp_min\":-5.63,\"temp_max\":-4.7,\"pressure\":1024.61,\"sea_level\":1024.61,\"grnd_level\":1012.43,\"humidity\":75,\"temp_kf\":-0.93},\"weather\":[{\"id\":800,\"main\":\"Clear\",\"description\":\"clear sky\",\"icon\":\"01n\"}],\"clouds\":{\"all\":0},\"wind\":{\"speed\":2.23,\"deg\":230.501},\"snow\":{},\"sys\":{\"pod\":\"n\"},\"dt_txt\":\"2019-03-13 06:00:00\"}],\"city\":{\"id\":6122081,\"name\":\"Richmond\",\"coord\":{\"lat\":45.1834,\"lon\":-75.8327},\"country\":\"CA\"}}";

          JsonObject& root = jsonBuffer.parseObject(text);

          //---------- Strings from received Json ----------\\
        
          JsonObject& current = root["list"][0];
          JsonObject& later = root["list"][1];

          // Temperature in measurement system of choice (see up top)
          String tempNow;
          current["main"]["temp"].printTo(tempNow);

          // "Feels-like" Temperature in measurement system of choice (see up top)
          String tempFeelsLike;
          current["main"]["feels_like"].printTo(tempFeelsLike);
        
          // Humidity in percent
          // String humidityNow;
          // current["main"]["humidity"].printTo(humidityNow);

          delay(5000);

          //Convert JSON temp strings to int via float for proper rounding, then back to string for display
          int tempNowInt = round(tempNow.toFloat());
          int tempFeelsLikeInt = round(tempFeelsLike.toFloat());
          tempNow = String(tempNowInt);
          tempFeelsLike = String(tempFeelsLikeInt);
          
          Serial.println("Temp: " + tempNow);
          Serial.println("Feels like: " + tempFeelsLike);
          Serial.println("..................................");

// Layer3 = Temp and Feels Like  
          sprintf(txtBuffer, "%s   %s%s", tempNow,"FL", tempFeelsLike);
          indexedLayer3.setFont(font5x7);
          indexedLayer3.setIndexedColor(1,{0xff, 0x00, 0x00});
          indexedLayer3.drawString(7, 24, 1, txtBuffer); 
          indexedLayer3.swapBuffers();
              
          text = "";                // clear text string for the next time
          startJson = false;        // set startJson to false to indicate that a new message has not yet started
        }
     }
     Serial.println(text);
    }
  }
//--------------------------------------------------//  

// Check for loss of WiFi connectivity

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid,pass);
    Serial.println("WiFi lost... reconnecting");
    delay(500);
    Serial.print(".");
  }
  delay(500);
}
