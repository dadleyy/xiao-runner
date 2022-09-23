#include <Arduino.h>
#include "WiFi.h"
#include "Adafruit_NeoPixel.h"
#include "esp_now.h"

#include "engine.hpp"

//
// Beetle Lights
//
// This is the "server" side of the architecture.
//
// Pin definitions based on current hardware: Teyleten ESP32
// https://www.amazon.com/gp/product/B08246MCL5
//

constexpr const uint32_t num_pixels = 280;
constexpr const uint32_t pixel_pin = 27;
constexpr const uint32_t busy_pin = 2;

// globals.
struct MessagePayload {
  char content[120];
};
static MessagePayload frame_payload;
static bool setup_complete = false;
static Adafruit_NeoPixel pixels(num_pixels, pixel_pin);
static std::optional<std::pair<uint32_t, uint32_t>> last_input = std::nullopt;

static const beetle_lights::Level current_level;

std::pair<uint32_t, uint32_t> parse_message(const char* data, int max_len) {
  const char * head = data + 0;
  uint32_t left = 0;
  uint32_t right = 0;
  uint8_t stage = 0;
  uint8_t cursor = 0;

  while (*head != ']' && cursor < max_len && cursor < 255) {
    cursor += 1;

    if (*head == '[' && stage == 0) {
      stage = 1;
      head++;
    } 

    if (*head == '|' && stage == 1) {
      stage = 2;
      head++;
    }

    if (stage == 1) {
      left = (left * 10) + (*head - '0');
    } else if (stage == 2) {
      right = (right * 10) + (*head - '0');
    }

    head++;
  }

  return std::make_pair(left, right);
}

void receive_cb(const uint8_t * mac, const uint8_t *incoming_data, int len) {
  memset(frame_payload.content, '\0', 120);
  memcpy(&frame_payload, incoming_data, sizeof(frame_payload));
  last_input = parse_message(frame_payload.content, len);
}

void setup(void) {
  Serial.begin(115200);
  pinMode(pixel_pin, OUTPUT);
  pinMode(busy_pin, OUTPUT);

  pixels.begin();
  pixels.setBrightness(20);
  pixels.fill(Adafruit_NeoPixel::Color(0, 0, 0));
  pixels.show();

  for (uint8_t i = 0; i < 10; i++) {
    log_d("booting %d", i);
    digitalWrite(busy_pin, i % 2 == 0 ? HIGH : LOW);
    delay(200);
  }

  WiFi.mode(WIFI_MODE_STA);
  log_d("setup complete");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    log_e("unable to initialize esp_now");
    setup_complete = false;
    return;
  }

  esp_now_register_recv_cb(receive_cb);
}

void loop(void) {
  auto [next_level, renderables] = std::move(current_level).update(last_input, millis());
  last_input = std::nullopt;

  pixels.fill(Adafruit_NeoPixel::Color(0, 0, 0));

  if (renderables.size() > 0) {
    for (auto start = renderables.cbegin(); start != renderables.cend(); start++) {
      auto led_index = std::get<0>(*start);
      auto colors = std::get<1>(*start);
      pixels.setPixelColor(led_index, Adafruit_NeoPixel::Color(colors[0], colors[1], colors[2]));
    }
  }
  pixels.show();

  current_level = std::move(next_level);
  if (current_level.is_complete()) {
    log_d("new level");
    current_level = beetle_lights::Level();
  }
}
