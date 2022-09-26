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
        Level(): _player_state(PlayerState()), _obstacles(new ObstacleBuffer()), _lights(new LightBuffer()) {
          log_d("allocating new memory for a level");
        };
        explicit Level(const char * layout):
          _player_state(PlayerState()),
          _obstacles(new ObstacleBuffer()),
          _lights(new LightBuffer()) {
          const char * cursor = layout;
          uint32_t position = 0, obstacle_index = 0;

          while (*cursor != '\n') {
            position += 1;

            if (*cursor == 'x') {
              log_d("found enemy at position %d (enemy #%d)", position, obstacle_index);
              (*_obstacles)[obstacle_index] = Obstacle(position);
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
          _lights(std::move(other._lights))
          { }

        // @kind MovementAssigment
        const Level& operator=(const Level&& other) const noexcept {
          this->_player_state = std::move(other._player_state);
          this->_obstacles = std::move(other._obstacles);
          this->_lights = std::move(other._lights);
          return *this;
        }

        bool is_complete(void) const noexcept {
          return _player_state.is_dead();
        }

        // ...
        const Level update(
          const std::optional<std::tuple<uint32_t, uint32_t, uint8_t>> &input,
          uint32_t time
        ) const && noexcept {
          // Update the state of the player.
          auto [new_player_state, player_light] = std::move(_player_state).update(input, time);
          _player_state = std::move(new_player_state);

          // Iterate over our obstacles, updating them based on the player position and new state.
          uint16_t obstacle_light_index = 1;

          for (auto obstacle_index = _obstacles->cbegin(); obstacle_index != _obstacles->cend(); obstacle_index++) {
            auto [new_state, _] = std::move(*obstacle_index).update(time, _player_state);

            for (auto light_index = new_state.cbegin(); light_index != new_state.cend(); light_index++) {
              if (*light_index == std::nullopt) {
                continue;
              }

              obstacle_light_index += 1;
              (*_lights)[obstacle_light_index] = light_index->value();
            }

            *obstacle_index = std::move(new_state);
          }

          (*_lights)[0] = player_light;
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
  };

}
