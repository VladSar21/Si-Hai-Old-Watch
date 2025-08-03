#include <Adafruit_NeoPixel.h>  //1.12.3
#include <ArduinoJson.h>        // https://github.com/bblanchon/ArduinoJson   6.18.5
#include <HTTPClient.h>         // In-built
#include <time.h>               // In-built
#include <EEPROM.h>

#include "GyverButton.h"   //3.8
#include <WiFiManager.h>   //2.0.17
#include <GyverNTP.h>      //1.3.1
#include <RtcDS1302.h>     //1.4.1
#include <Adafruit_GFX.h>  // Core graphics library
#include <Arduino_ST7789_Fast.h>
#include <SPI.h>
#include "pics.h"
#include "pins.h"
#include "other.h"

byte halfSec = 0;
byte oldhalfSec;
byte mode = 2;
int oldHour = -11, oldMin = -11, oldSec = -11;
float bright = 255;
byte maxMode = 3;
bool powerOn = true;
bool pixelOn = true;
int tindex = 0;
byte pixelBright = 5;
byte RedColor, GreenColor, BlueColor;
String str = "";
bool newCommand = false;
int sunR = 6, sunS = 21;
byte NOFF = 0, NTOff = 23, NTOn = 6;
bool nightSleep = false;

#define max_readings 24  // Limited to 3-days here, but could go to 5-days = 40

Forecast_record_type WxConditions[1];
Forecast_record_type WxForecast[max_readings];

float pressure_readings[max_readings] = { 0 };
float temperature_readings[max_readings] = { 0 };
float humidity_readings[max_readings] = { 0 };
float rain_readings[max_readings] = { 0 };
float snow_readings[max_readings] = { 0 };

enum alignment { LEFT,
                 RIGHT,
                 CENTER };

hw_timer_t *HalfSec_timer = NULL;

GyverNTP ntp(4);                   // GMT +4
GButton buttMode(BUTTON_MODE_PIN);
GButton buttPower(BUTTON_POWER_PIN);
GButton buttLeft(BUTTON_LEFT_PIN);
GButton buttRight(BUTTON_RIGHT_PIN);

const uint16_t *digitBig[] = { D0, D1, D2, D3, D4, D5, D6, D7, D8, D9 };
const uint16_t *digitSmall[] = { DD0, DD1, DD2, DD3, DD4, DD5, DD6, DD7, DD8, DD9 };
const uint16_t *iconWeather[] = { W1, W1, W2, W3, W4, W5, W6, W7, W20, W20, W20, W11, W12, W13, W14, W15, W16, W17, W20, W20, W20 };

ThreeWire myWire(DS1302_IO, DS1302_SCLK, DS1302_CE);  // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);

Arduino_ST7789 tft = Arduino_ST7789(TFT_DC, TFT_RST, 17);
Adafruit_NeoPixel pixels(NUMPIXELS, BACKLIGHTS_PIN, NEO_GRB + NEO_KHZ800);

RtcDateTime now;

void imageOut(int LCD, int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *img16);
void colorWS(uint32_t color);
void decode(int display, int simobl);
void InitialiseSystem();
void checkTime();
void checkButt();
void drawtext(int LCD, int x, int y, int size, char *text, uint16_t color, alignment align);
void tempOut(int LCD, float temp);
void weatherOut(int LCD, int iconNum);
void phOut(int LCD, int i);
void getWeater();
bool obtainWeatherData(WiFiClient &client, const String &RequestType);
bool DecodeWeather(WiFiClient &json, String Type);
byte DisplayConditionsSection(String IconName);
String ConvertUnixTime(int unix_time);
void sleepTime();
void nigthOff(int nm);
void weatherPrint();
void weatherOther(int LCD, int stage);
void commandStirng();
void help();

void IRAM_ATTR onTimer() {
  halfSec = halfSec + 1;
  if (halfSec > 119) halfSec = 0;
}

void setup() {
  InitialiseSystem();
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setConnectTimeout(60);
  bool res;
  res = wm.autoConnect("OldWatch");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
  }
  tft.println("");
  tft.println("WiFi connected");
  tft.println("IP address: ");
  tft.println(WiFi.localIP());
  ntp.begin();
  if (ntp.updateNow() == 0) {
    tft.println("NTP OK");
    RtcDateTime compiled = RtcDateTime(ntp.year(), ntp.month(), ntp.day(), ntp.hour(), ntp.minute(), ntp.second());
    now = Rtc.GetDateTime();
    if (compiled != now) {
      Rtc.SetDateTime(compiled);
      tft.println("RTC renew");
    }
  } else tft.println("NTP FAIL");
  WiFi.disconnect();
  delay(10000);
  now = Rtc.GetDateTime();
  checkTime(mode);
  HalfSec_timer = timerBegin(0, 80, true);
  timerAttachInterrupt(HalfSec_timer, &onTimer, true);
  timerAlarmWrite(HalfSec_timer, 500000, true);
  timerAlarmEnable(HalfSec_timer);
  EEPROM.get(1, RedColor);
  EEPROM.get(2, GreenColor);
  EEPROM.get(3, BlueColor);
  EEPROM.get(4, pixelBright);
  EEPROM.get(5, bright);
  EEPROM.get(9, NOFF);
  EEPROM.get(10, NTOff);
  EEPROM.get(11, NTOn);
}

