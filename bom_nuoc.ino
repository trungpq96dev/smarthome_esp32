// ============================================================
//  bom_nuoc - ESP32 water pump controller
//
//  Stage 1 firmware: WiFi + local web page + full pump logic.
//  Runs on a bare ESP32 DevKit V1 with nothing else attached:
//  turn SIM_MODE on and drive the float and flow inputs from
//  the web page.
//
//  No external libraries required.
//  Board: "ESP32 Dev Module"
// ============================================================

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>
#include <math.h>

#include "config.h"
#include "page.h"

#if USE_MQTT
  #include <WiFiClientSecure.h>
  #include <PubSubClient.h>
  WiFiClientSecure mqttNet;
  PubSubClient     mqtt(mqttNet);
  unsigned long    mqttLastTry = 0;
  unsigned long    mqttLastPub = 0;
  String           mqttLastSent = "";
#endif

#if USE_TEMPERATURE
  #include <OneWire.h>
  #include <DallasTemperature.h>
  OneWire           oneWire(PIN_ONEWIRE);
  DallasTemperature dsBus(&oneWire);
  int           dsCount   = 0;
  float         dsTemp[TEMP_MAX_SENSORS];
  bool          dsPending = false;
  unsigned long dsAt      = 0;
#endif

WebServer  server(80);
Preferences prefs;

// ---------- relay helpers -----------------------------------
// Low-level trigger board, pin driven open-drain:
// LOW = energised, floating = released.
static inline void relayWrite(uint8_t pin, bool on) {
  digitalWrite(pin, on ? LOW : HIGH);
}

// ---------- debounce ----------------------------------------
struct Deb {
  bool          state = true;
  bool          last  = true;
  unsigned long t     = 0;
  bool update(bool raw, unsigned long holdMs) {
    if (raw != last) { last = raw; t = millis(); }
    if (millis() - t >= holdMs) state = last;
    return state;
  }
};
Deb debLow, debFull;

// ---------- flow sensors ------------------------------------
volatile uint32_t pulseTank  = 0;
volatile uint32_t pulseBoost = 0;
portMUX_TYPE flowMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR isrFlowTank()  { pulseTank++; }
void IRAM_ATTR isrFlowBoost() { pulseBoost++; }

// ---------- live state --------------------------------------
enum TankMode { MODE_AUTO, MODE_ON, MODE_OFF };
TankMode tankMode = MODE_AUTO;

bool  aboveLow  = true;    // water still above the low mark
bool  isFull    = false;
float flowTank  = 0;       // L/min
float flowBoost = 0;
float waterTemp = -127;    // -127 means no sensor

bool          tankOn    = false;
unsigned long tankStart = 0;
unsigned long tankStop  = 0;
bool          dryLocked = false;
unsigned long tankDryCheckMs = TANK_DRY_CHECK_MS;
unsigned long tankMinRestMs  = TANK_MIN_REST_MS;
unsigned long tankMaxRunMs   = TANK_MAX_RUN_MS;

bool          boostOn      = false;
unsigned long boostFlowSince = 0;

bool          valveOpen   = false;   // true = bypass open, gravity flow
bool          valveTarget = false;
bool          valveMoving = false;
unsigned long valveMoveStart = 0;

String fault = "";

bool otaDenied = false;

// Saved in NVS, so the network can be changed from the web page.
String wifiSsid = "";
String wifiPass = "";

// ---------- simulation --------------------------------------
bool  simMode     = SIM_MODE_DEFAULT;
bool  simAboveLow = true;
bool  simIsFull   = false;
float simFlow     = 0;

// ---------- helpers -----------------------------------------
void setFault(const String& msg) {
  if (fault.length() == 0) {         // keep the first cause, not the last
    fault = msg;
    Serial.println("[FAULT] " + msg);
  }
}

void saveState() {
  prefs.putBool("dryLocked", dryLocked);
  prefs.putBool("valve",     valveTarget);
}

