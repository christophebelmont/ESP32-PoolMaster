/* TFT management for PoolMaster-Supervisor
Credits :
  TFT : https://randomnerdtutorials.com/esp32-tft-touchscreen-on-off-button-ili9341-arduino/
        https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
        https://www.rapidtables.com/web/color/RGB_Color.html
        https://javl.github.io/image2cpp/
        https://elmah.io/tools/base64-image-encoder/
        https://learn.adafruit.com/adafruit-gfx-graphics-library/loading-images
*/

#include <Preferences.h>
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <WiFiManager.h>
#include "SuperVisor.h"

//#define BUFFER_SIZE 1024
//#define LOG_BUFFER_SIZE 1024
extern JsonDocument PMInfo;
extern JsonDocument SVSettings;
extern int UpdateinProgress;
extern char barBuf[64];
extern char local_sbuf[LOG_BUFFER_SIZE];
extern void Local_Logs_Dispatch(const char *_log_message, uint8_t _targets = 7, const char* _telnet_separator = "\r\n");
extern int IRDetected;
extern Preferences preferences;

// TFT SPI 320X240
// ***************
// https://javl.github.io/image2cpp/
// 'poolmaster42x42', 42x42px
/*
const unsigned char epd_bitmap_poolmaster [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xe0, 
	0x00, 0x00, 0x00, 0x0f, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0x00, 0x00, 0x00, 0x7f, 
	0xff, 0xff, 0x80, 0x00, 0x01, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x03, 0xff, 0xff, 0xff, 0xf0, 0x00, 
	0x07, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x07, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x0f, 0xff, 0xff, 0xff, 
	0xfc, 0x00, 0x0f, 0xff, 0xc0, 0x7f, 0xfc, 0x00, 0x1f, 0xfe, 0x00, 0x0f, 0xfe, 0x00, 0x3f, 0xf8, 
	0x7f, 0x83, 0xfe, 0x00, 0x2f, 0xc1, 0xff, 0xf0, 0x7e, 0x00, 0x30, 0x07, 0xf8, 0xfc, 0x00, 0x00, 
	0x7c, 0x7f, 0x80, 0x1f, 0xc7, 0x00, 0x3f, 0xfe, 0x0f, 0xcf, 0xff, 0x80, 0x0f, 0xf8, 0x7f, 0xff, 
	0xff, 0x80, 0x40, 0x01, 0xf0, 0xff, 0xff, 0x80, 0x70, 0x07, 0xc0, 0x1f, 0xfe, 0x00, 0x7f, 0xff, 
	0xdf, 0x87, 0xf8, 0x00, 0x7f, 0xff, 0xff, 0xe0, 0xe1, 0x80, 0x7f, 0xff, 0xc1, 0xfc, 0x07, 0x80, 
	0x7f, 0xfe, 0x00, 0x3f, 0xff, 0x80, 0x7f, 0xf0, 0x3e, 0x0f, 0xff, 0x00, 0x3f, 0xc1, 0xff, 0xc1, 
	0xfe, 0x00, 0x00, 0x07, 0xff, 0xf0, 0x00, 0x00, 0x10, 0x3f, 0xff, 0xfe, 0x03, 0x00, 0x1f, 0xff, 
	0xff, 0xff, 0xfe, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xfc, 0x00, 
	0x0f, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x07, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x03, 0xff, 0xff, 0xff, 
	0xf0, 0x00, 0x01, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x00, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x7f, 
	0xff, 0xff, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x07, 0xff, 0xf8, 0x00, 0x00, 
	0x00, 0x00, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};*/

// TFT DISPLAY SPI PIN
#define TFT_RESX  240
#define TFT_RESY  320
#define TFT_BL    14  // BackLight
#define TFT_CS    15  // Chip select control pin
#define TFT_DC    04  // Data Command control pin
#define TFT_RST   05  // Reset pin (could connect to RST pin)
#define TFT_SDA   18  // MOSI
#define TFT_SCL   19  // CLOCK

