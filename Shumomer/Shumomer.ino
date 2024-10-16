.#include <driver/adc.h>
#include <WiFi.h>
#include "GyverTimer.h"
#include "arduinoFFT.h"
#include "LiquidCrystal_I2C.h"
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>

const char* SERVER_NAME = "...";
const char* SSID = "...";
const char* WIFI_PASSWORD = "...";
const char * pass = "...";

String SHOCK_SERIAL = "04";
String SHOCK_NAME = "SHOK-MN(" + SHOCK_SERIAL + ")";
String Akey = "Shum" + SHOCK_SERIAL;

const uint32_t INTERVAL_WORK = 500;
const uint32_t INTERVAL_SERVER = 60000;

WiFiManager WiFiManager;
GTimer workTimer(MS);
GTimer serverTimer(MS);

arduinoFFT Fft = arduinoFFT();
const int analog = 36;
const int NumberElements = 4096;
const int kolVoMax = 20;
const int sampleWindow = 50;
int sample;
int kolvo;
int count, cnt, counter, cntr;
int i, j, n;
int peakToPeak;
int signalMax;
int signalMin;
double masMax[kolVoMax];
double masCur[kolVoMax];
double masMaxNew[kolVoMax];
double masMid[kolVoMax];
double masMin[kolVoMax];
double Mas[NumberElements];
double Tmvl[NumberElements];
double Mmax;
double curMMax, curMMin;
double dBMax;
double dBMid;
double dBMin;
double dBHz[kolVoMax];
double volts;
double dBPrim;
double dBFinal;
double lastMax = -1;
long startMillis;

uint8_t graf0[8] = { B00000, B00000, B00000, B00000, B00000, B00000, B00000, B11111 };
uint8_t graf1[8] = { B00000, B00000, B00000, B00000, B00000, B00000, B11111, B11111 };
uint8_t graf2[8] = { B00000, B00000, B00000, B00000, B00000, B11111, B11111, B11111 };
uint8_t graf3[8] = { B00000, B00000, B00000, B00000, B11111, B11111, B11111, B11111 };
uint8_t graf4[8] = { B00000, B00000, B00000, B11111, B11111, B11111, B11111, B11111 };
uint8_t graf5[8] = { B00000, B00000, B11111, B11111, B11111, B11111, B11111, B11111 };
uint8_t graf6[8] = { B00000, B11111, B11111, B11111, B11111, B11111, B11111, B11111 };
uint8_t graf7[8] = { 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F };

#if defined(ARDUINO) && ARDUINO >= 100
#define printByte(args) write(args);
#else
#define printByte(args) print(args, BYTE);
#endif
LiquidCrystal_I2C lcd(0x27, 20, 4);

#ifdef __cplusplus

extern "C" {

#endif

  uint8_t temprature_sens_read();

#ifdef __cplusplus
}

#endif

uint8_t temprature_sens_read();

void setup() {
  Serial.begin(115200);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_3,ADC_ATTEN_DB_11);

    dBMin = 9999;
    dBMax = -9999;
    dBMid = 0;
    for (i = 0; i < 20; i++){
    masMax[i] = 0;
    masMid[i] = 0;
    masMin[i] = 999999;
    }
    counter = 0;
    cntr = 0;

  lcd.init();
  lcd.backlight();
  lcd.createChar(0, graf0);
  lcd.createChar(1, graf1);
  lcd.createChar(2, graf2);
  lcd.createChar(3, graf3);
  lcd.createChar(4, graf4);
  lcd.createChar(5, graf5);
  lcd.createChar(6, graf6);
  lcd.createChar(7, graf7);
  delay(500);
  lcd.clear();

  lcd.print(SHOCK_NAME);
  lcd.setCursor(0, 2);
  lcd.print("Pass: SHOK-MN");
  lcd.setCursor(0, 3);
  lcd.print("IP:192.168.4.1");

  workTimer.setTimeout(INTERVAL_WORK);
  serverTimer.setTimeout(INTERVAL_SERVER);

  WiFi.mode(WIFI_STA);

  char ssid[20];
  SHOCK_NAME.toCharArray(ssid, 20);
  WiFiManager.autoConnect(ssid, pass);
  lcd.clear();
  delay(100);
  if (WiFi.status() == WL_CONNECTED) {
    lcd.print(WiFi.SSID());
    lcd.setCursor(0, 2);
    lcd.print("IP:");
    lcd.print(WiFi.localIP());
    Serial.println(WiFi.localIP());

    ArduinoOTA.setPassword("42");

    ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else
          type = "filesystem";

        Serial.println("Start updating " + type);
      })
      .onEnd([]() {
        Serial.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
      });
      
    ArduinoOTA.begin();

  } else {
    lcd.print("Wi-Fi OFF");
    Serial.println("Wi-Fi OFF");
  }

  delay(1500);
  lcd.clear();
  Serial.println("Starting timers...");
  serverTimer.start();
  Serial.println("Booting done");
}

