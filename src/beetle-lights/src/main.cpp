#include <Arduino.h>
#include "WiFi.h"
#include "Adafruit_NeoPixel.h"
#include "esp_now.h"
#include <variant>

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

struct MessagePayload {
  char content[120];
};

MessagePayload frame_payload;

uint32_t led_index = 0;
bool setup_complete = false;

// 0 -> none
// 1 -> left
// 2 -> right
uint8_t direction = 0;

struct Blinker final {
  explicit Blinker(uint32_t pos): origin(pos), position(pos), direction(1) {};

  Blinker(Blinker&) = delete;
  Blinker& operator=(Blinker&) = delete;

  Blinker(Blinker&& other): origin(other.origin), position(other.position), direction(other.direction) {};
  Blinker& operator=(Blinker&& other) {
    this->origin = other.origin;
    this->position = other.position;
    this->direction = other.direction;
    return *this;
  }

  std::optional<std::pair<uint32_t, std::array<uint32_t, 3>>> update(uint32_t player_position) {
    if (direction == 1) {
      position = position + 1;
    } else if (direction == 2) {
      position = position - 1;
    }

    bool should_swap = (position > origin && position - origin > 10)
      || (origin > position && origin - position > 10);

    direction = should_swap ? (direction == 1 ? 2 : 1) : direction;

    return std::make_pair(position, std::array<uint32_t, 3> { {255,0,0} });
  }

  uint32_t origin;
  uint32_t position;
  uint32_t direction;
};

struct Noop final {
};

struct Obstacle final {
  Obstacle(): kind(Noop()) {};
  explicit Obstacle(std::variant<Noop, Blinker> k): kind(std::move(k)) {};

  std::variant<Noop, Blinker> kind;

  std::optional<std::pair<uint32_t, std::array<uint32_t, 3>>> update(uint32_t player_position) {
    switch (kind.index()) {
      case 1: 
        return std::get_if<Blinker>(&kind)->update(player_position);
      default:
        return std::nullopt;
    }
  }
};

struct Level final {
  std::array<Obstacle, 6> obstacles;
};

//
// Hard coded level data (for now)
//

Level levels[] = {
  Level {
    obstacles: {
      Obstacle(Blinker(50)),
      Obstacle(Blinker(80)),
      Obstacle(Blinker(100)),
      Obstacle(),
      Obstacle(),
      Obstacle()
    }
  }
};
uint8_t current_level_index = 0;

// Create our LED light strip.
Adafruit_NeoPixel pixels(num_pixels, pixel_pin);

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

  auto input_coordinates = parse_message(frame_payload.content, len);

  uint32_t x_dir = std::get<0>(input_coordinates);
  if (x_dir < 1000) {
    direction = 1;
  } else if (x_dir > 2000) {
    direction = 2;
  } else {
    direction = 0;
  }
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
  pixels.fill(Adafruit_NeoPixel::Color(0, 0, 0));

  if (direction == 1) {
    led_index = led_index < 1 ? num_pixels : led_index - 1;
  } else if (direction == 2) {
    led_index = led_index == num_pixels ? 0 : led_index + 1;
  }

  auto current_level = std::move(levels[current_level_index]);
  auto obstacles = std::move(current_level.obstacles);

  for (auto start = obstacles.begin(); start != obstacles.end(); start++) {
    auto result = start->update(led_index);

    if (result.has_value()) {
      auto position = std::get<0>(*result);
      auto color = std::get<1>(*result);
      pixels.setPixelColor(position, Adafruit_NeoPixel::Color(color[0], color[1], color[2]));
    }
  }

  current_level.obstacles = std::move(obstacles);
  levels[current_level_index] = std::move(current_level);

  pixels.setPixelColor(led_index, Adafruit_NeoPixel::Color(255, 255, 255));
  pixels.show();
}
