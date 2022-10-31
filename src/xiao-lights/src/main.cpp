#include <Arduino.h>
#include "WiFi.h"
#include "esp_now.h"
#include "Adafruit_NeoPixel.h"
#include "esp32-hal-log.h"

#include <memory>
#include <variant>

#include "timer.hpp"
#include "types.hpp"
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

enum ERuntimeMode {
  RUNNING,
  DISCONNECTED,
  FAILED
};

static const uint32_t debug_timer_ms = 2000;
static const uint32_t max_nomessage_time = 10000;

// Every message received by our esp-now listener will update this gloval state.
static MessagePayload frame_payload;

// Once received the esp now messages will be parsed into an optional controller input that will be
// sent into every frame of our game logic.
static std::optional<ControllerInput> last_input = std::nullopt;

// Current level and indices into our embedded memory for where levels exist.
static std::vector<std::pair<const char *, uint32_t>> level_indices;
static std::unique_ptr<const Level> current_level(nullptr);
static uint32_t current_level_index = 0;

static std::unique_ptr<xr::Timer> debug_timer(nullptr);
static Adafruit_NeoPixel pixels(num_pixels, pixel_pin);

// Disconnected state.
static uint32_t active_wifi_connections = 0;
static ERuntimeMode mode = ERuntimeMode::DISCONNECTED;
static uint32_t last_message_time = 0;

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
  last_message_time = millis();
  last_input = parse_message(frame_payload.content, len);
}

void on_connect(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
    log_d("client disconnected");

    if (active_wifi_connections != 0) {
      active_wifi_connections -= 1;
    }

    return;
  }

  log_d("client connected");
  active_wifi_connections += 1;
}

void setup(void) {
  Serial.begin(115200);
  log_d("setup");
  level_indices.reserve(10);
  delay(1000);

  log_d("initializing game engine");
  debug_timer = std::make_unique<xr::Timer>(debug_timer_ms);
  pixels.begin();
  pixels.setBrightness(20);
  pixels.fill(Adafruit_NeoPixel::Color(0, 0, 0));
  pixels.show();

  const char *cursor = level_data_start, *current_level_cursor = level_data_start;
  uint32_t level_size = 0;

  while (cursor != level_data_end) {
    if (*cursor != '\n' || *cursor == ':') {
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
  log_d("setup complete");
}

void loop(void) {
  // While we haven't had a client connected for some time, our loop in a single frame until we receive one
  // connection to our access point.
  if (mode == ERuntimeMode::DISCONNECTED) {
    pixels.fill(Adafruit_NeoPixel::Color(0, 0, 0));
    pixels.show();

    // Start our wifi access point
    WiFi.mode(WIFI_AP);

    // Register some callbacks so we know when a client connects.
    WiFi.onEvent(on_connect, ARDUINO_EVENT_WIFI_AP_STACONNECTED);
    WiFi.onEvent(on_connect, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);

    // Create the SSID; start broadcasting.
    bool result = WiFi.softAP("xiao-runner-light-host", "lights-host", 1, 0);

    if (!result) {
      mode = ERuntimeMode::FAILED;
      log_e("unable to start in soft ap mode");
      return;
    }

    // Wait until we have a connection; this will block the current `loop` until there is a connection.
    while (active_wifi_connections == 0) {
      auto now = millis();
      auto [new_timer, did_finish] = std::move(*debug_timer).tick(now);
      debug_timer = did_finish
        ? std::make_unique<xr::Timer>(debug_timer_ms)
        : std::make_unique<xr::Timer>(std::move(new_timer));

      if (did_finish) {
        log_d("still waiting for connection...");
        Serial.println(WiFi.macAddress());
        log_d("^-- my mac address");
      }
    }

    // Before terminating our access point, sleep for a small amount of time to allow things to settle.
    log_d("sleeping for a second to allow wifi to settle before terminating access point");
    delay(1000);

    // Remove callbacks, stop access point.
    WiFi.disconnect();
    log_d("sleeping for a second to settle with disconnected wifi");
    delay(200);
    log_d("awake, starting esp-now");

    // Switch into station mode, print our mac address and start esp-now.
    WiFi.mode(WIFI_MODE_STA);
    log_d("[WIFI] initializing wifi, my mac address is:");
    Serial.println(WiFi.macAddress());
    log_d("^ mac address");

    if (esp_now_init() != ESP_OK) {
      mode = ERuntimeMode::FAILED;
      log_e("unable to initialize esp_now");
      return;
    }

    // Move into our "running" state
    log_d("esp-now ready, client should be connecting soon");
    last_message_time = millis();
    esp_now_register_recv_cb(receive_cb);
    mode = ERuntimeMode::RUNNING;
    return;
  }

  if (current_level == nullptr || mode == ERuntimeMode::FAILED) {
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

  if (now > last_message_time && last_message_time > 0 && now - last_message_time > max_nomessage_time) {
    log_e("message not received in a while, moving to disconnected");
    esp_now_deinit();
    mode = ERuntimeMode::DISCONNECTED;
  }
}