void loop() {
  now = Rtc.GetDateTime();
  checkButt();
  if (mode == 1) {
    if (oldhalfSec != halfSec & halfSec % 2 == 0) {
      oldhalfSec = halfSec;
      imageOut(3, 0, 0, 135, 240, DE);
    } else if (oldhalfSec != halfSec & halfSec % 2 != 0) {
      oldhalfSec = halfSec;
      imageOut(3, 0, 0, 135, 240, DD);
    }
  }
  if (ntp.hour() > 6 & ntp.minute() == 30 & ntp.second() == 0) {
    WiFi.reconnect();
    int g = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      if (g > 5) break;
      g = g + 1;
    }
    ntp.updateNow();
    if (ntp.updateNow() == 0) {
      RtcDateTime compiled = RtcDateTime(ntp.year(), ntp.month(), ntp.day(), ntp.hour(), ntp.minute(), ntp.second());
      now = Rtc.GetDateTime();
      if (compiled != now) {
        Rtc.SetDateTime(compiled);
      }
    }
    getWeater();
    if (mode == 1) {
      weatherPrint(6, -1);
    }
    if (mode == 3) {
      weatherPrint(1, -1);
      weatherPrint(2, 0);
      weatherPrint(3, 1);
      weatherPrint(4, 2);
      weatherPrint(5, 3);
      weatherPrint(6, 4);
    }
    WiFi.disconnect();
  }
  checkTime(mode);
  sleepTime();
  if (Serial.available() > 0) {
    str = Serial.readString();
    Serial.println("# " + str.substring(0, str.length() - 1));
    newCommand = true;
  }
  if (newCommand) {
    commandStirng();
  }
}


void imageOut(int LCD, int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *img16) {
  if (LCD == 0) {
    digitalWrite(CSSR_LATCH_PIN, LOW);
    shiftOut(CSSR_DATA_PIN, CSSR_CLOCK_PIN, MSBFIRST, 0);
    digitalWrite(CSSR_LATCH_PIN, HIGH);
  } else {
    byte disp = ~(0X1 << (LCD - 1));
    digitalWrite(CSSR_LATCH_PIN, LOW);
    shiftOut(CSSR_DATA_PIN, CSSR_CLOCK_PIN, MSBFIRST, disp);
    digitalWrite(CSSR_LATCH_PIN, HIGH);
  }
  tft.drawImageF(x, y, w, h, img16);
}

void colorWS(uint32_t color) {
  for (int i = 0; i < pixels.numPixels(); i++) {  // For each pixel in strip...
    pixels.setPixelColor(i, color);               //  Set pixel's color (in RAM)
    pixels.show();                                //  Update strip to match
  }
}

void decode(int display, int simobl) {
  imageOut(display, 0, 0, 135, 240, digitBig[simobl]);
}

void InitialiseSystem() {
  Serial.begin(115200);
  EEPROM.begin(12);
  pinMode(TFT_ENABLE_PIN, OUTPUT);
  digitalWrite(TFT_ENABLE_PIN, LOW);
  pixels.begin();
  pixels.show();
  pixels.setBrightness(pixelBright);
  pixels.show();
  colorWS(pixels.Color(RedColor, GreenColor, BlueColor));
  Rtc.Begin();
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_DC);
  pinMode(CSSR_DATA_PIN, OUTPUT);
  pinMode(CSSR_CLOCK_PIN, OUTPUT);
  pinMode(CSSR_LATCH_PIN, OUTPUT);
  digitalWrite(CSSR_DATA_PIN, LOW);
  digitalWrite(CSSR_CLOCK_PIN, LOW);
  digitalWrite(CSSR_LATCH_PIN, LOW);
  shiftOut(CSSR_DATA_PIN, CSSR_CLOCK_PIN, MSBFIRST, 0);
  digitalWrite(CSSR_LATCH_PIN, HIGH);
  tft.init(TFT_WIDTH, TFT_HEIGHT);  // Init ST7789 240x135
  tft.setRotation(2);
  ledcSetup(0, 1000, 8);
  ledcAttachPin(TFT_ENABLE_PIN, 0);
  ledcWrite(0, bright);
  tft.cls();
  tft.println("LCD ST7789 135x240 OK");
  if (pixels.numPixels() != 0) tft.println("NeoPixel OK");
  else tft.println("NeoPixel FAIL");
  if (Rtc.IsDateTimeValid()) tft.println("RTC OK");
  else tft.println("RTC FAIL");
  buttMode.setType(LOW_PULL);
  buttPower.setType(LOW_PULL);
  buttLeft.setType(LOW_PULL);
  buttRight.setType(LOW_PULL);
}

