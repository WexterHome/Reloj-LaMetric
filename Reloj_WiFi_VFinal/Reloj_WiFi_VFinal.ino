#include "RelojWiFi.h"


WiFiClock Clock;

unsigned long lastTimeButtons = 0;

void setup(){
  delay(1000); 
  Clock.connectToWiFi(WLAN_SSID, WLAN_PASS);
  Clock.subscribeToMQTTtopics();
  
}

void loop(){
  Clock.MQTT_connect();
  Clock.MQTT_message_handler();
  if(millis() - lastTimeButtons > 200){
    lastTimeButtons = millis();
    Clock.clock_mode_handler();
  }
  Clock.colorPaletteHandler();
  //Clock.chronometer_handler();
  //Clock.weather_handler();
  //Clock.draw_menu("chronometer");
  //Clock.blink_led();
  //Clock.draw_emoticons("gmail_emoticon");
  //FastLED.show();
  Serial.println(millis());
}
