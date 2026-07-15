/*
UNIVERSAL SMART GATEWAY (USG) Version : 1.6 Icons + Graph
Developer : Ndegwa Chidoti
Hardware : Heltec WiFi LoRa 32 V3
Board: "Heltec WiFi LoRa 32(V3)" in Arduino IDE
Libs: ESP32 by Espressif, SSD1306 by Daniel Eichhorn
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "SSD1306Wire.h"
#include <WiFiUdp.h>
#include <NTPClient.h>
//=========================================================
// OLED
//=========================================================
SSD1306Wire display(0x3C, 17, 18); // SDA=17, SCL=18 for Heltec V3

//=========================================================
// OLED ICONS - 16x16 XBM bitmaps in PROGMEM
//=========================================================
const unsigned char icon_wifi [] PROGMEM = {
 0x00,0x00,0x01,0x80,0x03,0xc0,0x07,0xe0,0x0f,0xf0,0x1f,0xf8,0x3f,0xfc,0x7f,0xfe,
 0xff,0xff,0x7f,0xfe,0x3f,0xfc,0x1f,0xf8,0x0f,0xf0,0x07,0xe0,0x03,0xc0,0x00,0x00
};

const unsigned char icon_pump [] PROGMEM = {
 0x00,0x00,0x18,0x18,0x18,0x18,0x18,0x18,0x3c,0x3c,0x7e,0x7e,0xff,0xff,0xff,0xff,
 0xff,0xff,0x7e,0x7e,0x3c,0x3c,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00
};

const unsigned char icon_soil [] PROGMEM = {
 0x00,0x00,0x00,0x00,0x01,0x80,0x03,0xc0,0x07,0xe0,0x0f,0xf0,0x1f,0xf8,0x3f,0xfc,
 0x7f,0xfe,0xff,0xff,0xff,0xff,0x7f,0xfe,0x3f,0xfc,0x1f,0xf8,0x00,0x00,0x00,0x00
};

const unsigned char icon_auto [] PROGMEM = {
 0x00,0x00,0x10,0x08,0x28,0x14,0x44,0x22,0x82,0x41,0x82,0x41,0x82,0x41,0x44,0x22,
 0x28,0x14,0x10,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

const unsigned char icon_manual [] PROGMEM = {
 0x00,0x00,0x3e,0x7c,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,0x40,0x02,
 0x40,0x02,0x3e,0x7c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

//=========================================================
// PINS
//=========================================================
#define OLED_RST 21
#define VEXT_PIN 36

// Relay Outputs
#define RELAY_PIN 26      // Water Pump Relay
#define LAMP_PIN 47
// Sensor Input
#define SOIL_PIN 4

//=========================================================
// WIFI + LOGIN SETTINGS - CHANGE THESE
//=========================================================
const char* apSSID = "Universal_SmartBooster";
const char* apPASS = "12345678";
const char* staSSID = "YOUR_HOME_WIFI_NAME";
const char* staPASS = "YOUR_HOME_WIFI_PASSWORD";

const char* webUsername = "admin"; // <-- CHANGE LOGIN
const char* webPassword = "1234"; // <-- CHANGE PASSWORD

WebServer server(80);

//=========================================================
// RTC NTP - EAT = UTC+3
//=========================================================
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800, 60000);
String currentTime = "--:--";
String currentDate = "--/--/----";
IPAddress staIP;

//=========================================================
// BILLBOARD
//=========================================================
String ads[5] = {"WELCOME","SMART FARM","BUY TODAY","IRRIGATION ACTIVE","THANK YOU"};
const int TOTAL_ADS = 5;
int currentAd = 0;
int scrollX = 128;

//=========================================================
// TIMERS
//=========================================================
unsigned long previousAd = 0;
unsigned long previousPage = 0;
unsigned long previousRTC = 0;
unsigned long lastGraphSample = 0;
unsigned long previousScroll = 0;
const unsigned long scrollInterval = 60;   // milliseconds
//=========================================================
// OLED PAGES
//=========================================================
enum Pages { PAGE_HOME, PAGE_BILLBOARD, PAGE_IRRIGATION, PAGE_WIFI, PAGE_ABOUT };
byte currentPage = PAGE_HOME;

//=========================================================
// SYSTEM VARIABLES + GRAPH BUFFER
//=========================================================
bool autoMode = true;
bool pumpState = false;
bool lampState = false;
int soilRaw = 0;
int soilPercent = 0;
const int DRY_VALUE = 3200; // Calibrate: sensor in dry air
const int WET_VALUE = 1200; // Calibrate: sensor in water
const unsigned long adInterval = 12000;   // 12 seconds

#define GRAPH_SIZE 20
int soilHistory[GRAPH_SIZE] = {0};
byte historyIndex = 0;

//=========================================================
// FUNCTION PROTOTYPES - ALL OF THEM
//=========================================================
void initOLED();
void bootAnimation();
void setupWiFi();
void updateRTC();
bool checkAuth();
void handleRoot();
void handlePumpOn();
void handlePumpOff();
void handleLampOn();
void handleLampOff();
void handleUpdateAds();
void handleModeToggle();
void drawHome();
void drawBillboard();
void drawIrrigation();
void drawWiFi();
void drawAbout();
void drawIcon(int x, int y, const unsigned char *bitmap);
void handleDisplay();
void handleDashboard();
//=========================================================
// SETUP
//=========================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(RELAY_PIN,OUTPUT);
  digitalWrite(RELAY_PIN,LOW);
pinMode(LAMP_PIN, OUTPUT);
digitalWrite(LAMP_PIN, LOW);
  initOLED();
  bootAnimation();
  setupWiFi();

  timeClient.begin();
  updateRTC();

  server.on("/", handleRoot);
  server.on("/pumpOn", handlePumpOn);
  server.on("/pumpOff", handlePumpOff);
server.on("/lampOn", handleLampOn);
server.on("/lampOff", handleLampOff);
        server.on("/updateAds", HTTP_POST, handleUpdateAds);
  server.on("/toggleMode", handleModeToggle);

  server.begin();
  Serial.println("Web Server Started. Login Required.");
server.on("/display", handleDisplay);
server.on("/dashboard", handleDashboard);
}

//=========================================================
// LOOP
//=========================================================
void loop() {
  server.handleClient();
  timeClient.update();

  if(millis() - previousRTC > 1000) {
    previousRTC = millis();
    updateRTC();
  }

  soilRaw = analogRead(SOIL_PIN);
  soilPercent = map(soilRaw, DRY_VALUE, WET_VALUE, 0, 100);
  soilPercent = constrain(soilPercent,0,100);

  // Sample graph every 3s = 60s of history
  if(millis() - lastGraphSample > 3000) {
    lastGraphSample = millis();
    soilHistory[historyIndex] = soilPercent;
    historyIndex = (historyIndex + 1) % GRAPH_SIZE;
  }

  if(autoMode) {
    if(soilPercent < 30) { pumpState = true; digitalWrite(RELAY_PIN, HIGH); }
    else if(soilPercent > 60) { pumpState = false; digitalWrite(RELAY_PIN, LOW); }
  }

  if(millis()-previousPage > 5000) {
    previousPage = millis();
    currentPage++;
    if(currentPage > PAGE_ABOUT) { currentPage = PAGE_HOME; }
  }

  switch(currentPage) {
    case PAGE_HOME: drawHome(); break;
    case PAGE_BILLBOARD: drawBillboard(); break;
    case PAGE_IRRIGATION: drawIrrigation(); break;
    case PAGE_WIFI: drawWiFi(); break;
    case PAGE_ABOUT: drawAbout(); break;
    default: drawHome(); break;
  }
}

//=========================================================
// HELPER: Draw icon
//=========================================================
void drawIcon(int x, int y, const unsigned char *bitmap) {
  display.drawXbm(x, y, 16, 16, bitmap);
}

//=========================================================
// LOGIN CHECK FUNCTION
//=========================================================
bool checkAuth() {
  if(!server.authenticate(webUsername, webPassword)){
    server.requestAuthentication(); // Pops up the login box
    return false;
  }
  return true;
}

//=========================================================
// RTC UPDATE
//=========================================================
void updateRTC() {
  if(WiFi.status() == WL_CONNECTED){
    if (timeClient.update()) {}
      currentTime = timeClient.getFormattedTime();
      time_t epochTime = timeClient.getEpochTime();
      struct tm *ptm = gmtime(&epochTime);
      char dateStr[11];
      sprintf(dateStr, "%02d/%02d/%04d", ptm->tm_mday, ptm->tm_mon+1, ptm->tm_year+1900);
      currentDate = String(dateStr);
    }
  }


//=========================================================
// WIFI SETUP - AP + STA MODE
//=========================================================
void setupWiFi() {
  WiFi.softAP(apSSID, apPASS);
  WiFi.softAP(apSSID, apPASS);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  WiFi.begin(staSSID, staPASS);
  Serial.print("Connecting to "); Serial.println(staSSID);
  int attempts = 0;
  while (WiFi.status()!= WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if(WiFi.status() == WL_CONNECTED){
    staIP = WiFi.localIP();
    Serial.println("\nSTA Connected! LAN IP: ");
    Serial.println(staIP);
  } else {
    Serial.println("\nSTA Failed. AP Only Mode. No NTP.");
    staIP = IPAddress(0,0,0,0);
  }
}

//=========================================================
// WEB HANDLERS - ALL PROTECTED
//=========================================================
void handleRoot() {
  if(!checkAuth()) return;

  String page = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  page += "<title>USG</title><style>body{font-family:Arial;text-align:center;margin-top:20px}button,input{padding:8px;margin:5px}input{width:80%}.on{background:#4CAF50;color:white}.off{background:#f44336;color:white}</style></head><body>";
  page += "<h2>Universal Smart Gateway</h2>";
 page += "<h4>" + currentDate + " " + currentTime + "</h4><hr>"; // FIXED: was missing +
  page += "<h3>Mode: " + String(autoMode? "AUTO" : "MANUAL") + "</h3>";
  page += "<button class='" + String(autoMode? "on" : "off") + "' onclick=\"location.href='/toggleMode'\">Switch to " + String(autoMode? "MANUAL" : "AUTO") + "</button><hr>";
  page += "<h3>Current Ad: " + ads[currentAd] + "</h3>";
  page += "AP Users: Unknown | Soil: " + String(soilPercent) + "%<br>";
  page += "Pump: " + String(pumpState? "ON" : "OFF") + " | STA IP: " + staIP.toString() + "<br><br>"; // FIXED: removed ternary
  page += "<button onclick=\"location.href='/pumpOn'\">Pump ON</button>";
  page += "<button onclick=\"location.href='/pumpOff'\">Pump OFF</button>";
page += "<hr><h3>Lamp Control</h3>";

page += "Lamp: ";
page += String(lampState ? "ON" : "OFF");
page += "<br>";

page += "<button onclick=\"location.href='/lampOn'\">Lamp ON</button>";
page += "<button onclick=\"location.href='/lampOff'\">Lamp OFF</button>";
      page += "<hr><h3>Update Billboard Texts</h3><form action='/updateAds' method='POST'>";
  for(int i=0; i<TOTAL_ADS; i++){
    page += "Ad " + String(i+1) + ": <input type='text' name='ad" + String(i) + "' value='" + ads[i] + "' maxlength='20'><br>";
  }
  page += "<button type='submit'>Save Texts</button></form></body></html>";
  server.send(200,"text/html",page);
}

void handleModeToggle() {
  if(!checkAuth()) return;
  autoMode =!autoMode;
  Serial.print("Mode changed to: "); Serial.println(autoMode? "AUTO" : "MANUAL");
  server.sendHeader("Location", "/");
  server.send(303);
}

 void handleUpdateAds() {
  if(!checkAuth()) return;

  for(int i=0; i<TOTAL_ADS; i++) {
    String argName = "ad" + String(i);
    if(server.hasArg(argName)) {
      ads[i] = server.arg(argName);
      if(ads[i].length() == 0) ads[i] = " ";
    }
  }

  currentAd = 0;
  scrollX = 128;
  previousAd = millis();

  server.sendHeader("Location", "/");
  server.send(303);
}

void handlePumpOn() {
  if(!checkAuth()) return;
  pumpState = true;
  digitalWrite(RELAY_PIN, HIGH);
  server.sendHeader("Location", "/");
  server.send(303);
}

void handlePumpOff() {
  if(!checkAuth()) return;
  pumpState = false;
  digitalWrite(RELAY_PIN, LOW);
  server.sendHeader("Location", "/");
  server.send(303);
}
void handleLampOn() {
  if(!checkAuth()) return;

  lampState = true;
  digitalWrite(LAMP_PIN, HIGH);

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleLampOff() {
  if(!checkAuth()) return;

  lampState = false;
  digitalWrite(LAMP_PIN, LOW);

  server.sendHeader("Location", "/");
  server.send(303);
}
void handleDisplay() {
  if(!checkAuth()) return;

  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="refresh" content="1">
<title>USG OLED Mirror</title>

<style>
body{
    background:black;
    margin:0;
    display:flex;
    justify-content:center;
    align-items:center;
    height:100vh;
    font-family:monospace;
}

#oled{
    width:360px;
    height:180px;
    background:black;
    border:4px solid #555;
    color:white;
    padding:10px;
    box-sizing:border-box;
}
</style>
</head>

<body>

<div id="oled">
)";
   page += "UNIVERSAL<br><br>";

  page += "Mode : ";
  page += (autoMode ? "AUTO" : "MANUAL");
  page += "<br><br>";

  page += "Soil : ";
  page += String(soilPercent);
  page += "%<br>";

  page += "Pump : ";
  page += (pumpState ? "ON" : "OFF");
  page += "<br>";

  page += "Lamp : ";
  page += (lampState ? "ON" : "OFF");
  page += "<br><br>";

  page += "Ad : ";
  page += ads[currentAd];
  page += "<br><br>";

  page += currentDate;
  page += " ";
  page += currentTime;

  page += R"rawliteral(
</div>

</body>
</html>
)rawliteral";

  server.send(200, "text/html", page);
}  
void handleDashboard() {

  if(!checkAuth()) return;

  String page = R"rawliteral(
<!DOCTYPE html>
<html>

<head>

<meta name="viewport" content="width=device-width,initial-scale=1">

<meta http-equiv="refresh" content="2">

<title>USG Enterprise</title>

<style>

body{
background:#0f172a;
font-family:Arial;
margin:0;
color:white;
}

.header{

background:#111827;

padding:20px;

text-align:center;

font-size:30px;

font-weight:bold;

color:#38bdf8;

}

.card{

background:#1e293b;

margin:12px;

padding:15px;

border-radius:18px;

box-shadow:0 0 15px rgba(0,0,0,.5);

}

.status{

display:grid;

grid-template-columns:1fr 1fr;

gap:12px;

}

.box{

background:#111827;

padding:15px;

border-radius:12px;

text-align:center;

font-size:22px;

}

button{

width:100%;

padding:14px;

font-size:20px;

margin-top:10px;

border:none;

border-radius:10px;

}

.green{

background:#22c55e;

color:white;

}

.red{

background:#ef4444;

color:white;

}

.blue{

background:#0ea5e9;

color:white;

}

.bar{

height:24px;

background:#333;

border-radius:12px;

overflow:hidden;

}

.fill{

height:24px;

background:#22c55e;

}

</style>

</head>

<body>

<div class="header">

Universal Smart Gateway

</div>

<div class="card">

<h2>System Status</h2>
  page += "<b>Date:</b> " + currentDate + "<br>";
  page += "<b>Time:</b> " + currentTime + "<br>";
  page += "<b>Mode:</b> ";
  page += (autoMode ? "AUTO" : "MANUAL");
  page += R"rawliteral(

</div>

<div class="card">

<div class="status">

<div class="box">

<br>

Soil

<br><br>

)rawliteral";
  page += String(soilPercent);
  page += "%";
  page += R"rawliteral(

<div class="bar">

<div class="fill" style="width:
)rawliteral";

  page += String(soilPercent);

  page += R"rawliteral(
%;"></div>

</div>

</div>

<div class="box">

<br>

Pump<br><br>
)rawliteral";
  page += (pumpState ? "ON" : "OFF");
  page += "<br><br> Lamp<br>";
  page += (lampState ? "ON" : "OFF");
  page += R"rawliteral(

</div>

</div>

</div>

<div class="card">

<button class="green" onclick="location.href='/pumpOn'">
Pump ON
</button>

<button class="red" onclick="location.href='/pumpOff'">
Pump OFF
</button>

<button class="blue" onclick="location.href='/toggleMode'">
AUTO / MANUAL
</button>

</div>

</body>

</html>

)rawliteral";

  server.send(200,"text/html",page);

}
//=========================================================
// OLED SCREENS WITH ICONS + GRAPH
//=========================================================
void initOLED() {
 pinMode(VEXT_PIN, OUTPUT);
digitalWrite(VEXT_PIN, LOW);

pinMode(OLED_RST, OUTPUT);
digitalWrite(OLED_RST, LOW);
delay(50);
digitalWrite(OLED_RST, HIGH);
  display.init();
  display.flipScreenVertically();
  display.setContrast(255);
}

void bootAnimation() {
  display.clear();
  drawIcon(56,0, icon_soil);
  display.setFont(ArialMT_Plain_16);
  display.drawString(8,20,"UNIVERSAL");
  display.setFont(ArialMT_Plain_10);
  display.drawString(5,40,"SMART GATEWAY");
  display.display();
  delay(1000);
  for(int i=0;i<=100;i+=10) {
    display.clear();
    display.drawString(10,5,"Initializing");
    display.drawProgressBar(5,28,118,12,i);
    display.drawString(50,48,String(i)+"%");
    display.display();
    delay(150);
  }
  display.clear();
  display.drawString(30,25,"READY");
  display.display();
  delay(800);
}

void drawHome() {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(20,0,"UNIVERSAL");
  display.drawString(100,0,currentTime.substring(0,5));
  display.drawHorizontalLine(0,12,128);

  drawIcon(0,18, autoMode? icon_auto : icon_manual);
  display.drawString(20,22, autoMode?"AUTO":"MAN");

  drawIcon(0,36, icon_wifi);
  display.drawString(20,40, "WiFi AP");

  drawIcon(70,18, icon_soil);
  display.drawString(90,22, String(soilPercent) + "%");

  drawIcon(70,36, icon_pump);
  display.drawString(90,40, pumpState? "ON":"OFF");
display.drawString(70,54,"Lamp:");
display.drawString(100,54, lampState ? "ON" : "OFF");
  display.display();
}

void drawBillboard() {
  display.clear();
  display.setFont(ArialMT_Plain_10);

  drawIcon(0, 0, icon_wifi);
  display.drawString(20, 2, "BILLBOARD");
  display.drawString(100, 2, currentTime.substring(0,5));
  display.drawHorizontalLine(0, 12, 128);

  // Show message in the center (no scrolling)
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 30, ads[currentAd]);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  // Change message every 12 seconds
  if (millis() - previousAd >= adInterval) {
    previousAd = millis();
    currentAd++;
    if (currentAd >= TOTAL_ADS) {
      currentAd = 0;
    }
  }

  display.display();
}

void drawIrrigation() {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  drawIcon(0,0, icon_pump);
  display.drawString(20,2,"IRRIGATION");
  display.drawString(100,2,currentTime.substring(0,5));
  display.drawHorizontalLine(0,12,128);

  display.drawString(0,18,"Mode:");
  display.drawString(45,18, autoMode?"AUTO":"MAN");
  display.drawString(0,30,"Soil:");
  display.drawString(55,30,String(soilPercent) + "%");
  display.drawProgressBar(80,28,44,8,soilPercent);

  // 20-point trend graph
  display.drawString(0,44,"Trend:");
  display.drawRect(35,42, 90, 20);
  for(int i=0; i<GRAPH_SIZE; i++) {
    int idx = (historyIndex + i) % GRAPH_SIZE;
    int h = map(soilHistory[idx], 0, 100, 0, 18);
    display.drawVerticalLine(36 + i*4, 60-h, h);
  }
  display.display();
}

void drawWiFi() {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  drawIcon(0,0, icon_wifi);
  display.drawString(20,2,"WIFI STATUS");
  display.drawString(100,2,currentTime.substring(0,5));
  display.drawHorizontalLine(0,12,128);

  display.drawString(0,18,"AP:");
  display.drawString(25,18,apSSID);
  display.drawString(0,30,"STA:");
  display.drawString(25,30, staIP.toString()); // FIXED: shows 0.0.0.0 if not connected
  display.drawString(0,42,"Users:");
  display.drawString(50,42,"N/A");
  display.drawString(0,54,currentDate);
  display.display();
}

void drawAbout() {
  display.clear();
  drawIcon(56,0, icon_soil);
  display.setFont(ArialMT_Plain_10);
  display.drawString(10,20,"UNIVERSAL");
  display.drawString(5,32,"SMART GATEWAY");
  display.drawString(20,44,"Version 1.6");
  display.drawString(10,54,currentDate);
  display.display();
}