int sendToServer(double* values, double* mins, double* mids, double dB_maxs, double dB_mins, double dB_mids, int valuesAmount) {
  Serial.println("-> server");
  String ranges[21] = { "0", "12", "28", "109", "216", "427", "549", "671", "854", "1007", "1098", "1190", "1281", "1403", "1495", "1586", "1708", "1800", "1891", "1982", "2000" };
  HTTPClient http;
  String jsonSystem, jsonSound, requestBody;
  http.begin(SERVER_NAME);
  http.addHeader("Content-Type", "application/json");
  StaticJsonDocument<4000> doc;
  doc["Serial"] = SHOCK_SERIAL;
  doc["Akey"] = Akey;
  doc["MAC"] = WiFi.macAddress();
  doc["IP"] = WiFi.localIP().toString();
  doc["RSSI"] = String(WiFi.RSSI());
  serializeJson(doc, jsonSystem);
  doc.clear();
  for (int idk = 0; idk < valuesAmount; ++idk) {
    doc["Range(" + ranges[idk] + "-" + ranges[idk + 1] + ")_max"] = String(values[idk]);
    doc["Range(" + ranges[idk] + "-" + ranges[idk + 1] + ")_avg"] = String(mids[idk]);
    doc["Range(" + ranges[idk] + "-" + ranges[idk + 1] + ")_min"] = String(mins[idk]);
  }
  doc["dBMax"] = String(dB_maxs);
  doc["dBMid"] = String(dB_mids);
  doc["dBMin"] = String(dB_mins);
  serializeJson(doc, jsonSound);
  requestBody = "{\"system\": " + jsonSystem;
  jsonSound[0] = ',';
  requestBody = requestBody + jsonSound;
  Serial.println(requestBody);
  int httpResponseCode = http.POST(requestBody);
  Serial.println(httpResponseCode);
  return httpResponseCode;
}

double maxx(double mas[], int k, int count) {
  double maxx = mas[0];
  for (i = 1; i < count; i++) {
    if (maxx < mas[i]) {
      maxx = mas[i];
    }
  }
  if (maxx == infinity()) {
    return 9999.9;
  }
  return maxx;
}

double minn(double mas[], int count) {
  double minn = mas[0];
  for (i = 1; i < count; i++) {
    if (minn > mas[i]) {
      minn = mas[i];
    }
  }
  return minn;
}

void print_chart(double x[], int amountOfNums, bool startOnMin = true) {
  
  double maxim = 0, minim = 0;

  minim = minn(x, amountOfNums);
  if (startOnMin) {
    for (i = 0; i < amountOfNums; ++i) {
      x[i] -= minim;
    }
  }
  maxim = maxx(x, 0, amountOfNums);
  if (lastMax > maxim) {
    maxim = maxim + (lastMax - maxim) / 2;
  }
  lastMax = maxim;
  for (j = 0; j < amountOfNums; ++j) {
    x[j] /= maxim;
    printCol(x[j], j);
  }
}

