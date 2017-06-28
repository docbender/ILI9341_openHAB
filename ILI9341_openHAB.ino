//---------------------------------------------------------------------------
//
// Name:        ILI9341_openHAB.ino
// Author:      Vita Tucek
// Created:     20.11.2016
// Modified:    28.6.2017
// License:     MIT
// Description: Display unit for openHAB
//              
//
//---------------------------------------------------------------------------
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <ILI9341_ESP.h>
#include <arial.h>
#include "buttons.h"
#include "DebugPrint.h"
#include <simpleBinary.h>
#include "iliWeb.h"
#include "Arduino.h"

#define PWM_MAX_CHANNELS 1
extern "C" {
#include "pwm.h"
}

extern "C" {
#include "user_interface.h"
}

#define PWM_PERIOD 40000

#define WIFI_SSID "WIFI_NAME"
#define WIFI_PWD  "WIFI_PASSWORD"
#define CLIENT_IP "192.168.0.100"
#define CLIENT_PORT 43243
#define CLIENT_ID 2
#define OTA_HOST "ILI-ESP"
#define OTA_PASS "OTA_PASSWORD"

#define TOUCH_CS 2
#define LCD_DC  4
#define LCD_CS  5
#define LED_PIN 15

// display size
#define DISPLAY_W 320
#define DISPLAY_H 240
// touch basic calibration
#define TS_MINX 370
#define TS_MINY 370
#define TS_MAXX 3700
#define TS_MAXY 3700
// colors
#define TOP_PANEL_COLOR CL(0xFF,0x6A,0x00) //ILI9341_ORANGE //FF6A00//CL(0x21,0x21,0x21)
#define BACKGROUND_COLOR ILI9341_BLACK
#define TEXT_COLOR ILI9341_WHITE

#define BAR_HEIGHT 30

#define LOG_SIZE 20

enum Page{
   NORMAL,
   DEBUG,
   SETTING,
   RADIO,
   LIGHT,
   INFO
};

// LCD
ILI9341_ESP lcd = ILI9341_ESP(LCD_CS, LCD_DC);
Page actualpage = NORMAL;
bool pageChanged = false;
// touch
XPT2046_Touchscreen ts(TOUCH_CS);
bool wastouched = true;
bool istouched = false;
bool screenLock = false;
TS_Point p;
uint32_t lastTouchGetTime = 0, lastTouchTime = 0;
// wifi
volatile bool wifiConnected = false;
bool wifiConnectedMem = false;
WiFiClient client;
uint32_t lastTry = 0;
bool sbAccessDenied = false,firstTry = false;;
// time
uint32_t secondsOfDay = 0, uptime = 0;
uint32_t last_hours = 0, last_minutes = 0, lastTimer = 0;
uint32_t hours = 0, minutes = 0;
uint32_t displayTimeout = 30;
// labels
label labelTime(280,10,6,TOP_PANEL_COLOR,ILI9341_WHITE, arial_14);
label labelBattery(215,10,3,TOP_PANEL_COLOR,ILI9341_WHITE, arial_12, label::ALIGN_CENTER);
//label labelXY(0,9,20,TOP_PANEL_COLOR,ILI9341_WHITE, arial_16);
label labelSetTempLiv(240,50,6,ILI9341_BLACK,ILI9341_WHITE, arial_16);
label labelSetTempBed(240,110,6,ILI9341_BLACK,ILI9341_WHITE, arial_16);
label labelTempLiv(40,50,8,ILI9341_BLACK,ILI9341_WHITE, arial_36);
label labelHumLiv(40,95,5,ILI9341_BLACK,ILI9341_WHITE, arial_16);
label labelTempOut(40,120,10,ILI9341_BLACK,ILI9341_WHITE, arial_24);
label labelTempOutMinMax(40,150,20,ILI9341_BLACK,ILI9341_WHITE, arial_16);
//label labelTempCpu(200,150,10,ILI9341_BLACK,ILI9341_WHITE, arial_16);

label labelBrightness(20,60,5,ILI9341_BLACK,ILI9341_WHITE, arial_20);
label labelTimeout(20,85,5,ILI9341_BLACK,ILI9341_WHITE, arial_20);

label labelLightLib(20,55,20,ILI9341_BLACK,ILI9341_WHITE, arial_16);
label labelLightEdge(20,135,20,ILI9341_BLACK,ILI9341_WHITE, arial_16);

label labelIP(20,60,25,ILI9341_BLACK,ILI9341_WHITE, arial_16, "IP: %s");
label labelRssi(20,80,25,ILI9341_BLACK,ILI9341_WHITE, arial_16, "Síla signálu: %ddB");
label labelFreeMem(20,100,25,ILI9341_BLACK,ILI9341_WHITE, arial_16, "Volná paměť: %dB");
label labelVoltage(20,120,25,ILI9341_BLACK,ILI9341_WHITE, arial_16, "Napětí baterie: %1.2fV");
label labelZw01(20,140,45,ILI9341_BLACK,ILI9341_WHITE, arial_16, "Z-Wave 1: baterie %2d%%, online %3ds");
label labelZw02(20,160,45,ILI9341_BLACK,ILI9341_WHITE, arial_16, "Z-Wave 2: baterie %2d%%, online %3ds");
label labelZw03(20,180,45,ILI9341_BLACK,ILI9341_WHITE, arial_16, "Z-Wave 3: baterie %2d%%, online %3ds");

// buttons
//BaseButton btnDebug(260,160,40,40);
TextButton btnLivPlus(280,70,30,30,"+",arial_16);
TextButton btnLivMinus(240,70,30,30,"-",arial_16);
TextButton btnBedPlus(280,130,30,30,"+",arial_16);
TextButton btnBedMinus(240,130,30,30,"-",arial_16);
ImageButton btnInfo(170,180,48,48, "/info.rgb565");
ImageButton btnSetting(240,180,48,48, "/setting.rgb565");
ImageButton btnRadio(100,180,48,48, "/radio.rgb565");
ImageButton btnLight(30,180,48,48, "/light_off.rgb565");

TextButton btnBriPlus(210,56,24,24,"+",arial_20);
TextButton btnBriMinus(180,56,24,24,"-",arial_20);
TextButton btnTimeoutPlus(210,81,24,24,"+",arial_20);
TextButton btnTimeoutMinus(180,81,24,24,"-",arial_20);

ImageButton btnLightLibUp(250,80,48,48, "/light_up.rgb565");
ImageButton btnLightLibDown(190,80,48,48, "/light_down.rgb565");
ImageButton btnLightLibOn(130,80,48,48, "/light_on.rgb565");
ImageButton btnLightLibOff(70,80,48,48, "/light_off.rgb565");
ImageButton btnLightEdgeUp(250,160,48,48, "/light_up.rgb565");
ImageButton btnLightEdgeDown(190,160,48,48, "/light_down.rgb565");
ImageButton btnLightEdgeOn(130,160,48,48, "/light_on.rgb565");
ImageButton btnLightEdgeOff(70,160,48,48, "/light_off.rgb565");

