#pragma once

#include <variant>
#include <optional>
#include <numeric>
#include <array>
#include "esp32-hal-log.h"

#include "renderable.hpp"
#include "timer.hpp"
#include "obstacle.hpp"
#include "player.hpp"

namespace beetle_lights {
  using LightBuffer = std::array<std::optional<Renderable>, 100>;
  using ObstacleBuffer = std::array<const Obstacle, 10>;

  class Level final {
      public:
        Level():
          _player_state(PlayerState()),
          _obstacles(new ObstacleBuffer()),
          _lights(new LightBuffer()),
          _completion_timer(100) {
          log_d("allocating new memory for a level");
        }

        explicit Level(const char * layout, uint32_t size):
          _player_state(PlayerState()),
          _obstacles(new ObstacleBuffer()),
          _lights(new LightBuffer()),
          _completion_timer(100) {
          const char * cursor = layout;
          uint32_t position = 0, obstacle_index = 0;

          while (*cursor != '\n' && position < size) {
            position += 1;

            if (*cursor != ' ') {
              log_d("found enemy at position %d (enemy #%d)", position, obstacle_index);
              (*_obstacles)[obstacle_index] = Obstacle(*cursor, position);
              obstacle_index ++;
            }

            ++cursor;
          }
        }
        ~Level() = default;

        Level(Level&) = delete;
        Level& operator=(Level&) = delete;
        Level(const Level&) = delete;
        Level& operator=(const Level&) = delete;

        Level(const Level&& other):
          _player_state(std::move(other._player_state)),
          _obstacles(std::move(other._obstacles)),
          _lights(std::move(other._lights)),
          _completion_timer(std::move(other._completion_timer))
          { }

        // @kind MovementAssigment
        const Level& operator=(const Level&& other) const noexcept {
          this->_player_state = std::move(other._player_state);
          this->_obstacles = std::move(other._obstacles);
          this->_lights = std::move(other._lights);
          this->_completion_timer = std::move(other._completion_timer);
          return *this;
        }

        const bool is_complete(void) const {
          return _completion_timer == 0;
        }

        // ...
        const Level update(
          const std::optional<std::tuple<uint32_t, uint32_t, uint8_t>> &input,
          uint32_t time
        ) const && noexcept {
          _lights->fill(std::nullopt);

          if (_completion_timer < 100) {
            _completion_timer -= 1;
            return std::move(*this);
          }

          // Update the state of the player.
          auto [new_player_state, player_update] = std::move(_player_state).update(input, time);
          _player_state = std::move(new_player_state);

          // Iterate over our obstacles, updating them based on the player position and new state.
          uint16_t frame_light_index = 0;

          for (auto obstacle_index = _obstacles->cbegin(); obstacle_index != _obstacles->cend(); obstacle_index++) {
            auto [new_state, new_player_update] = std::move(*obstacle_index).update(time, std::move(player_update));

            for (auto ob_light = new_state.cbegin(); ob_light != new_state.cend(); ob_light++) {
              if (*ob_light == std::nullopt) {
                continue;
              }

              (*_lights)[frame_light_index] = ob_light->value();
              frame_light_index += 1;
            }

            *obstacle_index = std::move(new_state);
            player_update = std::move(new_player_update);
          }

          _player_state = std::move(_player_state).apply(player_update);

          for (auto pl = _player_state.light_begin(); pl != _player_state.light_end(); pl++) {
            if (*pl == std::nullopt) {
              continue;
            }
            (*_lights)[frame_light_index] = pl->value();
            frame_light_index += 1;
          }

          if (_completion_timer < 100 || _player_state.is_dead() || std::holds_alternative<GoalReached>(player_update)) {
            _completion_timer -= 1;
          }

          return std::move(*this);
        }

        LightBuffer::const_iterator cbegin(void) const {
          return _lights->cbegin();
        }

        LightBuffer::const_iterator cend(void) const {
          return _lights->cend();
        }

      private:
        const PlayerState _player_state;
        mutable std::unique_ptr<ObstacleBuffer> _obstacles;
        mutable std::unique_ptr<LightBuffer> _lights;
        mutable uint8_t _completion_timer;
  };

}