void printCol(double value, int colNum) {
  int curCursorRow = 3;
  int cFullRows = value / 0.25;
  value -= cFullRows * 0.25;
  int charNum = value / 0.03125 - 1;
  for (i = 0; i < cFullRows; ++i) {
    lcd.setCursor(colNum, curCursorRow--);
    lcd.printByte(7);
  }
  if (curCursorRow < 0) return;
  lcd.setCursor(colNum, curCursorRow);
  if (charNum >= 0) {
    lcd.printByte(charNum)
  }
}

void loop() {

  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
  }

  if(counter % 60 == 0){
    dBMin = 9999;
    dBMax = -9999;
    dBMid = 0;
    for (i = 0; i < 20; i++){
    masMax[i] = 0;
    masMid[i] = 0;
    masMin[i] = 999999;
    }
    counter = 0;
  }

   startMillis = millis();
   peakToPeak = 0;
   signalMax = 0;
   signalMin = 4095;

   while (millis() - startMillis < sampleWindow)
   {
      sample = adc1_get_raw(ADC1_CHANNEL_3);
      if (sample < 4095)
      {
         if (sample > signalMax)
         {
            signalMax = sample;
         }
         else if (sample < signalMin)
         {
            signalMin = sample;
         }
      }
   }

   peakToPeak = signalMax - signalMin;
   volts = ((peakToPeak * 3.3) / 4095) * 0.707;
   dBPrim = log10(volts/0.00631)*20;
   dBFinal = dBPrim + 94 - 44 - 25;

    if(dBFinal > dBMax){
    dBMax = dBFinal;
    }

    if(dBFinal < dBMin){
    dBMin = dBFinal;
    }

    dBMid += dBFinal;
    counter++;

  count = 0; Mmax = 0; cnt = 0;

  while (!workTimer.isReady()) {
    delay(50);
  }

  workTimer.start();
  while (count <= NumberElements) {
    Mas[count] = analogRead(analog);
    count++;
  }

  delay(50);

  while (cnt < NumberElements) {
    Tmvl[cnt] = 0.0;
    cnt++;
  }

  cnt = 0; i = 0; kolvo = 0;

  Fft.Compute(Mas, Tmvl, NumberElements, FFT_FORWARD);

  while (cnt < 20) {
    dBHz[cnt] = 0;
    masCur[cnt] = 0;
    cnt++;
  }

  int stages[19] = {12, 28, 109, 216, 427, 549, 671, 854, 1007, 1098, 1190, 1281, 1403, 1495, 1586, 1708, 1800, 1891, 1982};
  cnt = 0;
  int lasti = 0;
  for (i = 0; i < NumberElements / 2; i++) {
    if (cnt < 20) {
      if (masCur[cnt] < Tmvl[i]) {
        masCur[cnt] = Tmvl[i];
      }
      if (i == stages[cnt]) {
        cnt++;
      }
    }
  }

  for (i = 0; i < 20; i++){

    if(masCur[i] > masMax[i]){
    masMax[i] = masCur[i];
    }

    if(masCur[i] < masMin[i]){
    masMin[i] = masCur[i];
    }

    masMid[i] += masCur[i];
  }

  lcd.clear();

  for (int i = 0; i < 20; i++) {
    masMaxNew[i] = masCur[i];
  }

  print_chart(masMaxNew, 20, true);

  if (WiFi.status() == WL_CONNECTED && serverTimer.isReady()) {
    dBMid = dBMid / counter;
    for (i = 0; i < 20; i++){
    masMid[i] = masMid[i] / counter;
    masMax[i] = (log10(masMax[i])*20)-20;
    masMin[i] = (log10(masMin[i])*20)-20;
    masMid[i] = (log10(masMid[i])*20)-20;
    }
    int result = sendToServer(masMax, masMin, masMid, dBMax, dBMin, dBMid, 20);
    serverTimer.start();
    lcd.setCursor(0, 1);
    lcd.print(result);
    delay(2000);
    lcd.clear();
  }
}