void checkTime(byte m) {
  if (m == 0) {
    oldSec = -77;
    oldMin = -77;
    oldHour = -77;
  }
  if (oldSec != now.Second() && (m == 2 || m == 3)) {
    if (oldSec % 10 != now.Second() % 10) {
      if (m == 2) decode(6, now.Second() % 10);
    }
    if (oldSec / 10 != now.Second() / 10) {
      if (m == 2) decode(5, now.Second() / 10);
      if (m == 3) weatherOther(2, now.Second() / 10);
      //if (m == 1 && ((now.Minute() % 10) % 2 != 0)) weatherOther(6, now.Second() / 10);
    }
    oldSec = now.Second();
  }
  if (oldMin != now.Minute() && m == 2) {
    if (oldMin % 10 != now.Minute() % 10) {
      decode(4, now.Minute() % 10);
    }
    if (oldMin / 10 != now.Minute() / 10) {
      decode(3, now.Minute() / 10);
    }
    oldMin = now.Minute();
  } else if (oldMin != now.Minute() && m == 1) {
    if (oldMin % 10 != now.Minute() % 10) {
      decode(5, now.Minute() % 10);
    }
    if (oldMin / 10 != now.Minute() / 10) {
      decode(4, now.Minute() / 10);
    }
    oldMin = now.Minute();
  }
  if (oldHour != now.Hour() && m != 3) {
    if (oldHour % 10 != now.Hour() % 10) {
      decode(2, now.Hour() % 10);
    }
    if (oldHour / 10 != now.Hour() / 10) {
      decode(1, now.Hour() / 10);
    }
    oldHour = now.Hour();
  }
}

void checkButt() {
  buttMode.tick();
  buttPower.tick();
  buttLeft.tick();
  buttRight.tick();
  if (buttMode.isClick()) {
    mode = mode + 1;
    if (mode > maxMode) mode = 1;
    checkTime(0);
    if (mode == 1) {
      weatherPrint(6, -1);
    }
    if (mode == 3) {
      weatherPrint(1, -1);
      weatherPrint(2, 0);
      weatherPrint(3, 1);
      weatherPrint(4, 2);
      weatherPrint(5, 3);
      weatherPrint(6, 4);
    }
  }
  if (buttPower.isClick()) {
    powerOn = !powerOn;
    if (powerOn == true) {
      nigthOff(1);
    } else {
      nigthOff(0);
    }
  }
  if (buttPower.isStep(0)) {
    pixelBright = pixelBright + 5;
    if (pixelBright > 250) pixelBright = 5;
    pixels.setBrightness(pixelBright);
    colorWS(pixels.Color(RedColor, GreenColor, BlueColor));
  }
  if (buttMode.isStep(0)) {
    pixelOn = !pixelOn;
    if (pixelOn) {
      pixels.setBrightness(pixelBright);
      colorWS(pixels.Color(RedColor, GreenColor, BlueColor));
    } else {
      pixels.setBrightness(0);
      pixels.show();
    }
  }
  if (buttLeft.isStep(0) && mode == 2) {
    bright = bright - bright / 10;
    if (bright < 1) bright = 1;
    ledcWrite(0, bright);
  }
  if (buttRight.isStep(0) && mode == 2) {
    bright = bright + bright / 10;
    if (bright > 255) bright = 255;
    ledcWrite(0, bright);
  }
  if (buttRight.isClick() && mode == 1) {
    tindex = tindex + 1;
    if (tindex > max_readings) tindex = max_readings;
    weatherPrint(6, tindex);
  }
  if (buttLeft.isClick() && mode == 1) {
    tindex = tindex - 1;
    if (tindex < 1) tindex = 0;
    weatherPrint(6, tindex);
  }
}