ImageButton btnRadioOff(80,50,48,48, "/power_button.rgb565");
ImageButton btnRadioCd(140,50,48,48, "/cd.rgb565");
ImageButton btnRadioTv(200,50,48,48, "/tv.rgb565");
ImageButton btnRadioSpotifyT(50,110,48,48, "/spotify.rgb565");
ImageButton btnRadioSpotifyV(110,110,48,48, "/spotify.rgb565");
ImageButton btnRadioClementine(170,110,48,48, "/clementine.rgb565");
ImageButton btnRadioKodi(230,110,48,48, "/kodi.rgb565");
ImageButton btnRadioPrev(20,170,48,48, "/fast-backward.rgb565");
ImageButton btnRadioPlay(80,170,48,48, "/play-pause.rgb565");
ImageButton btnRadioNext(140,170,48,48, "/fast-forward.rgb565");
ImageButton btnRadioVolMinus(200,170,48,48, "/volume-down.rgb565");
ImageButton btnRadioVolPlus(260,170,48,48, "/volume-up.rgb565");

BaseButton* buttons[] = { &btnLivPlus,&btnLivMinus,&btnBedPlus,&btnBedMinus,
      &btnSetting,&btnRadio,&btnLight, &btnInfo };
BaseButton* setbuttons[] = { &btnBriPlus,&btnBriMinus,&btnTimeoutPlus,&btnTimeoutMinus };
BaseButton* lightbuttons[] = { &btnLightLibUp,&btnLightLibDown,&btnLightLibOn,&btnLightLibOff,
      &btnLightEdgeUp,&btnLightEdgeDown,&btnLightEdgeOn,&btnLightEdgeOff };
BaseButton* radiobuttons[] = { &btnRadioOff,&btnRadioCd,&btnRadioTv,
      &btnRadioSpotifyT,&btnRadioSpotifyV,&btnRadioClementine,&btnRadioKodi,
      &btnRadioPrev,&btnRadioPlay,&btnRadioNext,&btnRadioVolMinus,&btnRadioVolPlus };

DebugPrint debug(20);

// values
float tempLiv = 0.0f, setTempLiv = 0.0f, setTempBed = 0.0f, humLiv = 0.0f, 
   tempOut = 0.0f, tempOutMin = 0.0f, tempOutMax = 0.0f, tempCpu=0.0f,
   voltage = 0.0f, lastVoltage = 0.0f,
   savedWifiPower = 0.0f, wifiPower = 20.0f;
   
bool tempLivChanged = true, humLivChanged = true, setTempLivChanged = true, 
   setTempBedChanged = true, brightnessChanged = true, displayTimeoutChanged = true,
   tempOutChanged = true,tempOutMinChanged = true,tempOutMaxChanged = true,tempCpuChanged = true; 

uint32_t displayBrightness = 85;
uint8_t lightLibLevel = 0, lightEdgeLevel = 0;
bool lightLibLevelChanged = true, lightEdgeLevelChanged = true;

simpleBinary *items;
itemData *pFreeRamItem,*pUptimeItem,*pLivSetTemp,*pBedSetTemp,*pLivTemp,*pLivHum,
   *pVoltageItem,*pOutTemp,*pOutTempMin,*pOutTempMax,*pTempCpu,
   *pLightLib,*pLightLibOff,*pLightEdge,*pLightEdgeOff,*pRadioCmd,*pNow,*pRadioSta,
   *pLivSetTempCmd,*pBedSetTempCmd,
   *pZw01Batt,*pZw01Comm,*pZw02Batt,*pZw02Comm,*pZw03Batt,*pZw03Comm;

bool isDisplayOn = true, eepromDataChanged = false, isDisplayDimm = false;
int wifiRSSI = -128, last_wifiRSSI, wifiLevel = 0, wifiLevelLast = -1;

bool LightLibOn = 0, LightEdgeOn = 0;
bool wifiFpm;

uint32_t Zw01Comm = 0,Zw02Comm = 0,Zw03Comm = 0;
uint8_t Zw01Batt = 0,Zw02Batt = 0,Zw03Batt = 0;

byte avState = 0;

iliWeb web;


//-------------------------------------------------------------------------------------------------
void WiFiEvent(WiFiEvent_t event) {
   //Serial.printf("[WiFi-event] event: %d\n", event);

   switch(event) {
      case WIFI_EVENT_STAMODE_GOT_IP:
         wifiConnected = true;
         
         debug.println("WiFi connected");
         debug.print("IP address: ");  debug.println(WiFi.localIP());
         break;
      case WIFI_EVENT_STAMODE_DISCONNECTED:
         if(wifiConnected)         
            debug.println("WiFi lost connection");
         wifiConnected = false;
         wifiLevel = 0;
         break;
   }
}