// Fraction of a horizontal cylindrical tank that is full, given the
// water depth. Volume is NOT proportional to depth for this shape.
float horizontalTankPercent(float depth_m, float diameter_m) {
  float R = diameter_m * 0.5f;
  if (depth_m <= 0) return 0;
  if (depth_m >= diameter_m) return 100;
  float u = (R - depth_m) / R;
  if (u >  1) u =  1;
  if (u < -1) u = -1;
  float f = (acosf(u) - u * sqrtf(1.0f - u * u)) / (float)M_PI;
  return f * 100.0f;
}

// ---------- inputs ------------------------------------------
void readInputs() {
  if (simMode) {
    aboveLow  = simAboveLow;
    isFull    = simIsFull;
    flowTank  = tankOn  ? simFlow : 0;
    flowBoost = boostOn ? simFlow : 0;
    return;
  }

  // Internal pull-ups: an open circuit reads HIGH, which is the safe
  // state for both floats. See the wiring notes in config.h.
  aboveLow = debLow .update(digitalRead(PIN_FLOAT_LOW)  == HIGH, 2000);
  isFull   = debFull.update(digitalRead(PIN_FLOAT_FULL) == HIGH, 2000);
}

// ---------- water temperature -------------------------------
#if USE_TEMPERATURE

const char* tempName(int i) {
  switch (i) {
    case 0: return TEMP_NAME_0;
    case 1: return TEMP_NAME_1;
    case 2: return TEMP_NAME_2;
  }
  return "Cam bien";
}

void tempSetup() {
  for (int i = 0; i < TEMP_MAX_SENSORS; i++) dsTemp[i] = DEVICE_DISCONNECTED_C;
  dsBus.begin();
  // A 12-bit conversion takes about 750 ms. Blocking that long would stall
  // the web server and the pump logic, so ask now and collect on a later tick.
  dsBus.setWaitForConversion(false);

  dsCount = dsBus.getDeviceCount();
  if (dsCount > TEMP_MAX_SENSORS) dsCount = TEMP_MAX_SENSORS;
  Serial.printf("[TEMP] found %d sensor(s) on GPIO %d\n", dsCount, PIN_ONEWIRE);
  if (dsCount == 0)
    Serial.println("[TEMP] none found: check the 4.7k pull-up and the 3 wires");
  for (int i = 0; i < dsCount; i++)
    Serial.printf("[TEMP]   index %d -> %s\n", i, tempName(i));

  dsBus.requestTemperatures();
  dsPending = true;
  dsAt      = millis();
}

void tempTick() {
  unsigned long now = millis();
  if (dsPending) {
    if (now - dsAt < 900) return;          // conversion still running
    for (int i = 0; i < dsCount; i++) dsTemp[i] = dsBus.getTempCByIndex(i);
    waterTemp = dsCount ? dsTemp[0] : DEVICE_DISCONNECTED_C;
    dsPending = false;
    dsAt      = now;
  } else if (now - dsAt >= TEMP_READ_MS) {
    dsBus.requestTemperatures();
    dsPending = true;
    dsAt      = now;
  }
}

#endif  // USE_TEMPERATURE

void readFlow() {
  if (simMode) return;
  portENTER_CRITICAL(&flowMux);
  uint32_t pt = pulseTank;  pulseTank  = 0;
  uint32_t pb = pulseBoost; pulseBoost = 0;
  portEXIT_CRITICAL(&flowMux);
  flowTank  = (pt * 60.0f) / FLOW_PULSES_PER_LITRE;
  flowBoost = (pb * 60.0f) / FLOW_PULSES_PER_LITRE;
}

// ---------- control logic -----------------------------------
void driveTank(bool on) {
  if (on == tankOn) return;
  tankOn = on;
  if (on) tankStart = millis();
  else    tankStop  = millis();
  relayWrite(PIN_RELAY_TANKPUMP, on);
  digitalWrite(PIN_LED, on ? HIGH : LOW);
  Serial.printf("[TANK] %s\n", on ? "ON" : "OFF");
}

