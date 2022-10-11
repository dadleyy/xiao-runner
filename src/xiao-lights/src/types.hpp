#pragma once

enum Direction {
  LEFT,
  RIGHT,
  IDLE
};

using Light = std::tuple<uint32_t, uint8_t, uint8_t, uint8_t>;
using ControllerInput = std::tuple<uint32_t, uint32_t, uint8_t>;


struct PlayerMovement final {
  uint32_t position;
  bool attacking;
};

struct ObstacleCollision final {
  uint32_t position;
};

struct GoalReached final {
};

using FrameMessage = std::variant<PlayerMovement, ObstacleCollision, GoalReached>;