//-------------------------------------------------------------------------------------------------
void setup() {  
   Serial.begin(115200);
   debug.println("Starting..."); 
   
   pinMode(LED_PIN,OUTPUT);
   
//pwn setup for pwm.c - analogWrite replacement
   uint32_t pwm_duty_init[1] = {0}; //initial duty cycle
   uint32_t io_info[1][3] = {
      {PERIPHS_IO_MUX_MTDO_U, FUNC_GPIO15, LED_PIN}
   };
   pwm_init(PWM_PERIOD, pwm_duty_init, 1, io_info); 
   pwm_start();
   
//   analogWriteFreq(16000lu);
//   analogWriteRange(1023lu);
   
   EEPROM.begin(128);   
   EEPROM.get(0,displayBrightness);  
   EEPROM.get(4,displayTimeout);
   
   writeDisplayBrightness(displayBrightness);   
  
   lcd.begin();
   lcd.setRotation(1);

   lcd.beginTransaction();
   lcd.fillScreen(ILI9341_YELLOW);  
  
   lcd.setTextWrap(false); 
   
   //loadImages();
   lcd.beginTransaction();
   lcdBackground();
   lcdStatics();
   lcdTime();
   lcdBattery();
   
   btnLivPlus.OnClick = clickLivPlus;
   btnLivMinus.OnClick = clickLivMinus;
   btnBedPlus.OnClick = clickBedPlus;
   btnBedMinus.OnClick = clickBedMinus;
   btnLight.OnClick = clickLight;
   btnLight.EnableLongPress(true, clickLightSecondary);
   btnRadio.OnClick = clickRadio;
   btnRadio.EnableLongPress(true, clickRadioSecondary);
   btnSetting.OnClick = clickSetting;
   btnInfo.OnClick = clickInfo;
   btnBriPlus.OnClick = clickBriPlus;
   btnBriMinus.OnClick = clickBriMinus;   
   btnTimeoutPlus.OnClick = clickTimeoutPlus;
   btnTimeoutMinus.OnClick = clickTimeoutMinus;    

   btnBriPlus.EnableLongPress(true);
   btnBriMinus.EnableLongPress(true);
   btnTimeoutPlus.EnableLongPress(true);
   btnTimeoutMinus.EnableLongPress(true);

   btnLightLibUp.OnClick = clickLightLibUp;
   btnLightLibDown.OnClick = clickLightLibDown;
   btnLightLibOn.OnClick = clickLightLibOn;   
   btnLightLibOff.OnClick = clickLightLibOff;   
   btnLightEdgeUp.OnClick = clickLightEdgeUp;
   btnLightEdgeDown.OnClick = clickLightEdgeDown;
   btnLightEdgeOn.OnClick = clickLightEdgeOn;   
   btnLightEdgeOff.OnClick = clickLightEdgeOff;
   btnRadioOff.OnClick = clickRadioOff;     
   btnRadioCd.OnClick = clickRadioCd; 
   btnRadioTv.OnClick = clickRadioTv; 
   btnRadioSpotifyT.OnClick = clickRadioSpotifyT;
   btnRadioSpotifyV.OnClick = clickRadioSpotifyV;
   btnRadioClementine.OnClick = clickRadioClementine;
   btnRadioKodi.OnClick = clickRadioKodi;
   btnRadioPrev.OnClick = clickRadioPrev;
   btnRadioPlay.OnClick = clickRadioPlay;
   btnRadioNext.OnClick = clickRadioNext;
   btnRadioVolMinus.OnClick = clickRadioVolMinus;
   btnRadioVolPlus.OnClick = clickRadioVolPlus;

   btnLightLibUp.EnableLongPress(true);
   btnLightLibDown.EnableLongPress(true);
   btnLightEdgeUp.EnableLongPress(true);
   btnLightEdgeDown.EnableLongPress(true);

   btnRadioSpotifyT.SetGrayed();
   btnRadioSpotifyV.SetGrayed();
   btnRadioClementine.SetGrayed();
   btnRadioKodi.SetGrayed();
   btnRadioPrev.SetGrayed();
   btnRadioPlay.SetGrayed();
   btnRadioNext.SetGrayed();
   
   ts.begin();
   
   items = new simpleBinary(CLIENT_ID, 26, client, forceAllItemsAsNew);
   //RAM - state
   pFreeRamItem = items->initItem(0,DWORD); 
   pFreeRamItem->saveSet(ESP.getFreeHeap());
   //uptime - state
   pUptimeItem = items->initItem(1,DWORD);
   pUptimeItem->saveSet(0);
   //voltage
   pVoltageItem = items->initItem(2,FLOAT);
   pVoltageItem->saveSet(0);
   //living temp
   pLivTemp = items->initItem(3,FLOAT,recvLivTemp); 
   //living humidity
   pLivHum = items->initItem(4,FLOAT,recvLivHum);
   //set living
   pLivSetTemp = items->initItem(5,FLOAT,recvLivSetTemp);
   //set bed
   pBedSetTemp = items->initItem(6,FLOAT,recvBedSetTemp);
   //outdoor temp
   pOutTemp = items->initItem(7,FLOAT,recvOutTemp);    
   //outdoor temp min
   pOutTempMin = items->initItem(8,FLOAT,recvOutTempMin);    
   //outdoor temp max
   pOutTempMax = items->initItem(9,FLOAT,recvOutTempMax);    
   //temp CPU
   pTempCpu = items->initItem(10,FLOAT,recvCpuTemp);
   //RGBW library value
   pLightLib = items->initItem(11,RGBW,recvLibLight);
   //RGBW library off command
   pLightLibOff = items->initItem(12,BYTE);
   //RGBW edge value
   pLightEdge = items->initItem(13,RGBW,recvEdgeLight);
   //RGBW edge off command
   pLightEdgeOff = items->initItem(14,BYTE);
   //radio command
   pRadioCmd = items->initItem(15,BYTE);  
   //current time 
   pNow = items->initItem(16,DWORD,recvNow);  
   //radio state
   pRadioSta = items->initItem(17,BYTE,recvRadioSta);  
   //set living command
   pLivSetTempCmd = items->initItem(18,FLOAT);
   //set bed command
   pBedSetTempCmd = items->initItem(19,FLOAT);   
   //Z-Wave 1 battery
   pZw01Batt = items->initItem(20,BYTE,recvZwBatt);
   //Z-Wave 1 last communication
   pZw01Comm = items->initItem(21,DWORD,recvZwComm);
   //Z-Wave 2 battery
   pZw02Batt = items->initItem(22,BYTE,recvZwBatt);
   //Z-Wave 2 last communication
   pZw02Comm = items->initItem(23,DWORD,recvZwComm);
   //Z-Wave 3 battery
   pZw03Batt = items->initItem(24,BYTE,recvZwBatt);
   //Z-Wave 3 last communication
   pZw03Comm = items->initItem(25,DWORD,recvZwComm);   
   
   items->onDenyAccess = ([]() { sbAccessDenied = true; debug.println(F("Access denied")); });
   
// delete old config
   wifiFpm = false;
   WiFi.disconnect(true);

   delay(1000);
   
   WiFi.onEvent(WiFiEvent);

   WiFi.begin(WIFI_SSID, WIFI_PWD);

   ArduinoOTA.setHostname(OTA_HOST);
   ArduinoOTA.setPassword(OTA_PASS);
   ArduinoOTA.onStart([]() {
      lcd.fillRect(0,0,320,240,BACKGROUND_COLOR);   
      
      lcd.setFont(arial_36);
      lcd.setCursor(60,240/2-24);
      lcd.print("Uploading..."); 

      pinMode(LED_PIN,INPUT); 
   });

   ArduinoOTA.onEnd([]() { 
      pinMode(LED_PIN,OUTPUT);
   });

   ArduinoOTA.onError([](ota_error_t error) { ESP.restart(); });
   /* setup the OTA server */
   ArduinoOTA.begin();
   
   debug.println(F("ESP started"));
   debug.print(F("Free RAM:")); debug.print(ESP.getFreeHeap());debug.println("B");
   
   voltage = readAnalogValue();
   pVoltageItem->saveSet(voltage);
   
   web.begin(CLIENT_ID,[](int address){ 
         debug.print(F("Address updated to ")); 
         debug.println(address);
      },[](){
         String ram = String(ESP.getFreeHeap());
         ram += " B";
         String chid = String(ESP.getChipId(),HEX);
         String fchid = String(ESP.getFlashChipId(),HEX);
         String fchs = String(ESP.getFlashChipSize()/1024);
         fchs += " kB";
         String fchsp = String(ESP.getFlashChipSpeed()/1000000); 
         fchsp += " MHz";
         String rr = ESP.getResetReason();
         
         std::pair<const char*, const char*> values[] = {            
            std::make_pair("Free RAM",ram.c_str()),
            std::make_pair("Chip ID",chid.c_str()),
            std::make_pair("Flash chip ID",fchid.c_str()),
            std::make_pair("Flash chip size",fchs.c_str()),
            std::make_pair("Flash chip speed",fchsp.c_str()),
            std::make_pair("Last reset reason",rr.c_str())
         };
         web.json = iliWeb::makeJson(values,6);
      });
   web.onCreatePrintscreen = printscreen;
}

void printscreen()
{  
   lcd.printscreen(0,0,320,240,"/printscreen.bmp");
   yield();
}