Adafruit_ST7789 display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_SDA, TFT_SCL, TFT_RST);

#define WBAR      137
#define BARCOLOR  ST77XX_BLUE
#define LIGHTGREY 0xC618
#define CYAN      ST77XX_CYAN

void TFT_WifiDrawBar(int x, int y, int l, int w, bool fill)
{
  if (fill) display.fillRect(x, y, l, w, ST77XX_YELLOW);
  else      display.fillRect(x, y, l, w, LIGHTGREY);
}

void TFT_Wifi(int posx, int posy, bool refreshTFT)
{
  // Wifi signal strenght
  #define WIFI_NBBARES 6
  #define WIFI_WBAR    6
  #define WIFI_HBAR    3
  static int currentbars = -1;
  if (refreshTFT) currentbars = -1;

  int nbars=map(WiFi.RSSI(), -90, -50, 0, WIFI_NBBARES+1);
  if (nbars == currentbars) return;
  currentbars = nbars;
  for (int i=1; i<=WIFI_NBBARES; i++) {
    TFT_WifiDrawBar(posx, posy+WIFI_HBAR*(WIFI_NBBARES-i), WIFI_WBAR, WIFI_HBAR*i, nbars > i);
    posx+=WIFI_WBAR;
  }
}

struct val_mesures {
  char label[12];
  double value;
  int line;
};

void TFT_Measures_render(int posx, int posy, int line, char *label, char *text, int color)
{
  #define CanvasH  17
  GFXcanvas1 canvas(WBAR, CanvasH);
  canvas.setFont(&FreeSans9pt7b);
  canvas.fillScreen(0x0000);
  canvas.setCursor(0, CanvasH-2);
  canvas.print(label);
  canvas.setCursor(58,CanvasH-2);
  canvas.print(text);
  delay(100);
  display.drawBitmap(posx, posy+(line*CanvasH), canvas.getBuffer(), WBAR, CanvasH, color, ST77XX_BLACK);
}