void driveBooster(bool on) {
  if (on == boostOn) return;
  boostOn = on;
  if (!on) boostFlowSince = 0;
  relayWrite(PIN_RELAY_BOOSTER, on);
  Serial.printf("[BOOST] %s\n", on ? "ON" : "OFF");
}

void commandValve(bool open) {
  if (open == valveTarget) return;
  // Interlock: the booster must never run while water can loop
  // through the open bypass.
  if (open) driveBooster(false);
  valveTarget    = open;
  valveMoving    = true;
  valveMoveStart = millis();
  relayWrite(PIN_RELAY_VALVE_OPEN,  open);
  relayWrite(PIN_RELAY_VALVE_CLOSE, !open);
  saveState();
  Serial.printf("[VALVE] moving to %s\n", open ? "OPEN" : "CLOSED");
}

void applyLogic() {
  unsigned long now = millis();

  // ---- valve travel ----
  if (valveMoving && now - valveMoveStart >= VALVE_TRAVEL_MS) {
    valveMoving = false;
    valveOpen   = valveTarget;
    Serial.printf("[VALVE] %s\n", valveOpen ? "OPEN" : "CLOSED");
  }

  // ---- tank pump ----
  bool want = false;
  if (tankMode == MODE_ON)        want = true;
  else if (tankMode == MODE_AUTO) want = tankOn ? !isFull : !aboveLow;

  if (tankOn) {
    if (now - tankStart > tankMaxRunMs) {
      want = false;
      String m = "Bom len bon chay qua ";
      m += (uint32_t)(tankMaxRunMs / 60000UL);
      m += " phut ma chua day. Da tat de an toan.";
      setFault(m);
    } else if (now - tankStart > tankDryCheckMs && flowTank < TANK_MIN_FLOW_LPM) {
      want      = false;
      dryLocked = true;
      saveState();
      setFault("Bom len bon chay nhung khong co nuoc. Da tat va khoa.");
    }
  } else {
    // minimum rest between two runs, so the pump does not chatter
    if (tankStop != 0 && now - tankStop < tankMinRestMs) want = false;
  }

  if (dryLocked) want = false;
  if (isFull)    want = false;      // highest priority, always wins
  driveTank(want);

  // ---- booster pump ----
  if (boostOn && (valveOpen || valveMoving)) {
    driveBooster(false);
    setFault("Da tat bom ap vi van bypass dang mo.");
  }
  if (boostOn) {
    if (flowBoost >= TANK_MIN_FLOW_LPM) {
      if (boostFlowSince == 0) boostFlowSince = now;
      if (now - boostFlowSince > BOOSTER_MAX_RUN_MS) {
        driveBooster(false);
        String m = "Bom ap chay lien tuc qua ";
        m += (uint32_t)(BOOSTER_MAX_RUN_MS / 60000UL);
        m += " phut. Nghi ro ri nuoc, da cat nguon.";
        setFault(m);
      }
    } else {
      boostFlowSince = 0;
    }
  }
}

// ---------- web ---------------------------------------------
// One entry of the pin panel on the web page.
void addIo(String& s, bool first, const char* name, int gpio, bool on) {
  if (!first) s += ",";
  s += "{\"n\":\"";  s += name;
  s += "\",\"g\":";  s += gpio;
  s += ",\"v\":";    s += (on ? 1 : 0);
  s += "}";
}