void drawtext(int LCD, int x, int y, int size, char *text, uint16_t color, alignment align) {
  if (LCD == 0) {
    digitalWrite(CSSR_LATCH_PIN, LOW);
    shiftOut(CSSR_DATA_PIN, CSSR_CLOCK_PIN, MSBFIRST, 0);
    digitalWrite(CSSR_LATCH_PIN, HIGH);
  } else {
    byte disp = ~(0X1 << (LCD - 1));
    digitalWrite(CSSR_LATCH_PIN, LOW);
    shiftOut(CSSR_DATA_PIN, CSSR_CLOCK_PIN, MSBFIRST, disp);
    digitalWrite(CSSR_LATCH_PIN, HIGH);
  }
  int len = strlen(text);
  if (align == CENTER) {
    int dx = (135 - len * 16) / 2;
    tft.setCursor(dx, y);
  }
  if (align == LEFT) tft.setCursor(x, y);
  tft.setTextSize(size);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(text);
}


void tempOut(int LCD, float temp) {
  if (temp >= 0) imageOut(LCD, 0, 100, 31, 72, DPlus);
  else imageOut(LCD, 0, 100, 31, 72, DMin);
  int t = abs(temp);
  imageOut(LCD, 30, 100, 36, 72, digitSmall[t / 10]);
  imageOut(LCD, 66, 100, 36, 72, digitSmall[t % 10]);
  imageOut(LCD, 102, 100, 32, 72, DDoC);
}


void weatherOut(int LCD, int iconNum) {
  imageOut(LCD, 17, 0, 100, 100, iconWeather[iconNum]);
}


void phOut(int LCD, int i) {
  drawtext(LCD, 20, 180, 1, "H:", White, LEFT);
  int hr;
  if (i >= 0) {
    hr = round(WxForecast[i].Humidity);
  } else {
    hr = round(WxConditions[0].Humidity);
  }
  String h = String(hr);
  drawtext(LCD, 30, 180, 1, (char *)h.c_str(), White, LEFT);
  drawtext(LCD, 45, 180, 1, "%", White, LEFT);
  drawtext(LCD, 75, 180, 1, "P:", White, LEFT);
  int pr;
  if (i >= 0) {
    pr = round(WxForecast[i].Pressure);
  } else {
    pr = round(WxConditions[0].Pressure);
  }
  String p = String(pr);
  drawtext(LCD, 85, 180, 1, (char *)p.c_str(), White, LEFT);
  drawtext(LCD, 113, 180, 1, "kPa", White, LEFT);
}

void getWeater() {
  byte Attempts = 1;
  bool RxWeather = false;
  bool RxForecast = false;
  WiFiClient client;                                                      // wifi client object
  while ((RxWeather == false || RxForecast == false) && Attempts <= 2) {  // Try up-to 2 time for Weather and Forecast data
    if (RxWeather == false) RxWeather = obtainWeatherData(client, "weather");
    if (RxForecast == false) RxForecast = obtainWeatherData(client, "forecast");
    Attempts++;
  }
}

bool obtainWeatherData(WiFiClient &client, const String &RequestType) {
  const String units = (Units == "M" ? "metric" : "imperial");
  client.stop();  // close connection before sending a new request
  HTTPClient http;
  String uri = "/data/2.5/" + RequestType + "?q=" + City + "," + Country + "&APPID=" + apikey + "&mode=json&units=" + units + "&lang=" + Language;
  if (RequestType != "weather") {
    uri += "&cnt=" + String(max_readings);
  }
  http.begin(client, server, 80, uri);  //http.begin(uri,test_root_ca); //HTTPS example connection
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    if (!DecodeWeather(http.getStream(), RequestType)) return false;
    client.stop();
    http.end();
    return true;
  } else {
    Serial.printf("connection failed, error: %s", http.errorToString(httpCode).c_str());
    client.stop();
    http.end();
    return false;
  }
  http.end();
  return true;
}

