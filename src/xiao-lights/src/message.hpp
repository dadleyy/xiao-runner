#pragma once

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