//-------------------------------------------------------------------------------------------------
void loop() {  
   ArduinoOTA.handle();  
   web.handleClient();

   timers();
   touchHandle();

   if(!isDisplayOn){
      if(wifiFpm){
         sleepNow();
         return;
      }
   
      delay(100);
   }
   
   //wifi
   //connect
   if(wifiConnected && !client.connected() 
      && (!firstTry || !sbAccessDenied && (millis() - lastTry > 5000)
         || (millis() - lastTry > 10000)))
   {
      firstTry = true;
      sbAccessDenied = false;
      lastTry = millis();
      if (!client.connect(CLIENT_IP, CLIENT_PORT)) 
      {
         lastTry = millis();
         
         debug.println(F("connection failed"));
         debug.println(F("wait 5 sec..."));
      }
      else
      {
         debug.println(F("Client connected"));
                
         items->sendHi();
         forceAllItemsAsNew(items);
         
         wifiRSSI = WiFi.RSSI();
         debug.print("RSSI:");debug.print(wifiRSSI);debug.println("dB");
      }
   } 

   if(client.available())
   {
      //debug.println("Wifi - new data...");
      //process them
      items->processSerial(); 
      yield();      
   }
   
   if(items->available() && client.connected())
   {
      items->sendNewData();
      yield();
      debug.println(F("Wifi - data sent"));
   }
   
   //wifi power
   if(wifiRSSI != last_wifiRSSI){
      last_wifiRSSI = wifiRSSI;      
      
      if(!wifiConnected)
         wifiLevel = 0;
      else if(wifiRSSI < -84)
         wifiLevel = 1;
      else if(wifiRSSI < -60)
         wifiLevel = 2;
      else
         wifiLevel = 3;
      
      //0-20,5dBm
      //if(!wifiConnected)
      //   wifiPower = 2.0f;
      if(wifiRSSI < -84 && wifiPower<20.5f)
         wifiPower += 2.0f;
      else if(wifiRSSI > -70 && wifiPower>0.0f)
         wifiPower -= 2.0f;      
         
      if(savedWifiPower != wifiPower)
      {         
         debug.print(F("Wifi power:")); debug.print(wifiPower);debug.println("dBm");   
         savedWifiPower = wifiPower;
         WiFi.setOutputPower(wifiPower);
      }
   }
   
   if(isDisplayOn){
      //display
      lcdRoutine();  

      //resets
      tempLivChanged = false;
      humLivChanged = false;
      setTempLivChanged = false;
      setTempBedChanged = false;
      tempOutChanged = false;
      tempOutMinChanged = false;
      tempOutMaxChanged = false;
      tempCpuChanged = false;
      lightLibLevelChanged = false;
      lightEdgeLevelChanged = false;
   }
}

//-------------------------------------------------------------------------------------------------
void sleepNow() 
{
   //wifi force mode enabled   
   if(!wifiFpm)
   {
      wifiFpm = true;
      wifi_station_disconnect();
      wifi_set_opmode(NULL_MODE);
      wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
      wifi_fpm_open();
      delay(100);
      wifi_fpm_set_wakeup_cb(wakeupCallback);
   }
   //go into sleep
   wifi_fpm_do_sleep(5000000);
   delay(100);  
}

//-------------------------------------------------------------------------------------------------
void wakeup()
{
   wifi_fpm_close;
   wifi_set_opmode(STATION_MODE);
   wifi_station_connect();

   WiFi.begin(WIFI_SSID, WIFI_PWD);
   wifiFpm = false;
}

//-------------------------------------------------------------------------------------------------
void wakeupCallback(void) 
{

}

//-------------------------------------------------------------------------------------------------
float readAnalogValue()
{
   #define R1 146.74f
   #define R2 46.767f
   float value = analogRead(A0) / 1024.0f;   
   
   return value*0.96f*(R1+R2)/R2; //with correction
}

//-------------------------------------------------------------------------------------------------
void timers()
{
   uint32_t now = millis();
   
   if(now - lastTimer >= 1000)
   {
      lastTimer = now;
      
      uptime++;
      secondsOfDay++;
      if(secondsOfDay>=24*3600)
         secondsOfDay = 0;
      
      hours = secondsOfDay / 3600;
      minutes = (secondsOfDay / 60) % 60;
      
      if(!(uptime % 60)){
         pUptimeItem->saveSet(uptime);
         pFreeRamItem->saveSet(ESP.getFreeHeap());         
         voltage = readAnalogValue();
         if(voltage != lastVoltage){
            pVoltageItem->saveSet(voltage);
         }
         
         //debug.print("Free RAM:"); debug.print(ESP.getFreeHeap());debug.println("B");
      }   

      if(isDisplayOn && wifiConnected)
      {
         wifiRSSI = WiFi.RSSI();
      }     
      
      //debug.print("timer:");debug.println(secondsOfDay);
      //debug.print("Free RAM:"); debug.print(ESP.getFreeHeap());debug.println("B");
      
      if(!isDisplayOn && !wifiFpm)
      {
         if(secondsOfDay - lastTouchTime > displayTimeout + 60)
            sleepNow();
      }
            
      //turn off display
      if(isDisplayOn && (displayTimeout > 0))
      {
         uint32_t lastTouchBefore = secondsOfDay - lastTouchTime;
         //turn off display
         if(lastTouchBefore > displayTimeout)
         {
            //debug.print("S-L/D:");debug.print(secondsOfDay - lastTouchTime);
            //debug.print("/");debug.println(displayTimeout);
            turnOffDisplay();
         //dimm
         }else if(!isDisplayDimm && displayTimeout > 5 && (displayTimeout - lastTouchBefore <= 5)){
            isDisplayDimm = true;
            writeDisplayBrightness(1);
         }
      }
   }
}

//-------------------------------------------------------------------------------------------------  
inline void remapPoint(TS_Point &p)  
{
   p.x = map(p.x, TS_MINX, TS_MAXX, 0, DISPLAY_W);
   p.y = map(p.y, TS_MINY, TS_MAXY, 0, DISPLAY_H);
}
 
//-------------------------------------------------------------------------------------------------
inline void touchHandle() {  
   uint32_t now = millis();
   
   if(now - lastTouchGetTime < 60)
      return;
   
   lastTouchGetTime = now;
   
   wastouched = istouched;
   istouched = ts.touched();

   if (istouched) 
   {
      if(wifiFpm)
         wakeup();
      
      turnOnDisplay();      
      lastTouchTime = secondsOfDay;
      
      p = ts.getPoint();
      remapPoint(p);
    
      if (!wastouched) 
      {
         //lcdBackground();
      }
      
      if(!screenLock){
         if(actualpage == INFO)
         {
            //info buttons
             
         }else if(actualpage == SETTING)
         {
            //setting buttons
            for(int i=0;i<(sizeof(setbuttons)/sizeof(setbuttons[0]));i++)
            {
               if(setbuttons[i]->ContainsCursor(p.x,p.y))
                  setbuttons[i]->PerformPress();
               else
                  setbuttons[i]->PerformRelease();
            }   
         }else if(actualpage == LIGHT){
            //light buttons
            for(int i=0;i<(sizeof(lightbuttons)/sizeof(lightbuttons[0]));i++)
            {
               if(lightbuttons[i]->ContainsCursor(p.x,p.y))
                  lightbuttons[i]->PerformPress();
               else
                  lightbuttons[i]->PerformRelease();
            }    
         }else if(actualpage == RADIO){
            //radio buttons
            for(int i=0;i<(sizeof(radiobuttons)/sizeof(radiobuttons[0]));i++)
            {
               if(radiobuttons[i]->ContainsCursor(p.x,p.y))
                  radiobuttons[i]->PerformPress();
               else
                  radiobuttons[i]->PerformRelease();
            }
         }else{
            //buttons
            for(int i=0;i<(sizeof(buttons)/sizeof(buttons[0]));i++)
            {
               if(buttons[i]->ContainsCursor(p.x,p.y))
                  buttons[i]->PerformPress();
               else
                  buttons[i]->PerformRelease();
            }     
         }
      }      
   } else {
      if (wastouched) 
      {
         if(!screenLock){
            //was it in bar
            if(p.y < BAR_HEIGHT)
            {           
               if(actualpage == NORMAL)            
                  changePage(DEBUG);
               else
                  changePage(NORMAL); 
            }
                  
            if(actualpage == INFO)
            {
               //info buttons
                
            }else if(actualpage == SETTING)
            {
               //setting buttons
               for(int i=0;i<(sizeof(setbuttons)/sizeof(setbuttons[0]));i++)
               {
                  if(setbuttons[i]->ContainsCursor(p.x,p.y))
                     setbuttons[i]->PerformClick();
                  else
                     setbuttons[i]->PerformRelease();
               }              
            }else if(actualpage == LIGHT){
               //light buttons
               for(int i=0;i<(sizeof(lightbuttons)/sizeof(lightbuttons[0]));i++)
               {
                  if(lightbuttons[i]->ContainsCursor(p.x,p.y))
                     lightbuttons[i]->PerformClick();
                  else
                     lightbuttons[i]->PerformRelease();
               }    
            }else if(actualpage == RADIO){
               //radio buttons
               for(int i=0;i<(sizeof(radiobuttons)/sizeof(radiobuttons[0]));i++)
               {
                  if(radiobuttons[i]->ContainsCursor(p.x,p.y))
                     radiobuttons[i]->PerformClick();
                  else
                     radiobuttons[i]->PerformRelease();
               }
            }else{        
               //buttons
               for(int i=0;i<(sizeof(buttons)/sizeof(buttons[0]));i++)
               {
                  if(buttons[i]->ContainsCursor(p.x,p.y))
                     buttons[i]->PerformClick();
                  else
                     buttons[i]->PerformRelease();
               }         
            }
         }
         if(screenLock)
            screenLock=false;
      
      }
      else
         pageChanged = false;
   }
}