bool DecodeWeather(WiFiClient &json, String Type) {
  Serial.print(F("\nCreating object...and "));
  DynamicJsonDocument doc(64 * 1024);                       // allocate the JsonDocument
  DeserializationError error = deserializeJson(doc, json);  // Deserialize the JSON document
  if (error) {                                              // Test if parsing succeeds.
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return false;
  }
  // convert it to a JsonObject
  JsonObject root = doc.as<JsonObject>();
  Serial.println(" Decoding " + Type + " data");
  if (Type == "weather") {
    // All Serial.println statements are for diagnostic purposes and some are not required, remove if not needed with //
    //WxConditions[0].lon         = root["coord"]["lon"].as<float>();              Serial.println(" Lon: " + String(WxConditions[0].lon));
    //WxConditions[0].lat         = root["coord"]["lat"].as<float>();              Serial.println(" Lat: " + String(WxConditions[0].lat));
    WxConditions[0].Main0 = root["weather"][0]["main"].as<char *>();
    //Serial.println("Main: " + String(WxConditions[0].Main0));
    WxConditions[0].Forecast0 = root["weather"][0]["description"].as<char *>();
    //Serial.println("For0: " + String(WxConditions[0].Forecast0));
    //WxConditions[0].Forecast1   = root["weather"][1]["description"].as<char*>(); Serial.println("For1: " + String(WxConditions[0].Forecast1));
    //WxConditions[0].Forecast2   = root["weather"][2]["description"].as<char*>(); Serial.println("For2: " + String(WxConditions[0].Forecast2));
    WxConditions[0].Icon = root["weather"][0]["icon"].as<char *>();
    //Serial.println("Icon: " + String(WxConditions[0].Icon));
    WxConditions[0].Temperature = root["main"]["temp"].as<float>();
    //Serial.println("Temp: " + String(WxConditions[0].Temperature));
    WxConditions[0].Pressure = root["main"]["pressure"].as<float>();
    //Serial.println("Pres: " + String(WxConditions[0].Pressure));
    WxConditions[0].Humidity = root["main"]["humidity"].as<float>();
    //Serial.println("Humi: " + String(WxConditions[0].Humidity));
    WxConditions[0].Low = root["main"]["temp_min"].as<float>();
    //Serial.println("TLow: " + String(WxConditions[0].Low));
    WxConditions[0].High = root["main"]["temp_max"].as<float>();
    //Serial.println("THig: " + String(WxConditions[0].High));
    WxConditions[0].Windspeed = root["wind"]["speed"].as<float>();
    //Serial.println("WSpd: " + String(WxConditions[0].Windspeed));
    WxConditions[0].Winddir = root["wind"]["deg"].as<float>();
    //Serial.println("WDir: " + String(WxConditions[0].Winddir));
    WxConditions[0].Cloudcover = root["clouds"]["all"].as<int>();
    //Serial.println("CCov: " + String(WxConditions[0].Cloudcover));  // in % of cloud cover
    WxConditions[0].Visibility = root["visibility"].as<int>();
    //Serial.println("Visi: " + String(WxConditions[0].Visibility));  // in metres
    WxConditions[0].Rainfall = root["rain"]["1h"].as<float>();
    //Serial.println("Rain: " + String(WxConditions[0].Rainfall));
    WxConditions[0].Snowfall = root["snow"]["1h"].as<float>();
    //Serial.println("Snow: " + String(WxConditions[0].Snowfall));
    //WxConditions[0].Country     = root["sys"]["country"].as<char*>();            Serial.println("Ctry: " + String(WxConditions[0].Country));
    WxConditions[0].Sunrise = root["sys"]["sunrise"].as<int>();
    //Serial.println("SRis: " + String(WxConditions[0].Sunrise));
    WxConditions[0].Sunset = root["sys"]["sunset"].as<int>();
    //Serial.println("SSet: " + String(WxConditions[0].Sunset));
    WxConditions[0].Timezone = root["timezone"].as<int>();
    //Serial.println("TZon: " + String(WxConditions[0].Timezone));
  }
  if (Type == "forecast") {
    //Serial.println(json);
    Serial.print(F("\nReceiving Forecast period - "));  //------------------------------------------------
    JsonArray list = root["list"];
    for (byte r = 0; r < max_readings; r++) {
      Serial.println("\nPeriod-" + String(r) + "--------------");
      WxForecast[r].Dt = list[r]["dt"].as<int>();
      WxForecast[r].Temperature = list[r]["main"]["temp"].as<float>();
      //Serial.println("Temp: " + String(WxForecast[r].Temperature));
      WxForecast[r].Low = list[r]["main"]["temp_min"].as<float>();
      //Serial.println("TLow: " + String(WxForecast[r].Low));
      WxForecast[r].High = list[r]["main"]["temp_max"].as<float>();
      //Serial.println("THig: " + String(WxForecast[r].High));
      WxForecast[r].Pressure = list[r]["main"]["pressure"].as<float>();
      //Serial.println("Pres: " + String(WxForecast[r].Pressure));
      WxForecast[r].Humidity = list[r]["main"]["humidity"].as<float>();
      //Serial.println("Humi: " + String(WxForecast[r].Humidity));
      //WxForecast[r].Forecast0         = list[r]["weather"][0]["main"].as<char*>();        Serial.println("For0: " + String(WxForecast[r].Forecast0));
      //WxForecast[r].Forecast1         = list[r]["weather"][1]["main"].as<char*>();        Serial.println("For1: " + String(WxForecast[r].Forecast1));
      //WxForecast[r].Forecast2         = list[r]["weather"][2]["main"].as<char*>();        Serial.println("For2: " + String(WxForecast[r].Forecast2));
      WxForecast[r].Icon = list[r]["weather"][0]["icon"].as<char *>();
      //Serial.println("Icon: " + String(WxForecast[r].Icon));
      //WxForecast[r].Description       = list[r]["weather"][0]["description"].as<char*>(); Serial.println("Desc: " + String(WxForecast[r].Description));
      //WxForecast[r].Cloudcover        = list[r]["clouds"]["all"].as<int>();               Serial.println("CCov: " + String(WxForecast[r].Cloudcover)); // in % of cloud cover
      //WxForecast[r].Windspeed         = list[r]["wind"]["speed"].as<float>();             Serial.println("WSpd: " + String(WxForecast[r].Windspeed));
      //WxForecast[r].Winddir           = list[r]["wind"]["deg"].as<float>();               Serial.println("WDir: " + String(WxForecast[r].Winddir));
      WxForecast[r].Rainfall = list[r]["rain"]["3h"].as<float>();
      //Serial.println("Rain: " + String(WxForecast[r].Rainfall));
      WxForecast[r].Snowfall = list[r]["snow"]["3h"].as<float>();
      //Serial.println("Snow: " + String(WxForecast[r].Snowfall));
      WxForecast[r].Period = list[r]["dt_txt"].as<char *>();
      //Serial.println("Peri: " + String(WxForecast[r].Period));
    }
    //------------------------------------------
    float pressure_trend = WxForecast[0].Pressure - WxForecast[2].Pressure;  // Measure pressure slope between ~now and later
    pressure_trend = ((int)(pressure_trend * 10)) / 10.0;                    // Remove any small variations less than 0.1
    WxConditions[0].Trend = "=";
    if (pressure_trend > 0) WxConditions[0].Trend = "+";
    if (pressure_trend < 0) WxConditions[0].Trend = "-";
    if (pressure_trend == 0) WxConditions[0].Trend = "0";

    //if (Units == "I") Convert_Readings_to_Imperial();
  }
  String sRT = String(ConvertUnixTime(WxConditions[0].Sunrise + WxConditions[0].Timezone).substring(0, 2));
  String sST = String(ConvertUnixTime(WxConditions[0].Sunset + WxConditions[0].Timezone).substring(0, 2));
  sunR = sRT.toInt();
  sunS = sST.toInt();
  return true;
}

