#include <SPI.h>
#include "LedMatrix.h"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "MyBemfa.h"
/*
VCC   +3.3V
GND   GND
DIN   GPIO13(D7)
CS    GPIO15(D8)
CLK   GPOI14(D5 )
*/

/* 这四项需要更改 */
#define wifi_ssid "wifi名"
#define wifi_passwd "wifi密码"
#define TOKEN "巴法云的token"
#define THEME "巴法云的主题名"
#define NUMBER_OF_DEVICES 4
#define CS_PIN 15
#define DELAY_TIME 10
LedMatrix ledMatrix = LedMatrix(NUMBER_OF_DEVICES, CS_PIN);
MyBemfa bemfa = MyBemfa(wifi_ssid, wifi_passwd, TOKEN, THEME);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp1.aliyun.com",60*60*8, 30*60*1000);
char s[20];
unsigned long long last_blink_time, last_sync_ntp_time;


void wifiInit()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_passwd);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.println("WIFI not Connect");
  }
  Serial.println("Connected to AP");
  Serial.print("IP address is ");
  Serial.println(WiFi.localIP());
}

void ntpInit()
{
  timeClient.begin();
  timeClient.update();
}

void draw_text()
{
  ledMatrix.clear();
  ledMatrix.scrollTextLeft();
  ledMatrix.drawText();
  ledMatrix.commit();
}

bool show_next = true;
void draw_progressing(int hours, int minutes)
{
  show_next = !show_next;
  float percent = (float(hours) * 60 + float(minutes)) / (24*60);
  int pixel_showing_now_i, pixel_showing_now_j;
  for(int i=0; i<8*NUMBER_OF_DEVICES; i++){
    for(int j=0; j <8; j++){
      ledMatrix.setPixel(j, i, float(i*8+j)/(64.0*NUMBER_OF_DEVICES) < percent);
      if(float(i*8+j)/(64.0*NUMBER_OF_DEVICES) < percent) {
        pixel_showing_now_i = i;
        pixel_showing_now_j = j;
      }
    }
  }
  ledMatrix.setPixel(pixel_showing_now_j, pixel_showing_now_i, show_next);
  ledMatrix.commit();
}

void progressing_to_icon(byte* icon, int hours, int minutes)
{
  float percent = (float(hours) * 60 + float(minutes)) / (24*60);
  for(int i=0; i < 8*NUMBER_OF_DEVICES; i++){
    icon[i] = 0;
    for(int j=0; j <8; j++){
      if(float(i*8+j)/(64*NUMBER_OF_DEVICES) < percent){
        icon[i] += 1 << (7-j);
      }
    }
  }
}

bool shouldBeOn = true;
void handleMessage(String msg) {
  if(msg == "off") {
    shouldBeOn = false;
  }
  else{
    shouldBeOn = true;
  }
}

void setup()
{
  Serial.begin(115200);
  ledMatrix.init();
  ledMatrix.clear();

  ledMatrix.animation_fill();

  wifiInit();
  ntpInit();
  bemfa.setCallbackFunction(handleMessage);
  Serial.print("Starting");

  ledMatrix.animation_clear();
  byte* icon = new byte[8*NUMBER_OF_DEVICES];
  progressing_to_icon(icon, timeClient.getHours(), timeClient.getMinutes());
  ledMatrix.animation_show(icon);

  last_blink_time = millis();
  last_sync_ntp_time = millis();
}

void loop()
{
  bemfa.tick();
  if(shouldBeOn) {
    if(millis() - last_blink_time >= 1000) {
      draw_progressing(timeClient.getHours(), timeClient.getMinutes());
      last_blink_time = millis();
    }
  }
  else {
    ledMatrix.clear();
    ledMatrix.commit();
  } 
  if(millis() - last_sync_ntp_time >= 1000 * 60 * 60 * 12){
    Serial.println("Synchronizing time with NTP.");
    timeClient.update();
  }
}
