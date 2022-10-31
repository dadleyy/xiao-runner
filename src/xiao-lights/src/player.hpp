#pragma once

#include "esp32-hal-log.h"
#include <memory>
#include <vector>

#include "timer.hpp"
#include "types.hpp"

class Player final {
  public:
    static const uint32_t PLAYER_MOVEMENT_SPEED = 10;
    static const uint32_t PLAYER_DEBUFF_DURATION = 2000;
    static const uint32_t PLAYER_ATTACK_DURATION = 1000;
    static const uint32_t OBJECT_BUFFER_SIZE = 2;
    constexpr static const std::tuple<uint8_t, uint8_t, uint8_t> ATTACKING_COLOR = std::make_tuple(0, 255, 0);
    constexpr static const std::tuple<uint8_t, uint8_t, uint8_t> IDLE_COLOR = std::make_tuple(255, 255, 255);
    constexpr static const std::tuple<uint8_t, uint8_t, uint8_t> RECOVERING_COLOR = std::make_tuple(10, 180, 255);

    Player():
      _data(new std::vector<Light>(0)),
      _position(0),
      _direction(Direction::IDLE),
      _kind(PlayerStateKind::IDLE),
      _movement_timer(new xr::Timer(10)),
      _idle_timer(new xr::Timer(PLAYER_DEBUFF_DURATION)) {
      _data->reserve(OBJECT_BUFFER_SIZE);
    }

    ~Player() = default;

    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    Player(const Player&& other):
      _data(std::move(other._data)),
      _position(other._position),
      _direction(other._direction),
      _kind(other._kind),
      _movement_timer(std::move(other._movement_timer)),
      _idle_timer(std::move(other._idle_timer))
      { }

    Player& operator=(const Player&& other) {
      _data = std::move(other._data);
      _position = other._position;
      _direction = other._direction;
      _kind = other._kind;

      _movement_timer = std::move(other._movement_timer);
      _idle_timer = std::move(other._idle_timer);
      return *this;
    }

    std::vector<Light>::const_iterator light_begin() const {
      return _data->cbegin();
    }

    std::vector<Light>::const_iterator light_end() const {
      return _data->cend();
    }

    std::tuple<const Player, FrameMessage> frame(
      uint32_t current_time,
      const std::optional<ControllerInput>& input
    ) const && {
      _data->clear();

      // Tick our movement timer; if it has run out we will be able to move.
      auto [next_player_movement_timer, did_move] = std::move(*_movement_timer).tick(current_time);
      _movement_timer = did_move
        ? std::make_unique<xr::Timer>(PLAYER_MOVEMENT_SPEED)
        : std::make_unique<xr::Timer>(std::move(next_player_movement_timer));

      // Tick our idler timer; if it has run out we will be able to move into attack.
      auto [next_idle, has_acted] = std::move(*_idle_timer).tick(current_time);
      _idle_timer = has_acted
        ? std::make_unique<xr::Timer>(PLAYER_DEBUFF_DURATION)
        : std::make_unique<xr::Timer>(std::move(next_idle));

      // If we were recovering but now we're idle, update our state.
      if (_kind == PlayerStateKind::RECOVERING && has_acted) {
        _kind = PlayerStateKind::IDLE;
      }

      if (_kind == PlayerStateKind::ATTACKING && has_acted) {
        log_d("attack complete (duration %d) at time %d", PLAYER_ATTACK_DURATION, time);
        _kind = PlayerStateKind::RECOVERING;
        _idle_timer = std::make_unique<xr::Timer>(PLAYER_DEBUFF_DURATION);
      }

      // If we have an input message and it is above our threshold and we aren't already attacking,
      // update our state and kick off our action frames.
      if (input != std::nullopt && std::get<2>(*input) > 0 && _kind == PlayerStateKind::IDLE) {
        log_d("starting attack (duration %d) at time %d", PLAYER_ATTACK_DURATION, time);
        _kind = PlayerStateKind::ATTACKING;
        _idle_timer = std::make_unique<xr::Timer>(PLAYER_ATTACK_DURATION);
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

        // Controller inputs:
        //
        // - `0` -> neutral position
        // - `1` -> right position
        // - `2` -> left position
        if (x_input == 1) {
          if (_direction != Direction::RIGHT) {
            log_d("moving right (%d)", x_input);
          }
          _direction = Direction::RIGHT;
        } else if (x_input == 2) {
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

      switch (_kind) {
        case PlayerStateKind::ATTACKING: {
          auto [red, green, blue] = ATTACKING_COLOR;
          _data->push_back(std::make_tuple(_position, red, green, blue));
          break;
        }
        case PlayerStateKind::RECOVERING: {
          auto [red, green, blue] = RECOVERING_COLOR;
          _data->push_back(std::make_tuple(_position, red, green, blue));
          break;
        }
        default: {
          auto [red, green, blue] = IDLE_COLOR;
          _data->push_back(std::make_tuple(_position, red, green, blue));
          break;
        }
      }

      return std::make_tuple(
        std::move(*this),
        PlayerMovement { _position, _kind == PlayerStateKind::ATTACKING }
      );
    }

  private:
    enum PlayerStateKind {
      IDLE,
      ATTACKING,
      DEAD,
      RECOVERING,
    };

    mutable std::unique_ptr<std::vector<Light>> _data;

    mutable uint32_t _position;
    mutable Direction _direction;
    mutable PlayerStateKind _kind;

    mutable std::unique_ptr<xr::Timer> _movement_timer;
    mutable std::unique_ptr<xr::Timer> _idle_timer;
};