String stateJson() {
  float lvl = -1, litres = -1;
#if USE_LEVEL_SENSOR
  // filled in once the pressure sensor is wired up
#endif

  String s;
  s.reserve(480);
  s  = "{\"sim\":";       s += (simMode ? 1 : 0);
  s += ",\"up\":";        s += (uint32_t)(millis() / 1000UL);
  s += ",\"rssi\":";      s += (int)(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
  s += ",\"aboveLow\":";  s += (aboveLow ? 1 : 0);
  s += ",\"isFull\":";    s += (isFull ? 1 : 0);
  s += ",\"lvl\":";       s += String(lvl, 1);
  s += ",\"litres\":";    s += String(litres, 0);
  s += ",\"temp\":";      s += String(waterTemp, 1);
  s += ",\"tankOn\":";    s += (tankOn ? 1 : 0);
  s += ",\"tankMode\":\"";
  s += (tankMode == MODE_AUTO ? "auto" : (tankMode == MODE_ON ? "on" : "off"));
  s += "\",\"tankRun\":"; s += (uint32_t)(tankOn ? (millis() - tankStart) / 1000UL : 0UL);
  s += ",\"fTank\":";     s += String(flowTank, 1);
  s += ",\"boostOn\":";   s += (boostOn ? 1 : 0);
  s += ",\"fBoost\":";    s += String(flowBoost, 1);
  s += ",\"valve\":\"";
  s += (valveMoving ? "moving" : (valveOpen ? "open" : "closed"));
  s += "\",\"locked\":";  s += (dryLocked ? 1 : 0);
  s += ",\"fault\":\"";   s += fault;
  s += "\",\"fw\":\"";    s += FW_VERSION;
  s += "\",\"dryCheck\":"; s += (uint32_t)(tankDryCheckMs / 1000UL);
  s += ",\"minRest\":";  s += (tankMinRestMs % 60000UL == 0 ? String(tankMinRestMs / 60000UL) : String((float)tankMinRestMs / 60000.0f, 1));
  s += ",\"maxRun\":";   s += (uint32_t)(tankMaxRunMs / 60000UL);
  s += ",\"temps\":[";
#if USE_TEMPERATURE
  for (int i = 0; i < dsCount; i++) {
    if (i) s += ",";
    s += "{\"n\":\"";  s += tempName(i);
    s += "\",\"v\":";  s += String(dsTemp[i], 1);
    s += "}";
  }
#endif
  s += "],\"ssid\":\"";
  s += (WiFi.getMode() == WIFI_AP ? String(AP_SSID) + " (chế độ phát sóng)" : wifiSsid);
  s += "\",\"ip\":\"";
  s += (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString());
  s += "\",\"io\":[";
  addIo(s, true,  "Relay 1 · bơm áp",      PIN_RELAY_BOOSTER,     boostOn);
  addIo(s, false, "Relay 2 · bơm lên bồn", PIN_RELAY_TANKPUMP,    tankOn);
  addIo(s, false, "Relay 3 · van mở",      PIN_RELAY_VALVE_OPEN,  valveTarget);
  addIo(s, false, "Relay 4 · van đóng",    PIN_RELAY_VALVE_CLOSE, !valveTarget);
  addIo(s, false, "Vào · phao mức thấp",   PIN_FLOAT_LOW,         aboveLow);
  addIo(s, false, "Vào · phao mức đầy",    PIN_FLOAT_FULL,        isFull);
  s += "]}";
  return s;
}

void handleRoot()  { server.send_P(200, "text/html; charset=utf-8", INDEX_HTML); }
void handleState() { server.send(200, "application/json", stateJson()); }

// One command, from wherever. The local web page and MQTT both land here.
void execCmd(const String& k, const String& v) {
  if (k == "tank") {
    if      (v == "auto") tankMode = MODE_AUTO;
    else if (v == "on")   tankMode = MODE_ON;
    else if (v == "off")  tankMode = MODE_OFF;

  } else if (k == "booster") {
    driveBooster(v == "on");

  } else if (k == "valve") {
    commandValve(v == "open");

  } else if (k == "clear") {
    fault     = "";
    dryLocked = false;
    tankStop  = 0;
    saveState();
    Serial.println("[FAULT] cleared");

  } else if (k == "sim") {
    simMode = (v == "1");
    // drop any pulses counted while the inputs were being ignored
    portENTER_CRITICAL(&flowMux);
    pulseTank = 0; pulseBoost = 0;
    portEXIT_CRITICAL(&flowMux);
    flowTank = 0; flowBoost = 0;

  } else if (k == "simAboveLow") { simAboveLow = (v == "1");
  } else if (k == "simIsFull")   { simIsFull   = (v == "1");
  } else if (k == "simFlow")     { simFlow     = v.toFloat();
  } else if (k == "cfgDry") {
    float sec = v.toFloat();
    if (sec >= 5 && sec <= 600) {
      tankDryCheckMs = (unsigned long)(sec * 1000.0f);
      prefs.putULong("dryMs", tankDryCheckMs);
      Serial.printf("[CFG] tankDryCheckMs = %lu ms\n", tankDryCheckMs);
    }
  } else if (k == "cfgRest") {
    float min = v.toFloat();
    if (min >= 0.1f && min <= 180.0f) {
      tankMinRestMs = (unsigned long)(min * 60.0f * 1000.0f);
      prefs.putULong("restMs", tankMinRestMs);
      Serial.printf("[CFG] tankMinRestMs = %lu ms\n", tankMinRestMs);
    }
  } else if (k == "cfgRun") {
    float min = v.toFloat();
    if (min >= 1.0f && min <= 360.0f) {
      tankMaxRunMs = (unsigned long)(min * 60.0f * 1000.0f);
      prefs.putULong("runMs", tankMaxRunMs);
      Serial.printf("[CFG] tankMaxRunMs = %lu ms\n", tankMaxRunMs);
    }
  }
}

// Parse "tank=auto&booster=off" into individual commands.
void execCmdString(const String& qs) {
  int start = 0;
  while (start < (int)qs.length()) {
    int amp = qs.indexOf('&', start);
    if (amp < 0) amp = qs.length();
    String pair = qs.substring(start, amp);
    int eq = pair.indexOf('=');
    if (eq > 0) execCmd(pair.substring(0, eq), pair.substring(eq + 1));
    start = amp + 1;
  }
}

void handleCmd() {
  for (int i = 0; i < server.args(); i++) execCmd(server.argName(i), server.arg(i));
  server.send(200, "application/json", stateJson());
}

// ---------- WiFi settings from the web page -----------------
void handleScan() {
  int n = WiFi.scanNetworks();
  String s = "[";
  for (int i = 0; i < n && i < 20; i++) {
    if (i) s += ",";
    String ss = WiFi.SSID(i);
    ss.replace("\\", "\\\\");
    ss.replace("\"", "\\\"");
    s += "{\"s\":\""; s += ss;
    s += "\",\"r\":"; s += WiFi.RSSI(i);
    s += "}";
  }
  s += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", s);
}

void handleWifiSave() {
  if (server.arg("pw") != String(OTA_PASSWORD)) {
    server.send(403, "application/json", "{\"ok\":0,\"msg\":\"Sai mat khau cap nhat\"}");
    return;
  }
  if (server.hasArg("forget")) {
    prefs.remove("ssid");
    prefs.remove("pass");
    Serial.println("[WIFI] saved network forgotten");
    server.send(200, "application/json", "{\"ok\":1}");
    delay(600);
    ESP.restart();
    return;
  }

  String ssid = server.arg("ssid");
  if (ssid.length() == 0) {
    server.send(400, "application/json", "{\"ok\":0,\"msg\":\"Chua chon mang WiFi\"}");
    return;
  }
  prefs.putString("ssid", ssid);
  prefs.putString("pass", server.arg("pass"));
  Serial.println("[WIFI] new network saved: " + ssid);

  // Never reboot with a pump energised.
  tankMode = MODE_OFF;
  driveTank(false);
  driveBooster(false);

  server.send(200, "application/json", "{\"ok\":1}");
  delay(600);
  ESP.restart();
}

// ---------- serial console ----------------------------------
// Same commands as the web page, typed into the Serial Monitor.
// Lets the whole thing be driven with no browser and no hardware,
// which is how it gets tested in a simulator.
void printStatus() {
  Serial.println("--------------------------------------------------");
  Serial.printf("  bom bon   : %s   che do: %s\n",
                tankOn ? "DANG CHAY" : "tat",
                tankMode == MODE_AUTO ? "tu dong" : (tankMode == MODE_ON ? "bat tay" : "tat tay"));
  if (tankOn) Serial.printf("  da chay   : %lu giay\n", (millis() - tankStart) / 1000UL);
  Serial.printf("  phao      : %s / %s\n",
                aboveLow ? "con tren muc thap" : "DA CAN",
                isFull   ? "DA DAY"            : "chua day");
  Serial.printf("  luu luong : %.1f L/phut (bon)  %.1f L/phut (ap)\n", flowTank, flowBoost);
  Serial.printf("  bom ap    : %s\n", boostOn ? "DANG BAT" : "tat");
  Serial.printf("  van bi    : %s\n",
                valveMoving ? "dang quay" : (valveOpen ? "MO (chay thuong)" : "DONG (qua bom ap)"));
#if USE_TEMPERATURE
  for (int i = 0; i < dsCount; i++) {
    if (dsTemp[i] <= DEVICE_DISCONNECTED_C)
      Serial.printf("  nhiet do %d: mat ket noi\n", i);
    else
      Serial.printf("  nhiet do %d: %.1f C\n", i, dsTemp[i]);
  }
#endif
  Serial.printf("  mo phong  : %s%s\n", simMode ? "bat" : "tat", dryLocked ? "   [DANG KHOA]" : "");
  Serial.printf("  bao ve    : ktra can %lu s | nghi %lu p | chay max %lu p\n",
                tankDryCheckMs / 1000UL, tankMinRestMs / 60000UL, tankMaxRunMs / 60000UL);
  if (fault.length()) Serial.println("  LOI       : " + fault);
  Serial.println("--------------------------------------------------");
}

void printHelp() {
  Serial.println(F(
    "\nLenh (go roi Enter):\n"
    "  s                 xem trang thai\n"
    "  tank=auto|on|off  che do bom len bon\n"
    "  booster=on|off    bom ap\n"
    "  valve=open|close  van bi bypass\n"
    "  simAboveLow=0|1   0 = bon da can\n"
    "  simIsFull=0|1     1 = bon da day\n"
    "  simFlow=20        luu luong gia lap, L/phut\n"
    "  cfgDry=45         thoi gian cho kiem tra chay kho (giay)\n"
    "  cfgRest=2         thoi gian nghi toi thieu (phut)\n"
    "  cfgRun=45         thoi gian chay toi da (phut)\n"
    "  clear=1           xoa loi va mo khoa\n"
    "  sim=0|1           tat/bat che do mo phong\n"
    "Noi nhieu lenh bang dau &, vi du: simIsFull=0&simAboveLow=0\n"));
}

void serialConsole() {
  static String line;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      line.trim();
      if (line.length()) {
        if      (line == "?" || line == "help")   printHelp();
        else if (line == "s" || line == "status") printStatus();
        else { execCmdString(line); printStatus(); }
      }
      line = "";
    } else if (line.length() < 120) {
      line += c;
    }
  }
}

