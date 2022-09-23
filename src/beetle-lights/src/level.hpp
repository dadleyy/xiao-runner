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
        Level(): _player_state(PlayerState()), _obstacles() {
          log_d("allocating new memory for a level");
          _obstacles.push_back(Obstacle(10));
        };

        Level(Level&) = delete;
        Level& operator=(Level&) = delete;
        Level(const Level&) = delete;
        Level& operator=(const Level&) = delete;

        Level(const Level&& other): _player_state(std::move(other._player_state)) {
          _obstacles.swap(other._obstacles);
        }

        // @kind MovementAssigment
        const Level& operator=(const Level&& other) const noexcept {
          this->_player_state = std::move(other._player_state);
          this->_obstacles.swap(other._obstacles);
          return *this;
        }

        bool is_complete(void) const noexcept {
          return _player_state.is_dead();
        }

        // Level update
        //
        // ...
        const std::pair<Level &&, std::vector<Renderable> &&> update(
          std::optional<std::pair<uint32_t, uint32_t>> &input,
          uint32_t time
        ) const && noexcept {
          std::vector<Renderable> lights;

          // Update the state of the player.
          auto [new_player_state, player_light] = std::move(_player_state).update(input, time);
          _player_state = std::move(new_player_state);

          // Iterate over our obstacles, updating them based on the player position and new state.
          for (auto start = _obstacles.cbegin(); start != _obstacles.cend(); start++) {
            auto [new_state, _, renderables] = std::move(*start).update(time, _player_state);

            // Not all obstacles are renderable.
            if (renderables.size() > 0) {
              for (auto renderable : renderables) {
                lights.push_back(renderable);
              }
            }

            *start = std::move(new_state);
          }

          lights.push_back(player_light);
          return std::make_pair(std::move(*this), std::move(lights));
        }

      private:
        const PlayerState _player_state;
        mutable std::vector<Obstacle> _obstacles;
  };

}