void TFT_Measures(int posx, int posy, bool refreshTFT)
{
  static val_mesures data[] = {
    {"Air Ext:", -100, -1},
    {"Air Int:", -100, -1},
    {"Water:"  , -100, -1}, // 2 temperature
    {"pH:",      -100, -1},
    {"Orp:",     -100, -1},
    {"Water:"  , -100, -1}, // 5 pressure
    {"WM:",      -100, -1},
    {"Air Int:", -100, -1}, // 7 pressure
  };

  char *p=0;
  int color;
  static int line = 1;
  const char *value;
  double dvalue;
  char text[20];

  if (UpdateinProgress) return;

  int index = 0;  // External Air Temperature
  value   = PMInfo["Air Temperature"];
  dvalue  = PMInfo["Air Temperature"].as<double>();
  if (refreshTFT) data[index].value = -100;
  if (value && data[index].value != dvalue) {
    data[index].value = dvalue;
    sprintf(text, "%2.1f C", dvalue);
    if (data[index].line == -1) data[index].line = line++;
    if      (dvalue < 5)  color = ST77XX_RED;
    else if (dvalue < 15) color = CYAN;
    else if (dvalue < 30) color = ST77XX_GREEN;
    else                  color = ST77XX_RED;
    TFT_Measures_render(posx, posy, data[index].line, data[index].label, text, color);
  }

  value = PMInfo["BME680"];
  if (!value || (strcmp(value,"none")==0))
    value = PMInfo["BMP280"];
  if (value && (strcmp(value, "none")!=0)) {
    strncpy(text, value+2, sizeof(text));
    text[sizeof(text)-1] = 0;
    p=strchr(text, 'C');
    if (p) {
      *p=0;
      dvalue = atof(text);
    }
    else dvalue=0;
    index = 1; // External Temperature
    if (refreshTFT) data[index].value = -100;
    if (data[index].value != dvalue) {
      data[index].value = dvalue;
      sprintf(text, "%2.1f C", dvalue);
      if (data[index].line == -1) data[index].line = line++;
      if      (dvalue < 5)  color = ST77XX_RED;
      else if (dvalue < 15) color = CYAN;
      else if (dvalue < 30) color = ST77XX_GREEN;
      else                  color = ST77XX_RED;
      sprintf(text, "%2.1f C", dvalue);
      TFT_Measures_render(posx, posy, data[index].line, data[index].label, text, color);
    }

    index = 7; // External pressure
    p=strstr(value, "P=");
    if (p) strncpy(text, p+2, sizeof(text));
    text[sizeof(text)-1] = 0;
    p=strchr(text, 'h');
    if (p) {
      *p=0;
      dvalue = atof(text);
    }
    else dvalue=0;
    if (refreshTFT) data[index].value = -100;
    if (data[index].value != dvalue) {
      data[index].value = dvalue;
      sprintf(text, "%.0f mb", dvalue);
      if (data[index].line == -1) data[index].line = line++;
      color = ST77XX_WHITE;
      TFT_Measures_render(posx, posy, data[index].line, data[index].label, text, color);
    }
  }

  index = 2;  // Water Temperature
  value   = PMInfo["Water Temperature"];
  dvalue  = PMInfo["Water Temperature"].as<double>();
  if (refreshTFT) data[index].value = -100;
  if (value && data[index].value != dvalue) {
    data[index].value = dvalue;
    sprintf(text, "%2.1f C", dvalue);
    if (data[index].line == -1) data[index].line = line++;
    if      (dvalue < 5)  color = ST77XX_RED;
    else if (dvalue < 27) color = CYAN;
    else if (dvalue < 31) color = ST77XX_GREEN;
    else                  color = ST77XX_RED;
    TFT_Measures_render(posx, posy, data[index].line, data[index].label, text, color);
  }

  index = 5;  // Water Pressure
  value   = PMInfo["Water Pressure"];
  dvalue  = PMInfo["Water Pressure"].as<double>();
  if (refreshTFT) data[index].value = -100;
  if (value && data[index].value != dvalue) {
    data[index].value = dvalue;
    sprintf(text, "%.0f mb", dvalue);
    if (data[index].line == -1) data[index].line = line++;
    if      (dvalue < 300) color = ST77XX_RED;
    else if (dvalue > 1800) color = ST77XX_RED;
    else                   color = ST77XX_GREEN;
    TFT_Measures_render(posx, posy, data[index].line, data[index].label, text, color);
  }

  index = 3;  // pH
  value   = PMInfo["pH"];
  dvalue  = PMInfo["pH"].as<double>();
  if (refreshTFT) data[index].value = -100;
  if (value && data[index].value != dvalue) {
    data[index].value = dvalue;
    sprintf(text, "%1.1f", dvalue);
    if (data[index].line == -1) data[index].line = line++;
    if      (dvalue < 6.8) color = ST77XX_RED;
    else if (dvalue > 7.5) color = ST77XX_RED;
    else                   color = ST77XX_GREEN;
    TFT_Measures_render(posx, posy, data[index].line, data[index].label, text, color);
  }

  index = 4;  // Orp
  value   = PMInfo["Orp"];
  dvalue  = PMInfo["Orp"].as<double>();
  if (refreshTFT) data[index].value = -100;
  if (value && data[index].value != dvalue) {
    data[index].value = dvalue;
    sprintf(text, "%3.0f mv", dvalue);
    if (data[index].line == -1) data[index].line = line++;
    if      (dvalue < 645) color = ST77XX_RED;
    else if (dvalue > 800) color = ST77XX_RED;
    else                   color = ST77XX_GREEN;
    TFT_Measures_render(posx, posy, data[index].line, data[index].label, text, color);
  }


  index = 6;  // Water Meter Counter in liter
  value = PMInfo["WaterMeter"];
  if (value && (strcmp(value, "none")!=0)) {
    dvalue  = PMInfo["WaterMeter"].as<double>();
    if (refreshTFT) data[index].value = -100;
    if (data[index].value != dvalue) {
      // update SV WaterMeterValue got from PM
      SVSettings["WaterMeter L"] = dvalue;
      preferences.begin("PMSV", false);
      preferences.putString("WaterMeter L", value);
      preferences.end();

      data[index].value = dvalue;
      sprintf(text, "%.0f", dvalue);
      if (data[index].line == -1) data[index].line = line++;
      color = ST77XX_WHITE;
      TFT_Measures_render(posx, posy, data[index].line, data[index].label, text, color);
    }
  }
}

