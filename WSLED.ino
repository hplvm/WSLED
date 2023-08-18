
//Configure this block before uploading
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


//******WIFI*********************
#define WIFI_SSID         "ssid"
#define WIFI_PASSWORD     "passphrase"
//******WIFI*********************


//******SINRIC PRO****************
#define APP_KEY           "c327b4ce-470f-4040-a872-5dbc6c039fe5"      // Should look like "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET        "099400d6-4e72-403f-abfa-4fc15cd3d83c-83f39f70-c14e-43af-ab2e-5fa9582ec380"   // Should look like "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"
#define LIGHT_ID          "63e91b1e1bb4e19c119a019f"    // Should look like "5dc1564130xxxxxxxxxxxxxx"
#define BAUD_RATE         115200               // Change baudrate to your need
//******SINRIC PRO****************


//******LED STRIP*****************
#define LED_PIN            13                       // 0 = GPIO0, 2=GPIO2
#define LED_COUNT          120
#define MAX_BRIGHTNESS     100                  //max brightness(<=255)
//******LED STRIP*****************


#define WIFI_TIMEOUT      15              // checks WiFi every ...s. Reset after this time, if WiFi cannot reconnect.
#define AUTO_CYCLE_FREQ   15              // Seconds to wait to change mode if auto cycle of modes is enabled
#define HTTP_PORT         80              // http port, leave at 80 if you don't know what you are doing

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


#ifdef ARDUINO_ARCH_ESP32
  #include <WiFi.h>
  #include <WebServer.h>
  #define WEB_SERVER WebServer
  #define ESP_RESET ESP.restart()
#else
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #define WEB_SERVER ESP8266WebServer
  #define ESP_RESET ESP.reset()
#endif

#include <WS2812FX.h>

#include "SinricPro.h"
#include "SinricProLight.h"

extern const char index_html[];
extern const char main_js[];


#define STATIC_IP                       // uncomment for static IP, set IP below
#ifdef STATIC_IP
  IPAddress ip(192,168,100,10);
  IPAddress gateway(192,168,100,1);
  IPAddress subnet(255,255,255,0);
  IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional
#endif



// QUICKFIX...See https://github.com/esp8266/Arduino/issues/263
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))


bool powerState;        
int globalBrightness = 100;

unsigned long auto_last_change = 0;
unsigned long last_wifi_check_time = 0;
String modes = "";
uint8_t myModes[] = {}; // *** optionally create a custom list of effect/mode numbers
bool auto_cycle = false;

WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
WEB_SERVER server(HTTP_PORT);

bool onPowerState(const String &deviceId, bool &state) {
  powerState = state;
  if(state)
    ws2812fx.start();
  else
    ws2812fx.stop();
  return true;
}

bool onBrightness(const String &deviceId, int &brightness) {
  globalBrightness = brightness;
  ws2812fx.setBrightness(map(brightness, 0, 100, 0, MAX_BRIGHTNESS));
  return true;
}

bool onAdjustBrightness(const String &deviceId, int brightnessDelta) {
  globalBrightness += brightnessDelta;
  brightnessDelta = globalBrightness;
  ws2812fx.setBrightness(map(globalBrightness, 0, 100, 0, MAX_BRIGHTNESS));
  return true;
}

bool onColor(const String &deviceId, byte &r, byte &g, byte &b) {
  ws2812fx.setColor(r<<16|g<<8|b);
  return true;
}

void setupSinricPro() {
  // get a new Light device from SinricPro
  SinricProLight &myLight = SinricPro[LIGHT_ID];

  // set callback function to device
  myLight.onPowerState(onPowerState);
  myLight.onBrightness(onBrightness);
  myLight.onAdjustBrightness(onAdjustBrightness);
  myLight.onColor(onColor);

  // setup SinricPro
  SinricPro.onConnected([](){ Serial.printf("Connected to SinricPro\r\n"); }); 
  SinricPro.onDisconnected([](){ Serial.printf("Disconnected from SinricPro\r\n"); });
  //SinricPro.restoreDeviceStates(true); // Uncomment to restore the last known state from the server.
  SinricPro.begin(APP_KEY, APP_SECRET);
}

void setup(){
  Serial.begin(BAUD_RATE);
  delay(500);
  Serial.println("\n\nStarting...");

  modes.reserve(5000);
  modes_setup();

  Serial.println("WS2812FX setup");
  ws2812fx.init();
  ws2812fx.setMode(FX_MODE_STATIC);
  ws2812fx.setColor(0xFF5900);
  ws2812fx.setSpeed(1000);
  ws2812fx.setBrightness(0);
  ws2812fx.start();

  Serial.println("Wifi setup");
  wifi_setup();
 
  Serial.println("HTTP server setup");
  server.on("/", srv_handle_index_html);
  server.on("/main.js", srv_handle_main_js);
  server.on("/modes", srv_handle_modes);
  server.on("/set", srv_handle_set);
  server.onNotFound(srv_handle_not_found);
  server.begin();
  Serial.println("HTTP server started.");

  Serial.println("ready!");
  //setupSinricPro();
}


