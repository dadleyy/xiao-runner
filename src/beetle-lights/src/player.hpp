#pragma once

#include <optional>
#include <variant>
#include "timer.hpp"

namespace beetle_lights {
  const static uint32_t PLAYER_ATTACK_DURATION = 3000;
  const static uint32_t PLAYER_DEBUFF_DURATION = 3000;

  // The player movement speed is represented here as the amount of milliseconds it takes the
  // player to move a single tile.
  const static uint32_t PLAYER_MOVEMENT_SPEED = 33;

  class PlayerState final {
    public:
      enum PlayerStateKind {
        IDLE,
        ATTACKING,
      };

      PlayerState() = default;
      ~PlayerState() = default;

      PlayerState& operator=(const PlayerState&) = delete;
      PlayerState(const PlayerState&) = delete;
      PlayerState& operator=(PlayerState&) = delete;
      PlayerState(PlayerState&) = delete;

      // @kind MovementAssigment
      const PlayerState& operator=(const PlayerState&& other) const noexcept {
        this->_attack_timer = std::move(other._attack_timer);
        this->_idle_timer = std::move(other._idle_timer);
        this->_kind = other._kind;
        return *this;
      }

      PlayerState(const PlayerState&& other):
        _kind(other._kind),
        _attack_timer(std::move(other._attack_timer)),
        _idle_timer(std::move(other._idle_timer))
        {}

      const PlayerState update(
        std::optional<std::pair<uint32_t, uint32_t>> &input,
        uint32_t time
      ) const && noexcept {
        auto [next_idle, has_idled] = std::move(_idle_timer).tick(time);
        _idle_timer = std::move(next_idle);

        // If we have an input message and it is above our threshold and we aren't already attacking,
        // update our state and kick off our action frames.
        if (input != std::nullopt && std::get<1>(*input) > 2000 && _kind == PlayerStateKind::IDLE && has_idled) {
          _kind = PlayerStateKind::ATTACKING;
          _attack_timer = Timer(PLAYER_ATTACK_DURATION);
        }

        if (_kind == PlayerStateKind::ATTACKING) {
          // While we are attacking, continuously reset the idle timer.
          _idle_timer = Timer(PLAYER_DEBUFF_DURATION);

          auto [next, is_done] = std::move(_attack_timer).tick(time);

          if (is_done) {
            _kind = PlayerStateKind::IDLE;
          }

          _attack_timer = is_done ? std::move(next) : Timer(PLAYER_ATTACK_DURATION);
        }

        return std::move(*this);
      }

      const bool is_attacking(void) const {
        return _kind == PlayerStateKind::ATTACKING;
      }

      const std::array<uint8_t, 3> color(void) const {
        if (is_attacking()) {
          return { 0, 255, 0 };
        }

        return { 255, 255, 255 };
      }

    private:
      mutable PlayerStateKind _kind = PlayerStateKind::IDLE;
      const Timer _attack_timer = Timer(PLAYER_ATTACK_DURATION);
      const Timer _idle_timer = Timer(PLAYER_DEBUFF_DURATION);
  };
}