byte DisplayConditionsSection(String IconName) {
  byte IconNum = 1;
  //Serial.println("Icon name: " + IconName);
  if (IconName == "01d") IconNum = 1;
  else if (IconName == "01n") IconNum = 11;
  else if (IconName == "02d") IconNum = 2;
  else if (IconName == "02n") IconNum = 12;  //MostlySunny(x, y, IconSize, IconName);
  else if (IconName == "03d") IconNum = 4;
  else if (IconName == "03n") IconNum = 14;  // Cloudy(x, y, IconSize, IconName);
  else if (IconName == "04d") IconNum = 3;
  else if (IconName == "04n") IconNum = 13;  // MostlySunny(x, y, IconSize, IconName);
  else if (IconName == "09d") IconNum = 6;
  else if (IconName == "09n") IconNum = 16;  // ChanceRain(x, y, IconSize, IconName);
  else if (IconName == "10d") IconNum = 6;
  else if (IconName == "10n") IconNum = 16;  // Rain(x, y, IconSize, IconName);
  else if (IconName == "11d") IconNum = 5;
  else if (IconName == "11n") IconNum = 15;  // Tstorms(x, y, IconSize, IconName);
  else if (IconName == "13d") IconNum = 7;
  else if (IconName == "13n") IconNum = 17;  // Snow(x, y, IconSize, IconName);
  else if (IconName == "50d") IconNum = 20;  /// Haze(x, y, IconSize, IconName);
  else if (IconName == "50n") IconNum = 20;  // Fog(x, y, IconSize, IconName);
  else IconNum = 1;                          // Nodata(x, y, IconSize, IconName);
  return IconNum;
}

String ConvertUnixTime(int unix_time) {
  // Returns either '21:12  ' or ' 09:12pm' depending on Units mode
  time_t tm = unix_time;
  struct tm *now_tm = localtime(&tm);
  char output[40];
  if (Units == "M") {
    strftime(output, sizeof(output), "%H:%M %d/%m/%y", now_tm);
  } else {
    strftime(output, sizeof(output), "%I:%M%P %m/%d/%y", now_tm);
  }
  return output;
}

void sleepTime() {
  if (ntp.hour() > sunR && ntp.hour() < sunS && ntp.second() == 0) {
    if (powerOn && !nightSleep) ledcWrite(0, bright);
  }
  if (ntp.hour() <= sunR || ntp.hour() >= sunS && ntp.second() == 0) {
    if (powerOn && !nightSleep) ledcWrite(0, 1);
  }
  if (NTOn < NTOff) {
    if ((ntp.hour() <= NTOn || ntp.hour() >= NTOff && ntp.second() == 0) && NOFF == 1) {
      nightSleep = true;
      nigthOff(0);
    } else nightSleep = false;
  } else {
    if ((ntp.hour() <= NTOn && ntp.hour() >= NTOff && ntp.second() == 0) && NOFF == 1) {
      nightSleep = true;
      nigthOff(0);
    } else nightSleep = false;
  }
}

