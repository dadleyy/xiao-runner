#pragma once

#include <optional>
#include <array>
#include <variant>
#include "esp32-hal-log.h"
#include "timer.hpp"
#include "renderable.hpp"
#include "message.hpp"

namespace beetle_lights {
  const static uint32_t PLAYER_ATTACK_DURATION = 1000;
  const static uint32_t PLAYER_DEBUFF_DURATION = 2000;

  const static uint32_t X_TOLERANCE_MIN = 1000;
  const static uint32_t X_TOLERANCE_MAX = 3000;

  // The player movement speed is represented here as the amount of milliseconds it takes the
  // player to move a single tile.
  const static uint32_t PLAYER_MOVEMENT_SPEED = 10;

  using PlayerLightBuffer = std::array<std::optional<Renderable>, 10>;

  class PlayerState final {
    public:
      PlayerState():
        _light_buffer(new PlayerLightBuffer()),
        _kind(PlayerStateKind::IDLE),
        _direction(Direction::IDLE),
        _position(0),
        _attack_timer(Timer(PLAYER_ATTACK_DURATION)),
        _idle_timer(Timer(PLAYER_DEBUFF_DURATION)),
        _movement_timer(Timer(PLAYER_MOVEMENT_SPEED))
        {}
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
        this->_light_buffer = std::move(other._light_buffer);
        return *this;
      }

      PlayerState(const PlayerState&& other):
        _light_buffer(std::move(other._light_buffer)),
        _kind(other._kind),
        _direction(other._direction),
        _position(other._position),
        _attack_timer(std::move(other._attack_timer)),
        _idle_timer(std::move(other._idle_timer)),
        _movement_timer(std::move(other._movement_timer))
        {}

      const std::pair<PlayerState, Message> update(
        const std::optional<std::tuple<uint32_t, uint32_t, uint8_t>> &input,
        uint32_t time
      ) const && noexcept {
        _light_buffer->fill(std::nullopt);

        if (_kind == PlayerStateKind::DEAD) {
          return std::make_pair(std::move(*this), Message { std::in_place_type<PlayerMovement>, 0, false });
        }

        // Tick our idler timer; if it has run out we will be able to move into attack.
        auto [next_idle, has_idled] = std::move(_idle_timer).tick(time);
        _idle_timer = std::move(next_idle);

        // Tick our movement timer; if it has run out we will be able to move.
        auto [next_player_movement_timer, did_move] = std::move(_movement_timer).tick(time);
        _movement_timer = did_move ? Timer(PLAYER_MOVEMENT_SPEED) : std::move(next_player_movement_timer);

        // If we were recovering but now we're idle, update our state.
        if (_kind == PlayerStateKind::RECOVERING && has_idled) {
          _kind = PlayerStateKind::IDLE;
        }

        // If we have an input message and it is above our threshold and we aren't already attacking,
        // update our state and kick off our action frames.
        if (input != std::nullopt && std::get<2>(*input) > 0 && _kind == PlayerStateKind::IDLE) {
          log_d("starting attack (duration %d) at time %d", PLAYER_ATTACK_DURATION, time);
          _kind = PlayerStateKind::ATTACKING;
          _attack_timer = Timer(PLAYER_ATTACK_DURATION);
        }

        // Update our position
        if (did_move) {
          if (_direction == Direction::RIGHT) {
            _position = _position + 1;
          } else if (_direction == Direction::LEFT) {
            _position = _position > 0 ? _position - 1 : _position;
          }
        }

        // Update the direction of the player if we had a valid input this update.
        if (input != std::nullopt) {
          auto x_input = std::get<0>(*input);

          if (x_input > X_TOLERANCE_MAX) {
            if (_direction != Direction::RIGHT) {
              log_d("moving right (%d)", x_input);
            }
            _direction = Direction::RIGHT;
          } else if (x_input < X_TOLERANCE_MIN) {
            if (_direction != Direction::LEFT) {
              log_d("moving left (%d)", x_input);
            }
            _direction = Direction::LEFT;
          } else {
            if (_direction != Direction::IDLE) {
              log_d("idle (%d)", x_input);
            }
            _direction = Direction::IDLE;
          }
        }

        if (_kind == PlayerStateKind::ATTACKING) {
          // While we are attacking, continuously reset the idle timer.
          _idle_timer = Timer(PLAYER_DEBUFF_DURATION);

          auto [next, is_done] = std::move(_attack_timer).tick(time);

          if (is_done) {
            log_d("done with attack");
            _kind = PlayerStateKind::RECOVERING;
          }

          _attack_timer = !is_done ? std::move(next) : Timer(PLAYER_ATTACK_DURATION);
        }

        std::array<uint8_t, 3> color = _kind == PlayerStateKind::ATTACKING
          ? (std::array<uint8_t, 3>) { 0, 255, 0 }
          : _kind == PlayerStateKind::RECOVERING
            ? (std::array<uint8_t, 3>) { 100, 0, 100 }
            : (std::array<uint8_t, 3>) { 255, 255, 255 };

        (*_light_buffer)[0] = std::make_pair(_position, color);

        return std::make_pair(
          std::move(*this),
          Message { std::in_place_type<PlayerMovement>, _position, _kind == PlayerStateKind::ATTACKING }
        );
      }

      const PlayerLightBuffer::const_iterator light_begin() const {
        return _light_buffer->cbegin();
      }

      const PlayerLightBuffer::const_iterator light_end() const {
        return _light_buffer->cend();
      }

      const PlayerState apply(const Message &message) const && noexcept {
        if (std::holds_alternative<ObstacleCollision>(message)) {
          log_d("player is dead");
          _kind = PlayerStateKind::DEAD;
        }

        return std::move(*this);
      }

      const bool is_dead(void) const {
        return _kind == PlayerStateKind::DEAD;
      }

    private:
      enum PlayerStateKind {
        IDLE,
        ATTACKING,
        DEAD,
        RECOVERING,
      };

      mutable std::unique_ptr<PlayerLightBuffer> _light_buffer;
      mutable PlayerStateKind _kind;
      mutable Direction _direction;
      mutable uint32_t _position;
      const Timer _attack_timer;
      const Timer _idle_timer;
      const Timer _movement_timer;
  };
}