void loop() {
  unsigned long now = millis();
  SinricPro.handle();
  server.handleClient();
  ws2812fx.service();

  if(now - last_wifi_check_time > WIFI_TIMEOUT*1000) {
    Serial.print("Checking WiFi... ");
    if(WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi connection lost. Reconnecting...");
      wifi_setup();
    } else {
      Serial.println("OK");
    }
    last_wifi_check_time = now;
  }

  if(auto_cycle && (now - auto_last_change > AUTO_CYCLE_FREQ*1000)) { // cycle effect mode every x seconds
    uint8_t next_mode = (ws2812fx.getMode() + 1) % ws2812fx.getModeCount();
    if(sizeof(myModes) > 0) { // if custom list of modes exists
      for(uint8_t i=0; i < sizeof(myModes); i++) {
        if(myModes[i] == ws2812fx.getMode()) {
          next_mode = ((i + 1) < sizeof(myModes)) ? myModes[i + 1] : myModes[0];
          break;
        }
      }
    }
    ws2812fx.setMode(next_mode);
    Serial.print("mode is "); Serial.println(ws2812fx.getModeName(ws2812fx.getMode()));
    auto_last_change = now;
  }
}



/*
 * Connect to WiFi. If no connection is made within WIFI_TIMEOUT, ESP gets resettet.
 */
void wifi_setup() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  
  #ifdef STATIC_IP
  if (!WiFi.config(ip, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Failed to configure IP");
  }
  #endif
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long connect_start = millis();
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    if(millis() - connect_start > WIFI_TIMEOUT*1000) {
      Serial.println();
      Serial.print("Tried ");
      Serial.print(WIFI_TIMEOUT);
      Serial.print("ms. Resetting ESP now.");
      ESP_RESET;
    }
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  setupSinricPro();
}


/*
 * Build <li> string for all modes.
 */
void modes_setup() {
  modes = "";
  uint8_t num_modes = sizeof(myModes) > 0 ? sizeof(myModes) : ws2812fx.getModeCount();
  for(uint8_t i=0; i < num_modes; i++) {
    uint8_t m = sizeof(myModes) > 0 ? myModes[i] : i;
    modes += "<li><a href='#'>";
    modes += ws2812fx.getModeName(m);
    modes += "</a></li>";
  }
}

/* #####################################################
#  Webserver Functions
##################################################### */

void srv_handle_not_found() {
  server.send(404, "text/plain", "File Not Found");
}

void srv_handle_index_html() {
  server.send_P(200,"text/html", index_html);
}

void srv_handle_main_js() {
  server.send_P(200,"application/javascript", main_js);
}

void srv_handle_modes() {
  server.send(200,"text/plain", modes);
}

void srv_handle_set() {
  for (uint8_t i=0; i < server.args(); i++){
    if(server.argName(i) == "c") {
      uint32_t tmp = (uint32_t) strtol(server.arg(i).c_str(), NULL, 10);
      if(tmp <= 0xFFFFFF) {
        ws2812fx.setColor(tmp);
      }
    }

    if(server.argName(i) == "m") {
      uint8_t tmp = (uint8_t) strtol(server.arg(i).c_str(), NULL, 10);
      uint8_t new_mode = sizeof(myModes) > 0 ? myModes[tmp % sizeof(myModes)] : tmp % ws2812fx.getModeCount();
      ws2812fx.setMode(new_mode);
      auto_cycle = false;
      Serial.print("mode is "); Serial.println(ws2812fx.getModeName(ws2812fx.getMode()));
    }

    if(server.argName(i) == "b") {
      if(server.arg(i)[0] == '-') {
        ws2812fx.setBrightness(ws2812fx.getBrightness() * 0.8);
      } else if(server.arg(i)[0] == ' ') {
        ws2812fx.setBrightness(min(max(ws2812fx.getBrightness(), 5) * 1.2, 255));
      } else { // set brightness directly
        uint8_t tmp = (uint8_t) strtol(server.arg(i).c_str(), NULL, 10);
        ws2812fx.setBrightness(tmp);
      }
      Serial.print("brightness is "); Serial.println(ws2812fx.getBrightness());
    }

    if(server.argName(i) == "s") {
      if(server.arg(i)[0] == '-') {
        ws2812fx.setSpeed(max(ws2812fx.getSpeed(), 5) * 1.2);
      } else if(server.arg(i)[0] == ' ') {
        ws2812fx.setSpeed(ws2812fx.getSpeed() * 0.8);
      } else {
        uint16_t tmp = (uint16_t) strtol(server.arg(i).c_str(), NULL, 10);
        ws2812fx.setSpeed(tmp);
      }
      Serial.print("speed is "); Serial.println(ws2812fx.getSpeed());
    }

    if(server.argName(i) == "a") {
      if(server.arg(i)[0] == '-') {
        auto_cycle = false;
      } else {
        auto_cycle = true;
        auto_last_change = 0;
      }
    }

    if(server.argName(i) == "p") {
      if(server.arg(i)[0] == '1') {
        ws2812fx.start(); 
      } else {
        ws2812fx.stop();
      }
    }
  }
  server.send(200, "text/plain", "OK");
}
