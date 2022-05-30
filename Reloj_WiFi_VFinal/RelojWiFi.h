#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <ESP8266HTTPClient.h> // http web access library
#include <ArduinoJson.h> // JSON decoding library

#include <FastLED.h>
#include <LEDMatrix.h>
#include <LEDText.h>
#include <FontMatrise.h>
#include "TextFonts.h"
#include "Emoticons.h"
#include <avr/pgmspace.h>

#include <Wire.h>
#include "RTClib.h"

/////////////////////////////////////////////////////////
////////////VARIABLES QUE TIENES QUE CAMBIAR/////////////
/////////////////////////////////////////////////////////
#define WLAN_SSID ""
#define WLAN_PASS ""
#define AIO_USERNAME ""
#define AIO_KEY ""
const String CITY = "";
const String COUNTRY_CODE = "";
#define OP_KEY ""


#define AIO_SERVER "io.adafruit.com"
#define AIO_SERVERPORT 1883


#define LED_PIN D8
#define COLOR_ORDER GRB
#define LEDS_TYPE WS2812B
#define BRIGHTNESS 150
#define MATRIX_WIDTH 36
#define MATRIX_HEIGHT -8
#define NUM_LEDS abs(MATRIX_WIDTH*MATRIX_HEIGHT)
#define MATRIX_TYPE HORIZONTAL_ZIGZAG_MATRIX

#define CUBE_LED_PIN D3
#define CUBE_NUM_LEDS 22
#define CUBE_BRIGHTNESS 5
#define CUBE_LEDS_TYPE WS2812B
#define CUBE_COLOR_ORDER GRB
CRGB cube_leds[CUBE_NUM_LEDS]; 

extern CRGBPalette16 myRedWhiteBluePalette;
extern const TProgmemPalette16 myRedWhiteBluePalette_p PROGMEM;

const TProgmemPalette16 myRedWhiteBluePalette_p PROGMEM ={
  CRGB::Red,
  CRGB::Gray, // 'white' is too bright compared to red and blue
  CRGB::Blue,
  CRGB::Black,
  
  CRGB::Red,
  CRGB::Gray,
  CRGB::Blue,
  CRGB::Black,
  
  CRGB::Red,
  CRGB::Red,
  CRGB::Gray,
  CRGB::Gray,
  CRGB::Blue,
  CRGB::Blue,
  CRGB::Black,
  CRGB::Black
};


class WiFiClock{
  private:
    int LED = D4;
    
    int next_mode_button = D7;
    int previous_mode_button = D6;
    int action_button = D5;
    String clock_mode[3] = {"time", "weather", "chronometer"};   //It can be time, weather or chronometer
    byte mode_counter = 1;

    String weather = "";
    String temperature = "";
    
    WiFiClient client;
    Adafruit_MQTT_Client mqtt = Adafruit_MQTT_Client(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_USERNAME, AIO_KEY);
    // Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
    Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/onoff");
    Adafruit_MQTT_Subscribe apps_notifications = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/app_notification"); 

    HTTPClient http;

    cLEDMatrix<MATRIX_WIDTH, MATRIX_HEIGHT, MATRIX_TYPE> leds;
    cLEDText CHARMsg;

    RTC_DS3231 rtc;
    String daysOfTheWeek[7] = { "Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado" };
    String monthsNames[12] = { "Enero", "Febrero", "Marzo", "Abril", "Mayo",  "Junio", "Julio","Agosto","Septiembre","Octubre","Noviembre","Diciembre" };

    //Variables del efecto ColorPalette
    CRGBPalette16 currentPalette; 
    TBlendType currentBlending;

   
  public:
    WiFiClock(){
      Serial.begin(115200);
      pinMode(LED, OUTPUT);

      pinMode(next_mode_button, INPUT);
      pinMode(previous_mode_button, INPUT);
      pinMode(action_button, INPUT);

      if (!rtc.begin()){
        Serial.println(F("Couldn't find RTC"));
        while (1);
      }

      
      if(rtc.lostPower()){
        Serial.println("Fecha y hora ajustada");
        rtc.adjust(DateTime(2022, 5, 25, 9, 26, 0));
      }
     
      
      FastLED.addLeds<LEDS_TYPE, LED_PIN, COLOR_ORDER>(leds[0], NUM_LEDS); 
      FastLED.addLeds<CUBE_LEDS_TYPE, CUBE_LED_PIN, CUBE_COLOR_ORDER>(cube_leds, CUBE_NUM_LEDS);
      FastLED.setBrightness(BRIGHTNESS);
      FastLED.clear();

      currentPalette = RainbowColors_p;
      currentBlending = LINEARBLEND;
      
      for(int i=0; i<CUBE_NUM_LEDS; i++){
        cube_leds[i] = 0xff0000;
      }
      
      FastLED.show();   
    }

