#pragma once

#include <optional>
#include <variant>
#include "timer.hpp"
#include "renderable.hpp"

namespace beetle_lights {
  const static uint32_t PLAYER_ATTACK_DURATION = 3000;
  const static uint32_t PLAYER_DEBUFF_DURATION = 3000;

  // The player movement speed is represented here as the amount of milliseconds it takes the
  // player to move a single tile.
  const static uint32_t PLAYER_MOVEMENT_SPEED = 33;

  class PlayerState final {
    public:
      PlayerState() = default;
      ~PlayerState() = default;

      PlayerState& operator=(const PlayerState&) = delete;
      PlayerState(const PlayerState&) = delete;
      PlayerState& operator=(PlayerState&) = delete;
      PlayerState(PlayerState&) = delete;

      // @kind MovementAssigment
      const PlayerState& operator=(const PlayerState&& other) const noexcept {
        this->_kind = other._kind;
        this->_direction = other._direction;
        this->_position = other._position;
        this->_attack_timer = std::move(other._attack_timer);
        this->_idle_timer = std::move(other._idle_timer);
        this->_movement_timer = std::move(other._movement_timer);
        return *this;
      }

      PlayerState(const PlayerState&& other):
        _kind(other._kind),
        _direction(other._direction),
        _position(other._position),
        _attack_timer(std::move(other._attack_timer)),
        _idle_timer(std::move(other._idle_timer)),
        _movement_timer(std::move(other._movement_timer))
        {}

      const std::pair<PlayerState, Renderable> update(
        std::optional<std::pair<uint32_t, uint32_t>> &input,
        uint32_t time
      ) const && noexcept {
        if (_kind == PlayerStateKind::DEAD) {
          return std::make_pair(std::move(*this), std::make_pair(_position, this->color()));
        }

        // Tick our idler timer; if it has run out we will be able to move into attack.
        auto [next_idle, has_idled] = std::move(_idle_timer).tick(time);
        _idle_timer = std::move(next_idle);

        // Tick our movement timer; if it has run out we will be able to move.
        auto [next_player_movement_timer, did_move] = std::move(_movement_timer).tick(time);
        _movement_timer = did_move ? Timer(PLAYER_MOVEMENT_SPEED) : std::move(next_player_movement_timer);

        // If we have an input message and it is above our threshold and we aren't already attacking,
        // update our state and kick off our action frames.
        if (input != std::nullopt && std::get<1>(*input) > 2000 && _kind == PlayerStateKind::IDLE && has_idled) {
          _kind = PlayerStateKind::ATTACKING;
          _attack_timer = Timer(PLAYER_ATTACK_DURATION);
        }

        // Update our position
        if (_direction == PlayerMovementKinds::FORWARD) {
          _position = _position >= 100 ? _position : _position + 1;
        } else if (_direction == PlayerMovementKinds::BACK) {
          _position = _position > 0 ? _position - 1 : _position;
        }

        // Update the direction of the player if we had a valid input this update.
        if (input != std::nullopt) {
          auto x_input = std::get<0>(*input);

          if (x_input > 2000) {
            _direction = PlayerMovementKinds::FORWARD;
          } else if (x_input < 1000) {
            _direction = PlayerMovementKinds::BACK;
          } else {
            _direction = PlayerMovementKinds::NONE;
          }
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

        return std::make_pair(std::move(*this), std::make_pair(_position, this->color()));
      }

      const bool is_attacking(void) const {
        return _kind == PlayerStateKind::ATTACKING;
      }

      const bool is_dead(void) const {
        return _kind == PlayerStateKind::DEAD;
      }

      const std::array<uint8_t, 3> color(void) const {
        if (is_attacking()) {
          return { 0, 255, 0 };
        }

        return { 255, 255, 255 };
      }

    private:
      enum PlayerStateKind {
        IDLE,
        ATTACKING,
        DEAD,
      };


      enum PlayerMovementKinds {
        NONE,
        FORWARD,
        BACK,
      };

      mutable PlayerStateKind _kind = PlayerStateKind::IDLE;
      mutable PlayerMovementKinds _direction = PlayerMovementKinds::NONE;
      mutable uint32_t _position = 0;
      const Timer _attack_timer = Timer(PLAYER_ATTACK_DURATION);
      const Timer _idle_timer = Timer(PLAYER_DEBUFF_DURATION);
      const Timer _movement_timer = Timer(PLAYER_MOVEMENT_SPEED);
  };
}
