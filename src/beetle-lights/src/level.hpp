#pragma once

#include <variant>
#include <optional>
#include <vector>
#include <numeric>
#include <array>
#include "esp32-hal-log.h"

#include "renderable.hpp"
#include "timer.hpp"
#include "obstacle.hpp"
#include "player.hpp"

namespace beetle_lights {

  class Level final {
      public:
        Level():
          _player_position(0),
          _player_direction(PlayerMovementKinds::NONE),
          _player_movement_timer(Timer(PLAYER_MOVEMENT_SPEED)),
          _player_state(PlayerState()),
          _obstacles()
        {
          log_d("allocating new memory for a level");
          _obstacles.push_back(Obstacle(10));
        };

        Level(Level&) = delete;
        Level& operator=(Level&) = delete;
        Level(const Level&) = delete;
        Level& operator=(const Level&) = delete;

        Level(const Level&& other):
          _player_position(other._player_position),
          _player_direction(other._player_direction),
          _player_movement_timer(std::move(other._player_movement_timer)),
          _player_state(std::move(other._player_state)) {
            _obstacles.swap(other._obstacles);
          }

        // @kind MovementAssigment
        const Level& operator=(const Level&& other) const noexcept {
          this->_player_position = other._player_position;
          // this->_player_movement_timer = std::move(other._player_movement_timer);
          this->_player_direction = other._player_direction;
          this->_player_state = std::move(other._player_state);

          this->_obstacles.swap(other._obstacles);

          return *this;
        }

        bool is_complete(void) const noexcept {
          return _player_direction == PlayerMovementKinds::DEAD;
        }

        // Level update
        //
        // ...
        const std::pair<Level &&, std::vector<Renderable> &&> update(
          std::optional<std::pair<uint32_t, uint32_t>> &input,
          uint32_t time
        ) const && noexcept {
          std::vector<Renderable> lights;

          if (_player_direction != PlayerMovementKinds::DEAD) {
            // Check to see if we can move the player. If so, reset our timer.
            auto [next_player_movement_timer, did_move] = std::move(_player_movement_timer).tick(time);
            _player_movement_timer = did_move ? Timer(PLAYER_MOVEMENT_SPEED) : std::move(next_player_movement_timer);

            // If we moved, calculate the new player position.
            if (did_move) {
              if (_player_direction == PlayerMovementKinds::FORWARD) {
                _player_position = _player_position >= 100 ? _player_position : _player_position + 1;
              } else if (_player_direction == PlayerMovementKinds::BACK) {
                _player_position = _player_position > 0 ? _player_position - 1 : _player_position;
              }
            }

            // Update the state of the player.
            _player_state = std::move(_player_state).update(input, time);

            // Update the direction of the player if we had a valid input this update.
            if (input != std::nullopt) {
              auto x_input = std::get<0>(*input);

              if (x_input > 2000) {
                _player_direction = PlayerMovementKinds::FORWARD;
              } else if (x_input < 1000) {
                _player_direction = PlayerMovementKinds::BACK;
              } else {
                _player_direction = PlayerMovementKinds::NONE;
              }
            }
          }

          // Iterate over our obstacles, updating them based on the player position and new state.
          for (auto start = _obstacles.cbegin(); start != _obstacles.cend(); start++) {
            auto [new_state, update] = std::move(*start).update(time, _player_position, _player_state);
            auto renderable = new_state.renderable();

            if (update) {
              _player_direction = PlayerMovementKinds::DEAD;
            }

            // Not all obstacles are renderable.
            if (renderable != std::nullopt) {
              lights.push_back(*renderable);
            }

            *start = std::move(new_state);
          }

          Renderable player_light = _player_direction == PlayerMovementKinds::DEAD
            ? std::make_pair((uint32_t) 0, (std::array<uint8_t, 3>) { 0, 0, 0 })
            : std::make_pair(_player_position, _player_state.color());

          lights.push_back(player_light);
          return std::make_pair(std::move(*this), std::move(lights));
        }

      private:
        enum PlayerMovementKinds {
          NONE,
          FORWARD,
          BACK,
          DEAD,
        };

        // A note on player state:
        //
        // The state ofthe player here has been split into basically two bits:
        //  1. The player's position and what direction it is moving.
        //  2. What the player is doing (idle vs. attack).
        mutable uint32_t _player_position;
        mutable PlayerMovementKinds _player_direction;
        const Timer _player_movement_timer;
        const PlayerState _player_state;

        mutable std::vector<Obstacle> _obstacles;
  };

}
