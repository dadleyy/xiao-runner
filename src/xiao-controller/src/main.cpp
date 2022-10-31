#include <Arduino.h>
#include "WiFi.h"
#include "esp_now.h"

#define X_AXIS_PIN A0
#define Y_AXIS_PIN A1
#define Z_BUTTON_PIN A2

#ifndef X_TOLERANCE_LOWER
#define X_TOLERANCE_LOWER 1200
#endif

#ifndef X_TOLERANCE_UPPER
#define X_TOLERANCE_UPPER 3200
#endif

//
// Beetle Controller
//
// This is the code for the controller that users will hold in their
// hands.
//
// Pin definitions based on current hardware: Seeed Studio XIAO ESP32C3
//
typedef struct message_payload {
  char content[120];
} message_payload_t;

enum ERuntimeMode {
  CONNECTED,
  DISCONNECTED,
  FAILED,
};

// MAC address of beetle-lights.
//
// This will be replaced with the parsed contents of the `LIGHTS_PHYSICAL_ADDRESS` environment variable that
// is injected at compile into the `LIGHTS_PHYSICAL_ADDRESS` macro.
static uint8_t broadcast_address[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint32_t max_failed_message_sends = 100;
static const uint32_t max_failed_ap_connection_attempts = 30;

esp_now_peer_info_t peer_info;
message_payload_t message_payload;
uint32_t last_debug_log = 0;
uint32_t failed_send_count = 0;
ERuntimeMode mode = ERuntimeMode::DISCONNECTED;

// TODO: is this empty callback necessary?
void sent_cb(const uint8_t* mac_addr, esp_now_send_status_t status) {}

// cppcheck-suppress unusedFunction
void setup(void) {
  Serial.begin(115200);
  delay(1000);
  pinMode(Z_BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(Z_BUTTON_PIN, HIGH);

  log_d("initializing wifi in station mode");
  WiFi.mode(WIFI_MODE_STA);
  Serial.println(WiFi.macAddress());
  log_d("^-- my mac address");

  last_debug_log = millis();
  log_d("setup complete");
}

// cppcheck-suppress unusedFunction
void loop(void) {
  auto now = millis();

  if (mode == ERuntimeMode::FAILED) {
    log_e("failed");
    delay(1000);
    return;
  }

  if (mode == ERuntimeMode::DISCONNECTED) {
    log_d("scanning for networks");
    int n = WiFi.scanNetworks();
    bool setup_complete = false;
    log_d("found %d networks on initial scan", n);

    for (int i = 0; i < n; ++i) {
      auto ssid = WiFi.SSID(i);

      if (ssid == "xiao-runner-light-host") {
        auto addr_ptr = WiFi.BSSID(i);

        for (uint8_t i = 0; i < 6; i++) {
          broadcast_address[i] = *addr_ptr;

          // TODO: for some reason the mac address provided by our scan is off by a value of one.
          if (i == 5) {
            broadcast_address[i] -= 1;
          }

          addr_ptr++;
        }

        WiFi.begin("xiao-runner-light-host", "lights-host");
        uint32_t attempts = 0;

        while (WiFi.status() != WL_CONNECTED) {
          delay(500);
          log_d("connecting...");
          attempts += 1;

          if (attempts > max_failed_ap_connection_attempts) {
            log_e("too many connection attempts, exiting with re-scan next");

            return;
          }
        }

        log_d("connection established with light host, swapping to esp-now");
        WiFi.disconnect();
        setup_complete = true;
        break;
      }
    }

    if (!setup_complete) {
      log_e("no light host network found, exiting");
      return;
    }

    WiFi.scanDelete();

    log_d(
      "final broadcast addr: %02X:%02X:%02X:%02X:%02X:%02X",
      broadcast_address[0],
      broadcast_address[1],
      broadcast_address[2],
      broadcast_address[3],
      broadcast_address[4],
      broadcast_address[5]
    );

    log_d("sleeping for a 5 seconds before initializing esp now");
    delay(5000);
    log_d("awake, starting esp-now");

    if (esp_now_init() != ESP_OK) {
      log_e("unable to initialize esp_now");
      mode = ERuntimeMode::FAILED;
      return;
    }

    esp_now_register_send_cb(sent_cb);

    memcpy(peer_info.peer_addr, broadcast_address, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;

    if (esp_now_add_peer(&peer_info) != ESP_OK){
      mode = ERuntimeMode::FAILED;
      log_e("Failed to add peer");
      return;
    }

    failed_send_count = 0;
    mode = ERuntimeMode::CONNECTED;
    return;
  }

  delay(10);

#ifndef SWAP_XY_POSITION
  int32_t raw_x = analogRead(X_AXIS_PIN);
  int32_t raw_y = analogRead(Y_AXIS_PIN);
#else
  int32_t raw_y = analogRead(X_AXIS_PIN);
  int32_t raw_x = analogRead(Y_AXIS_PIN);
#endif

  auto y_position = raw_y > X_TOLERANCE_UPPER
    ? 1
    : (raw_y < X_TOLERANCE_LOWER ? 2 : 0);
  auto x_position = raw_x > X_TOLERANCE_UPPER
    ? 1
    : (raw_x < X_TOLERANCE_LOWER ? 2 : 0);

  // TODO(hardware-understanding) The push button switch appears to be normally closed when tested
  // by a voltmeter (the voltmeter reads "open loop" (0L) until pressed). 
  //
  // Assuming that is true, it is not immediately clear why the `digitalRead` would be returning `1`
  // while unpressed and `0` when pressed; it is likely something is being misunderstood.
  int32_t z_position = digitalRead(Z_BUTTON_PIN);
  uint8_t normalized_z = 0;

#ifdef BUTTON_NORMAL_OPEN
  if (z_position == 0) {
    normalized_z = 1;
  }
#else
  if (z_position == 1) {
    normalized_z = 1;
  }
#endif

  memset(message_payload.content, '\0', 40);
  sprintf(message_payload.content, "[%d|%d|%d]", x_position, y_position, normalized_z);

  esp_err_t result = esp_now_send(broadcast_address, (uint8_t *) &message_payload, sizeof(message_payload));

  failed_send_count += result == 0 ? 0 : failed_send_count + 1;

  if (failed_send_count > max_failed_message_sends) {
    log_e("too many failed message attempts, moving to disconnect");
    esp_now_deinit();
    WiFi.disconnect();
    mode = ERuntimeMode::DISCONNECTED;
    return;
  }

  if (now - last_debug_log > 1000) {
    log_e(
      "frame (%d, %d, %d) '%s' result: %d (sent to %02X:%02X:%02X:%02X:%02X:%02X)",
      x_position,
      y_position,
      z_position,
      message_payload.content,
      result,
      broadcast_address[0],
      broadcast_address[1],
      broadcast_address[2],
      broadcast_address[3],
      broadcast_address[4],
      broadcast_address[5]
    );
    last_debug_log = now;
  }
}