//-------------------------------------------------------------------------------------------------
inline void lcdRoutine()
{
   uint32_t starttime = millis();
   lcd.beginTransaction();
   
   if(pageChanged)
   {     
      lcdBackground();
      lcdStatics();   
   }
   
   //header   
   //print time
   if(pageChanged || last_hours != hours || last_minutes != minutes)
   {
      last_hours = hours;
      last_minutes = minutes;
      
      lcdTime();
   }
   
   if(pageChanged || voltage != lastVoltage)
   {
      lastVoltage = voltage;
      lcdBattery();
   }
      
   if(pageChanged || wifiLevel != wifiLevelLast){
      if(wifiLevel==0)
         lcd.drawRawBmp565(250,3,24,24,"/wifi_dis_orange.rgb565");
      else if(wifiLevel==1)
         lcd.drawRawBmp565(250,3,24,24,"/wifi_33_orange.rgb565");
      else if(wifiLevel==2)
         lcd.drawRawBmp565(250,3,24,24,"/wifi_66_orange.rgb565");      
      else
         lcd.drawRawBmp565(250,3,24,24,"/wifi_100_orange.rgb565");      

      wifiLevelLast = wifiLevel;
   }   
   
   //coordinates
/*   if (istouched) 
   {      
      lcd.sprintc(labelXY);
      sprintf(labelXY.text,"X:%d   Y:%d",p.x,p.y);
      lcd.sprints(labelXY);        
   }   
*/  
   //values
   if(actualpage == NORMAL)
   {
      //labels
      if(tempLivChanged || pageChanged)
      {
         lcd.sprintc(labelTempLiv);
         sprintf(labelTempLiv.text,"%2.1f°C",tempLiv);
         lcd.sprints(labelTempLiv);  
      }
      if(humLivChanged || pageChanged)
      {
         lcd.sprintc(labelHumLiv);
         sprintf(labelHumLiv.text,"%2.0f%%",humLiv);
         lcd.sprints(labelHumLiv);  
      }
      if(setTempLivChanged || pageChanged)
      {
         lcd.sprintc(labelSetTempLiv);
         sprintf(labelSetTempLiv.text,"%2.1f°C",setTempLiv);
         lcd.sprints(labelSetTempLiv);  
      }      
      if(setTempBedChanged || pageChanged)
      {
         lcd.sprintc(labelSetTempBed);
         sprintf(labelSetTempBed.text,"%2.1f°C",setTempBed);
         lcd.sprints(labelSetTempBed);  
      } 
      if(tempOutChanged || pageChanged)
      {
         lcd.sprintc(labelTempOut);
         sprintf(labelTempOut.text,"%2.1f°C",tempOut);
         lcd.sprints(labelTempOut);  
      }      
      if(tempOutMinChanged || tempOutMaxChanged || pageChanged)
      {
         lcd.sprintc(labelTempOutMinMax);
         sprintf(labelTempOutMinMax.text,"%2.1f°C / %2.1f°C",tempOutMin,tempOutMax);
         lcd.sprints(labelTempOutMinMax);  
      }
/*      if(tempCpuChanged || pageChanged)
      {      
         lcd.sprintc(labelTempCpu);
         sprintf(labelTempCpu.text,"%2.1f°C",tempCpu);
         lcd.sprints(labelTempCpu);  
      }      
*/     
      //buttons
      if(pageChanged)
      {
         for(int i=0;i<(sizeof(buttons)/sizeof(buttons[0]));i++)
         {
            buttons[i]->ForceRedraw();
         }
      }
      
      //buttons
      for(int i=0;i<(sizeof(buttons)/sizeof(buttons[0]));i++)
      {
         buttons[i]->Draw(lcd);
      }      
   }
   else if(actualpage == DEBUG)
   {    
      if(pageChanged) 
         debug.renderAll(lcd,0,BAR_HEIGHT,DISPLAY_W,DISPLAY_H-BAR_HEIGHT,BACKGROUND_COLOR,TEXT_COLOR);                 
      else
         debug.renderRest(lcd,0,BAR_HEIGHT,DISPLAY_W,DISPLAY_H-BAR_HEIGHT,BACKGROUND_COLOR,TEXT_COLOR);
   }else if(actualpage == SETTING){
      //buttons
      if(pageChanged)
      {
         for(int i=0;i<(sizeof(setbuttons)/sizeof(setbuttons[0]));i++)
         {
            setbuttons[i]->ForceRedraw();
         }
      }
      
      //buttons
      for(int i=0;i<(sizeof(setbuttons)/sizeof(setbuttons[0]));i++)
      {
         setbuttons[i]->Draw(lcd);
      } 
      
      //labels
      if(brightnessChanged || pageChanged)
      {
         lcd.sprintc(labelBrightness);
         sprintf(labelBrightness.text,"Jas: %2d%%",displayBrightness);
         lcd.sprints(labelBrightness);  

         brightnessChanged = false; 
      }         
      
      if(displayTimeoutChanged || pageChanged)
      {
         lcd.sprintc(labelTimeout);
         sprintf(labelTimeout.text,"Timeout: %2ds",displayTimeout);
         lcd.sprints(labelTimeout);  
         
         displayTimeoutChanged = false;
      }         
   }else if(actualpage == LIGHT){
      //buttons
      if(pageChanged)
      {
         for(int i=0;i<(sizeof(lightbuttons)/sizeof(lightbuttons[0]));i++)
         {
            lightbuttons[i]->ForceRedraw();
         }
      }
      
      //buttons
      for(int i=0;i<(sizeof(lightbuttons)/sizeof(lightbuttons[0]));i++)
      {
         lightbuttons[i]->Draw(lcd);
      }

      if(lightLibLevelChanged || pageChanged)
      {
         lcd.sprintc(labelLightLib);
         sprintf(labelLightLib.text, "Knihovna - %d%%", lightLibLevel*100/255);
         lcd.sprints(labelLightLib);  
      } 
      if(lightEdgeLevelChanged || pageChanged)
      {
         lcd.sprintc(labelLightEdge);
         sprintf(labelLightEdge.text, "Roh - %d%%", lightEdgeLevel*100/255);
         lcd.sprints(labelLightEdge);  
      }      
   }else if(actualpage == RADIO){
      //buttons
      if(pageChanged)
      {
         for(int i=0;i<(sizeof(radiobuttons)/sizeof(radiobuttons[0]));i++)
         {
            radiobuttons[i]->ForceRedraw();
         }
      }
      
      //buttons
      for(int i=0;i<(sizeof(radiobuttons)/sizeof(radiobuttons[0]));i++)
      {
         radiobuttons[i]->Draw(lcd);
      }    
   }else if(actualpage == INFO){
      //labels
      //IP
      char ip[16];
      sprintf(ip,"%d.%d.%d.%d",WiFi.localIP()[0],WiFi.localIP()[1],WiFi.localIP()[2],WiFi.localIP()[3]);
      lcd.printLabel(labelIP, pageChanged, ip);
      //Wifi signal strength
      lcd.printLabel(labelRssi, pageChanged, wifiRSSI);
      //freemem
      lcd.printLabel(labelFreeMem, pageChanged, ESP.getFreeHeap());   
      //battery
      lcd.printLabel(labelVoltage, pageChanged, voltage);
      //zwave battery/last1
      lcd.printLabel(labelZw01, pageChanged, Zw01Batt, Zw01Comm);      
      //zwave battery2
      lcd.printLabel(labelZw02, pageChanged, Zw02Batt, Zw02Comm);     
      //zwave battery3
      lcd.printLabel(labelZw03, pageChanged, Zw03Batt, Zw03Comm);     
   }
}