// ---------- over-the-air update -----------------------------
// Reply sent once the whole body has been received.
void handleOtaDone() {
  if (otaDenied) {
    otaDenied = false;
    server.send(403, "application/json", "{\"ok\":0,\"msg\":\"Sai mat khau cap nhat\"}");
    return;
  }
  bool ok = !Update.hasError();
  server.sendHeader("Connection", "close");
  server.send(ok ? 200 : 500, "application/json",
              ok ? "{\"ok\":1}" : "{\"ok\":0,\"msg\":\"Ghi firmware that bai\"}");
  if (ok) { delay(800); ESP.restart(); }
}

// Called repeatedly as the .bin streams in.
void handleOtaUpload() {
  HTTPUpload& up = server.upload();

  if (up.status == UPLOAD_FILE_START) {
    otaDenied = (server.header("X-OTA-Password") != String(OTA_PASSWORD));
    if (otaDenied) { Serial.println("[OTA] rejected, wrong password"); return; }

    // Never rewrite flash with a pump energised.
    tankMode = MODE_OFF;
    driveTank(false);
    driveBooster(false);

    Serial.printf("[OTA] start: %s\n", up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);

  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (otaDenied) return;
    if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);

  } else if (up.status == UPLOAD_FILE_END) {
    if (otaDenied) return;
    if (Update.end(true)) Serial.printf("[OTA] done, %u bytes\n", up.totalSize);
    else                  Update.printError(Serial);
  }
}

