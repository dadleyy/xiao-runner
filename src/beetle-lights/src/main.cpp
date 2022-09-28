#include <Arduino.h>
#include "WiFi.h"
#include "Adafruit_NeoPixel.h"
#include "esp_now.h"

#include "timer.hpp"
#include "level.hpp"

//
// Beetle Lights
//
// This is the "server" side of the architecture.
//
// Pin definitions based on current hardware: Teyleten ESP32
// https://www.amazon.com/gp/product/B08246MCL5
//

constexpr const uint32_t num_pixels = 280;
constexpr const uint32_t pixel_pin = D0;
// constexpr const uint32_t busy_pin = 2;
constexpr const uint32_t debug_timer_time_ms = 500;

// globals.
struct MessagePayload {
  char content[120];
};
static MessagePayload frame_payload;
static bool setup_complete = false;
static Adafruit_NeoPixel pixels(num_pixels, pixel_pin);
static std::optional<std::tuple<uint32_t, uint32_t, uint8_t>> last_input = std::nullopt;

static const beetle_lights::Level current_level;
static uint16_t current_level_index = 0;
static const beetle_lights::Timer debug_timer(debug_timer_time_ms);

extern const char level_data_start[] asm("_binary_embed_levels_txt_start");
extern const char level_data_end[] asm("_binary_embed_levels_txt_end");

std::tuple<uint32_t, uint32_t, uint8_t> parse_message(const char* data, int max_len) {
  const char * head = data + 0;
  uint32_t left = 0;
  uint32_t right = 0;
  uint32_t up = 0;
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

    if (*head == '|' && stage == 2) {
      stage = 3;
      head++;
    }

    if (stage == 1) {
      left = (left * 10) + (*head - '0');
    } else if (stage == 2) {
      right = (right * 10) + (*head - '0');
    } else if (stage == 3) {
      up = (up * 10) + (*head - '0');
    }

    head++;
  }

  return std::make_tuple(left, right, up);
}

void receive_cb(const uint8_t * mac, const uint8_t *incoming_data, int len) {
  memset(frame_payload.content, '\0', 120);
  memcpy(&frame_payload, incoming_data, sizeof(frame_payload));
  last_input = parse_message(frame_payload.content, len);
}

void setup(void) {
  Serial.begin(115200);

  pinMode(pixel_pin, OUTPUT);

  pixels.begin();
  pixels.setBrightness(20);
  pixels.fill(Adafruit_NeoPixel::Color(0, 0, 0));
  pixels.show();

  for (uint8_t i = 0; i < 10; i++) {
    log_d("booting %d", i);

    pixels.setBrightness(20);
    pixels.fill(i % 2 == 0 ? Adafruit_NeoPixel::Color(0, 0, 0) : Adafruit_NeoPixel::Color(0, 255, 0));
    pixels.show();

    delay(200);
  }

  // TODO(level_parsing): what we'd like to do here is get a pointer to the location within our
  // `levels.txt` file for a specific level, including the amount of characters in that level
  // definition. We're duplicating this logic below.
  const char * head = level_data_start;
  while (*head != '\n') {
    head++;
  }
  uint8_t level_size = 0;
  const char *level = (++head);
  while (*level != '\n') {
    level_size ++;
    level++;
  }
  log_d("first level: '%.*s'", level_size, head);
  current_level = beetle_lights::Level(head, level_size);

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
  auto now = millis();
  auto [next_timer, is_done] = std::move(debug_timer).tick(now);
  debug_timer = std::move(next_timer);

  if (is_done) {
    auto stack_size = uxTaskGetStackHighWaterMark(NULL);
    log_d("memory: %d (max %d) (stack %d)", ESP.getFreeHeap(), ESP.getMaxAllocHeap(), stack_size);
    debug_timer = beetle_lights::Timer(debug_timer_time_ms);
  }

  current_level = std::move(current_level).update(last_input, millis());
  last_input = std::nullopt;

  pixels.fill(Adafruit_NeoPixel::Color(0, 0, 0));

  for (auto start = current_level.first_light(); start != current_level.last_light(); start++) {
    if (*start == std::nullopt) {
      continue;
    }

    auto [led_index, colors] = start->value();
    pixels.setPixelColor(led_index, Adafruit_NeoPixel::Color(colors[0], colors[1], colors[2]));
  }

  pixels.show();

  // TODO(level_parsing)
  if (current_level.is_complete()) {
    current_level_index = current_level_index == 0 ? 1 : 0;
    uint8_t level_index = 0;
    const char * head = level_data_start;
    while (level_index < current_level_index + 1) {
      while (*head != '\n') {
        head++;
      }
      log_d("skipping level %d", level_index);
      level_index = level_index + 1;

      if (head != level_data_end) {
        head++;
      }
    }
    uint8_t level_size = 0;
    const char *level = (++head);
    while (*level != '\n') {
      level_size ++;
      level++;
    }
    log_d("level %d: '%.*s'", current_level_index, level_size, head);
    current_level = beetle_lights::Level(head, level_size);
  }
}