//-------------------------------------------------------------------------------------------------
void lcdBackground()
{
   lcd.fillRect(0,0,320,BAR_HEIGHT,TOP_PANEL_COLOR);
   lcd.fillRect(0,BAR_HEIGHT,320,210,BACKGROUND_COLOR);   
}

//-------------------------------------------------------------------------------------------------
void lcdStatics()
{
   if(actualpage == NORMAL)
   {
      //lcd.setFont(arial_16);
      //lcd.setCursor(160,60);
      //lcd.print("Termostaty");
      lcd.setFont(arial_16);
      lcd.setCursor(180,50);
      lcd.print("Obyvák");
      lcd.setCursor(180,110);
      lcd.print("Ložnice");
      
      //bottom bar
      lcd.setFont(arial_10);
      lcd.setCursor(40,230);
      lcd.print("Světlo");      
      lcd.setCursor(110,230);
      lcd.print("Rádio");    
      lcd.setCursor(185,230);
      lcd.print("Info");      
      lcd.setCursor(240,230);
      lcd.print("Nastavení");      

      drawImages();      
   }else if(actualpage == LIGHT)
   {
/*      lcd.setFont(arial_16);
      lcd.setCursor(20,55);
      lcd.print("Knihovna");
      lcd.setCursor(20,135);
      lcd.print("Roh");*/
   }
}

//-------------------------------------------------------------------------------------------------
void lcdTime()
{
   char time[6];
   snprintf(time,6,"%02d:%02d",hours,minutes);
        
   //size_t len = lcd.strPixelLen(time);
   //lcd.setCursor(lcd.width() - len - 20, 20);
   //lcd.print(time);
   //labelTime.x = lcd.width() - len - 20;
   lcd.sprintcs(labelTime,time);
}

//-------------------------------------------------------------------------------------------------
void lcdBattery()
{
   int capacity = 0;
   
   if(voltage >= 3.9f)
      capacity = 100;
   else if(voltage <= 3.3f)
      capacity = 0;
   else
      capacity = (voltage - 3.3f)*1000 / 6; 
   
   char battery[4];
   snprintf(battery,4,"%d",capacity);  

   lcd.sprintcs(labelBattery,battery);

   lcd.drawLine(211,10,213,10,ILI9341_WHITE);
   lcd.drawLine(237,10,239,10,ILI9341_WHITE);
   lcd.drawLine(211,21,239,21,ILI9341_WHITE);
   lcd.drawLine(211,10,211,21,ILI9341_WHITE);
   lcd.drawLine(239,10,239,21,ILI9341_WHITE);
   lcd.fillRect(240,14,2,4,ILI9341_WHITE);   
}

//-------------------------------------------------------------------------------------------------
uint16_t* loadImage(char* path)
{  
   File f = SPIFFS.open(path, "r");
   if(!f)
   {
      debug.print("Cannot open file ");
      debug.println(path);
      return NULL;
   }
   
   int32_t size = f.size(); 

   uint8_t *image = new uint8_t[size];
   f.read((uint8_t*)image,size); 

   f.close();
      
   return (uint16_t*)image;
}

//-------------------------------------------------------------------------------------------------
void loadImages()
{
   if(!SPIFFS.begin())
   {
      debug.println("Cannot open FS");
      return;
   }
   
   btnSetting.SetImage(loadImage("/setting.rgb565"));
   btnRadio.SetImage(loadImage("/radio.rgb565"));
   btnLight.SetImage(loadImage("/light_on.rgb565"));
   
   SPIFFS.end();   
}

//-------------------------------------------------------------------------------------------------
void drawImages()
{
   //debug.print("Loading images... Result=");
   int32_t rv = lcd.drawRawBmp565(15,42,20,40,"/tempback.rgb565");
   //debug.println(rv);
   rv = lcd.drawRawBmp565(14,90,22,24,"/drop.rgb565");
   //debug.println(rv);   
   rv = lcd.drawRawBmp565(15,122,20,40,"/temp_bi.rgb565");
   //debug.println(rv);
   
/*   return;
   debug.println("loadTemperatureImage");
   if(!SPIFFS.begin())
   {
      debug.println("Cannot open FS");
      return;
   }
   
   FSInfo fs_info;
   SPIFFS.info(fs_info);   
   debug.println("FS info:");
   debug.print("   Total/Used:");
   debug.print(fs_info.totalBytes);debug.print("B/");
   debug.print(fs_info.usedBytes);debug.println("B");
   
   File f = SPIFFS.open("/temp.rgb565", "r");
   if(!f)
   {
      debug.println("Cannot open file temp.rgb565");
      return;
   }
   
   int32_t size = f.size();
   
   debug.print("temp.rgb565 size:");
   debug.print(size);debug.println("B");  

   //lcd.setAddrWindow(5,40,34,83);
   
   //lcd.initializedatatransfer();
   
   uint8_t buffer[64];
   uint32_t bytes = 64;
   
   while(size>0){      
      if(size>=bytes)
         f.read(buffer,bytes);
      else{
         bytes = size;
         f.read(buffer,size);
      }
      size-=bytes;  

      for(int i=0;i<62;i++){
         debug.print(buffer[i],HEX);
      }
      debug.println();
      
      //SPI.writePattern(buffer,bytes,1);
   }
   
   //lcd.finishtransfer();
   
   debug.println("All data read"); 
   
   
   f.close();
   SPIFFS.end();
*/   
}

//-------------------------------------------------------------------------------------------------
void changePage(int page)
{
   if(actualpage == SETTING)
      saveFlash();   
   actualpage = (Page)page;
   pageChanged = true;   
}

//-------------------------------------------------------------------------------------------------
void clickLivPlus()
{
   if(setTempLiv >= 25.0f)
      return;   
   setTempLiv += 0.5f;
   pLivSetTempCmd->saveSet(setTempLiv);
   setTempLivChanged = true;   
}