// ---------- MQTT: control from mobile data -------------------
#if USE_MQTT

String mqttLastDigest = "";

// Only the fields worth reacting to. Uptime and flow change every second,
// so comparing the whole JSON would publish non-stop.
String controlDigest() {
  String d;
  d += (int)tankOn;      d += (int)tankMode;  d += (int)boostOn;
  d += (int)valveOpen;   d += (int)valveMoving;
  d += (int)aboveLow;    d += (int)isFull;    d += (int)dryLocked;
  d += (int)simMode;     d += fault;
  d += (int)(tankDryCheckMs / 1000UL);
  d += (int)(tankMinRestMs / 1000UL);
  d += (int)(tankMaxRunMs / 1000UL);
  return d;
}

void mqttPublishState() {
  String s = stateJson();
  mqttLastPub = millis();
  // retained, so a phone opening the page later sees the state at once
  mqtt.publish(TOPIC_STATE, s.c_str(), true);
}

void mqttOnMessage(char* topic, byte* payload, unsigned int len) {
  String msg;
  msg.reserve(len + 1);
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  Serial.println("[MQTT] cmd: " + msg);
  execCmdString(msg);
  mqttLastDigest = controlDigest();
  mqttPublishState();
}

void mqttSetup() {
  #if MQTT_INSECURE_TLS
    mqttNet.setInsecure();
  #endif
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttOnMessage);
  mqtt.setBufferSize(768);     // state JSON exceeds the 256-byte default
  mqtt.setKeepAlive(30);
  Serial.println("MQTT enabled, broker " MQTT_HOST);
}

