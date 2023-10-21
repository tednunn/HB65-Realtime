   HackerBox 0065 Clock Demo for 64x32 LED Array
   Adapted from SmartMatrix example file

   <<< Use board ESP32 Arduino > DOIT ESP32 DEVKIT V1 >>>
   **** Don't upgrade the ESP32 board manager file - use version 1.0.6 ****

   v4 - Added code to check for Wifi disconnect and restart, if necessary.
      Added code for NTP time check
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
   
