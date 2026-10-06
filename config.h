#pragma once

// ============================================================
//  config.h - edit this file, then flash. No other file needs
//  to change for day-to-day tweaks.
// ============================================================

// ---------- Firmware ----------
#define FW_VERSION       "1.5"

// ---------- Over-the-air update ----------
// Password asked for on the web page before a .bin is accepted.
// Change it: anyone on your WiFi can reach this page.
#define OTA_PASSWORD     "bomnuoc2026"

// ---------- WiFi ----------
// Only used on the very first boot. After that the board remembers whatever
// was last saved from the web page, so changing network needs no reflashing.
// Forget the saved one with the "Quên WiFi đã lưu" button on that page.
#define WIFI_SSID        "Wifi_Free_5G"
#define WIFI_PASS        "968552983"
#define HOSTNAME         "bomnuoc"       // reach it at http://bomnuoc.local
#define WIFI_TIMEOUT_MS  20000UL

// Fallback access point, used when the home WiFi cannot be joined.
#define AP_SSID          "MySmartHome"
#define AP_PASS          "12345678"

// ---------- Simulation ----------
// true  = no sensors wired, float and flow values come from the web page.
// false = real sensors wired to the pins below.
// Can also be toggled at runtime from the web page.
#define SIM_MODE_DEFAULT  true

// ---------- Optional features ----------
// Keep these at 0 until the parts arrive. At 0 the sketch needs
// no external libraries at all.
#define USE_TEMPERATURE   0    // needs OneWire + DallasTemperature
#define USE_LEVEL_SENSOR  0    // needs Adafruit ADS1X15

// ---------- DS18B20 water temperature ----------
// Several sensors share one data wire on PIN_ONEWIRE. They are listed in
// address order, which has nothing to do with the order you wired them, so
// at boot the serial log prints each index with its reading: warm one up
// with your hand, see which index moves, then reorder the names below.
#define TEMP_MAX_SENSORS  3
#define TEMP_READ_MS      10000UL
#define TEMP_NAME_0       "Nhiệt độ bồn mái"
#define TEMP_NAME_1       "Nhiệt độ bình nóng 1"
#define TEMP_NAME_2       "Nhiệt độ bình nóng 2"

// ---------- MQTT: control over mobile data ----------
// Set to 1 only AFTER:
//   1. installing "PubSubClient" by Nick O'Leary
//      (Arduino IDE: Tools > Manage Libraries, search PubSubClient)
//   2. creating a free HiveMQ Cloud cluster and filling in the fields below
#define USE_MQTT          1

#define MQTT_HOST         "o00449c1.ala.asia-southeast1.emqxsl.com"
#define MQTT_PORT         8883
#define MQTT_USER         "trungsmarthome_esp32"
#define MQTT_PASS         "12345678"
#define MQTT_CLIENT_ID    "trungsmarthome-esp32"

#define TOPIC_STATE       "bomnuoc/state"
#define TOPIC_CMD         "bomnuoc/cmd"
#define TOPIC_STATUS      "bomnuoc/status"

#define MQTT_PUBLISH_MS   5000UL    // heartbeat; changes are sent immediately
#define MQTT_RETRY_MS     5000UL

// Skip TLS certificate checking. Traffic is still encrypted, but a
// man-in-the-middle on the path could impersonate the broker. Fine for a
// home project; paste the broker root CA below and use setCACert instead
// if that matters to you.
#define MQTT_INSECURE_TLS 1

// ---------- Pin map (ESP32 DevKit V1) ----------
#define PIN_RELAY_BOOSTER      18   // pressure pump
#define PIN_RELAY_TANKPUMP     19   // tank filling pump
#define PIN_RELAY_VALVE_OPEN   23   // motorised ball valve, open line
#define PIN_RELAY_VALVE_CLOSE  13   // motorised ball valve, close line
#define PIN_FLOAT_LOW          25
#define PIN_FLOAT_FULL         26
#define PIN_FLOW_TANK          27
#define PIN_FLOW_BOOSTER       14
#define PIN_BUTTON_RESET       33
#define PIN_ONEWIRE             4
#define PIN_LED                 2   // onboard LED, mirrors the tank pump

// ---------- Relay drive ----------
// Relay board jumper set to LOW-LEVEL TRIGGER, pins driven open-drain.
// Pin pulled to GND  = relay energised.
// Pin left floating  = relay released.
// This keeps a 3.3V MCU working correctly with a 12V relay board.

// ---------- Float wiring (fail-safe) ----------
// Both float inputs use the internal pull-up, so a disconnected or
// broken wire reads HIGH. Wire each float so that HIGH is the SAFE
// state, meaning "do not run the pump":
//
//   FULL float  : contact CLOSED while the tank is NOT full.
//                 Opens when full  -> HIGH -> pump stops.
//   LOW  float  : contact CLOSED while the water is BELOW the low mark.
//                 Opens when water is above -> HIGH -> no need to run.
//
// A cut cable therefore stops the pump instead of overflowing the tank.

// ---------- Flow sensor ----------
// YF-S201 datasheet: about 450 pulses per litre.
// Measure your own sensor and correct this number during commissioning.
#define FLOW_PULSES_PER_LITRE   450.0f

// ---------- Protection timings ----------
#define TANK_MIN_REST_MS     (2UL  * 60UL * 1000UL)  // rest between two runs
#define TANK_MAX_RUN_MS      (45UL * 60UL * 1000UL)  // hard stop
#define TANK_DRY_CHECK_MS    (45UL * 1000UL)         // grace before dry-run test
#define TANK_MIN_FLOW_LPM    1.0f                    // below this = dry running
#define BOOSTER_MAX_RUN_MS   (15UL * 60UL * 1000UL)  // suspect a leak
#define VALVE_TRAVEL_MS      (15UL * 1000UL)

// ---------- Tank geometry (horizontal cylinder) ----------
#define TANK_DIAMETER_M   1.40f
#define TANK_VOLUME_L     1500.0f
