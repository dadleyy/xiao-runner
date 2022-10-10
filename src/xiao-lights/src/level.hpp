#pragma once

#include <variant>
#include <optional>
#include <numeric>
#include <array>
#include "esp32-hal-log.h"

#include "renderable.hpp"
#include "animation.hpp"
#include "timer.hpp"
#include "obstacle.hpp"
#include "player.hpp"

namespace beetle_lights {
  // TODO(static-light-amount): The level's light buffer here should be able to contain the
  // total amount of LEDs that we have available on our strip. This might also be better as
  // a vector, but the embedded memory constraints (i.e avoiding memory fragmentation) are
  // still being learned.
  using LightBuffer = std::array<std::optional<Renderable>, 100>;

  // TODO(obstacle-count): we're saying here that the most amount of obstacles a level can
  // have - including "corpses" - is 10.
  using ObstacleBuffer = std::array<std::optional<Obstacle>, 10>;

  enum LevelResultKinds {
    PENDING,
    SUCCESS,
    FAILURE,
  };

  class Level final {
      public:
        Level(): Level(std::make_pair("", 0), 0) {}

        explicit Level(std::pair<const char *, uint32_t> level_data, uint32_t bound):
          _boundary(bound),
          _player_state(PlayerState()),
          _obstacles(std::make_unique<ObstacleBuffer>()),
          _lights(std::make_unique<LightBuffer>()),
          _completion_timer(std::make_unique<Animation>()),
          _result(LevelResultKinds::PENDING) {
          auto [layout, size] = level_data;
          const char * cursor = layout;
          uint32_t position = 0, en_count = 0;

          while (*cursor != '\n' && position < size) {
            uint32_t mapped_position = position * 2;

            if (mapped_position > _boundary) {
              log_e("attempting to create level with out-of-bounds obstacle: %d", mapped_position);
              continue;
            }

            auto attempt = Obstacle::try_from(*cursor, mapped_position);

            if (attempt != std::nullopt) {
              log_d("found enemy at position %d (enemy #%d)", mapped_position, en_count);
              (*_obstacles)[en_count].emplace(std::move(*attempt->release()));
              en_count += 1;
            }

            position++;
            cursor++;
          }
        }

        ~Level() = default;

        Level(Level&) = delete;
        Level& operator=(Level&) = delete;
        Level(const Level&) = delete;
        Level& operator=(const Level&) = delete;

        Level(const Level&& other):
          _boundary(other._boundary),
          _player_state(std::move(other._player_state)),
          _obstacles(std::move(other._obstacles)),
          _lights(std::move(other._lights)),
          _completion_timer(std::move(other._completion_timer)),
          _result(other._result)
          { }

        // @kind MovementAssigment
        const Level& operator=(const Level&& other) const noexcept {
          this->_boundary = other._boundary;
          this->_player_state = std::move(other._player_state);
          this->_obstacles = std::move(other._obstacles);
          this->_lights = std::move(other._lights);
          this->_completion_timer = std::move(other._completion_timer);
          this->_result = std::move(other._result);
          return *this;
        }

        const LevelResultKinds result(void) const {
          return _completion_timer->is_done() ? _result : LevelResultKinds::PENDING;
        }

        // ...
        const Level && update(
          const std::optional<std::tuple<uint32_t, uint32_t, uint8_t>> &input,
          uint32_t time
        ) const && noexcept {
          _lights->fill(std::nullopt);

          if (_result != LevelResultKinds::PENDING) {
            _completion_timer = std::make_unique<Animation>(std::move(_completion_timer)->tick(time));

            uint8_t i = 0;
            for (auto s = _completion_timer->lights_start(); s != _completion_timer->lights_end(); s++) {
              if (*s == std::nullopt) {
                continue;
              }
              (*_lights)[i] = s->value();
              i++;
            }

            return std::move(*this);
          }

          // Update the state of the player.
          auto [new_player_state, player_update] = std::move(_player_state).update(input, time);
          _player_state = std::move(new_player_state);

          // Iterate over our obstacles, updating them based on the player position and new state.
          uint16_t frame_light_index = 0;
          for (auto obstacle = _obstacles->begin(); obstacle != _obstacles->end(); obstacle++) {
            if (*obstacle == std::nullopt) {
              continue;
            }

            auto [new_state, new_player_update] = std::move(obstacle->value())
              .update(time, std::move(player_update), 0);

            // With the updated obstacle state, update our own copy of the lights that we will render next frame.
            for (auto ob_light = new_state.first_light(); ob_light != new_state.last_light(); ob_light++) {
              if (*ob_light == std::nullopt) {
                continue;
              }

              (*_lights)[frame_light_index] = ob_light->value();
              frame_light_index += 1;
            }

            obstacle->emplace(std::move(new_state));
            player_update = std::move(new_player_update);
          }

          // Before rendering the player, make sure to apply any updates that came from our obstacles
          // back into the player itself.
          _player_state = std::move(_player_state).apply(player_update);

          // Add the player's lights to our own.
          for (auto pl = _player_state.first_light(); pl != _player_state.last_light(); pl++) {
            if (*pl == std::nullopt) {
              continue;
            }
            (*_lights)[frame_light_index] = pl->value();
            frame_light_index += 1;
          }

          bool is_dead = _player_state.is_dead();
          if (is_dead || std::holds_alternative<GoalReached>(player_update)) {
            _completion_timer = std::make_unique<Animation>(!is_dead);
            _result = is_dead ? LevelResultKinds::FAILURE : LevelResultKinds::SUCCESS;
          }

          return std::move(*this);
        }

        LightBuffer::const_iterator first_light(void) const {
          return _lights->cbegin();
        }

        LightBuffer::const_iterator last_light(void) const {
          return _lights->cend();
        }

      private:
        mutable uint32_t _boundary;
        const PlayerState _player_state;
        mutable std::unique_ptr<ObstacleBuffer> _obstacles;
        mutable std::unique_ptr<LightBuffer> _lights;
        mutable std::unique_ptr<Animation> _completion_timer;
        mutable LevelResultKinds _result;
  };

}