    void clock_mode_handler(){
      if(digitalRead(next_mode_button)){
        if(mode_counter < 2) mode_counter++;
        else mode_counter = 0;
      }

      else if(digitalRead(previous_mode_button)){
        if(mode_counter > 0) mode_counter--;
        else mode_counter = 2;
      }
      
      
      if(clock_mode[mode_counter] == "time"){
        time_handler();
      }

      else if(clock_mode[mode_counter] == "weather"){
        weather_handler();
      }

      else if(clock_mode[mode_counter] == "chronometer"){
        chronometer_handler();
      }
    }
    
    void connectToWiFi(char* ssid, char* password){
       Serial.print("Connecting to WiFi");
       WiFi.begin(ssid, password);
       while(WiFi.status() != WL_CONNECTED){
          delay(500);
          Serial.print(".");
       }
       Serial.println("");
       Serial.println("WiFi connected");
       Serial.print("IP address: ");
       Serial.println(WiFi.localIP());
       WiFi.setAutoReconnect(true);
       WiFi.persistent(true);
    }
  
    void subscribeToMQTTtopics(){
      mqtt.subscribe(&onoffbutton);
      mqtt.subscribe(&apps_notifications);
    }


    // Ensure the connection to the MQTT server is alive (this will make the first
    // connection and automatically reconnect when disconnected).  See the MQTT_connect
    // function definition further below.
    void MQTT_connect(){
        int8_t ret;

        // Stop if already connected.
        if (mqtt.connected()) {
          return;
        }
      
        Serial.print("Connecting to MQTT... ");
      
        uint8_t retries = 3;
        while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
             Serial.println(mqtt.connectErrorString(ret));
             Serial.println("Retrying MQTT connection in 5 seconds...");
             mqtt.disconnect();
             delay(5000);  // wait 5 seconds
             retries--;
             if (retries == 0) {
               // basically die and wait for WDT to reset me
               while (1);
             }
        }
        Serial.println("MQTT Connected!");
    }

    void MQTT_message_handler(){
      Adafruit_MQTT_Subscribe *subscription;
      while ((subscription = mqtt.readSubscription(10))) {
        if (subscription == &onoffbutton) {
          onoffbutton_topic_handler();
        }

        else if(subscription == &apps_notifications){
          apps_notifications_handler();
        }
      }
    }

    void onoffbutton_topic_handler(){
      unsigned char txtWord[] = "LED";
      
      Serial.print(F("On-Off button: "));
      Serial.println((char *)onoffbutton.lastread);
      
      if (strcmp((char *)onoffbutton.lastread, "ON") == 0) {
        digitalWrite(LED, LOW); 
      }
      if (strcmp((char *)onoffbutton.lastread, "OFF") == 0) {
        digitalWrite(LED, HIGH); 
      }
      
      clear_screen_leds();
      CHARMsg.SetFont(FontArialP7x6Data);
      CHARMsg.Init(&leds, MATRIX_WIDTH, 8, 0, 0);
      CHARMsg.SetText((unsigned char *)txtWord, sizeof(txtWord) - 1);
      CHARMsg.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 0xff, 0x00, 0x00);
      CHARMsg.UpdateText();
      FastLED.show();
      delay(5000);
    }

    void apps_notifications_handler(){
      char* appName = (char *)apps_notifications.lastread;
      Serial.print("Notification from: ");
      Serial.println(appName);
      
      clear_screen_leds();
      
      
      if(!strcmp(appName, "Gmail")){
        draw_emoticons("gmail_emoticon");
        unsigned char gmailText[] = "GMAIL";
        CHARMsg.SetFont(FontArialP7x6Data);
        CHARMsg.Init(&leds, MATRIX_WIDTH, 8, 10, 0);
        CHARMsg.SetText((unsigned char *)gmailText, sizeof(gmailText) - 1);
        CHARMsg.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 0xff, 0xff, 0xff);
        CHARMsg.UpdateText();
      }

      else if(!strcmp(appName, "WhatsApp")){
        draw_emoticons("whatsapp_emoticon");
        unsigned char whatsappText[] = "WHATS";
        CHARMsg.SetFont(FontArialP7x6Data);
        CHARMsg.Init(&leds, MATRIX_WIDTH, 8, 10, 0);
        CHARMsg.SetText((unsigned char *)whatsappText, sizeof(whatsappText) - 1);
        CHARMsg.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 0xff, 0xff, 0xff);
        CHARMsg.UpdateText();      
      }

      else if(!strcmp(appName, "Instagram")){
        draw_emoticons("instagram_emoticon");
        unsigned char instagramText[] = "INSTAGRAM";
        CHARMsg.SetFont(FontArialP7x6Data);
        CHARMsg.Init(&leds, MATRIX_WIDTH, 8, 10, 0);
        CHARMsg.SetText((unsigned char *)instagramText, sizeof(instagramText) - 1);
        CHARMsg.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 0xff, 0xff, 0xff);
        CHARMsg.UpdateText();
      }

      else if(!strcmp(appName, "TikTok")){
        draw_emoticons("tiktok_emoticon");
        unsigned char tiktokText[] = "TIKTOK";
        CHARMsg.SetFont(FontArialP7x6Data);
        CHARMsg.Init(&leds, MATRIX_WIDTH, 8, 9, 0);
        
        CHARMsg.SetText((unsigned char *)tiktokText, sizeof(tiktokText) - 1);
        CHARMsg.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 0xff, 0xff, 0xff);
        CHARMsg.UpdateText();  
      }
      
      FastLED.show();
      delay(5000);
    }    

    void time_handler(){
      DateTime date = rtc.now();
      int date_hour = date.hour();
      int date_minute = date.minute();

      String str_date = "";
      if(date_hour < 10) str_date += "0";
      str_date += String(date_hour) + ":";

      if(date_minute < 10) str_date += "0";
      str_date += String(date_minute);

      char char_date[str_date.length()+1];
      str_date.toCharArray(char_date, str_date.length()+1);

      clear_screen_leds();
      draw_emoticons("clock_emoticon");
      CHARMsg.SetFont(FontArialP7x6Data);
      CHARMsg.Init(&leds, MATRIX_WIDTH, 8, 12, 0);
      CHARMsg.SetText((unsigned char *)char_date, sizeof(char_date) - 1);
      CHARMsg.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 0xff, 0xff, 0xff);
      CHARMsg.UpdateText();
      draw_menu(clock_mode[mode_counter]);
      FastLED.show();
    }
    
    void weather_handler(){
      static bool first_call = true;
      static unsigned long lastTime = 0;
     
      if((millis()-lastTime)>300000 || first_call == true){
        Serial.println("Holaaaaa");
        lastTime = millis();
        first_call = false;
        
        http.begin("http://api.openweathermap.org/data/2.5/weather?q=" + CITY + ", " + COUNTRY_CODE + "&APPID=" + OP_KEY);
        int httpCode = http.GET();
  
        if(httpCode > 0){
          String payload = http.getString(); //Get the request response payload
          DynamicJsonBuffer jsonBuffer(512);
  
          // Parse JSON object
          JsonObject& root = jsonBuffer.parseObject(payload);
          if (!root.success()) {
            Serial.println(F("Parsing failed!"));
            return;
          }
  
          //get temperature in ºC
          float temp_float = (float)(root["main"]["temp"]) - 273.15;
          int temp_int = (int)temp_float;
          temperature = String(temp_int);
          String weather_aux = root["weather"][0]["main"];
          weather = weather_aux;
          temperature += "*C";
        }
  
        else{
          Serial.println("HTTP NOT WORKING");
          http.end();
          return;
        }
      }

      char aux_temp[temperature.length()+1];
      temperature.toCharArray(aux_temp, temperature.length()+1);
      clear_screen_leds();
      if(weather == "Clear") draw_emoticons("sun_emoticon");
      CHARMsg.SetFont(FontArialP7x6Data);
      CHARMsg.Init(&leds, MATRIX_WIDTH, 8, 15, 0);
      CHARMsg.SetText((unsigned char *)aux_temp, sizeof(aux_temp) - 1);
      CHARMsg.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 0xff, 0xff, 0xff);
      CHARMsg.UpdateText();
      draw_menu(clock_mode[mode_counter]);
      FastLED.show();
      http.end();
    }
    

    void chronometer_handler(){
      static long chrono_start = 0;
      String state[3] = {"reset", "start", "stop"};
      static byte state_counter = 0;
      static long elapsed_time = 0;
      static int m = 0;
      static int s = 0;

      //Cuando se pulse botón y pase a modo start, igualar chrono_start a millis
      
      if(digitalRead(action_button)){
        if(state_counter < 2) state_counter++;
        else state_counter = 0;

        if(state[state_counter] == "start") chrono_start = millis();
      }
      
      if(state[state_counter] == "reset"){
        chrono_start = 0;
        elapsed_time = 0;
        m = 0;
        s = 0;
      }

      else if(state[state_counter] == "start"){
        elapsed_time = millis() - chrono_start;
        m = elapsed_time / 60000;
        float remainder = elapsed_time % 60000;
        s = remainder / 1000;
      }

      String str_chrono_result = "";
      if(m < 10) str_chrono_result += "0";
      str_chrono_result += String(m) + ":";
      if(s < 10) str_chrono_result += "0";
      str_chrono_result += String(s);

      char char_chrono_result[str_chrono_result.length()+1];
      str_chrono_result.toCharArray(char_chrono_result, str_chrono_result.length()+1);
     
      clear_screen_leds();
      draw_emoticons("hourglass_emoticon");
      CHARMsg.SetFont(FontArialP7x6Data);
      CHARMsg.Init(&leds, MATRIX_WIDTH, 8, 12, 0);
      CHARMsg.SetText((unsigned char *)char_chrono_result, sizeof(char_chrono_result) - 1);
      CHARMsg.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 0xff, 0xff, 0xff);
      CHARMsg.UpdateText();
      draw_menu(clock_mode[mode_counter]);
      FastLED.show();
    }


    void draw_emoticons(char* emoticon_name){
      uint32_t (*emoticon)[8];     
      
      if(!strcmp(emoticon_name, "whatsapp_emoticon")) emoticon = whatsapp_emoticon;
      else if(!strcmp(emoticon_name, "tiktok_emoticon")) emoticon = tiktok_emoticon;
      else if(!strcmp(emoticon_name, "gmail_emoticon")) emoticon = gmail_emoticon;
      else if(!strcmp(emoticon_name, "instagram_emoticon")) emoticon = instagram_emoticon;
      else if(!strcmp(emoticon_name, "cloud_emoticon")) emoticon = cloud_emoticon;
      else if(!strcmp(emoticon_name, "rain_emoticon")) emoticon = rain_emoticon;
      else if(!strcmp(emoticon_name, "partly_cloud_emoticon")) emoticon = partly_cloud_emoticon;
      else if(!strcmp(emoticon_name, "sun_emoticon")) emoticon = sun_emoticon;
      else if(!strcmp(emoticon_name, "rain_emoticon")) emoticon = rain_emoticon;
      else if(!strcmp(emoticon_name, "partly_cloud_emoticon")) emoticon = partly_cloud_emoticon;
      else if(!strcmp(emoticon_name, "clock_emoticon")) emoticon = clock_emoticon;
      else if(!strcmp(emoticon_name, "hourglass_emoticon")) emoticon = hourglass_emoticon;
      else return;
      

      int i_max = sizeof(*emoticon)/sizeof(emoticon);
      int j_max = sizeof(emoticon[0])/sizeof(uint32_t);
      
      int led_cont = 0;
      for(int i=0; i<i_max; i++){
        for(int j=0; j<j_max; j++){
          leds[0][led_cont] = emoticon[i][j];
          if(i%2 == 0)
            led_cont++;
          else
            led_cont--;
        }
        if(i%2 == 0){
          led_cont = (i+2) * MATRIX_WIDTH - 1;
        }
        else{
          led_cont = (i+1) * MATRIX_WIDTH;
        }
      } 
    }


    void draw_menu(String active_mode){
      //a los números actuales habrá que sumar 6*32
      uint32_t color;


      if(active_mode == "time") color = 0xffffff;
      else color = 0x303030;
        leds[0][274] = color;
        leds[0][273] = color;
        leds[0][272] = color;
        leds[0][271] = color;

      if(active_mode == "weather") color = 0xffffff;
      else color = 0x303030;
        leds[0][269] = color;
        leds[0][268] = color;
        leds[0][267] = color;
        leds[0][266] = color;

      if(active_mode == "chronometer") color = 0xffffff;
      else color = 0x303030;
        leds[0][264] = color;
        leds[0][263] = color;
        leds[0][262] = color;
        leds[0][261] = color;
             
    }

    void clear_screen_leds(){
      for(int i=0; i<NUM_LEDS; i++){
        leds[0][i] = 0x000000;
      }
    }






    ///////////////////////////////////////////////////
    //////////////////COLOR PALETTE////////////////////
    ///////////////////////////////////////////////////
    void colorPaletteHandler(){
      ChangePalettePeriodically();
    
      static uint8_t startIndex = 0;
      startIndex = startIndex + 1; /* motion speed */
      
      FillLEDsFromPaletteColors( startIndex);
    }

    void FillLEDsFromPaletteColors( uint8_t colorIndex){
      uint8_t brightness = 255;
      
      for( int i = 0; i < CUBE_NUM_LEDS; ++i) {
          cube_leds[i] = ColorFromPalette( currentPalette, colorIndex, brightness, currentBlending);
          colorIndex += 3;
      }
    }

    
    void ChangePalettePeriodically(){
      uint8_t secondHand = (millis() / 1000) % 60;
      static uint8_t lastSecond = 99;
      
      if( lastSecond != secondHand) {
          lastSecond = secondHand;
          if( secondHand ==  0)  { currentPalette = RainbowColors_p;         currentBlending = LINEARBLEND; }
          if( secondHand == 10)  { currentPalette = RainbowStripeColors_p;   currentBlending = NOBLEND;  }
          if( secondHand == 15)  { currentPalette = RainbowStripeColors_p;   currentBlending = LINEARBLEND; }
          if( secondHand == 20)  { SetupPurpleAndGreenPalette();             currentBlending = LINEARBLEND; }
          if( secondHand == 25)  { SetupTotallyRandomPalette();              currentBlending = LINEARBLEND; }
          if( secondHand == 30)  { currentPalette = CRGBPalette16(
                                   0xff0000, 0xff0000, 0xff0000, 0xff0000,
                                   0xff0000, 0xff0000, 0xff0000, 0xff0000,
                                   0xff0000, 0xff0000, 0xff0000, 0xff0000,
                                   0xff0000, 0xff0000, 0xff0000, 0xff0000 ); }
          if( secondHand == 35)  { SetupBlackAndWhiteStripedPalette();       currentBlending = LINEARBLEND; }
          if( secondHand == 40)  { currentPalette = CloudColors_p;           currentBlending = LINEARBLEND; }
          if( secondHand == 45)  { currentPalette = PartyColors_p;           currentBlending = LINEARBLEND; }
          if( secondHand == 50)  { currentPalette = CRGBPalette16(
                                   0x0000ff, 0x0000ff, 0x0000ff, 0x0000ff,
                                   0x0000ff, 0x0000ff, 0x0000ff, 0x0000ff,
                                   0x0000ff, 0x0000ff, 0x0000ff, 0x0000ff,
                                   0x0000ff, 0x0000ff, 0x0000ff, 0x0000ff ); }
          if( secondHand == 55)  { currentPalette = myRedWhiteBluePalette_p; currentBlending = LINEARBLEND; }
      }
    }


    void SetupTotallyRandomPalette(){
      for( int i = 0; i < 16; ++i) {
          currentPalette[i] = CHSV( random8(), 255, random8());
      }
    }

    void SetupBlackAndWhiteStripedPalette(){
      // 'black out' all 16 palette entries...
      fill_solid( currentPalette, 16, CRGB::Black);
      // and set every fourth one to white.
      currentPalette[0] = CRGB::White;
      currentPalette[4] = CRGB::White;
      currentPalette[8] = CRGB::White;
      currentPalette[12] = CRGB::White;    
    }

    void SetupPurpleAndGreenPalette(){
      CRGB purple = CHSV( HUE_PURPLE, 255, 255);
      CRGB green  = CHSV( HUE_GREEN, 255, 255);
      CRGB black  = CRGB::Black;
      
      currentPalette = CRGBPalette16(
                                     green,  green,  black,  black,
                                     purple, purple, black,  black,
                                     green,  green,  black,  black,
                                     purple, purple, black,  black );
    }
};
