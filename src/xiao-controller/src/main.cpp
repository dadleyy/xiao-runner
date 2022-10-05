#include <Arduino.h>
#include "WiFi.h"
#include "esp_now.h"

#define X_AXIS_PIN A0
#define Y_AXIS_PIN A1
#define Z_BUTTON_PIN A2

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

// MAC address of beetle-lights.
constexpr const uint8_t broadcast_address[] = {0xA0, 0x76, 0x4E, 0x44, 0xFA, 0x8C};

esp_now_peer_info_t peer_info;
message_payload_t message_payload;
bool setup_complete = false;
uint32_t last_debug_log = 0;

// TODO: is this empty callback necessary?
void sent_cb(const uint8_t* mac_addr, esp_now_send_status_t status) {}

void setup(void) {
  Serial.begin(115200);

  delay(1000);

  pinMode(Z_BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(Z_BUTTON_PIN, HIGH);

  WiFi.mode(WIFI_MODE_STA);
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    log_e("unable to initialize esp_now");
    setup_complete = false;
    return;
  }

  esp_now_register_send_cb(sent_cb);

  memcpy(peer_info.peer_addr, broadcast_address, 6);
  peer_info.channel = 0;
  peer_info.encrypt = false;

  if (esp_now_add_peer(&peer_info) != ESP_OK){
    log_e("Failed to add peer");
    setup_complete = false;
    return;
  }

  setup_complete = true;
  last_debug_log = millis();
  log_d("setup complete");
}

void loop(void) {
  auto now = millis();
  delay(10);

  if (setup_complete == false) {
    return;
  }

  int32_t x_position = analogRead(X_AXIS_PIN);
  int32_t y_position = analogRead(Y_AXIS_PIN);

  // TODO(hardware-understanding) The push button switch appears to be normally closed when tested
  // by a voltmeter (the voltmeter reads "open loop" (0L) until pressed). 
  //
  // Assuming that is true, it is not immediately clear why the `digitalRead` would be returning `1`
  // while unpressed and `0` when pressed; it is likely something is being misunderstood.
  int32_t z_position = digitalRead(Z_BUTTON_PIN);
  uint8_t normalized_z = 0;
  if (z_position == 1) {
    normalized_z = 1;
  }

  memset(message_payload.content, '\0', 40);
  sprintf(message_payload.content, "[%d|%d|%d]", x_position, y_position, normalized_z);

  esp_err_t result = esp_now_send(broadcast_address, (uint8_t *) &message_payload, sizeof(message_payload));

  if (now - last_debug_log > 1000) {
    log_e("frame (%d, %d, %d) '%s' result: %d", x_position, y_position, z_position, message_payload.content, result);
    last_debug_log = now;
  }
}