void nigthOff(int nm) {
  if (nm != 0) {
    ledcWrite(0, bright);
    pixels.setBrightness(pixelBright);
    colorWS(pixels.Color(RedColor, GreenColor, BlueColor));
  } else {
    ledcWrite(0, 0);
    pixels.setBrightness(0);
    pixels.show();
  }
}

void weatherPrint(int LCD, int indexT) {
  byte iconToPrint;
  imageOut(LCD, 0, 0, 135, 240, FON);
  if (indexT < 0) iconToPrint = DisplayConditionsSection(WxConditions[0].Icon);
  else iconToPrint = DisplayConditionsSection(WxForecast[indexT].Icon);
  if (indexT < 0) tempOut(LCD, WxConditions[0].Temperature);
  else tempOut(LCD, WxForecast[indexT].Temperature);
  weatherOut(LCD, iconToPrint);
  phOut(LCD, indexT);
  String myTime;
  if (indexT < 0) myTime = ntp.timeString().substring(0, 5);
  else myTime = String(ConvertUnixTime(WxForecast[indexT].Dt + WxConditions[0].Timezone).substring(0, 5));
  drawtext(LCD, 40, 200, 2, (char *)myTime.c_str(), White, LEFT);
}

void weatherOther(int LCD, int stage) {
  String tmp;
  if (stage == 0) {
    imageOut(LCD, 0, 0, 135, 240, TEMP);
    drawtext(LCD, 0, 10, 3, (char *)String(WxConditions[0].High).c_str(), Red, CENTER);
    drawtext(LCD, 0, 210, 3, (char *)String(WxConditions[0].Low).c_str(), White, CENTER);
  }
  if (stage == 1) {
    imageOut(LCD, 0, 0, 135, 240, WIND);
    drawtext(LCD, 0, 85, 3, (char *)String(WxConditions[0].Windspeed).c_str(), White, CENTER);
    drawtext(LCD, 0, 160, 3, (char *)String(WxConditions[0].Winddir).c_str(), White, CENTER);
  }
  if (stage == 2) {
    imageOut(LCD, 0, 0, 135, 240, CLOUD);
    drawtext(LCD, 1, 220, 2, "CLOUD COVER", White, LEFT);
    drawtext(LCD, 0, 110, 3, (char *)String(WxConditions[0].Cloudcover).c_str(), White, CENTER);
  }
  if (stage == 3) {
    imageOut(LCD, 0, 0, 135, 240, FOG);
    drawtext(LCD, 7, 220, 2, "VISIBILITY", White, LEFT);
    drawtext(LCD, 0, 120, 3, (char *)String(WxConditions[0].Visibility).c_str(), White, CENTER);
  }
  if (stage == 4) {
    bool Sammer;
    if (WxConditions[0].Rainfall > WxConditions[0].Snowfall) Sammer = true;
    if (Sammer) {
      imageOut(LCD, 0, 0, 135, 240, RAIN);
      drawtext(LCD, 23, 220, 2, "RAINFALL", White, LEFT);
      drawtext(LCD, 0, 130, 3, (char *)String(WxConditions[0].Rainfall).c_str(), White, CENTER);
    } else {
      imageOut(LCD, 0, 0, 135, 240, RAIN);
      drawtext(LCD, 23, 220, 2, "SNOWFALL", White, LEFT);
      drawtext(LCD, 0, 120, 3, (char *)String(WxConditions[0].Snowfall).c_str(), White, CENTER);
    }
  }
  if (stage == 5) {
    imageOut(LCD, 0, 0, 135, 240, SUN);
    //drawtext(2, 25, 40, 2, "SUNRISE", BLUE, LEFT);
    drawtext(LCD, 0, 80, 3, (char *)String(ConvertUnixTime(WxConditions[0].Sunrise + WxConditions[0].Timezone).substring(0, 5)).c_str(), White, CENTER);
    //drawtext(2, 30, 120, 2, "SUNSET", BLUE, LEFT);
    drawtext(LCD, 0, 155, 3, (char *)String(ConvertUnixTime(WxConditions[0].Sunset + WxConditions[0].Timezone).substring(0, 5)).c_str(), White, CENTER);
  }
}