// Called every loop: keeps the session alive and delivers incoming commands.
void mqttService() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (!mqtt.connected()) {
    if (millis() - mqttLastTry < MQTT_RETRY_MS) return;
    mqttLastTry = millis();

    String cid = String(MQTT_CLIENT_ID) + "-";
    cid += String((uint32_t)ESP.getEfuseMac(), HEX);

    // Last will: if this board drops off the net, the broker tells the
    // phone "offline" instead of leaving a stale screen looking healthy.
    if (mqtt.connect(cid.c_str(), MQTT_USER, MQTT_PASS,
                     TOPIC_STATUS, 1, true, "offline")) {
      Serial.println("[MQTT] connected");
      mqtt.publish(TOPIC_STATUS, "online", true);
      mqtt.subscribe(TOPIC_CMD, 1);
      mqttLastDigest = controlDigest();
      mqttPublishState();
    } else {
      Serial.printf("[MQTT] connect failed, rc=%d\n", mqtt.state());
    }
    return;
  }
  mqtt.loop();
}

// Called once a second: publish on change, plus a slow heartbeat.
void mqttTick() {
  if (!mqtt.connected()) return;
  String d = controlDigest();
  if (d != mqttLastDigest || millis() - mqttLastPub >= MQTT_PUBLISH_MS) {
    mqttLastDigest = d;
    mqttPublishState();
  }
}

#endif  // USE_MQTT

