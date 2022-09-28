#pragma once

#include <variant>

namespace beetle_lights {

  struct PlayerMovement final {
    explicit PlayerMovement(uint32_t p, bool a): position(p), is_attacking(a) {}
    ~PlayerMovement() = default;
    PlayerMovement(const PlayerMovement&) = delete;
    PlayerMovement& operator=(const PlayerMovement&) = delete;

    PlayerMovement(const PlayerMovement&& other): position(other.position), is_attacking(other.is_attacking) {}
    PlayerMovement& operator=(const PlayerMovement&& other) {
      this->is_attacking = other.is_attacking;
      this->position = other.position;
      return *this;
    }

    mutable uint32_t position;
    mutable bool is_attacking;
  };

  struct GoalReached final {
  };

  struct ObstacleCollision final {
    explicit ObstacleCollision(uint8_t i) {}
    ~ObstacleCollision() = default;
    ObstacleCollision(const ObstacleCollision&) = delete;
    ObstacleCollision& operator=(const ObstacleCollision&) = delete;

    ObstacleCollision(const ObstacleCollision&&) {}
    ObstacleCollision& operator=(const ObstacleCollision&&) {
      return *this;
    }
  };

  using Message = std::variant<PlayerMovement, ObstacleCollision, GoalReached>;

}
