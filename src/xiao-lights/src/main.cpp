#include <Arduino.h>
#include "WiFi.h"
#include "esp_now.h"
#include "Adafruit_NeoPixel.h"
#include "esp32-hal-log.h"

#include <memory>
#include <variant>

#include "timer.hpp"
#include "message.hpp"
#include "player.hpp"
#include "obstacle.hpp"
#include "level.hpp"

#ifndef NUM_PIXELS
constexpr const uint32_t num_pixels = 146;
#else
constexpr const uint32_t num_pixels = NUM_PIXELS;
#endif
constexpr const uint32_t pixel_pin = D0;

extern const char level_data_start[] asm("_binary_embed_levels_txt_start");
extern const char level_data_end[] asm("_binary_embed_levels_txt_end");

struct MessagePayload final {
  char content[120];
};

static MessagePayload frame_payload;
static std::optional<ControllerInput> last_input = std::nullopt;
static std::unique_ptr<const Level> current_level(nullptr);
static const uint32_t debug_timer_ms = 2000;
static std::unique_ptr<xr::Timer> debug_timer(nullptr);
static std::vector<std::pair<const char *, uint32_t>> level_indices;
static uint32_t current_level_index = 0;
static Adafruit_NeoPixel pixels(num_pixels, pixel_pin);

// The messages sent from the `beetle-controller` are received and parsed into a tuple containing
// three unsigned integer values - x, y and z (button press).
ControllerInput parse_message(const char* data, int max_len) {
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
  log_d("setup");
  level_indices.reserve(10);
  delay(1000);

  log_d("initializing game engine");
  pixels.begin();
  pixels.setBrightness(20);
  pixels.fill(Adafruit_NeoPixel::Color(0, 0, 0));
  pixels.show();

  const char *cursor = level_data_start, *current_level_cursor = level_data_start;
  uint32_t level_size = 0;

  while (cursor != level_data_end) {
    if (*cursor != '\n') {
      level_size++;
      cursor++;
      continue;
    }

    log_d("found level %.*s", level_size, current_level_cursor);
    level_indices.push_back(std::make_pair(current_level_cursor, level_size));
    cursor++;
    current_level_cursor = cursor;
    level_size = 0;
  }

  current_level = std::make_unique<Level>(
    level_indices.size() > 0 ? Level{ level_indices[current_level_index], num_pixels } : Level {}
  );

  log_d("initializing wifi");
  // WiFi/esp-now initialization.
  WiFi.mode(WIFI_MODE_STA);
  Serial.println(WiFi.macAddress());
  if (esp_now_init() != ESP_OK) {
    log_e("unable to initialize esp_now");
    return;
  }
  esp_now_register_recv_cb(receive_cb);
  debug_timer = std::make_unique<xr::Timer>(debug_timer_ms);
  log_d("setup complete");
}

void loop(void) {
  if (current_level == nullptr) {
    delay(1000);
    log_e("no current level");
    return;
  }
  pixels.fill(Adafruit_NeoPixel::Color(0, 0, 0));

  auto now = millis();
  auto [new_timer, did_finish] = std::move(*debug_timer).tick(now);
  debug_timer = did_finish
    ? std::make_unique<xr::Timer>(debug_timer_ms)
    : std::make_unique<xr::Timer>(std::move(new_timer));

  if (did_finish) {
    auto stack_size = uxTaskGetStackHighWaterMark(NULL);
    log_d("memory: %d (max %d) (stack %d)", ESP.getFreeHeap(), ESP.getMaxAllocHeap(), stack_size);
  }

  current_level = std::make_unique<Level>(std::move(*current_level).frame(now, last_input));
  last_input = std::nullopt;
  auto next = current_level->state();

  if (next != Level::LevelStateKind::IN_PROGRESS) {
    auto new_level_index = next == Level::LevelStateKind::COMPLETE
      ? current_level_index + 1
      : 0;

    if (new_level_index > level_indices.size() - 1) {
      new_level_index = 0;
    }

    log_d("level %d complete, moving to next level %d", current_level_index, new_level_index);
    current_level_index = new_level_index;
    current_level = std::make_unique<Level>(Level{ level_indices[current_level_index], num_pixels });
  }

  for (auto light = current_level->light_begin(); light != current_level->light_end(); light++) {
    auto [pos, red, green, blue] = *light;
    pixels.setPixelColor(pos, Adafruit_NeoPixel::Color(red, green, blue));
  }

  pixels.show();
}