//-------------------------------------------------------------------------------------------------
void clickLivMinus()
{
   if(setTempLiv <= 15.0f)
      return;
   setTempLiv -= 0.5f;
   pLivSetTempCmd->saveSet(setTempLiv);
   setTempLivChanged = true;   
}

//-------------------------------------------------------------------------------------------------
void clickBedPlus()
{
   if(setTempBed >= 25.0f)
      return;
   setTempBed += 0.5f;
   pBedSetTempCmd->saveSet(setTempBed); 
   setTempBedChanged = true;   
}

//-------------------------------------------------------------------------------------------------
void clickBedMinus()
{
   if(setTempBed <= 15.0f)
      return;
   setTempBed -= 0.5f;
   pBedSetTempCmd->saveSet(setTempBed);   
   setTempBedChanged = true;      
}

//-------------------------------------------------------------------------------------------------
void clickLight()
{
   changePage(LIGHT);
}

//-------------------------------------------------------------------------------------------------
void clickLightSecondary()
{
   lightTurnOffAll();
}

//-------------------------------------------------------------------------------------------------
void clickRadio()
{
   changePage(RADIO);
}

//-------------------------------------------------------------------------------------------------
void clickRadioSecondary()
{
   radioTurnOff();
}

//-------------------------------------------------------------------------------------------------
void clickSetting()
{
   changePage(SETTING);
}

//-------------------------------------------------------------------------------------------------
void clickInfo()
{
   changePage(INFO);
}

#define BRIGHTNESS_STEP 5
//-------------------------------------------------------------------------------------------------
void clickBriPlus()
{
   if(displayBrightness + BRIGHTNESS_STEP >= 100)
      displayBrightness = 100;
   else if(displayBrightness < 10)
      displayBrightness += 1;
   else
      displayBrightness += BRIGHTNESS_STEP;   

   setDisplayBrightness(displayBrightness);
}

//-------------------------------------------------------------------------------------------------
void clickBriMinus()
{
   if(displayBrightness == 1) 
      return;
   
   if(displayBrightness <= 10)
      displayBrightness -= 1;   
   else
      displayBrightness -= BRIGHTNESS_STEP;   

   setDisplayBrightness(displayBrightness);     
}

#define TIMEOUT_STEP 1
//-------------------------------------------------------------------------------------------------
void clickTimeoutPlus()
{
   if(displayTimeout + TIMEOUT_STEP >= 30)
      displayTimeout = 30;
   else
      displayTimeout += TIMEOUT_STEP;   

   displayTimeoutChanged = true;
   
   EEPROM.put(4,displayTimeout);
   eepromDataChanged = true;
}

//-------------------------------------------------------------------------------------------------
void clickTimeoutMinus()
{
   if(displayTimeout <= TIMEOUT_STEP)
      displayTimeout = 0;
   else
      displayTimeout -= TIMEOUT_STEP;    

   displayTimeoutChanged = true;
   
   EEPROM.put(4,displayTimeout);
   eepromDataChanged = true;
}

//-------------------------------------------------------------------------------------------------
void clickLightLibUp()
{
   lightLibUp();
}

//-------------------------------------------------------------------------------------------------
void clickLightLibDown()
{
   lightLibDown();
}

//-------------------------------------------------------------------------------------------------
void clickLightLibOn()
{
   lightLibTurnOn();
}

//-------------------------------------------------------------------------------------------------
void clickLightLibOff()
{
   lightLibTurnOff();
}

//-------------------------------------------------------------------------------------------------
void clickLightEdgeUp()
{
   lightEdgeUp();
}

//-------------------------------------------------------------------------------------------------
void clickLightEdgeDown()
{
   lightEdgeDown();
}

//-------------------------------------------------------------------------------------------------
void clickLightEdgeOn()
{
   lightEdgeTurnOn();
}

//-------------------------------------------------------------------------------------------------
void clickLightEdgeOff()
{
   lightEdgeTurnOff();
}

//-------------------------------------------------------------------------------------------------
void clickRadioOff()
{
   radioTurnOff();
}

//-------------------------------------------------------------------------------------------------
void clickRadioSpotifyT()
{
   pRadioCmd->save((byte)2);
   pRadioCmd->setNewData();   
}

//-------------------------------------------------------------------------------------------------
void clickRadioSpotifyV()
{
   pRadioCmd->save((byte)3);
   pRadioCmd->setNewData();   
}

//-------------------------------------------------------------------------------------------------
void clickRadioClementine()
{
   pRadioCmd->save((byte)4);
   pRadioCmd->setNewData();   
}

//-------------------------------------------------------------------------------------------------
void clickRadioKodi()
{
   pRadioCmd->save((byte)5);
   pRadioCmd->setNewData();   
}

//-------------------------------------------------------------------------------------------------
void clickRadioPrev()
{
   pRadioCmd->save((byte)6);
   pRadioCmd->setNewData();   
}

//-------------------------------------------------------------------------------------------------
void clickRadioPlay()
{
   pRadioCmd->save((byte)7);
   pRadioCmd->setNewData();   
}

//-------------------------------------------------------------------------------------------------
void clickRadioNext()
{
   pRadioCmd->save((byte)8);
   pRadioCmd->setNewData();   
}

//-------------------------------------------------------------------------------------------------
void clickRadioVolMinus()
{
   pRadioCmd->save((byte)9);
   pRadioCmd->setNewData();   
}

//-------------------------------------------------------------------------------------------------
void clickRadioVolPlus()
{
   pRadioCmd->save((byte)10);
   pRadioCmd->setNewData();   
}

//-------------------------------------------------------------------------------------------------
void clickRadioCd()
{
   pRadioCmd->save((byte)11);
   pRadioCmd->setNewData(); 
}

//-------------------------------------------------------------------------------------------------
void clickRadioTv()
{
   pRadioCmd->save((byte)12);
   pRadioCmd->setNewData(); 
}

//-------------------------------------------------------------------------------------------------
void writeDisplayBrightness(uint32_t brightness)
{  
   int value;
   if(brightness>=100)
      value = 0;
   else if(brightness<=0)
      value = PWM_PERIOD;
   else
      value = PWM_PERIOD - (brightness*PWM_PERIOD/100);
   
   // LED PWM
   pwm_set_duty(value, 0);  
   pwm_start();   
}

//-------------------------------------------------------------------------------------------------
void setDisplayBrightness(uint32_t brightness)
{
   brightnessChanged = true;
   
   writeDisplayBrightness(brightness);
   
   EEPROM.put(0,brightness); 
   eepromDataChanged = true;
}


//--------------------------------- Force data as new ---------------------------------------------
void forceAllItemsAsNew(simpleBinary *allItems)
{  
   //mark first 3 items
   for(int i=0;i<3;i++)
   {
      (*allItems)[i].setNewData();
   }
}

//-------------------------------------------------------------------------------------------------
void turnOffDisplay()
{
   writeDisplayBrightness(0);
   lcd.sleep(true);
   isDisplayOn = false;
   screenLock = true;
}

//-------------------------------------------------------------------------------------------------
void turnOnDisplay()
{
   if(isDisplayOn && !isDisplayDimm)
      return;
   
   writeDisplayBrightness(displayBrightness);
   lcd.sleep(false);
   isDisplayOn = true;
   isDisplayDimm = false;
}

//---------------------------------------------------------------------------------------------------------
void saveFlash()
{
   if(!eepromDataChanged)
      return; 
   
   EEPROM.commit();   
}