void commandStirng() {
  if (str.indexOf("help") >= 0) {
    help();
    Serial.println("> Ok");
  } else if (str.indexOf("RGB?") >= 0) {
    Serial.println("> R=" + String(RedColor) + " G=" + String(GreenColor) + " B=" + String(BlueColor));
    Serial.println("> Ok");
  } else if (str.indexOf("RGB=") >= 0) {
    byte f = str.indexOf(",");
    byte s = str.indexOf(",", f + 1);
    RedColor = str.substring(str.indexOf("RGB=") + 4, f).toInt();
    GreenColor = str.substring(f + 1, s).toInt();
    BlueColor = str.substring(s + 1, str.length()).toInt();
    if (RedColor >= 0 && RedColor <= 255) EEPROM.put(1, RedColor);        //Red
    if (GreenColor >= 0 && GreenColor <= 255) EEPROM.put(2, GreenColor);  //Green
    if (BlueColor >= 0 && BlueColor <= 255) EEPROM.put(3, BlueColor);     //Blue
    EEPROM.commit();
    delay(200);
    Serial.println("R=" + String(RedColor) + " G=" + String(GreenColor) + " B=" + String(BlueColor));
    Serial.println("> Ok");
  } else if (str.indexOf("bLCD?") >= 0) {
    Serial.println("> LCD Brightness = " + String(int(bright)));
    Serial.println("> Ok");
  } else if (str.indexOf("bLCD=") >= 0) {
    bright = str.substring(str.indexOf("bLCD=") + 5, str.length()).toInt();
    if (bright >= 0 && bright <= 255) EEPROM.put(5, bright);
    EEPROM.commit();
    delay(200);
    Serial.println("> LCD Brightness = " + String(int(bright)));
    Serial.println("> Ok");
  } else if (str.indexOf("bLED?") >= 0) {
    Serial.println("> LED Brightness = " + String(pixelBright));
    Serial.println("> Ok");
  } else if (str.indexOf("bLED=") >= 0) {
    pixelBright = str.substring(str.indexOf("bLED=") + 5, str.length()).toInt();
    if (pixelBright >= 0 && pixelBright <= 255) EEPROM.put(4, pixelBright);
    EEPROM.commit();
    delay(200);
    Serial.println("> LED Brightness = " + String(pixelBright));
    Serial.println("> Ok");
  } else if (str.indexOf("NOFF?") >= 0) {
    Serial.println("> Night Auto OFF = " + String(NOFF));
    Serial.println("> OFF Time = " + String(NTOff));
    Serial.println("> ON Time = " + String(NTOn));
    Serial.println("> Ok");
  } else if (str.indexOf("NOFF=") >= 0) {
    NOFF = str.substring(str.indexOf("NOFF=") + 5, str.length()).toInt();
    if (NOFF >= 0 && NOFF <= 255) EEPROM.put(9, NOFF);
    EEPROM.commit();
    delay(200);
    Serial.println("> Night Auto OFF = " + String(NOFF));
    Serial.println("> Ok");
  } else if (str.indexOf("NTOff=") >= 0) {
    NTOff = str.substring(str.indexOf("NTOff=") + 6, str.length()).toInt();
    if (NTOff >= 0 && NTOff <= 23) EEPROM.put(10, NTOff);
    EEPROM.commit();
    delay(200);
    Serial.println("> OFF Time = " + String(NTOff));
    Serial.println("> Ok");
  } else if (str.indexOf("NTOn=") >= 0) {
    NTOn = str.substring(str.indexOf("NTOn=") + 5, str.length()).toInt();
    if (NTOn >= 0 && NTOn <= 23) EEPROM.put(11, NTOn);
    EEPROM.commit();
    delay(200);
    Serial.println("> ON Time = " + String(NTOn));
    Serial.println("> Ok");
  } else Serial.println("> Bad command or operator!");
  newCommand = false;
  str = "";
}

void help() {
  Serial.println("===================================================");
  Serial.println("   help ,RGB? ,RGB=, bLCD?, bLСD=, bLED?, bLED=");
  Serial.println("           NOFF?, NOFF=, NTOff=, NTOn=");
  Serial.println("help - справка");
  Serial.println("RGB? - установленное значение цвета светодиодов");
  Serial.println("RGB= - установить значение для цвета светодиодов");
  Serial.println("RGB=255,200,0 - желтый цвет");
  Serial.println("bLCD? - установленное значение яркости дисплея");
  Serial.println("bLCD= - установить значение яркости дисплея");
  Serial.println("bLED? - установленное значение яркости WS диодов");
  Serial.println("bLED= - установить значение яркости WS диодов");
  Serial.println("NOFF? - статус авто выключения на ночь");
  Serial.println("NOFF= - установить режим авто выключения на ночь");
  Serial.println("NTOff= - установить время выключения подсветки");
  Serial.println("NTOn= - установить время включения подсветки");
  Serial.println("===================================================");
}