void TFT_Time(int posx, int posy)
{
  char t[15] = {0};
  char currenttime[15];
  time_t rawtime; 
  time(&rawtime); 
  struct tm* timeinfo = localtime(&rawtime);
  strftime(currenttime, sizeof(t), "%H:%M",timeinfo);
  if (strcmp(t, currenttime) == 0) return;

  GFXcanvas1 canvas(100, 37);
  strcpy(t, currenttime);
  canvas.fillScreen(0x0000);
  canvas.setFont(&FreeSans9pt7b);
  canvas.setCursor(28, 18);
  canvas.print(t);
  canvas.setCursor(15, 35);
  char d[15];
  strftime(d, sizeof(d), "%d/%m/%y",timeinfo);
  canvas.print(d);
  display.drawBitmap(posx, posy, canvas.getBuffer(), canvas.width(), canvas.height(), ST77XX_YELLOW, ST77XX_BLACK);
}

void TFT_URL(int posx, int posy, bool refreshTFT)
{
  static char url[25] = {0};
  char currenturl[25];

  if (refreshTFT) strcpy(url, "");
  snprintf(currenturl,sizeof(currenturl),"http://%s", WiFi.localIP().toString().c_str());
  if (strcmp(url, currenturl) == 0) return;

  strcpy(url, currenturl);
  GFXcanvas1 canvas(TFT_RESY-posx, 24);
  canvas.fillScreen(0x0000);
  canvas.setFont(&FreeSans12pt7b);
  canvas.setCursor(55, 20);
  canvas.print(url);
  display.drawBitmap(posx, posy, canvas.getBuffer(), canvas.width(), canvas.height(), ST77XX_WHITE, ST77XX_BLUE); 
}

void TFT_DrawLEDs(const char* ledcolor, int x, int y)
{
  y -= 13;
  int color;
  if (strstr(ledcolor, "red"))          color = ST77XX_RED;
  else if (strstr(ledcolor, "blue"))    color = ST77XX_BLUE;
  else if (strstr(ledcolor, "yellow"))  color = ST77XX_YELLOW;
  else if (strstr(ledcolor, "green"))   color = ST77XX_GREEN;
  else if (strstr(ledcolor, "cyan"))    color = ST77XX_CYAN;
  else if (strstr(ledcolor, "orange"))  color = ST77XX_ORANGE;
  else if (strstr(ledcolor, "magenta")) color = ST77XX_MAGENTA;
  else color = ST77XX_BLACK;
  display.fillRoundRect(x+1, y+1, 22, 13, 90, color);
  display.drawRoundRect(x, y, 24, 15, 90, LIGHTGREY);
}