//---------------------------------------------------------------------------------------------------------
void recvLivTemp(itemData *item)
{
   float value = *((float*)(item->getData()));
   debug.print(F("Receive LivTemp:"));debug.println(value,1);
   tempLiv = value;
   tempLivChanged = true;
}

//---------------------------------------------------------------------------------------------------------
void recvLivHum(itemData *item)
{
   float value = *((float*)(item->getData()));
   debug.print(F("Receive LivHum:"));debug.println(value,1); 
   humLiv = value;
   humLivChanged = true;
}

//---------------------------------------------------------------------------------------------------------
void recvLivSetTemp(itemData *item)
{
   float value = *((float*)(item->getData()));
   debug.print(F("Receive LivSetTemp:"));debug.println(value,1); 
   setTempLiv = value;
   setTempLivChanged = true;
}

//---------------------------------------------------------------------------------------------------------
void recvBedSetTemp(itemData *item)
{
   float value = *((float*)(item->getData()));
   debug.print(F("Receive BedSetTemp:"));debug.println(value,1);   
   setTempBed = value;
   setTempBedChanged = true;
}

//---------------------------------------------------------------------------------------------------------
void recvOutTemp(itemData *item)
{
   float value = *((float*)(item->getData()));
   debug.print(F("Receive OutTemp:"));debug.println(value,1);   
   tempOut = value;
   tempOutChanged = true;
}

//---------------------------------------------------------------------------------------------------------
void recvOutTempMin(itemData *item)
{
   float value = *((float*)(item->getData()));
   debug.print(F("Receive OutTempMin:"));debug.println(value,1);   
   tempOutMin = value;
   tempOutMinChanged = true;
}

//---------------------------------------------------------------------------------------------------------
void recvOutTempMax(itemData *item)
{
   float value = *((float*)(item->getData()));
   debug.print(F("Receive OutTempMax:"));debug.println(value,1);   
   tempOutMax = value;
   tempOutMaxChanged = true;
}

//---------------------------------------------------------------------------------------------------------
void recvCpuTemp(itemData *item)
{
   float value = *((float*)(item->getData()));
   debug.print(F("Receive CpuTemp:"));debug.println(value,1);   
   tempCpu = value;
   tempCpuChanged = true;
}

//---------------------------------------------------------------------------------------------------------
void recvNow(itemData *item)
{
   uint32_t value = *((uint32_t*)(item->getData()));
   debug.print(F("Receive Time:"));debug.println(value);   

   uint32_t diff = secondsOfDay - lastTouchTime;
   secondsOfDay = value;
   lastTouchTime = secondsOfDay - diff;
}

//---------------------------------------------------------------------------------------------------------
void recvLibLight(itemData *item)
{   
   debug.println(F("Receive LibLight:"));
 
   uint32_t colors = *((uint32_t*)(item->getData()));   
   LightLibOn = colors ? 1 : 0;
   
   uint8_t* cp = (uint8_t*)&colors;
   uint8_t max = cp[0]>cp[1] ? cp[0] : cp[1];
   if(max < cp[2])
      max = cp[2];
   if(max < cp[3])
      max = cp[3];   

   if(max != lightLibLevel){
      lightLibLevelChanged = true;
      lightLibLevel = max;
   } 
   
   if(LightLibOn || LightEdgeOn)
      btnLight.SetImage("/light_on.rgb565");
   else
      btnLight.SetImage("/light_off.rgb565");
}

//---------------------------------------------------------------------------------------------------------
void recvEdgeLight(itemData *item)
{   
   debug.println(F("Receive EdgeLight"));

   uint32_t colors = *((uint32_t*)(item->getData()));   
   LightEdgeOn = colors ? 1 : 0; 

   uint8_t* cp = (uint8_t*)&colors;
   uint8_t max = cp[0]>cp[1] ? cp[0] : cp[1];
   if(max < cp[2])
      max = cp[2];
   if(max < cp[3])
      max = cp[3];

   if(max != lightEdgeLevel){
      lightEdgeLevelChanged = true;
      lightEdgeLevel = max;
   }  

   if(LightLibOn || LightEdgeOn)
      btnLight.SetImage("/light_on.rgb565");
   else
      btnLight.SetImage("/light_off.rgb565");   
}

//---------------------------------------------------------------------------------------------------------
void recvRadioSta(itemData *item)
{
   byte tmp = item->getData()[0];
   if(avState==tmp)
      return;
   
   avState = tmp;
   
   debug.print(F("Receive Radio state: "));    
   debug.println(avState);
   
   btnRadioKodi.SetGrayed(!(avState & 1));
   btnRadioPrev.SetGrayed(!(avState & 1));
   btnRadioPlay.SetGrayed(!(avState & 1));
   btnRadioNext.SetGrayed(!(avState & 1));   
   btnRadioClementine.SetGrayed(!(avState & 2));
   btnRadioSpotifyT.SetGrayed(!(avState & 4));
   btnRadioSpotifyV.SetGrayed(!(avState & 8));
}

//---------------------------------------------------------------------------------------------------------
void recvZwBatt(itemData *item)
{
   byte tmp = item->getData()[0];
   if(item==pZw01Batt)
      Zw01Batt = tmp;
   else if(item==pZw02Batt)
      Zw02Batt = tmp;
   else if(item==pZw03Batt)
      Zw03Batt = tmp;   
}

//---------------------------------------------------------------------------------------------------------
void recvZwComm(itemData *item)
{
   uint32_t tmp = *((uint32_t*)(item->getData()));
   if(item==pZw01Comm)
      Zw01Comm = tmp;
   else if(item==pZw02Comm)
      Zw02Comm = tmp;
   else if(item==pZw03Comm)
      Zw03Comm = tmp;   
}

//---------------------------------------------------------------------------------------------------------
void lightTurnOffAll()
{
   lightLibTurnOff();  
   lightEdgeTurnOff();    
}

//---------------------------------------------------------------------------------------------------------
void lightLibTurnOff()
{
   pLightLibOff->save((byte)1);
   pLightLibOff->setNewData();  
}

//---------------------------------------------------------------------------------------------------------
void lightLibTurnOn()
{
   pLightLibOff->save((byte)2);
   pLightLibOff->setNewData();  
}

//---------------------------------------------------------------------------------------------------------
void lightLibUp()
{
   pLightLibOff->save((byte)3);
   pLightLibOff->setNewData();  
}

//---------------------------------------------------------------------------------------------------------
void lightLibDown()
{
   pLightLibOff->save((byte)4);
   pLightLibOff->setNewData();  
}

//---------------------------------------------------------------------------------------------------------
void lightEdgeTurnOff()
{
   pLightEdgeOff->save((byte)1);
   pLightEdgeOff->setNewData();   
}

//---------------------------------------------------------------------------------------------------------
void lightEdgeTurnOn()
{
   pLightEdgeOff->save((byte)2);
   pLightEdgeOff->setNewData();   
}

//---------------------------------------------------------------------------------------------------------
void lightEdgeUp()
{
   pLightEdgeOff->save((byte)3);
   pLightEdgeOff->setNewData();   
}

//---------------------------------------------------------------------------------------------------------
void lightEdgeDown()
{
   pLightEdgeOff->save((byte)4);
   pLightEdgeOff->setNewData();   
}

//---------------------------------------------------------------------------------------------------------
void radioTurnOff()
{
   pRadioCmd->save((byte)1);
   pRadioCmd->setNewData();    
}