// ---------- setup / loop ------------------------------------
void startWifi() {
  // Compiled-in values are only the first-boot fallback; whatever was saved
  // from the web page wins after that.
  wifiSsid = prefs.getString("ssid", WIFI_SSID);
  wifiPass = prefs.getString("pass", WIFI_PASS);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  Serial.println("WiFi: joining \"" + wifiSsid + "\"");
  Serial.print("    ");

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TIMEOUT_MS) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected. Open  http://");
    Serial.println(WiFi.localIP());
    Serial.printf("          or  http://%s.local\n", HOSTNAME);
  } else {
    // AP_STA, not plain AP: the setup page can still scan for networks.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.printf("No WiFi. Access point \"%s\" (pass %s)\n", AP_SSID, AP_PASS);
    Serial.print("Open  http://");
    Serial.println(WiFi.softAPIP());
    Serial.println("Set the right network there, no reflashing needed.");
  }
  MDNS.begin(HOSTNAME);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== bom_nuoc ===");

  pinMode(PIN_RELAY_TANKPUMP,    OUTPUT_OPEN_DRAIN);
  pinMode(PIN_RELAY_BOOSTER,     OUTPUT_OPEN_DRAIN);
  pinMode(PIN_RELAY_VALVE_OPEN,  OUTPUT_OPEN_DRAIN);
  pinMode(PIN_RELAY_VALVE_CLOSE, OUTPUT_OPEN_DRAIN);
  relayWrite(PIN_RELAY_TANKPUMP,    false);
  relayWrite(PIN_RELAY_BOOSTER,     false);
  relayWrite(PIN_RELAY_VALVE_OPEN,  false);
  relayWrite(PIN_RELAY_VALVE_CLOSE, true);   // default: bypass closed

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  pinMode(PIN_FLOAT_LOW,     INPUT_PULLUP);
  pinMode(PIN_FLOAT_FULL,    INPUT_PULLUP);
  pinMode(PIN_FLOW_TANK,     INPUT_PULLUP);
  pinMode(PIN_FLOW_BOOSTER,  INPUT_PULLUP);
  pinMode(PIN_BUTTON_RESET,  INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_FLOW_TANK),    isrFlowTank,  FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW_BOOSTER), isrFlowBoost, FALLING);

  prefs.begin("bomnuoc", false);
  dryLocked      = prefs.getBool("dryLocked", false);
  valveTarget    = prefs.getBool("valve", false);
  tankDryCheckMs = prefs.getULong("dryMs",  TANK_DRY_CHECK_MS);
  tankMinRestMs  = prefs.getULong("restMs", TANK_MIN_REST_MS);
  tankMaxRunMs   = prefs.getULong("runMs",  TANK_MAX_RUN_MS);
  valveOpen      = valveTarget;
  relayWrite(PIN_RELAY_VALVE_OPEN,  valveTarget);
  relayWrite(PIN_RELAY_VALVE_CLOSE, !valveTarget);
  if (dryLocked) setFault("Dang khoa vi lan truoc bom chay can. Bam xoa loi de mo.");

  startWifi();

  // Headers are dropped unless explicitly collected, and the OTA password
  // travels as a header so multipart form parsing cannot swallow it.
  const char* wanted[] = { "X-OTA-Password" };
  server.collectHeaders(wanted, (size_t)1);   // exact types: pick the array overload

  server.on("/",           handleRoot);
  server.on("/api/state",  handleState);
  server.on("/api/cmd",    handleCmd);
  server.on("/api/ota",    HTTP_POST, handleOtaDone, handleOtaUpload);
  server.on("/api/scan",   handleScan);
  server.on("/api/wifi",   handleWifiSave);
  server.onNotFound([]()   { server.send(404, "text/plain", "not found"); });
  server.begin();
  Serial.println("Web server started, firmware " FW_VERSION);

#if USE_TEMPERATURE
  tempSetup();
#endif

#if USE_MQTT
  mqttSetup();
#endif

  printHelp();
  printStatus();
}

void loop() {
  server.handleClient();
  serialConsole();

#if USE_MQTT
  mqttService();
#endif

  static unsigned long tTick = 0;
  if (millis() - tTick >= 1000) {
    tTick = millis();
    readFlow();
    readInputs();
    applyLogic();
#if USE_TEMPERATURE
    tempTick();
#endif
#if USE_MQTT
    mqttTick();
#endif
  }

  // manual fault reset button
  static unsigned long tBtn = 0;
  if (digitalRead(PIN_BUTTON_RESET) == LOW && millis() - tBtn > 1000) {
    tBtn      = millis();
    fault     = "";
    dryLocked = false;
    tankStop  = 0;
    saveState();
    Serial.println("[FAULT] cleared by button");
  }
}
