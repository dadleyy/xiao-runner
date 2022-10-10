#pragma once

enum Direction {
  LEFT,
  RIGHT,
  IDLE
};

using Light = std::tuple<uint32_t, uint8_t, uint8_t, uint8_t>;
using ControllerInput = std::tuple<uint32_t, uint32_t, uint8_t>;

constexpr const uint32_t CONTAINER_BUFFER_SIZE = 256;
constexpr const uint32_t OBJECT_BUFFER_SIZE = 30;
constexpr const uint32_t OBSTACLE_BUFFER_SIZE = 15;