void TFT_Alerts(int posx, int posy)
{
  if (UpdateinProgress) return;
  
  display.setTextWrap(false); 
  display.setFont(&FreeSans9pt7b);
  JsonObject root = PMInfo.as<JsonObject>();
  int i=0;
  for (JsonPair kv : root) {
    const char *ledcolor=kv.value().as<const char*>();
    if (!ledcolor) continue;
    if (ledcolor[0] != '*') continue; // this is not a LED
    TFT_DrawLEDs(ledcolor+1, posx, posy);
    display.setCursor(posx+28, posy);
    display.print(kv.key().c_str());
    posy+=17;i++;
    if (posy > TFT_RESX) return;
  }
  if (i==0) return;
  //if (i!=12) refreshTFT=true;
}
/*
void TFT_SetBackground()
{
//  #define deltaY  30
//  display.fillRect(0, deltaY, WBAR, TFT_RESX-deltaY, BARCOLOR);
  display.drawBitmap(7, 190, epd_bitmap_poolmaster, 42, 42, ST77XX_YELLOW);
//  display.setCursor(WBAR+10, 50);
//  display.setFont(&FreeSans9pt7b);
//  display.print("Starting...");
}
*/
void TFT_Backlight()
{
  // Check for IR Detector and motion
  if (digitalRead(IR_DETECTOR_PIN) == HIGH) {
    Local_Logs_Dispatch("IR : Presence detected");
    digitalWrite(TFT_BL, HIGH); // TFT Backlight on
    IRDetected=0; // IR detected, let's run the cycle
  }
  if (IRDetected == -1) return;
  if (IRDetected > TFT_PowerSaving) digitalWrite(TFT_BL, LOW);
  else IRDetected++;
}

void TFT_DrawProgressBar(int x, int y, int l, int progress, bool firsttime)
{
  if (firsttime) display.fillRoundRect(x, y, l+4, 20, 90, LIGHTGREY);
  if (progress<3) return;           // fix garbage
  if (progress>99) progress = 100;  // fix garbage
  display.fillRoundRect(x+2, y+2, (l*progress)/100, 16, 90, ST77XX_BLUE);
}

void TFT_OTA()
{
  static bool firsttime = true;
  if (UpdateinProgress<2) { // 0: no OTA, 1=init OTA
    firsttime = true;   // no OTA
    return;
  }

  if (firsttime) {
      display.fillScreen(ST77XX_BLACK);
      digitalWrite(TFT_BL, HIGH); // TFT Backlight on
      if (IRDetected != -1) IRDetected=0;
    }
  
  GFXcanvas1 canvas(TFT_RESY-30, 24);
  canvas.fillScreen(0x0000);
  canvas.setFont(&FreeSans12pt7b);
  canvas.setCursor(1, 20);
  canvas.print(barBuf);
  display.drawBitmap(30, 110, canvas.getBuffer(), canvas.width(), canvas.height(), ST77XX_WHITE, ST77XX_BLACK);
  TFT_DrawProgressBar(30, 150, 250, UpdateinProgress, firsttime);
  firsttime=false;
}


void TFT_Refresh(bool refresh)
{
  if (refresh) display.fillScreen(ST77XX_BLACK);

  // update PoolMaster info
  TFT_Backlight();
  TFT_URL(1, 1, refresh);
  TFT_Wifi(3, 4, refresh);
  TFT_Time(16, 35);
  TFT_OTA();
  TFT_Alerts(WBAR+4, 45);
  TFT_Measures(0, 70, refresh);
}

void TFT_Init()
{
  display.init(TFT_RESX, TFT_RESY);
  display.fillScreen(ST77XX_BLACK);
  display.setRotation(1);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH); // TFT Backlight on
//  TFT_SetBackground();        // draw static values
}


void configModeCallback(WiFiManager *myWiFiManager) 
{
  int deltax = 10;
  display.setFont(&FreeSans12pt7b);
  display.setCursor(deltax, 25);
  display.print("PoolMaster WIFI");
  display.setCursor(deltax, 70);
  display.print("From your phone,");
  display.setCursor(deltax, 100);
  display.print("Connect to WIFI :");
  display.setCursor(deltax, 140);
  display.print(myWiFiManager->getConfigPortalSSID());
  display.setCursor(deltax, 180);
  display.print("then configure");
  display.setCursor(deltax, 210);
  display.print("PoolMaster WIFI");
}
